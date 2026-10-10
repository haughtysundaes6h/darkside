#pragma once

#include "../../darkside.hpp"

class c_eng_pred {
	struct c_movement_prestate {
		c_cs_player_pawn* pawn{};
		int command_number{};
		int flags{};
		vec3_t velocity{};
		bool valid{};
	} m_movement_prestate{};
	struct c_local_data {
		float m_absolute_frame_time{}, m_absolute_frame_start_time_std_dev{}, m_spread{}, m_inaccuracy{}, m_player_tick_fraction{}, m_render_tick_fraction{};
		float m_current_time{}, m_current_time2{};
		int m_tick_count{}, m_tick_base{}, m_player_tick{}, m_render_tick{}, m_shoot_tick{};
		vec3_t m_velocity{}, m_eye_pos{};
	} m_pred_data{};

public:
	struct landing_state { bool grounded{}, will_land{}, valid{}; float fraction{}; };
	void capture_movement_prestate( c_cs_player_pawn* pawn, c_user_cmd* cmd, int flags, const vec3_t& velocity );
	bool predicted_grounded( c_cs_player_pawn* pawn, c_user_cmd* cmd ) const;
	landing_state landing(c_cs_player_pawn* pawn, c_user_cmd* cmd) const;
	void run( );
	void predict( );
	void end( );

	c_local_data* get_local_data( ) {
		return &m_pred_data;
	}
};

inline const auto g_prediction = std::make_unique<c_eng_pred>( );
