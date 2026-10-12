#include "movement.hpp"
#include "../eng_pred/eng_pred.hpp"
#include "command_movement.hpp"
bool c_movement::bunnyhop(c_user_cmd* cmd, bool jump_held) {
    if (!g_cfg->misc.m_bunny_hop || !cmd || !cmd->pb.has_base() || !g_ctx->m_local_pawn) return false;
    constexpr std::uint64_t jump = static_cast<std::uint64_t>(IN_JUMP);
    const bool pb_jump = cmd->pb.base().has_buttons_pb() &&
        (cmd->pb.base().buttons_pb().buttonstate1() & jump) != 0;
    const bool requested = (cmd->m_button_state.m_button_state & jump) != 0 || pb_jump || jump_held;
    if (!requested) return false;
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn->is_alive() || pawn->m_actual_move_type() != movetype_walk ||
        (pawn->m_flags() & (FL_INWATER | FL_WATERJUMP))) return false;

    if (pawn->m_flags() & FL_ONGROUND) return false;

    bool changed = false;
    const auto clear_jump = [&changed, jump](std::uint64_t& state) {
        const auto filtered = state & ~jump;
        changed |= filtered != state;
        state = filtered;
    };
    clear_jump(cmd->m_button_state.m_button_state);
    clear_jump(cmd->m_button_state.m_button_state2);
    clear_jump(cmd->m_button_state.m_button_state3);

    auto* base = cmd->pb.mutable_base();
    if (base->has_buttons_pb()) {
        auto* buttons = base->mutable_buttons_pb();
        auto value = buttons->buttonstate1(); clear_jump(value); buttons->set_buttonstate1(value);
        value = buttons->buttonstate2(); clear_jump(value); buttons->set_buttonstate2(value);
        value = buttons->buttonstate3(); clear_jump(value); buttons->set_buttonstate3(value);
    }

    const int count = std::clamp(base->subtick_moves_size(), 0, 64);
    for (int i = 0; i < count; ++i) {
        auto* step = base->mutable_subtick_moves(i);
        if (!step || !(step->button() & jump)) continue;
        step->set_button(step->button() & ~jump);
        if (!step->button()) step->set_pressed(false);
        changed = true;
    }
    return changed;
}













void c_movement::auto_strafe(c_user_cmd* cmd, float previous_yaw) {
    if (!g_cfg->misc.m_auto_strafe || !cmd || !cmd->pb.has_base() || !g_ctx->m_local_pawn ||
        !g_ctx->m_local_pawn->is_alive() ||
        g_ctx->m_local_pawn->m_actual_move_type() != movetype_walk ||
        (g_ctx->m_local_pawn->m_flags() & (FL_ONGROUND | FL_INWATER | FL_WATERJUMP))) return;
    auto* base = cmd->pb.mutable_base();
    const float yaw = base->viewangles().y();
    if (!std::isfinite(yaw)) return;
    const auto velocity = g_ctx->m_local_pawn->m_vec_abs_velocity();
    const float speed_2d = std::hypot(velocity.x, velocity.y);
    if (!std::isfinite(speed_2d) || speed_2d < 5.f) return;
    static std::uint64_t last_buttons = 0;
    static std::uint64_t last_pressed = 0;
    static int substep = 0;
    const std::uint64_t buttons = cmd->m_button_state.m_button_state;
    const auto check = [&](std::uint64_t button) {
        constexpr std::uint64_t l = std::uint64_t(IN_MOVELEFT);
        constexpr std::uint64_t r = std::uint64_t(IN_MOVERIGHT);
        constexpr std::uint64_t f = std::uint64_t(IN_FORWARD);
        constexpr std::uint64_t b = std::uint64_t(IN_BACK);
        if (!(buttons & button)) { last_pressed &= ~button; return; }
        const bool fresh = !(last_buttons & button);
        const bool free = ((button & l) && !(last_pressed & r)) ||
                          ((button & r) && !(last_pressed & l)) ||
                          ((button & f) && !(last_pressed & b)) ||
                          ((button & b) && !(last_pressed & f));
        if (!fresh && !free) return;
        if (button & l) last_pressed &= ~r;
        else if (button & r) last_pressed &= ~l;
        else if (button & f) last_pressed &= ~b;
        else if (button & b) last_pressed &= ~f;
        last_pressed |= button;
    };
    check(std::uint64_t(IN_MOVELEFT));
    check(std::uint64_t(IN_MOVERIGHT));
    check(std::uint64_t(IN_FORWARD));
    check(std::uint64_t(IN_BACK));
    last_buttons = buttons;
    float fwd = 0.f, left = 0.f;
    if (last_pressed & std::uint64_t(IN_FORWARD)) fwd = 1.f;
    else if (last_pressed & std::uint64_t(IN_BACK)) fwd = -1.f;
    if (last_pressed & std::uint64_t(IN_MOVELEFT)) left = -1.f;
    else if (last_pressed & std::uint64_t(IN_MOVERIGHT)) left = 1.f;
    if (fwd == 0.f && left == 0.f) return;
    const float base_offset = std::atan2(-left, fwd) * 180.f / std::numbers::pi_v<float>;
    const float target = movement_math::norm(yaw + base_offset);
    float start = 0.f;
    const int count = base->subtick_moves_size();
    for (int i = 0; i < count && i < 64; ++i) {
        const auto& s = base->subtick_moves(i);
        if (std::isfinite(s.when()) && s.when() > start) start = s.when();
    }
    if (start >= 0.99f) return;
    constexpr int n = 16;
    constexpr float tick = 1.f / 64.f;
    const float frame = tick / float(n);
    const float step_when = (1.f - start) / float(n + 1);
    float acc = yaw;
    float vx = velocity.x, vy = velocity.y;
    for (int i = 1; i <= n; ++i) {
        const bool side = ((substep + i) % 2) == 0;
        const float wish = movement_math::air_strafer(vx, vy, target, frame, side, 260.f);
        const float view = movement_math::norm(wish - base_offset);
        const float delta = movement_math::norm(view - acc);
        if (std::fabs(delta) <= 0.01f) break;
        auto* s = g_protobuf->add_subtick_move_step(cmd);
        if (!s) break;
        s->set_when(std::clamp(start + float(i) * step_when, 0.f, 1.f));
        s->set_yaw_delta(delta);
        acc = view;
        movement_math::air_accel(vx, vy, wish, frame, 260.f);
    }
    ++substep;
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
