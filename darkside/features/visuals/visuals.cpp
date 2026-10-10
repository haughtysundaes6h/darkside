#include "visuals.hpp"
#include "../../render/render.hpp"
#include "../../entity_system/entity.hpp"
#include "../rage_bot/rage_bot.hpp"

// ============================================================================
// BBOX CALCULATION
// ============================================================================

bbox_t c_visuals::calculate_bbox(vec3_t bottom, vec3_t top) {
    bbox_t out{};

    vec3_t screen_points[2]{};

    if (g_render->world_to_screen(top, screen_points[1]) && g_render->world_to_screen(bottom, screen_points[0])) {
        out.height = std::abs((screen_points[1] - screen_points[0]).y + 6);
        out.width = out.height / 2.0f;

        out.x = screen_points[1].x - out.width / 2.f;
        out.y = screen_points[1].y;

        out.m_found = true;
        return out;
    }

    return {};
}

// ============================================================================
// PLAYER DATA COLLECTION
// ============================================================================

void c_visuals::store_players() {
    if (!g_interfaces->m_engine->is_in_game() || !g_ctx->m_local_pawn) {
        clear();
        return;
    }
    
    decltype(m_player_map) players;
    auto* local = g_ctx->m_local_pawn;
    vec3_t local_pos = local->m_scene_node()->m_abs_origin();
    
    for (auto* instance : g_entity_system->get("CCSPlayerController")) {
        auto* controller = reinterpret_cast<c_cs_player_controller*>(instance);
        if (!controller || controller == g_ctx->m_local_controller || !controller->m_pawn_is_alive())
            continue;
        
        const auto handle = controller->m_pawn();
        if (!handle.is_valid())
            continue;
        
        auto* pawn = g_interfaces->m_entity_system->get_base_entity<c_cs_player_pawn>(handle.get_entry_index());
        if (!pawn || !pawn->is_alive() || pawn == local)
            continue;
        
        // Check team
        bool is_enemy = pawn->m_team_num() != local->m_team_num();
        if (!is_enemy && !g_cfg->visuals.m_player_esp.m_teammates)
            continue;
        
        auto* scene = pawn->m_scene_node();
        auto* collision = pawn->m_collision();
        if (!scene || !collision)
            continue;
        
        player_info_t info{};
        info.m_valid = true;
        info.m_handle = handle.to_int();
        info.m_team = pawn->m_team_num();
        
        // Positions
        info.m_bottom = scene->m_abs_origin() - vec3_t{0, 0, 1};
        info.m_top = info.m_bottom + vec3_t{0, 0, collision->m_maxs().z};
        
        // Get head position
        int head_bone = pawn->get_bone_index("head");
        if (head_bone != -1) {
            info.m_head_pos = pawn->get_bone_position(head_bone);
        } else {
            info.m_head_pos = info.m_top;
        }
        
        // Basic info
        info.m_health = pawn->m_health();
        info.m_armor = pawn->m_armor_value();
        info.m_distance = local_pos.dist(scene->m_abs_origin());
        
        // Name
        const auto* name = controller->m_player_name();
        info.m_name = name ? name : "";
        
        // State flags
        info.m_is_scoped = pawn->m_scoped();
        info.m_is_flashed = pawn->m_flash_max_alpha() > 200.0f;
        
        // Weapon info
        if (auto* weapon = pawn->get_active_weapon()) {
            if (auto* data = weapon->get_weapon_data()) {
                info.m_ammo = weapon->m_clip1();
                info.m_max_ammo = data->m_max_clip1();
                
                if (const auto* wname = data->m_name()) {
                    const auto* prefix = strstr(wname, "weapon_");
                    info.m_weapon_name = prefix ? prefix + 7 : wname;
                    std::transform(info.m_weapon_name.begin(), info.m_weapon_name.end(), 
                        info.m_weapon_name.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                }
            }
        }
        
        // Flags
        if (g_cfg->visuals.m_player_esp.m_flags) {
            auto* items = pawn->m_item_services();
            
            if (info.m_armor > 0) {
                info.m_flags.emplace_back(items && items->m_has_helmet() ? "HK" : "K");
            }
            if (info.m_is_scoped) {
                info.m_flags.emplace_back("ZOOM");
            }
            if (info.m_is_flashed) {
                info.m_flags.emplace_back("FLASH");
            }
            // Add more flags as needed
        }
        
        // Skeleton data
        if (g_cfg->visuals.m_player_esp.m_skeleton) {
            // Define bone connections
            static const std::vector<std::pair<const char*, const char*>> bone_pairs = {
                {"head", "neck_0"},
                {"neck_0", "spine_2"},
                {"spine_2", "spine_1"},
                {"spine_1", "spine_0"},
                {"spine_2", "arm_upper_L"},
                {"arm_upper_L", "arm_lower_L"},
                {"arm_lower_L", "hand_L"},
                {"spine_2", "arm_upper_R"},
                {"arm_upper_R", "arm_lower_R"},
                {"arm_lower_R", "hand_R"},
                {"spine_0", "leg_upper_L"},
                {"leg_upper_L", "leg_lower_L"},
                {"leg_lower_L", "ankle_L"},
                {"spine_0", "leg_upper_R"},
                {"leg_upper_R", "leg_lower_R"},
                {"leg_lower_R", "ankle_R"}
            };
            
            for (const auto& [bone_from, bone_to] : bone_pairs) {
                int idx_from = pawn->get_bone_index(bone_from);
                int idx_to = pawn->get_bone_index(bone_to);
                
                if (idx_from != -1 && idx_to != -1) {
                    vec3_t pos_from = pawn->get_bone_position(idx_from);
                    vec3_t pos_to = pawn->get_bone_position(idx_to);
                    info.m_bones.emplace_back(pos_from, pos_to);
                }
            }
        }
        
        players.emplace(info.m_handle, std::move(info));
    }
    
    std::lock_guard lock(m_player_mutex);
    m_player_map.swap(players);
    m_local_scoped = local->is_alive() && local->m_scoped();
    m_snapshot_time = std::chrono::steady_clock::now();
}

// ============================================================================
// DRAWING HELPERS
// ============================================================================

void c_visuals::draw_box(const bbox_t& bbox, const c_color& color) {
    static const c_color outline_color = c_color{0.f, 0.f, 0.f, 1.f};
    
    g_render->rect_outline({bbox.x - 1, bbox.y - 1}, {bbox.x + bbox.width + 1, bbox.y + bbox.height + 1}, outline_color);
    g_render->rect_outline({bbox.x, bbox.y}, {bbox.x + bbox.width, bbox.y + bbox.height}, color);
    g_render->rect_outline({bbox.x + 1, bbox.y + 1}, {bbox.x + bbox.width - 1, bbox.y + bbox.height - 1}, outline_color);
}

void c_visuals::draw_health_bar(const bbox_t& bbox, int health) {
    static const c_color outline_color = c_color{0.f, 0.f, 0.f, 1.f};
    c_color full_hp_color = c_color{0.69f, 1.f, 0.31f, 1.f};
    c_color low_hp_color = c_color{1.f, 0.2f, 0.31f, 1.f};
    
    int esp_health = std::clamp(health, 0, 100);
    float esp_step = std::clamp(static_cast<float>(esp_health) / 100.f, 0.f, 1.f);
    
    float height = (bbox.height - ((bbox.height * static_cast<float>(esp_health)) / 100.f));
    height = std::max<float>(std::min<float>(height, bbox.height), 0.f);
    
    vec3_t bb_outline_min{bbox.x - 5.f, bbox.y - 1.f};
    vec3_t bb_outline_max{3.f, bbox.height + 2.f};
    
    vec3_t bb_min{bbox.x - 4.f, bbox.y + height};
    vec3_t bb_max{2.f, bbox.height - height};
    
    g_render->rect(bb_outline_min, bb_outline_min + bb_outline_max, outline_color);
    g_render->rect(bb_min, bb_min + bb_max, low_hp_color.lerp(full_hp_color, esp_step));
    
    // Health text when low
    if (esp_health < 100) {
        g_render->text(
            vec3_t(bbox.x - 6.f, bbox.y + height - 4.f),
            c_color{1.f, 1.f, 1.f, 1.f},
            font_flags_right | font_flags_dropshadow,
            g_render->fonts.onetap_pixel,
            std::to_string(esp_health),
            g_render->fonts.onetap_pixel->FontSize
        );
    }
}

void c_visuals::draw_armor_bar(const bbox_t& bbox, int armor) {
    if (armor <= 0)
        return;
    
    static const c_color outline_color = c_color{0.f, 0.f, 0.f, 1.f};
    c_color armor_color = c_color{0.4f, 0.6f, 1.f, 1.f};
    
    int clamped = std::clamp(armor, 0, 100);
    float esp_step = static_cast<float>(clamped) / 100.f;
    
    float height = (bbox.height - ((bbox.height * static_cast<float>(clamped)) / 100.f));
    
    vec3_t bb_outline_min{bbox.x + bbox.width + 2.f, bbox.y - 1.f};
    vec3_t bb_outline_max{3.f, bbox.height + 2.f};
    
    vec3_t bb_min{bbox.x + bbox.width + 3.f, bbox.y + height};
    vec3_t bb_max{2.f, bbox.height - height};
    
    g_render->rect(bb_outline_min, bb_outline_min + bb_outline_max, outline_color);
    g_render->rect(bb_min, bb_min + bb_max, armor_color);
}

void c_visuals::draw_ammo_bar(const bbox_t& bbox, int ammo, int max_ammo) {
    if (ammo <= 0 || max_ammo <= 0)
        return;
    
    static const c_color outline_color = c_color{0.f, 0.f, 0.f, 1.f};
    c_color full_ammo_color = c_color{0.67f, 0.725f, 0.98f, 1.f};
    c_color low_ammo_color = c_color{1.f, 0.3f, 0.3f, 1.f};
    
    float esp_step = std::clamp(static_cast<float>(ammo) / static_cast<float>(max_ammo), 0.f, 1.f);
    float width = (bbox.width * esp_step);
    
    vec3_t bb_outline_min{bbox.x - 1.f, bbox.y + bbox.height + 2.f};
    vec3_t bb_outline_max{bbox.width + 2.f, 3.f};
    
    vec3_t bb_min{bbox.x, bbox.y + bbox.height + 3.f};
    vec3_t bb_max{width, 2.f};
    
    g_render->rect(bb_outline_min, bb_outline_min + bb_outline_max, outline_color);
    g_render->rect(bb_min, bb_min + bb_max, low_ammo_color.lerp(full_ammo_color, esp_step));
}

void c_visuals::draw_name(const bbox_t& bbox, const std::string& name, const c_color& color) {
    ImGui::PushFont(g_render->fonts.verdana_small);
    auto text_size = ImGui::CalcTextSize(name.c_str());
    
    g_render->text(
        vec3_t(bbox.x + bbox.width / 2.f, bbox.y - text_size.y - 3.f),
        color,
        font_flags_center | font_flags_dropshadow,
        g_render->fonts.verdana_small,
        name,
        g_render->fonts.verdana_small->FontSize
    );
    
    ImGui::PopFont();
}

void c_visuals::draw_weapon(const bbox_t& bbox, const std::string& weapon) {
    c_color main_color{1.f, 1.f, 1.f, 1.f};
    
    float y_offset = g_cfg->visuals.m_player_esp.m_ammo_bar ? 8.f : 3.f;
    
    g_render->text(
        vec3_t(bbox.x + bbox.width / 2.f, bbox.y + bbox.height + y_offset),
        main_color,
        font_flags_center | font_flags_outline,
        g_render->fonts.onetap_pixel,
        weapon,
        g_render->fonts.onetap_pixel->FontSize
    );
}

void c_visuals::draw_flags(const bbox_t& bbox, const std::vector<std::string>& flags) {
    float y = bbox.y;
    
    for (const auto& flag : flags) {
        g_render->text(
            {bbox.x + bbox.width + 4.f, y},
            {1.f, 1.f, 1.f, 1.f},
            font_flags_outline,
            g_render->fonts.onetap_pixel,
            flag,
            g_render->fonts.onetap_pixel->FontSize
        );
        y += 10.f;
    }
}

void c_visuals::draw_skeleton(const player_info_t& info) {
    c_color bone_color{1.f, 1.f, 1.f, 0.8f};
    
    for (const auto& [bone_from, bone_to] : info.m_bones) {
        vec3_t screen_from, screen_to;
        
        if (g_render->world_to_screen(bone_from, screen_from) && 
            g_render->world_to_screen(bone_to, screen_to)) {
            g_render->line(
                {screen_from.x, screen_from.y},
                {screen_to.x, screen_to.y},
                bone_color
            );
        }
    }
}

void c_visuals::draw_distance(const bbox_t& bbox, float distance) {
    char dist_str[32];
    snprintf(dist_str, sizeof(dist_str), "%.0fm", distance / 100.0f);
    
    g_render->text(
        {bbox.x + bbox.width / 2.f, bbox.y + bbox.height + 18.f},
        {0.8f, 0.8f, 0.8f, 1.f},
        font_flags_center | font_flags_outline,
        g_render->fonts.onetap_pixel,
        dist_str,
        g_render->fonts.onetap_pixel->FontSize
    );
}

// ============================================================================
// PLAYER RENDERING
// ============================================================================

void c_visuals::handle_players() {
    for (auto it = m_player_map.begin(); it != m_player_map.end(); it = std::next(it)) {
        player_info_t& info = it->second;
        if (!info.m_valid)
            continue;

        const bbox_t bbox = calculate_bbox(info.m_bottom, info.m_top);
        if (!bbox.m_found)
            continue;
        
        // Box
        if (g_cfg->visuals.m_player_esp.m_bounding_box) {
            draw_box(bbox, g_cfg->visuals.m_player_esp.m_box_color);
        }
        
        // Health bar
        if (g_cfg->visuals.m_player_esp.m_health_bar) {
            draw_health_bar(bbox, info.m_health);
        }
        
        // Name
        if (g_cfg->visuals.m_player_esp.m_name) {
            draw_name(bbox, info.m_name, g_cfg->visuals.m_player_esp.m_name_color);
        }
        
        // Weapon
        if (g_cfg->visuals.m_player_esp.m_weapon && !info.m_weapon_name.empty()) {
            draw_weapon(bbox, info.m_weapon_name);
        }
        
        // Ammo bar
        if (g_cfg->visuals.m_player_esp.m_ammo_bar && info.m_ammo > 0) {
            draw_ammo_bar(bbox, info.m_ammo, info.m_max_ammo);
        }
        
        // Flags
        if (g_cfg->visuals.m_player_esp.m_flags && !info.m_flags.empty()) {
            draw_flags(bbox, info.m_flags);
        }
        
        // Skeleton
        if (g_cfg->visuals.m_player_esp.m_skeleton) {
            draw_skeleton(info);
        }
        
        // Distance
        if (g_cfg->visuals.m_player_esp.m_distance) {
            draw_distance(bbox, info.m_distance);
        }
    }
}

// ============================================================================
// EFFECTS
// ============================================================================

void c_visuals::draw_hitmarker() {
    if (!g_cfg->visuals.m_effects.m_hitmarker)
        return;
    
    if (m_hitmarker_alpha <= 0.0f)
        return;
    
    // Decay
    m_hitmarker_alpha -= g_interfaces->m_global_vars->m_frame_time * 2.0f;
    m_hitmarker_alpha = std::max(0.0f, m_hitmarker_alpha);
    
    auto& io = ImGui::GetIO();
    vec3_t center{io.DisplaySize.x / 2.f, io.DisplaySize.y / 2.f, 0};
    
    c_color color = m_hitmarker_kill ? 
        c_color{1.f, 0.2f, 0.2f, m_hitmarker_alpha} : 
        c_color{1.f, 1.f, 1.f, m_hitmarker_alpha};
    
    float size = m_hitmarker_kill ? 12.f : 8.f;
    float gap = 4.f;
    
    g_render->line({center.x - size, center.y - size}, {center.x - gap, center.y - gap}, color);
    g_render->line({center.x + size, center.y - size}, {center.x + gap, center.y - gap}, color);
    g_render->line({center.x - size, center.y + size}, {center.x - gap, center.y + gap}, color);
    g_render->line({center.x + size, center.y + size}, {center.x + gap, center.y + gap}, color);
}

void c_visuals::on_player_hurt() {
    m_hitmarker_alpha = 1.0f;
    m_hitmarker_time = g_interfaces->m_global_vars->m_current_time;
    m_hitmarker_kill = false;
    
    if (g_cfg->visuals.m_effects.m_hitmarker_sound) {
        // Play sound - would need sound system integration
    }
}

void c_visuals::add_bullet_tracer(const vec3_t& start, const vec3_t& end) {
    if (!g_cfg->visuals.m_effects.m_bullet_tracers)
        return;
    
    std::lock_guard lock(m_tracer_mutex);
    
    bullet_tracer_t tracer;
    tracer.m_start = start;
    tracer.m_end = end;
    tracer.m_time = g_interfaces->m_global_vars->m_current_time;
    tracer.m_color = g_cfg->visuals.m_effects.m_tracer_color;
    
    m_bullet_tracers.push_back(tracer);
}

void c_visuals::draw_bullet_tracers() {
    if (!g_cfg->visuals.m_effects.m_bullet_tracers)
        return;
    
    std::lock_guard lock(m_tracer_mutex);
    
    float current_time = g_interfaces->m_global_vars->m_current_time;
    const float duration = 2.0f;
    
    // Remove old tracers
    m_bullet_tracers.erase(
        std::remove_if(m_bullet_tracers.begin(), m_bullet_tracers.end(),
            [current_time, duration](const bullet_tracer_t& t) {
                return current_time - t.m_time > duration;
            }),
        m_bullet_tracers.end()
    );
    
    // Draw remaining
    for (const auto& tracer : m_bullet_tracers) {
        float alpha = 1.0f - ((current_time - tracer.m_time) / duration);
        
        vec3_t screen_start, screen_end;
        if (g_render->world_to_screen(tracer.m_start, screen_start) &&
            g_render->world_to_screen(tracer.m_end, screen_end)) {
            
            c_color color = tracer.m_color;
            color.a() = alpha;
            
            g_render->line(
                {screen_start.x, screen_start.y},
                {screen_end.x, screen_end.y},
                color
            );
        }
    }
}

void c_visuals::add_bullet_impact(const vec3_t& position) {
    if (!g_cfg->visuals.m_effects.m_bullet_impacts)
        return;
    
    std::lock_guard lock(m_impact_mutex);
    
    bullet_impact_t impact;
    impact.m_position = position;
    impact.m_time = g_interfaces->m_global_vars->m_current_time;
    
    m_bullet_impacts.push_back(impact);
}

void c_visuals::draw_bullet_impacts() {
    if (!g_cfg->visuals.m_effects.m_bullet_impacts)
        return;
    
    std::lock_guard lock(m_impact_mutex);
    
    float current_time = g_interfaces->m_global_vars->m_current_time;
    const float duration = 3.0f;
    
    // Remove old impacts
    m_bullet_impacts.erase(
        std::remove_if(m_bullet_impacts.begin(), m_bullet_impacts.end(),
            [current_time, duration](const bullet_impact_t& i) {
                return current_time - i.m_time > duration;
            }),
        m_bullet_impacts.end()
    );
    
    // Draw remaining
    for (const auto& impact : m_bullet_impacts) {
        float alpha = 1.0f - ((current_time - impact.m_time) / duration);
        
        vec3_t screen;
        if (g_render->world_to_screen(impact.m_position, screen)) {
            g_render->circle(
                {screen.x, screen.y, 0},
                c_color{1.f, 0.f, 0.f, alpha},
                5.f
            );
        }
    }
}

// ============================================================================
// INDICATORS
// ============================================================================

void c_visuals::draw_crosshair() {
    if (!g_cfg->visuals.m_indicators.m_crosshair)
        return;
    
    auto& io = ImGui::GetIO();
    vec3_t center{io.DisplaySize.x / 2.f, io.DisplaySize.y / 2.f, 0};
    
    c_color color{1.f, 1.f, 1.f, 1.f};
    float size = 4.f;
    float gap = 2.f;
    
    // Horizontal
    g_render->line({center.x - size - gap, center.y}, {center.x - gap, center.y}, color);
    g_render->line({center.x + gap, center.y}, {center.x + size + gap, center.y}, color);
    
    // Vertical
    g_render->line({center.x, center.y - size - gap}, {center.x, center.y - gap}, color);
    g_render->line({center.x, center.y + gap}, {center.x, center.y + size + gap}, color);
}

void c_visuals::draw_spread_circle() {
    if (!g_cfg->visuals.m_indicators.m_spread_circle)
        return;
    
    if (!g_ctx->m_local_pawn || !g_ctx->m_local_pawn->is_alive())
        return;
    
    auto* weapon = g_ctx->m_local_pawn->get_active_weapon();
    if (!weapon)
        return;
    
    float spread = weapon->get_inaccuracy() + weapon->get_spread();
    
    auto& io = ImGui::GetIO();
    vec3_t center{io.DisplaySize.x / 2.f, io.DisplaySize.y / 2.f, 0};
    
    float radius = std::tan(spread) * (io.DisplaySize.x / 2.f);
    
    g_render->circle(center, c_color{1.f, 1.f, 1.f, 0.3f}, radius);
}

void c_visuals::draw_antiaim_indicators() {
    if (!g_cfg->visuals.m_indicators.m_aa_lines)
        return;
    
    // Draw anti-aim indicators at bottom of screen
}

void c_visuals::draw_desync_indicator() {
    if (!g_cfg->visuals.m_indicators.m_desync)
        return;
    
    // Draw desync bar
}

// ============================================================================
// MAIN ENTRY POINTS
// ============================================================================

void c_visuals::on_present() {
    std::lock_guard lock(m_player_mutex);
    
    if (std::chrono::steady_clock::now() - m_snapshot_time > std::chrono::milliseconds(500))
        return;
    
    handle_players();
    
    // Effects
    draw_hitmarker();
    draw_bullet_tracers();
    draw_bullet_impacts();
    
    // Indicators
    draw_crosshair();
    draw_spread_circle();
    draw_antiaim_indicators();
    draw_desync_indicator();
}

void c_visuals::clear() {
    std::lock_guard lock(m_player_mutex);
    m_player_map.clear();
    m_snapshot_time = {};
    m_local_scoped = false;
}

bool c_visuals::local_scoped() {
    std::lock_guard lock(m_player_mutex);
    return m_local_scoped && 
        std::chrono::steady_clock::now() - m_snapshot_time < std::chrono::milliseconds(500);
}
