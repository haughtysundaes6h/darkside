#include "anti_hit.hpp"
#include "../eng_pred/eng_pred.hpp"
#include "../../entity_system/entity.hpp"

void c_anti_hit::at_target( float& yaw ) {
	if ( !g_cfg->anti_hit.m_at_target )
		return;

	if ( !g_ctx->m_local_pawn->m_scene_node( ) )
		return;

	c_cs_player_pawn* best_target = nullptr;

	auto local_data = g_prediction->get_local_data( );

	if ( !local_data )
		return;

	vec3_t shoot_pos = local_data->m_eye_pos;

	float best_distance = FLT_MAX;

	for ( auto entity : g_entity_system->get_players( ENTITY_ENEMIES_ONLY ) ) {
		if ( !entity || !entity->is_alive( ) || !entity->m_scene_node( ) )
			continue;

		float distance = entity->m_scene_node( )->m_abs_origin( ).dist( shoot_pos );

		if ( distance < best_distance ) {
			best_distance = distance;
			best_target = entity;
		}
	}

	if ( !best_target )
		return;

	const float x = best_target->m_scene_node( )->m_abs_origin( ).x - g_ctx->m_local_pawn->m_scene_node( )->m_abs_origin( ).x;
	const float y = best_target->m_scene_node( )->m_abs_origin( ).y - g_ctx->m_local_pawn->m_scene_node( )->m_abs_origin( ).y;

	yaw = ( x == 0.f && y == 0.f ) ? 0.f : rad2deg( std::atan2( y, x ) );
}

void c_anti_hit::set_pitch( c_user_cmd* user_cmd ) {
	if ( !g_cfg->anti_hit.m_override_pitch )
		return;

	float pitch = static_cast<float>(std::clamp(g_cfg->anti_hit.m_pitch_amount, -89, 89));

	user_cmd->pb.mutable_base( )->mutable_viewangles( )->set_x( pitch );
}

void c_anti_hit::set_yaw( c_user_cmd* user_cmd ) {
	if ( !g_cfg->anti_hit.m_override_yaw  )
		return;

	const float input_yaw = g_interfaces->m_csgo_input->get_view_angles().y;
	float yaw = std::isfinite(input_yaw) ? input_yaw : user_cmd->pb.mutable_base()->viewangles().y();

	at_target( yaw );

	yaw += g_cfg->anti_hit.m_yaw_amount;

	m_should_invert = (user_cmd->m_command_number & 1) != 0;

	yaw += g_cfg->anti_hit.m_jitter_amount * ( m_should_invert ? 0.5f : -0.5f );

	user_cmd->pb.mutable_base( )->mutable_viewangles( )->set_y( std::remainder(yaw,360.f) );
}

void c_anti_hit::on_create_move( c_user_cmd* user_cmd )
{
	if ( !g_cfg->anti_hit.m_enabled || !user_cmd || !user_cmd->pb.has_base() )
		return;

	if ( !g_interfaces->m_engine->is_in_game( ) )
		return;

	if ( !g_ctx->m_local_pawn
		|| !g_ctx->m_local_pawn->is_alive( )
		|| g_ctx->m_local_pawn->m_actual_move_type( ) != movetype_walk
		|| (user_cmd->m_button_state.m_button_state & (IN_ATTACK | IN_ATTACK2 | IN_USE))
		|| g_ctx->m_local_pawn->is_throwing( ) )
		return;

	set_pitch( user_cmd );
	set_yaw( user_cmd );
}
