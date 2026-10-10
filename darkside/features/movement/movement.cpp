#include "movement.hpp"
#include "../eng_pred/eng_pred.hpp"
#include "command_movement.hpp"
bool c_movement::bunnyhop(c_user_cmd* cmd, bool jump_held) {
    if (!g_cfg->misc.m_bunny_hop || !cmd || !cmd->pb.has_base() || !g_ctx->m_local_pawn) return false;
    const bool requested = (cmd->m_button_state.m_button_state & IN_JUMP) != 0 || jump_held;
    if (!requested) return false;
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn->is_alive() || pawn->m_actual_move_type() != movetype_walk ||
        (pawn->m_flags() & (FL_INWATER | FL_WATERJUMP))) return false;
    const auto landing = g_prediction->landing(pawn, cmd);
    const bool grounded = landing.valid ? landing.grounded : (pawn->m_flags() & FL_ONGROUND) != 0;
    auto allocate = [&] { return g_protobuf->add_subtick_move_step(cmd); };
    if (!grounded && landing.will_land && landing.fraction > 0.f && landing.fraction < 1.f)
        return command_movement::landing_jump(*cmd, landing.fraction, allocate);
    return command_movement::set_jump(*cmd, grounded, 0.f, allocate);
}













void c_movement::auto_strafe(c_user_cmd* cmd, float previous_yaw) {
    if (!g_cfg->misc.m_auto_strafe || !cmd || !cmd->pb.has_base() || !g_ctx->m_local_pawn ||
        !g_ctx->m_local_pawn->is_alive() ||
        (g_ctx->m_local_pawn->m_flags() & (FL_ONGROUND | FL_INWATER | FL_WATERJUMP))) return;
    auto* base = cmd->pb.mutable_base();
    const auto buttons = cmd->m_button_state.m_button_state;
    const movement_math::input keys{
        float(bool(buttons & IN_FORWARD)) - float(bool(buttons & IN_BACK)),
        float(bool(buttons & IN_MOVELEFT)) - float(bool(buttons & IN_MOVERIGHT))};
    const auto velocity = g_ctx->m_local_pawn->m_vec_abs_velocity();
    const auto move = movement_math::strafe(base->viewangles().y(), previous_yaw, keys,
        velocity.x, velocity.y, cmd->m_command_number, g_cfg->misc.m_strafe_smooth / 100.f);
    base->set_forwardmove(move.forward);
    base->set_leftmove(move.left);
}

void c_movement::limit_speed( c_user_cmd* user_cmd, c_cs_player_pawn* local_player, c_base_player_weapon* active_weapon, float max_speed ) {
	if ( !user_cmd || !local_player || !active_weapon || !user_cmd->pb.has_base( ) )
		return;
	auto* base = user_cmd->pb.mutable_base( );
	const float weapon_speed = active_weapon->get_max_speed( );
	if ( !std::isfinite( weapon_speed ) || weapon_speed <= 0.f || !std::isfinite(max_speed) )
		return;
	max_speed = std::clamp( max_speed, 0.f, weapon_speed );
	auto velocity = local_player->m_vec_abs_velocity( );
	const float speed = velocity.length_2d( );
	if (!std::isfinite(speed)) return;
	if ( speed > max_speed + 2.f ) {
		const float yaw = deg2rad( base->viewangles( ).y( ) );
		const float sine = std::sin( yaw );
		const float cosine = std::cos( yaw );
		const float strength = std::clamp( ( speed - max_speed ) / ( weapon_speed * 0.12f ), 0.f, 1.f );
		base->set_forwardmove( std::clamp( -( velocity.x * cosine + velocity.y * sine ) / speed * strength, -1.f, 1.f ) );
		base->set_leftmove( std::clamp( ( velocity.x * sine - velocity.y * cosine ) / speed * strength, -1.f, 1.f ) );
		return;
	}
	const float input_limit = std::clamp( max_speed / weapon_speed, 0.f, 1.f );
	const float forward = base->forwardmove( );
	const float side = base->leftmove( );
	const float magnitude = std::hypot( forward, side );
	if ( magnitude > input_limit && magnitude > 0.f ) {
		const float scale = input_limit / magnitude;
		base->set_forwardmove( forward * scale );
		base->set_leftmove( side * scale );
	}
}

void c_movement::auto_stop( c_user_cmd* user_cmd, c_cs_player_pawn* local_player, c_base_player_weapon* active_weapon, bool no_spread, bool enabled ) {
	if ( !enabled )
		return;

	if ( no_spread )
		return;

	if ( !user_cmd || !local_player || !active_weapon || !( local_player->m_flags( ) & FL_ONGROUND ) )
		return;

	auto remove_button = [ & ]( int button ) {
		user_cmd->m_button_state.m_button_state &= ~button;
		user_cmd->m_button_state.m_button_state2 &= ~button;
		user_cmd->m_button_state.m_button_state3 &= ~button;
	};

	remove_button( IN_SPEED );

	float wish_speed = active_weapon->get_max_speed( ) * 0.25f;

	limit_speed( user_cmd, local_player, active_weapon, wish_speed );
}

