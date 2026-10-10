#pragma once
#include <atomic>
#include <memory>

#include "../../darkside.hpp"

#include "../../valve/classes/c_envy_sky.hpp"
#include "../../valve/classes/c_post_processing.hpp"
#include "../../valve/classes/c_scene_light_obj.hpp"

class c_world {
    std::atomic<std::shared_ptr<const c_color>> m_sky_settings{};
public:
    void clear() { m_sky_settings.store(nullptr, std::memory_order_release); }
    auto sky_settings() const { return m_sky_settings.load(std::memory_order_acquire); }
    bool apply_shader_fog(void* output, int* mode);
	void skybox( );


	void lighting( c_scene_light_object* );

	void exposure( c_cs_player_pawn* );
	void exposure( c_post_processing_volume* );
	void draw_scope_overlay( );
};

inline const auto g_world = std::make_unique<c_world>( );