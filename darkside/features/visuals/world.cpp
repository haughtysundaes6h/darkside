#include "world.hpp"
#include "../../entity_system/entity.hpp"
#include "../../render/render.hpp"
#include "visuals.hpp"

void c_world::skybox() {
    // Publish the existing Darkside setting for the sky draw hook. Do not
    // modify C_EnvSky fields or invoke its network/resource update callback.
    c_color color = g_cfg->world.m_sky;
    if (!std::isfinite(color.r) || !std::isfinite(color.g) ||
        !std::isfinite(color.b) || !std::isfinite(color.a)) {
        m_sky_settings.store(nullptr, std::memory_order_release);
        return;
    }
    m_sky_settings.store(std::make_shared<const c_color>(color), std::memory_order_release);
}



void c_world::lighting( c_scene_light_object* light_object ) {
    c_color lighintg_color = g_cfg->world.m_lighting;

    light_object->m_color = lighintg_color * 3;
}

void c_world::exposure(c_cs_player_pawn* pawn) {
    if (!g_interfaces->m_engine->is_in_game() || !pawn) return;
    auto* camera = pawn->m_camera_services();
    if (!camera || !camera->m_active_post_processing().is_valid()) return;
    auto* volume = reinterpret_cast<c_post_processing_volume*>(g_interfaces->m_entity_system->get_base_entity(camera->m_active_post_processing().get_entry_index()));
    if (!volume) return;
    const float expected = std::clamp(g_cfg->world.m_exposure, 1, 100) * 0.01f;
    const bool changed = !volume->m_exposure_control() || volume->m_min() != expected || volume->m_max() != expected;
    exposure(volume);
    static auto update = reinterpret_cast<void(__fastcall*)(c_player_camera_service*, int)>(g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), "48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8B D9 E8 ?? ?? ?? ?? 84 C0 0F 84"));
    if (changed && update) update(camera, 0);
}

void c_world::exposure( c_post_processing_volume* post_processing ) {
    float exposure = g_cfg->world.m_exposure * 0.01f;

    post_processing->m_exposure_control( ) = true;

    post_processing->m_fade_speed_down( ) = post_processing->m_fade_speed_up( ) = 0;

    post_processing->m_min( ) = post_processing->m_max( ) = exposure;
}

void c_world::draw_scope_overlay( ) {
    if ( !g_interfaces->m_engine->is_in_game( ) )
        return;

    if (!g_visuals->local_scoped())
        return;

    vec3_t m_screen_size = g_render->m_screen_size;

    g_render->line( vec3_t( 0, m_screen_size.y / 2 ), vec3_t( m_screen_size.x, m_screen_size.y / 2 ), c_color( 0, 0, 0, 1.f ) );
    g_render->line( vec3_t( m_screen_size.x / 2, 0 ), vec3_t( m_screen_size.x / 2, m_screen_size.y ), c_color( 0, 0, 0, 1.f ) );
}
bool c_world::apply_shader_fog(void* output, int* mode) {
    if (!g_cfg->world.m_render_fog || !output || !mode) return false;
    using setter_t = std::uintptr_t(__fastcall*)(void*, std::uint32_t, int);
    static auto setter = reinterpret_cast<setter_t>(g_opcodes->scan(g_modules->m_modules.client_dll.get_name(),
        "48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 20 66 0F 6E CA 41 8B F0"));
    if (!setter) return false;
    setter(static_cast<std::byte*>(output) + 0x110, 0x6E0FAD7E, 0);
    *mode = 0;
    return true;
}