void c_movement::movement_fix(c_user_cmd* cmd, vec3_t original_angles) {
    if (!cmd || !cmd->pb.has_base() || !g_ctx->m_local_pawn ||
        g_ctx->m_local_pawn->m_actual_move_type() != movetype_walk) return;
    auto* base = cmd->pb.mutable_base();
    const auto move = movement_math::bounded(movement_math::rotate(
        {base->forwardmove(), base->leftmove()}, original_angles.y, base->viewangles().y()));
    base->set_forwardmove(move.forward);
    base->set_leftmove(move.left);
}

void c_movement::on_create_move(c_user_cmd* cmd) {
    if (!cmd || !cmd->pb.has_base() || !g_ctx->m_local_pawn || !g_ctx->m_local_pawn->is_alive()) return;
    const float previous_yaw = m_yaw_history.sample(g_ctx->m_local_pawn, cmd->m_command_number,
        cmd->pb.base().viewangles().y());
    if (g_ctx->m_local_pawn->m_actual_move_type() != movetype_walk) return;
    auto_strafe(cmd, previous_yaw);
}

bool c_movement::read_movement_analog( c_cs_player_pawn* pawn, float& forward, float& left ) const {
	if ( !pawn )
		return false;
	auto* movement = pawn->m_movement_services();
	if ( !movement )
		return false;
	static const auto forward_offset = schema_get_offset( "CPlayer_MovementServices", "m_flCmdForwardMove" );
	static const auto left_offset = schema_get_offset( "CPlayer_MovementServices", "m_flCmdLeftMove" );
	if ( !forward_offset || !left_offset )
		return false;
	const auto* raw = reinterpret_cast<const std::uint8_t*>( movement );
	forward = *reinterpret_cast<const float*>( raw + forward_offset );
	left = *reinterpret_cast<const float*>( raw + left_offset );
	return std::isfinite( forward ) && std::isfinite( left );
}

bool c_movement::sync_subtick_movement(c_user_cmd* cmd, float prior_forward, float prior_left) {
    if (!cmd || !cmd->pb.has_base()) return false;
    return command_movement::analog(*cmd->pb.mutable_base(), prior_forward, prior_left,
        [&] { return g_protobuf->add_subtick_move_step(cmd); });
}


// ============================================================================
// ADVANCED MOVEMENT IMPLEMENTATIONS
// ============================================================================

void c_movement::edge_jump(c_user_cmd* user_cmd, c_cs_player_pawn* local_player) {
    if (!g_cfg->misc.m_edge_jump)
        return;
    
    if (!g_key_handler->is_pressed(g_cfg->misc.m_edge_jump_key, g_cfg->misc.m_edge_jump_key_style))
        return;
    
    if (!local_player || !local_player->is_alive())
        return;
    
    bool on_ground = (local_player->m_flags() & FL_ONGROUND) != 0;
    
    // Check for edge
    if (on_ground && is_at_edge(local_player)) {
        // Jump!
        user_cmd->m_button_state.m_button_state |= IN_JUMP;
        m_edge_detected = true;
    }
    
    m_was_on_ground = on_ground;
    m_last_origin = local_player->m_scene_node()->m_abs_origin();
}

bool c_movement::is_at_edge(c_cs_player_pawn* local_player) {
    if (!local_player)
        return false;
    
    vec3_t origin = local_player->m_scene_node()->m_abs_origin();
    vec3_t velocity = local_player->m_vec_abs_velocity();
    
    // Calculate predicted position
    float frametime = g_interfaces->m_global_vars->m_frame_time;
    vec3_t predicted = origin + velocity * frametime;
    
    // Simple edge detection - check if we'll leave ground
    // In a real implementation, you'd do a trace here
    
    float speed = velocity.length_2d();
    if (speed < 50.0f)
        return false;
    
    // Check if moving towards edge
    vec3_t forward = velocity.normalize_2d();
    vec3_t check_pos = origin + forward * 20.0f;
    
    // Simplified: consider at edge if height difference ahead
    // Would need proper tracing in production
    return speed > 100.0f && (local_player->m_flags() & FL_ONGROUND);
}

