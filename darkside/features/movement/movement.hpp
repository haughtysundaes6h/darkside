#pragma once

#include "../protobufs/protobufs.hpp"
#include "movement_math.hpp"

class c_movement {
	movement_math::yaw_history m_yaw_history;
	void auto_strafe( c_user_cmd* user_cmd, float old_yaw );
	void limit_speed( c_user_cmd* user_cmd, c_cs_player_pawn* local_player, c_base_player_weapon* active_weapon, float max_speed );
public:
	bool bunnyhop( c_user_cmd* user_cmd, bool jump_held );
	void auto_stop( c_user_cmd* user_cmd, c_cs_player_pawn* local_player, c_base_player_weapon* active_weapon, bool no_spread, bool enabled );
	void movement_fix( c_user_cmd* user_cmd, vec3_t angle );
	void on_create_move( c_user_cmd* user_cmd );
	bool read_movement_analog( c_cs_player_pawn* pawn, float& forward, float& left ) const;
	bool sync_subtick_movement( c_user_cmd* user_cmd, float prior_forward, float prior_left );
};

inline const auto g_movement = std::make_unique<c_movement>( );
