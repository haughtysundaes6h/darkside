#include <cmath>
#include <mutex>
#include "../darkside.hpp"
#include "../directx/directx.hpp"
#include "../features/anti_hit/anti_hit.hpp"
#include "../features/movement/movement.hpp"
#include "../features/eng_pred/eng_pred.hpp"
#include "../entity_system/entity.hpp"
#include "../features/skins/skins.hpp"
#include "../features/visuals/world.hpp"

#include "../valve/classes/c_envy_sky.hpp"
#include "../features/visuals/visuals.hpp"
#include "../features/rage_bot/rage_bot.hpp"

#include "../valve/cs2_build.hpp"
#include "../features/protobufs/command_input.hpp"
#include "../features/movement/command_movement.hpp"
using namespace hooks;
bool c_hooks::initialize() {
    const auto status = MH_Initialize();
    if (status != MH_OK) { LOG_ERROR("MinHook initialization failed: %s", MH_StatusToString(status)); return false; }
    bool hooks_ok = true;
	hooks_ok &= create_move::m_create_move.hook( vmt::get_v_method( g_interfaces->m_csgo_input, 5 ), create_move::hk_create_move );
	hooks_ok &= validate_view_angles::m_validate_view_angles.hook( vmt::get_v_method( g_interfaces->m_csgo_input, 8 ), validate_view_angles::hk_validate_view_angles );
	hooks_ok &= enable_cursor::m_enable_cursor.hook( vmt::get_v_method( g_interfaces->m_input_system, 76 ), enable_cursor::hk_enable_cursor );
	hooks_ok &= mouse_input_enabled::m_mouse_input_enabled.hook( g_opcodes->scan( g_modules->m_modules.client_dll.get_name(), "40 53 48 83 EC ? 80 B9 ? ? ? ? ? 48 8B D9 75 ? 48 8B 0D ? ? ? ? 48 8B 01"), mouse_input_enabled::hk_mouse_input_enabled);
	hooks_ok &= present::m_present.hook(g_directx->m_present_address, present::hk_present);
	hooks_ok &= update_global_vars::m_update_global_vars.hook( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "48 8B 0D ? ? ? ? 4C 8D 05 ? ? ? ? 48 85 D2" ), update_global_vars::hk_update_global_vars );
	hooks_ok &= frame_stage_notify::m_frame_stage_notify.hook( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "48 89 5C 24 ? 48 89 6C 24 ? 57 48 83 EC 40 48 8B F9 33 ED" ), frame_stage_notify::hk_frame_stage_notify ); // x-ref to "FramePostDataUpdate(%.3f %d)", first mov in function
	hooks_ok &= override_view::m_override_view.hook( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "53 57 48 83 EC 58 48 8B FA E8 ? ? ? ? 48 8B 05 ? ? ? ? 80 78 58 00" ), override_view::hk_override_view );
	hooks_ok &= on_add_entity::m_on_add_entity.hook( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "48 89 74 24 ? 57 48 83 EC ? 41 B9 ? ? ? ? 41 8B C0 41 23 C1 48 8B F2 41 83 F8 ? 48 8B F9 44 0F 45 C8 41 81 F9 ? ? ? ? 73 ? FF 81" ), on_add_entity::hk_on_add_entity );
	hooks_ok &= on_remove_entity::m_on_remove_entity.hook( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "48 89 74 24 ? 57 48 83 EC ? 41 B9 ? ? ? ? 41 8B C0 41 23 C1 48 8B F2 41 83 F8 ? 48 8B F9 44 0F 45 C8 41 81 F9 ? ? ? ? 73 ? FF 89" ), on_remove_entity::hk_on_remove_entity );
	hooks_ok &= on_level_init::m_on_level_init.hook( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "40 55 56 41 56 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B 01" ), on_level_init::hk_on_level_init );
	hooks_ok &= on_level_shutdown::m_on_level_shutdown.hook( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "48 83 EC ? 48 8B 0D ? ? ? ? 48 8D 15 ? ? ? ? 45 33 C9 45 33 C0 48 8B 01 FF 50 ? 48 85 C0 74 ? 48 8B 0D ? ? ? ? 48 8B D0 4C 8B 01 41 FF 50 ? 48 83 C4" ), on_level_shutdown::hk_on_level_shutdown );
	hooks_ok &= draw_light_scene::m_draw_light_scene.hook( g_opcodes->scan( g_modules->m_modules.scenesystem_dll.get_name( ), "48 89 54 24 ?? 55 57 41 56 48 83 EC" ), draw_light_scene::hk_draw_light_scene );
	hooks_ok &= update_aggregate_scene_object::m_update_aggregate_scene_object.hook( g_opcodes->scan( g_modules->m_modules.scenesystem_dll.get_name( ), "48 8B C4 48 89 50 ? 48 89 48 ? 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 70" ), update_aggregate_scene_object::hk_update_aggregate_scene_object );
	hooks_ok &= draw_aggregate_scene_object::m_draw_aggregate_scene_object.hook( g_opcodes->scan( g_modules->m_modules.scenesystem_dll.get_name( ), "48 8B C4 4C 89 40 ? 48 89 50 ? 55 53 41 57" ), draw_aggregate_scene_object::hk_draw_aggregate_scene_object );
	hooks_ok &= draw_skybox_array::m_draw_skybox_array.hook( g_opcodes->scan( g_modules->m_modules.scenesystem_dll.get_name( ), "45 85 C9 0F 8E ? ? ? ? 4C 8B DC" ), draw_skybox_array::hk_draw_skybox_array );
	hooks_ok &= should_draw_legs::m_should_draw_legs.hook( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "4C 8B DC 55 53 56 57 41 57 49 8D AB ? ? ? ? 48 81 EC ? ? ? ? 8B 42 ? 4D 8B F9 F2 0F 10 42" ), should_draw_legs::hk_should_draw_legs );
	hooks_ok &= mark_interp_latch_flags_dirty::m_mark_interp_latch_flags_dirty.hook(g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), "40 53 56 57 48 83 EC ? 80 3D"), mark_interp_latch_flags_dirty::hk_mark_interp_latch_flags_dirty);
	hooks_ok &= draw_scope_overlay::m_draw_scope_overlay.hook( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "48 8B C4 53 57 48 83 EC ? 48 8B FA" ), draw_scope_overlay::hk_draw_scope_overlay );
	hooks_ok &= get_field_of_view::m_get_field_of_view.hook( g_modules->m_modules.client_dll.get() + cs2_build::get_field_of_view, get_field_of_view::hk_get_field_of_view );
    hooks_ok &= setup_fog::m_setup_fog.hook(g_opcodes->scan(g_modules->m_modules.client_dll.get_name(),
        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 4C 24 ? 57 41 54 41 55 41 56 41 57 48 83 EC 20 48 63 02"), setup_fog::hk_setup_fog);
    if (hooks_ok) hooks_ok = MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
    if (!hooks_ok) { MH_Uninitialize(); LOG_ERROR("Hook initialization failed"); return false; }
    LOG_INFO("Hooks initialized");
    return true;
}
void c_hooks::destroy( ) {
	hooks::enable_cursor::unhook( );
	g_directx->unitialize( );

	create_move::m_create_move.unhook( );
	validate_view_angles::m_validate_view_angles.unhook( );
	enable_cursor::m_enable_cursor.unhook( );
	mouse_input_enabled::m_mouse_input_enabled.unhook( );
	present::m_present.unhook( );
	update_global_vars::m_update_global_vars.unhook( );
	frame_stage_notify::m_frame_stage_notify.unhook( );
	override_view::m_override_view.unhook( );
	on_add_entity::m_on_add_entity.unhook( );
	on_remove_entity::m_on_remove_entity.unhook( );
	on_level_init::m_on_level_init.unhook( );
	on_level_shutdown::m_on_level_shutdown.unhook( );
	draw_light_scene::m_draw_light_scene.unhook( );
	update_aggregate_scene_object::m_update_aggregate_scene_object.unhook( );
	draw_aggregate_scene_object::m_draw_aggregate_scene_object.unhook( );
	draw_skybox_array::m_draw_skybox_array.unhook( );
	should_update_sequences::m_should_update_sequences.unhook( );
	should_draw_legs::m_should_draw_legs.unhook( );
	mark_interp_latch_flags_dirty::m_mark_interp_latch_flags_dirty.unhook( );
	draw_scope_overlay::m_draw_scope_overlay.unhook( );
	get_field_of_view::m_get_field_of_view.unhook( );
	setup_fog::m_setup_fog.unhook();

	MH_Uninitialize( );
}
vec3_t calculate_camera_pos( vec3_t anchor_pos, float distance, vec3_t view_angles ) {
	float yaw = DirectX::XMConvertToRadians( view_angles.y );
	float pitch = DirectX::XMConvertToRadians( view_angles.x );

	float x = anchor_pos.x + distance * cosf( yaw ) * cosf( pitch );
	float y = anchor_pos.y + distance * sinf( yaw ) * cosf( pitch );
	float z = anchor_pos.z + distance * sinf( pitch );

	return vec3_t{ x, y, z };
}
enum e_model_type : int { MODEL_SUN, MODEL_EFFECTS, MODEL_OTHER };
int get_model_type( const std::string_view& name ) {
	if ( name.find( "sun" ) != std::string::npos
		|| name.find( "clouds" ) != std::string::npos )
		return MODEL_SUN;

	if ( name.find( "effects" ) != std::string::npos )
		return MODEL_EFFECTS;

	return MODEL_OTHER;
}
namespace {
bool read_sky_descriptor(void* meshes, int count, void*& descriptor) {
    __try {
        auto* slot = static_cast<std::byte*>(meshes) + static_cast<std::size_t>(count) * 0x70 - 0x58;
        descriptor = *reinterpret_cast<void**>(slot);
        return descriptor != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool sky_tint(void* descriptor, float (&tint)[3], bool write) {
    __try {
        auto* target = reinterpret_cast<float*>(static_cast<std::byte*>(descriptor) + 0xE8);
        if (write) std::memcpy(target,tint,sizeof(tint));
        else std::memcpy(tint,target,sizeof(tint));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}

void __fastcall hooks::create_move::hk_create_move(i_csgo_input* rcx, int slot, bool active) {
	static auto original = m_create_move.get_original< decltype(&hk_create_move) >();
	#ifdef _DEBUG
	static bool logged_entry = false;
	if (!logged_entry) {
		LOG("bhop diag: CreateMove entered slot=%d active=%d", slot, active);
		logged_entry = true;
	}
	#endif
	c_user_cmd* pre_cmd = nullptr;
	const bool jump_held=(rcx->held_buttons() & IN_JUMP)!=0;
	g_ctx->m_local_pawn = g_interfaces->m_entity_system->get_local_pawn();
	g_ctx->m_local_controller = g_interfaces->m_entity_system->get_local_controller();
	int pre_command_number = -1;
	auto* pre_pawn = g_ctx->m_local_pawn;
	const int pre_flags = pre_pawn ? pre_pawn->m_flags() : 0;
	const vec3_t pre_velocity = pre_pawn ? pre_pawn->m_vec_abs_velocity() : vec3_t{};
	float prior_cmd_forward=0.f, prior_cmd_left=0.f;
	const bool analog_prestate_valid=g_movement->read_movement_analog(pre_pawn, prior_cmd_forward, prior_cmd_left);
	if (g_ctx->m_local_controller && g_ctx->m_local_pawn) {
		#ifdef _DEBUG
		static bool logged_local = false;
		if (!logged_local) {
			LOG("bhop diag: local ready cfg=%d", g_cfg->misc.m_bunny_hop);
			logged_local = true;
		}
		static bool logged_enabled = false;
		if (g_cfg->misc.m_bunny_hop && !logged_enabled) {
			LOG("bhop diag: config enabled");
			logged_enabled = true;
		}
		#endif
		auto* current_cmd = rcx->get_user_cmd(g_ctx->m_local_controller);
		if (current_cmd) {
			#ifdef _DEBUG
			static bool logged_command = false;
			if (!logged_command) {
				LOG("bhop diag: command ready number=%d", current_cmd->m_command_number);
				logged_command = true;
			}
			#endif
			pre_cmd = current_cmd;
			pre_command_number = current_cmd->m_command_number;
		}
	}
	// RVA B6651E reuses type-2 commands; RVA B667F6 also sets type 2 on new commands.
	// This value is an engine command mode, not a reason to skip movement edits.
	original(rcx, slot, active);
	g_ctx->m_user_cmd=nullptr;
	g_ctx->m_local_pawn=g_interfaces->m_entity_system->get_local_pawn();
	g_ctx->m_local_controller=g_interfaces->m_entity_system->get_local_controller();
	if (!active || slot != 0) {
		#ifdef _DEBUG
		static int skipped_diagnostics = 0;
		if ( (g_cfg->misc.m_bunny_hop || g_cfg->misc.m_auto_strafe) && skipped_diagnostics < 8 ) {
			++skipped_diagnostics;
			LOG("move skipped: active=%d slot=%d pre_command=%d", active, slot, pre_command_number);
		}
		#endif
		return;
	}

	const auto process_current_command = [&]() {

	if (!g_ctx->m_local_controller)
		return;

	std::int32_t command_sequence = -1;
	auto user_cmd = g_ctx->m_user_cmd = rcx->get_user_cmd(g_ctx->m_local_controller, &command_sequence);
	if (!user_cmd) {
		#ifdef _DEBUG
		static ULONGLONG last_lookup_failure = 0;
		const auto now = GetTickCount64();
		if (now - last_lookup_failure >= 5000) {
			last_lookup_failure = now;
			LOG_ERROR("command lookup failed: controller=%p pawn=%p input=%p sequence=%d", g_ctx->m_local_controller,
				g_ctx->m_local_pawn, rcx, command_sequence);
		}
		#endif
		return;
	}
	if (!user_cmd->pb.has_base())
		return;
	if (!g_ctx->m_local_pawn)
		return;

	const vec3_t old_view_angles = {
		user_cmd->pb.mutable_base()->viewangles().x(),
		user_cmd->pb.mutable_base()->viewangles().y(),
		user_cmd->pb.mutable_base()->viewangles().z()
	};
	const auto old_button_state = user_cmd->m_button_state.m_button_state;
	const auto old_button_state2 = user_cmd->m_button_state.m_button_state2;
	const auto old_button_state3 = user_cmd->m_button_state.m_button_state3;
	const float old_forward_move = user_cmd->pb.mutable_base()->forwardmove();
	const float old_left_move = user_cmd->pb.mutable_base()->leftmove();
	const float old_up_move = user_cmd->pb.mutable_base()->upmove();

	bool menu_modified = g_menu->on_create_move();

	if (pre_pawn == g_ctx->m_local_pawn)
		g_prediction->capture_movement_prestate(g_ctx->m_local_pawn, user_cmd, pre_flags, pre_velocity);
	else
		g_prediction->capture_movement_prestate(g_ctx->m_local_pawn, user_cmd,
			g_ctx->m_local_pawn->m_flags(), g_ctx->m_local_pawn->m_vec_abs_velocity());
	const bool bhop_modified = !g_menu->m_opened &&
		g_movement->bunnyhop(user_cmd,jump_held);
	g_movement->on_create_move(user_cmd);

	g_prediction->run();
	g_anti_hit->on_create_move(user_cmd);
	g_movement->movement_fix(user_cmd, old_view_angles);
	const vec3_t aim_movement_angles{user_cmd->pb.base().viewangles().x(),
		user_cmd->pb.base().viewangles().y(), user_cmd->pb.base().viewangles().z()};

	if (!g_menu->blocks_attacks()) {
		g_rage_bot->on_create_move();
	}

	g_prediction->end();

	g_movement->movement_fix(user_cmd, aim_movement_angles);
	if (g_menu->blocks_attacks())
		menu_modified |= command_input::suppress_attacks(*user_cmd);

	const auto& final_angles = user_cmd->pb.mutable_base()->viewangles();
	const bool angles_modified = old_view_angles.x != final_angles.x() ||
		old_view_angles.y != final_angles.y() || old_view_angles.z != final_angles.z();
	const auto* final_base = user_cmd->pb.mutable_base();
	const bool movement_modified = std::abs( old_forward_move - final_base->forwardmove() ) > 0.00001f ||
		std::abs( old_left_move - final_base->leftmove() ) > 0.00001f ||
		std::abs( old_up_move - final_base->upmove() ) > 0.00001f;
	bool movement_synced = false;
	if (movement_modified) {
		movement_synced = analog_prestate_valid && pre_pawn==g_ctx->m_local_pawn && g_movement->sync_subtick_movement(
			user_cmd, prior_cmd_forward, prior_cmd_left);
		auto& state = user_cmd->m_button_state;
		auto& buttons = state.m_button_state;
		const auto previous_directions=buttons;
		buttons &= ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT);
		if (final_base->forwardmove() > 0.01f)
			buttons |= IN_FORWARD;
		else if (final_base->forwardmove() < -0.01f)
			buttons |= IN_BACK;
		if (final_base->leftmove() > 0.01f)
			buttons |= IN_MOVELEFT;
		else if (final_base->leftmove() < -0.01f)
			buttons |= IN_MOVERIGHT;
		state.m_button_state2 |= (previous_directions ^ buttons) & command_movement::directions;
		state.m_button_state3 &= ~command_movement::directions;
	}
	if (bhop_modified || movement_modified)
		g_protobuf->sort_subtick_move_steps(user_cmd);
	const bool buttons_modified = bhop_modified ||
		old_button_state != user_cmd->m_button_state.m_button_state ||
		old_button_state2 != user_cmd->m_button_state.m_button_state2 ||
		old_button_state3 != user_cmd->m_button_state.m_button_state3;
	if (buttons_modified) {
		auto* pb_buttons = user_cmd->pb.mutable_base()->mutable_buttons_pb();
		pb_buttons->set_buttonstate1(user_cmd->m_button_state.m_button_state);
		pb_buttons->set_buttonstate2(user_cmd->m_button_state.m_button_state2);
		pb_buttons->set_buttonstate3(user_cmd->m_button_state.m_button_state3);
	}
	bool crc_ok=true;
	if (menu_modified || buttons_modified || angles_modified || movement_modified)
		crc_ok=g_protobuf->update_move_crc(user_cmd);
	if (!crc_ok) {
		#ifdef _DEBUG
		static bool logged_crc_failure = false;
		if (!logged_crc_failure) {
			LOG_ERROR("user command CRC update failed");
			logged_crc_failure = true;
		}
		#endif
	}
	#ifdef _DEBUG
	static ULONGLONG last_movement_log=0;
	static unsigned last_movement_config=0;
	const unsigned movement_config=unsigned(g_cfg->misc.m_bunny_hop) | (unsigned(g_cfg->misc.m_auto_strafe)<<1);
	const auto log_time=GetTickCount64();
	if (movement_config && (movement_config!=last_movement_config || log_time-last_movement_log>=1000)) {
		last_movement_log=log_time;
		float jump_when=-1.f;
		for (int i=0;i<final_base->subtick_moves_size() && i<64;++i) {
			const auto& step=final_base->subtick_moves(i);
			if ((step.button() & IN_JUMP) && step.pressed()) jump_when=step.when();
		}
		const auto velocity=g_ctx->m_local_pawn->m_vec_abs_velocity();
		LOG("move diag: cmd=%d seq=%d same=%d cfg=%u raw=(%.3f,%.3f) final=(%.3f,%.3f) prior=(%.3f,%.3f) sync=%d crc=%d held_jump=%d jump=%d when=%.4f ground=%d speed=%.1f vz=%.1f steps=%d",
			user_cmd->m_command_number,command_sequence,pre_cmd==user_cmd,movement_config,
			old_forward_move,old_left_move,final_base->forwardmove(),final_base->leftmove(),
			prior_cmd_forward,prior_cmd_left,movement_synced,crc_ok,
			jump_held,
			(user_cmd->m_button_state.m_button_state & IN_JUMP)!=0,jump_when,
			(g_ctx->m_local_pawn->m_flags() & FL_ONGROUND)!=0,std::hypot(velocity.x,velocity.y),velocity.z,
			final_base->subtick_moves_size());
	}
	last_movement_config=movement_config;
	#endif
	};
	process_current_command();
}

bool hooks::mouse_input_enabled::hk_mouse_input_enabled( void* ptr ) {
	static auto original = m_mouse_input_enabled.get_original< decltype( &hk_mouse_input_enabled ) >( );
	return g_menu->m_opened ? false : original( ptr );
}

void* hooks::enable_cursor::hk_enable_cursor( void* rcx, bool active ) {
	static auto original = m_enable_cursor.get_original< decltype( &hk_enable_cursor ) >( );

	m_enable_cursor_input = active;
	if ( g_menu->m_opened )
		active = false;

	return original( rcx, active );
}

void hooks::enable_cursor::unhook( ) {
	static auto original = m_enable_cursor.get_original< decltype( &hk_enable_cursor ) >( );

	original( g_interfaces->m_input_system, m_enable_cursor_input );
}

HRESULT hooks::present::hk_present( IDXGISwapChain* swap_chain, unsigned int sync_interval, unsigned int flags ) {
	static auto original = m_present.get_original< decltype( &hk_present ) >( );

	g_directx->start_frame( swap_chain );

	g_directx->new_frame( );
	{
		g_render->update_background_drawlist( ImGui::GetBackgroundDrawList( ) );
		g_visuals->on_present( );
		g_world->draw_scope_overlay( );
		g_menu->draw( );
	}
	g_directx->end_frame( );

	return original( swap_chain, sync_interval, flags );
}

void hooks::validate_view_angles::hk_validate_view_angles( i_csgo_input* input, void* a2 ) {
	static auto original = m_validate_view_angles.get_original< decltype( &hk_validate_view_angles ) >( );

	vec3_t view_angles = input->get_view_angles( );

	original( input, a2 );

	input->set_view_angles( view_angles );
}

void hooks::update_global_vars::hk_update_global_vars( void* source_to_client, void* new_global_vars ) {
	static auto original = m_update_global_vars.get_original< decltype( &hk_update_global_vars ) >( );

	original( source_to_client, new_global_vars );

	g_interfaces->m_global_vars = *reinterpret_cast<i_global_vars**>(
		g_modules->m_modules.client_dll.get() + cs2_build::global_vars );
}

void hooks::frame_stage_notify::hk_frame_stage_notify( void* source_to_client, int stage ) {
	static auto original = m_frame_stage_notify.get_original< decltype( &hk_frame_stage_notify ) >( );

	g_ctx->m_local_pawn = g_interfaces->m_entity_system->get_local_pawn( );
	g_ctx->m_local_controller = g_interfaces->m_entity_system->get_local_controller();

	//g_skins->agent_changer( stage );

	original( source_to_client, stage );

	switch ( stage ) {
	case FRAME_RENDER_START:
		//g_skins->knife_changer( stage );
		break;
	case FRAME_RENDER_END:
		g_visuals->store_players( );
		break;
	case FRAME_NET_UPDATE_END:
		g_rage_bot->store_records( );
		break;
	case FRAME_SIMULATE_END:
		g_key_handler->update();
		g_world->skybox();
		g_world->exposure(g_ctx->m_local_pawn);
		break;
	default:
		break;
	}
}

void hooks::override_view::hk_override_view( void* source_to_client, c_view_setup* view_setup ) {
	static auto original = m_override_view.get_original< decltype( &hk_override_view ) >( );

	original( source_to_client, view_setup );

	c_cs_player_pawn* local_player = g_ctx->m_local_pawn;

	if ( !local_player )
		return;

	bool in_third_person = false;
	if ( g_cfg->world_esp.m_enable_thirdperson && g_key_handler->is_pressed( g_cfg->world_esp.m_thirdperson_key_bind, g_cfg->world_esp.m_thirdperson_key_bind_style ) )
		in_third_person = true;

	if ( in_third_person && local_player->is_alive( ) )
	{
		const vec3_t eye_pos = local_player->get_eye_pos( );
		if ( !std::isfinite( eye_pos.x ) || !std::isfinite( eye_pos.y ) || !std::isfinite( eye_pos.z ) )
			return;
		const float distance = static_cast<float>( std::clamp( g_cfg->world_esp.m_distance, 35, 180 ) );
		vec3_t adjusted_cam_view_angle = g_interfaces->m_csgo_input->get_view_angles( );
		if ( !std::isfinite( adjusted_cam_view_angle.x ) || !std::isfinite( adjusted_cam_view_angle.y ) )
			return;
		adjusted_cam_view_angle.x = -adjusted_cam_view_angle.x;
		vec3_t camera_pos = calculate_camera_pos( eye_pos, -distance, adjusted_cam_view_angle );
		view_setup->m_origin = camera_pos;

		ray_t ray{};
		game_trace_t trace{};
		trace_filter_t filter{ 0x1C3003, local_player, NULL, 4 };

		if ( g_interfaces->m_trace->trace_shape( &ray, eye_pos, camera_pos, &filter, &trace ) ) {
			if ( std::isfinite( trace.m_fraction ) && trace.m_fraction > 0.f && trace.m_fraction < 1.f ) {
				const float safe_fraction = (std::max)( 0.f, trace.m_fraction - 10.f / distance );
				view_setup->m_origin = eye_pos + ( camera_pos - eye_pos ) * safe_fraction;
			}
		}

	}

}

void hooks::on_add_entity::hk_on_add_entity( void* a1, c_entity_instance* entity_instance, int handle ) {
	static auto original = m_on_add_entity.get_original< decltype( &hk_on_add_entity ) >( );

	g_entity_system->add_entity( entity_instance, handle );

	original( a1, entity_instance, handle );
}

void hooks::on_remove_entity::hk_on_remove_entity( void* a1, c_entity_instance* entity_instance, int handle ) {
	static auto original = m_on_remove_entity.get_original< decltype( &hk_on_remove_entity ) >( );

	g_entity_system->remove_entity( entity_instance, handle );

	original( a1, entity_instance, handle );
}

void* hooks::on_level_init::hk_on_level_init( void* a1, const char* map_name ) {
	static auto original = m_on_level_init.get_original< decltype( &hk_on_level_init ) >( );

	g_entity_system->level_init( );
	g_visuals->clear();
	g_world->clear();

	return original( a1, map_name );
}

std::uintptr_t hooks::on_level_shutdown::hk_on_level_shutdown( void* a1 ) {
	static auto original = m_on_level_shutdown.get_original< decltype( &hk_on_level_shutdown ) >( );

	g_entity_system->level_shutdown( );
	g_visuals->clear();
	g_world->clear();

	return original( a1 );
}



void hooks::draw_light_scene::hk_draw_light_scene( void* a1, c_scene_light_object* a2, __int64 a3 ) {
	static auto original = m_draw_light_scene.get_original< decltype( &hk_draw_light_scene ) >( );

	if ( a2 )
		g_world->lighting( a2 );

	original( a1, a2, a3 );
}

void hooks::update_aggregate_scene_object::hk_update_aggregate_scene_object(void* a1, void* a2, c_aggregate_object_array* a3) {
    static auto original = m_update_aggregate_scene_object.get_original<decltype(&hk_update_aggregate_scene_object)>();
    original(a1, a2, a3);
    if (!a3 || !a3->data || !g_interfaces->m_light_data_queue) return;
    auto* queue = *g_interfaces->m_light_data_queue;
    if (!queue || !queue->light_data) return;
    const int count = a3->data->count, first = a3->data->index;
    if (count <= 0 || count > (1 << 20) || first < 0 || first > (1 << 22) - count) return;
    const auto color = (g_cfg->world.m_wall * 255.f).to_byte();
    for (int i = 0; i < count; ++i) {
        auto* record = reinterpret_cast<c_byte_color*>(static_cast<std::uint8_t*>(queue->light_data) + (static_cast<std::size_t>(first) + i) * 0x20);
        *record = color;
    }
}

std::uintptr_t hooks::draw_aggregate_scene_object::hk_draw_aggregate_scene_object( void* a1, void* a2, c_base_scene_data* a3, int a4, int a5, void* a6, void* a7, void* a8 ) {
	static auto original = m_draw_aggregate_scene_object.get_original< decltype( &hk_draw_aggregate_scene_object ) >( );
	if (a3 && a4 > 0 && a4 <= (1 << 20)) {
		for (int i = 0; i < a4; ++i) {
			auto& scene = a3[i];
			if (!scene.m_material)
				continue;
			const char* name = scene.m_material->get_name();
			if (!name)
				continue;
						const int type = get_model_type(name);
			const c_byte_color color = ((type == MODEL_OTHER ? g_cfg->world.m_wall : g_cfg->world.m_sky_clouds) * 255).to_byte();
			scene.r = color.r;
			scene.g = color.g;
			scene.b = color.b;
		}
	}
	return original(a1, a2, a3, a4, a5, a6, a7, a8);
}

void hooks::update_post_processing::hk_update_post_processing( c_post_processing_volume* a1, int a2 ) {
	static auto original = m_update_post_processing.get_original< decltype( &hk_update_post_processing ) >( );

	original( a1, a2 );

	g_world->exposure( a1 );
}

void* hooks::should_update_sequences::hk_should_update_sequences( void* a1, void* a2, void* a3 ) {
	static auto original = m_should_update_sequences.get_original< decltype( &hk_should_update_sequences ) >( );

	const char* model_name = *reinterpret_cast<const char**>( reinterpret_cast<std::uintptr_t>( a1 ) + 0x8 );
	if ( model_name == nullptr )
		return original( a1, a2, a3 );

	std::string str_model_name = std::string( model_name );
	if ( str_model_name.starts_with( "weapons/models/knife/" ) )
		*reinterpret_cast<__int64*>( reinterpret_cast<std::uintptr_t>( a3 ) + 0x30 ) = 0;

	return original( a1, a2, a3 );
}

void* hooks::should_draw_legs::hk_should_draw_legs( void* a1, void* a2, void* a3, void* a4, void* a5 ) {
	static auto original = m_should_draw_legs.get_original< decltype( &hk_should_draw_legs ) >( );

	return nullptr;
}

void hooks::mark_interp_latch_flags_dirty::hk_mark_interp_latch_flags_dirty( void* a1, unsigned int a2 ) {
	return;
}

void hooks::draw_scope_overlay::hk_draw_scope_overlay( void* a1, void* a2 ) {
	
}

float hooks::get_field_of_view::hk_get_field_of_view(void* a1) {
    static auto original = m_get_field_of_view.get_original<decltype(&hk_get_field_of_view)>();
    const float fov = original(a1);
    if (!g_ctx->m_local_pawn || !g_ctx->m_local_pawn->is_alive() || g_ctx->m_local_pawn->m_scoped()) return fov;
    return g_cfg->world_esp.m_enable_override_fov ? static_cast<float>(std::clamp(g_cfg->world_esp.m_override_fov, 60, 140)) : fov;
}

void __fastcall hooks::draw_skybox_array::hk_draw_skybox_array(void* a1, void* a2, void* draw_primitive, int count,
    std::uintptr_t a5, std::uintptr_t a6, std::uintptr_t a7) {
    const auto original = m_draw_skybox_array.get_original<decltype(&hk_draw_skybox_array)>();
    if (!original) return;

    const auto color = g_world->sky_settings();
    if (color && count > 0 && count < 100 && draw_primitive) {
        static std::recursive_mutex tint_mutex;
        const std::lock_guard lock(tint_mutex);
        void* skybox_object = nullptr;
        if (read_sky_descriptor(draw_primitive, count, skybox_object)) {
            float rgb[3]{color->r, color->g, color->b};
            sky_tint(skybox_object, rgb, true);
        }
    }
    original(a1, a2, draw_primitive, count, a5, a6, a7);
}

std::uintptr_t __fastcall hooks::setup_fog::hk_setup_fog(void* output,int* mode) {
	static auto original=m_setup_fog.get_original<std::uintptr_t(__fastcall*)(void*,int*)>();
	if(g_world->apply_shader_fog(output,mode))return 0;
	return original(output,mode);
}