void c_movement::jump_bug(c_user_cmd* user_cmd, c_cs_player_pawn* local_player) {
    if (!g_cfg->misc.m_jump_bug)
        return;
    
    if (!g_key_handler->is_pressed(g_cfg->misc.m_jump_bug_key, g_cfg->misc.m_jump_bug_key_style))
        return;
    
    if (!local_player || !local_player->is_alive())
        return;
    
    bool on_ground = (local_player->m_flags() & FL_ONGROUND) != 0;
    
    if (on_ground) {
        m_jumpbug_should_duck = false;
        m_jumpbug_tick = 0;
        return;
    }
    
    // Check if we're falling
    float fall_velocity = local_player->m_vec_abs_velocity().z;
    if (fall_velocity >= 0)
        return;
    
    int ticks_until_landing = 0;
    if (predict_landing(local_player, ticks_until_landing)) {
        // 1 tick before landing: release duck
        // On landing tick: duck and jump
        if (ticks_until_landing <= 2) {
            if (ticks_until_landing == 2) {
                // Duck
                user_cmd->m_button_state.m_button_state |= IN_DUCK;
            } else if (ticks_until_landing == 1) {
                // Release duck, prepare jump
                user_cmd->m_button_state.m_button_state &= ~IN_DUCK;
            } else {
                // Jump frame
                user_cmd->m_button_state.m_button_state |= IN_JUMP;
            }
        }
    }
}

bool c_movement::predict_landing(c_cs_player_pawn* local_player, int& ticks_until_landing) {
    if (!local_player)
        return false;
    
    vec3_t origin = local_player->m_scene_node()->m_abs_origin();
    vec3_t velocity = local_player->m_vec_abs_velocity();
    
    // Simple gravity simulation
    float gravity = 800.0f; // sv_gravity default
    float tickrate = 1.0f / 64.0f; // 64 tick
    
    vec3_t predicted = origin;
    vec3_t pred_vel = velocity;
    
    for (int i = 0; i < 64; i++) {
        pred_vel.z -= gravity * tickrate;
        predicted = predicted + pred_vel * tickrate;
        
        // Check if we hit ground (simplified)
        // Would need proper tracing
        if (predicted.z <= origin.z - 10.0f && pred_vel.z < -100.0f) {
            ticks_until_landing = i;
            return true;
        }
    }
    
    return false;
}

void c_movement::edge_bug(c_user_cmd* user_cmd, c_cs_player_pawn* local_player) {
    if (!g_cfg->misc.m_edge_bug)
        return;
    
    if (!g_key_handler->is_pressed(g_cfg->misc.m_edge_bug_key, g_cfg->misc.m_edge_bug_key_style))
        return;
    
    if (!local_player || !local_player->is_alive())
        return;
    
    bool on_ground = (local_player->m_flags() & FL_ONGROUND) != 0;
    
    if (on_ground) {
        m_edgebug_active = false;
        return;
    }
    
    vec3_t velocity = local_player->m_vec_abs_velocity();
    
    // Only when falling
    if (velocity.z >= 0)
        return;
    
    // Detect edge
    if (is_on_edge(local_player)) {
        // Stop horizontal movement to stick to edge
        auto* base = user_cmd->pb.mutable_base();
        base->set_forwardmove(0);
        base->set_leftmove(0);
        
        // Duck to catch edge
        user_cmd->m_button_state.m_button_state |= IN_DUCK;
        
        m_edgebug_active = true;
    }
}

bool c_movement::is_on_edge(c_cs_player_pawn* local_player) {
    // Simplified edge detection
    // In real implementation, trace rays to detect surfaces
    return false;
}

void c_movement::long_jump(c_user_cmd* user_cmd, c_cs_player_pawn* local_player) {
    // Long jump technique: crouch before jump for speed boost
    // Implementation depends on game mechanics
}

void c_movement::infinite_duck(c_user_cmd* user_cmd, c_cs_player_pawn* local_player) {
    if (!g_cfg->misc.m_infinite_duck)
        return;
    
    if (!local_player || !local_player->is_alive())
        return;
    
    // Toggle duck each tick to reset duck stamina
    static bool duck_state = false;
    
    if (user_cmd->m_button_state.m_button_state & IN_DUCK) {
        if (duck_state) {
            // Keep ducked
        } else {
            duck_state = true;
        }
    }
}

// ============================================================================
// STRAFE IMPLEMENTATIONS
// ============================================================================

