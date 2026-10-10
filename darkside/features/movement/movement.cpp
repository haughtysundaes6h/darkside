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
