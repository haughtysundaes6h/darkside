#pragma once

#include "../schema/schema.hpp"
#include "../../sdk/typedefs/c_handle.hpp"

class c_player_camera_service {
public:
	SCHEMA( m_active_post_processing, c_base_handle, "CPlayer_CameraServices", "m_hActivePostProcessingVolume" );
	SCHEMA( m_view_punch_angle, vec3_t, "CPlayer_CameraServices", "m_vecCsViewPunchAngle" );
	SCHEMA( m_override_fog_color, bool, "CPlayer_CameraServices", "m_bOverrideFogColor" );
	SCHEMA( m_fog_color, std::uint32_t, "CPlayer_CameraServices", "m_OverrideFogColor" );
	SCHEMA( m_override_fog_start_end, bool, "CPlayer_CameraServices", "m_bOverrideFogStartEnd" );
	SCHEMA( m_fog_start, float, "CPlayer_CameraServices", "m_fOverrideFogStart" );
	SCHEMA( m_fog_end, float, "CPlayer_CameraServices", "m_fOverrideFogEnd" );
};