void c_movement::legit_strafe(c_user_cmd* user_cmd, c_cs_player_pawn* local_player) {
    if (!local_player || !local_player->is_alive())
        return;
    
    if (local_player->m_flags() & FL_ONGROUND)
        return;
    
    vec3_t velocity = local_player->m_vec_abs_velocity();
    float speed = velocity.length_2d();
    
    if (speed < 15.0f)
        return;
    
    // Apply smooth strafing based on mouse movement
    // This is a legit-looking strafe that follows mouse input
    
    auto* base = user_cmd->pb.mutable_base();
    float yaw = base->viewangles().y();
    
    // Calculate velocity angle
    float vel_angle = rad2deg(std::atan2(velocity.y, velocity.x));
    float delta = g_math->normalize_yaw(yaw - vel_angle);
    
    // Apply strafe input based on delta
    float strafe_strength = std::clamp(std::abs(delta) / 90.0f, 0.0f, 1.0f);
    
    if (delta > 0) {
        base->set_leftmove(-strafe_strength);
    } else {
        base->set_leftmove(strafe_strength);
    }
}

void c_movement::directional_strafe(c_user_cmd* user_cmd, c_cs_player_pawn* local_player, float old_yaw) {
    if (!local_player || !local_player->is_alive())
        return;
    
    if (local_player->m_flags() & FL_ONGROUND)
        return;
    
    auto* base = user_cmd->pb.mutable_base();
    float current_yaw = base->viewangles().y();
    float yaw_delta = g_math->normalize_yaw(current_yaw - old_yaw);
    
    vec3_t velocity = local_player->m_vec_abs_velocity();
    float speed = velocity.length_2d();
    
    if (speed < 15.0f)
        return;
    
    // Optimal strafe angle calculation
    float max_speed = get_max_speed(local_player);
    float wish_angle = std::clamp(std::asin(30.0f / speed), 0.0f, 1.5707f);
    float strafe_angle = rad2deg(wish_angle);
    
    // Apply strafe based on yaw delta direction
    if (yaw_delta > 0) {
        base->set_leftmove(-1.0f);
    } else if (yaw_delta < 0) {
        base->set_leftmove(1.0f);
    }
}

void c_movement::rage_strafe(c_user_cmd* user_cmd, c_cs_player_pawn* local_player, float old_yaw) {
    if (!local_player || !local_player->is_alive())
        return;
    
    if (local_player->m_flags() & FL_ONGROUND)
        return;
    
    auto* base = user_cmd->pb.mutable_base();
    vec3_t velocity = local_player->m_vec_abs_velocity();
    float speed = velocity.length_2d();
    
    if (speed < 15.0f) {
        base->set_forwardmove(1.0f);
        return;
    }
    
    // Calculate optimal strafe
    float tick_interval = g_interfaces->m_global_vars->m_interval_per_tick;
    float accel = 5.5f * tick_interval * 320.0f;
    
    float wish_angle = std::clamp(std::asin(accel / speed), 0.0f, 1.5707f);
    float vel_angle = std::atan2(velocity.y, velocity.x);
    
    float current_yaw = deg2rad(base->viewangles().y());
    float delta = g_math->normalize_yaw(rad2deg(current_yaw - vel_angle));
    
    // Alternate strafe direction for maximum speed
    static bool strafe_right = false;
    strafe_right = !strafe_right;
    
    if (strafe_right) {
        base->set_leftmove(-1.0f);
        base->mutable_viewangles()->set_y(g_math->normalize_yaw(rad2deg(vel_angle + wish_angle)));
    } else {
        base->set_leftmove(1.0f);
        base->mutable_viewangles()->set_y(g_math->normalize_yaw(rad2deg(vel_angle - wish_angle)));
    }
    
    base->set_forwardmove(0.0f);
}

// ============================================================================
// HELPERS
// ============================================================================

float c_movement::get_max_speed(c_cs_player_pawn* local_player) {
    if (!local_player)
        return 250.0f;
    
    auto* weapon = local_player->get_active_weapon();
    if (!weapon)
        return 250.0f;
    
    return weapon->get_max_speed();
}

bool c_movement::is_on_ground(c_cs_player_pawn* local_player) {
    if (!local_player)
        return false;
    
    return (local_player->m_flags() & FL_ONGROUND) != 0;
}

vec3_t c_movement::predict_position(c_cs_player_pawn* local_player, int ticks) {
    if (!local_player)
        return {};
    
    vec3_t origin = local_player->m_scene_node()->m_abs_origin();
    vec3_t velocity = local_player->m_vec_abs_velocity();
    
    float tickrate = g_interfaces->m_global_vars->m_interval_per_tick;
    float gravity = 800.0f;
    
    vec3_t predicted = origin;
    vec3_t pred_vel = velocity;
    
    for (int i = 0; i < ticks; i++) {
        pred_vel.z -= gravity * tickrate;
        predicted = predicted + pred_vel * tickrate;
    }
    
    return predicted;
}

float c_movement::get_fall_velocity(c_cs_player_pawn* local_player) {
    if (!local_player)
        return 0.0f;
    
    return -local_player->m_vec_abs_velocity().z;
}
