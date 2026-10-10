#include "legit_bot.hpp"
#include "../../darkside.hpp"
#include "../../entity_system/entity.hpp"
#include "../../render/render.hpp"
#include "../movement/movement.hpp"

// ============================================================================
// BACKTRACK RECORD
// ============================================================================

void backtrack_record_t::store(c_cs_player_pawn* pawn) {
    if (!pawn || !pawn->is_alive()) {
        m_valid = false;
        return;
    }
    
    m_pawn = pawn;
    m_simulation_time = pawn->m_sim_time();
    m_origin = pawn->m_scene_node()->m_abs_origin();
    
    // Get head position
    int head_bone = pawn->get_bone_index("head");
    if (head_bone != -1) {
        m_head_position = pawn->get_bone_position(head_bone);
    } else {
        m_head_position = m_origin + vec3_t(0, 0, 64); // Fallback
    }
    
    m_tick_count = TIME_TO_TICKS(m_simulation_time);
    m_valid = true;
}

bool backtrack_record_t::is_valid() const {
    if (!m_valid || !m_pawn)
        return false;
    
    // Check if record is too old
    float curtime = g_interfaces->m_global_vars->m_current_time;
    float delta = curtime - m_simulation_time;
    
    static auto sv_maxunlag = g_interfaces->m_var->get_by_name("sv_maxunlag");
    float max_unlag = sv_maxunlag ? sv_maxunlag->get_float() : 0.2f;
    
    return delta <= max_unlag && delta >= 0.0f;
}

// ============================================================================
// LEGIT BOT - MAIN
// ============================================================================

void c_legit_bot::on_create_move() {
    if (!g_interfaces->m_engine->is_in_game() || !g_interfaces->m_engine->is_connected())
        return;
    
    if (!g_ctx->m_local_pawn || !g_ctx->m_local_pawn->is_alive())
        return;
    
    // Store backtrack records
    if (g_cfg->legit_bot.m_backtrack_enabled) {
        store_records();
    }
    
    // Run aimbot
    if (g_cfg->legit_bot.m_enabled) {
        run_aimbot();
    }
    
    // Run triggerbot
    if (g_cfg->legit_bot.m_triggerbot_enabled) {
        run_triggerbot();
    }
}

void c_legit_bot::on_present() {
    if (!g_cfg->legit_bot.m_backtrack_enabled || !g_cfg->legit_bot.m_backtrack_visualize)
        return;
    
    draw_backtrack();
}

void c_legit_bot::reset() {
    m_targets.clear();
    m_current_target = nullptr;
    m_backtrack_records.clear();
}

// ============================================================================
// LEGIT BOT - AIMBOT
// ============================================================================

void c_legit_bot::run_aimbot() {
    c_base_player_weapon* weapon = g_ctx->m_local_pawn->get_active_weapon();
    if (!weapon)
        return;
    
    c_cs_weapon_base_v_data* weapon_data = weapon->get_weapon_data();
    if (!weapon_data || !is_weapon_valid(weapon_data))
        return;
    
    // Find all valid targets
    find_targets();
    
    // Get best target
    legit_target_t* target = get_best_target();
    if (!target || !target->m_pawn)
        return;
    
    // Check if target is in FOV
    if (target->m_fov > g_cfg->legit_bot.m_fov)
        return;
    
    // Check if we should only aim when attacking (for non-silent)
    bool attacking = g_ctx->m_user_cmd->m_button_state.m_button_state & IN_ATTACK;
    if (!g_cfg->legit_bot.m_silent && !attacking)
        return;
    
    // Calculate aim angles
    vec3_t eye_pos = g_ctx->m_local_pawn->get_eye_pos();
    vec3_t aim_angles = g_math->aim_direction(eye_pos, target->m_aim_point);
    
    // Apply smoothing
    vec3_t current_angles;
    g_interfaces->m_csgo_input->get_view_angles(current_angles);
    
    smooth_aim(current_angles, aim_angles);
    
    // Set angles
    if (g_cfg->legit_bot.m_silent) {
        // Silent aim - only modify user cmd angles
        g_ctx->m_user_cmd->pb.mutable_base()->mutable_viewangles()->set_x(current_angles.x);
        g_ctx->m_user_cmd->pb.mutable_base()->mutable_viewangles()->set_y(current_angles.y);
        g_ctx->m_user_cmd->pb.mutable_base()->mutable_viewangles()->set_z(0.0f);
    } else {
        // Visible aim - move viewangles
        g_interfaces->m_csgo_input->set_view_angles(current_angles);
    }
    
    m_current_target = target;
}

void c_legit_bot::find_targets() {
    m_targets.clear();
    
    vec3_t eye_pos = g_ctx->m_local_pawn->get_eye_pos();
    vec3_t view_angles;
    g_interfaces->m_csgo_input->get_view_angles(view_angles);
    
    const auto& entities = g_entity_system->get("CCSPlayerController");
    
    for (auto* entity : entities) {
        if (!entity)
            continue;
        
        auto* controller = reinterpret_cast<c_cs_player_controller*>(entity);
        if (!controller || controller == g_ctx->m_local_controller)
            continue;
        
        if (!controller->m_pawn_is_alive())
            continue;
        
        auto handle = controller->m_pawn();
        if (!handle.is_valid())
            continue;
        
        auto* pawn = g_interfaces->m_entity_system->get_base_entity<c_cs_player_pawn>(handle.get_entry_index());
        if (!pawn || !is_target_valid(pawn))
            continue;
        
        // Get aim point
        vec3_t aim_point = get_aim_point(pawn);
        
        // Check visibility
        bool visible = can_see_player(pawn, aim_point);
        if (!visible)
            continue;
        
        // Calculate FOV
        float fov = get_fov_to_player(pawn, aim_point);
        
        // Create target info
        legit_target_t target;
        target.m_pawn = pawn;
        target.m_aim_point = aim_point;
        target.m_fov = fov;
        target.m_distance = eye_pos.dist(aim_point);
        target.m_health = pawn->m_health();
        target.m_visible = visible;
        
        m_targets.push_back(target);
    }
}

legit_target_t* c_legit_bot::get_best_target() {
    if (m_targets.empty())
        return nullptr;
    
    legit_target_t* best = nullptr;
    
    switch (g_cfg->legit_bot.m_target_selection) {
        case 0: // Closest to crosshair
            for (auto& target : m_targets) {
                if (!best || target.m_fov < best->m_fov) {
                    best = &target;
                }
            }
            break;
            
        case 1: // Lowest health
            for (auto& target : m_targets) {
                if (!best || target.m_health < best->m_health) {
                    best = &target;
                }
            }
            break;
            
        case 2: // Closest distance
            for (auto& target : m_targets) {
                if (!best || target.m_distance < best->m_distance) {
                    best = &target;
                }
            }
            break;
    }
    
    return best;
}

vec3_t c_legit_bot::get_aim_point(c_cs_player_pawn* pawn) {
    vec3_t best_point = pawn->m_scene_node()->m_abs_origin();
    best_point.z += 64; // Default to head height
    
    // Check which hitboxes are enabled
    struct { const char* bone; bool* enabled; float z_offset; } hitboxes[] = {
        {"head", &g_cfg->legit_bot.m_hitboxes[0], 0.0f},
        {"neck_0", &g_cfg->legit_bot.m_hitboxes[1], 0.0f},
        {"spine_2", &g_cfg->legit_bot.m_hitboxes[2], 0.0f},
        {"spine_0", &g_cfg->legit_bot.m_hitboxes[3], 0.0f}
    };
    
    vec3_t eye_pos = g_ctx->m_local_pawn->get_eye_pos();
    float best_fov = FLT_MAX;
    
    for (auto& hb : hitboxes) {
        if (!*hb.enabled)
            continue;
        
        int bone_idx = pawn->get_bone_index(hb.bone);
        if (bone_idx == -1)
            continue;
        
        vec3_t point = pawn->get_bone_position(bone_idx);
        
        // Calculate FOV to this point
        vec3_t angles = g_math->aim_direction(eye_pos, point);
        vec3_t current_angles;
        g_interfaces->m_csgo_input->get_view_angles(current_angles);
        
        float fov = g_math->get_fov(current_angles, angles);
        
        if (fov < best_fov) {
            best_fov = fov;
            best_point = point;
        }
    }
    
    return best_point;
}

float c_legit_bot::get_fov_to_player(c_cs_player_pawn* pawn, const vec3_t& aim_point) {
    vec3_t eye_pos = g_ctx->m_local_pawn->get_eye_pos();
    vec3_t aim_angles = g_math->aim_direction(eye_pos, aim_point);
    
    vec3_t current_angles;
    g_interfaces->m_csgo_input->get_view_angles(current_angles);
    
    return g_math->get_fov(current_angles, aim_angles);
}

void c_legit_bot::smooth_aim(vec3_t& current_angles, const vec3_t& target_angles) {
    float smooth = g_cfg->legit_bot.m_smooth;
    if (smooth <= 1.0f) {
        current_angles = target_angles;
        return;
    }
    
    // Calculate delta
    vec3_t delta = target_angles - current_angles;
    
    // Normalize angles
    g_math->normalize_angles(delta);
    
    // Apply smooth factor
    delta /= smooth;
    
    // Add to current angles
    current_angles += delta;
    
    // Normalize result
    g_math->normalize_angles(current_angles);
    g_math->clamp_angles(current_angles);
}

// ============================================================================
// LEGIT BOT - TRIGGERBOT
// ============================================================================

void c_legit_bot::run_triggerbot() {
    // Check key
    if (!g_key_handler->is_pressed(g_cfg->legit_bot.m_triggerbot_key, g_cfg->legit_bot.m_triggerbot_key_style))
        return;
    
    c_base_player_weapon* weapon = g_ctx->m_local_pawn->get_active_weapon();
    if (!weapon)
        return;
    
    c_cs_weapon_base_v_data* weapon_data = weapon->get_weapon_data();
    if (!weapon_data || !is_weapon_valid(weapon_data))
        return;
    
    // Check if we can shoot
    if (weapon->m_clip1() <= 0)
        return;
    
    if (weapon->m_next_primary_attack() > g_ctx->m_local_controller->m_tick_base())
        return;
    
    // Trace from eye to crosshair
    vec3_t eye_pos = g_ctx->m_local_pawn->get_eye_pos();
    vec3_t view_angles;
    g_interfaces->m_csgo_input->get_view_angles(view_angles);
    
    vec3_t forward;
    g_math->angle_vectors(view_angles, forward);
    
    vec3_t end_pos = eye_pos + forward * weapon_data->m_range();
    
    // Check for enemy at crosshair
    const auto& entities = g_entity_system->get("CCSPlayerController");
    
    for (auto* entity : entities) {
        auto* controller = reinterpret_cast<c_cs_player_controller*>(entity);
        if (!controller || controller == g_ctx->m_local_controller)
            continue;
        
        if (!controller->m_pawn_is_alive())
            continue;
        
        auto handle = controller->m_pawn();
        if (!handle.is_valid())
            continue;
        
        auto* pawn = g_interfaces->m_entity_system->get_base_entity<c_cs_player_pawn>(handle.get_entry_index());
        if (!pawn || !is_target_valid(pawn))
            continue;
        
        if (should_shoot(pawn)) {
            // Handle delay
            if (g_cfg->legit_bot.m_triggerbot_delay > 0) {
                if (!m_triggerbot_ready) {
                    m_triggerbot_time = std::chrono::steady_clock::now();
                    m_triggerbot_ready = true;
                }
                
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - m_triggerbot_time
                ).count();
                
                if (elapsed < g_cfg->legit_bot.m_triggerbot_delay)
                    return;
            }
            
            // Shoot!
            g_ctx->m_user_cmd->m_button_state.m_button_state |= IN_ATTACK;
            m_triggerbot_ready = false;
            return;
        }
    }
    
    m_triggerbot_ready = false;
}

bool c_legit_bot::should_shoot(c_cs_player_pawn* target) {
    vec3_t eye_pos = g_ctx->m_local_pawn->get_eye_pos();
    vec3_t view_angles;
    g_interfaces->m_csgo_input->get_view_angles(view_angles);
    
    // Get hitboxes to check
    std::vector<vec3_t> points;
    
    if (g_cfg->legit_bot.m_triggerbot_head_only) {
        int head_bone = target->get_bone_index("head");
        if (head_bone != -1) {
            points.push_back(target->get_bone_position(head_bone));
        }
    } else {
        // Check multiple hitboxes
        const char* bones[] = {"head", "neck_0", "spine_2", "spine_0"};
        for (auto& bone_name : bones) {
            int bone_idx = target->get_bone_index(bone_name);
            if (bone_idx != -1) {
                points.push_back(target->get_bone_position(bone_idx));
            }
        }
    }
    
    // Calculate forward direction
    vec3_t forward;
    g_math->angle_vectors(view_angles, forward);
    
    for (auto& point : points) {
        vec3_t delta = point - eye_pos;
        float distance = delta.length();
        
        // Normalize delta
        delta /= distance;
        
        // Calculate dot product
        float dot = forward.dot(delta);
        
        // Check if point is within tolerance (based on hitbox size)
        float tolerance = 0.998f; // ~4 degree cone
        
        if (dot >= tolerance && can_see_player(target, point)) {
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// LEGIT BOT - BACKTRACK
// ============================================================================

void c_legit_bot::store_records() {
    if (!g_interfaces->m_engine->is_in_game())
        return;
    
    int max_ticks = g_cfg->legit_bot.m_backtrack_ticks;
    
    const auto& entities = g_entity_system->get("CCSPlayerController");
    
    for (auto* entity : entities) {
        auto* controller = reinterpret_cast<c_cs_player_controller*>(entity);
        if (!controller || controller == g_ctx->m_local_controller)
            continue;
        
        auto handle = controller->m_pawn();
        if (!handle.is_valid())
            continue;
        
        auto* pawn = g_interfaces->m_entity_system->get_base_entity<c_cs_player_pawn>(handle.get_entry_index());
        if (!pawn || !pawn->is_alive())
            continue;
        
        int key = handle.to_int();
        auto& records = m_backtrack_records[key];
        
        // Check if this is a new tick
        float sim_time = pawn->m_sim_time();
        if (!records.empty() && records.front().m_simulation_time == sim_time)
            continue;
        
        // Store new record
        backtrack_record_t record;
        record.store(pawn);
        
        if (record.m_valid) {
            records.push_front(record);
            
            // Limit records
            while (records.size() > static_cast<size_t>(max_ticks))
                records.pop_back();
        }
    }
    
    // Clean up invalid records
    for (auto& [key, records] : m_backtrack_records) {
        records.erase(
            std::remove_if(records.begin(), records.end(), [](const backtrack_record_t& r) {
                return !r.is_valid();
            }),
            records.end()
        );
    }
}

void c_legit_bot::run_backtrack() {
    if (!m_current_target || !g_cfg->legit_bot.m_aim_backtrack)
        return;
    
    auto handle = m_current_target->m_pawn->get_handle();
    auto* record = get_best_record(m_current_target->m_pawn);
    
    if (!record)
        return;
    
    // Update aim point to backtrack position
    m_current_target->m_aim_point = record->m_head_position;
}

void c_legit_bot::draw_backtrack() {
    for (auto& [key, records] : m_backtrack_records) {
        if (records.empty())
            continue;
        
        auto& latest = records.front();
        if (!latest.m_pawn || !latest.m_pawn->is_alive())
            continue;
        
        // Draw backtrack trail
        for (size_t i = 0; i < records.size(); i++) {
            auto& record = records[i];
            if (!record.is_valid())
                continue;
            
            vec3_t screen;
            if (g_render->world_to_screen(record.m_head_position, screen)) {
                float alpha = 1.0f - (static_cast<float>(i) / records.size());
                g_render->circle(
                    vec3_t(screen.x, screen.y, 0),
                    c_color(1.0f, 1.0f, 1.0f, alpha * 0.5f),
                    4.0f
                );
            }
        }
    }
}

backtrack_record_t* c_legit_bot::get_best_record(c_cs_player_pawn* pawn) {
    auto handle = pawn->get_handle();
    int key = handle.to_int();
    
    auto it = m_backtrack_records.find(key);
    if (it == m_backtrack_records.end())
        return nullptr;
    
    vec3_t eye_pos = g_ctx->m_local_pawn->get_eye_pos();
    vec3_t view_angles;
    g_interfaces->m_csgo_input->get_view_angles(view_angles);
    
    backtrack_record_t* best = nullptr;
    float best_fov = FLT_MAX;
    
    for (auto& record : it->second) {
        if (!record.is_valid())
            continue;
        
        vec3_t angles = g_math->aim_direction(eye_pos, record.m_head_position);
        float fov = g_math->get_fov(view_angles, angles);
        
        if (fov < best_fov) {
            best_fov = fov;
            best = &record;
        }
    }
    
    return best;
}

// ============================================================================
// LEGIT BOT - HELPERS
// ============================================================================

bool c_legit_bot::is_weapon_valid(c_cs_weapon_base_v_data* weapon_data) {
    int type = weapon_data->m_weapon_type();
    
    return type == WEAPONTYPE_PISTOL ||
           type == WEAPONTYPE_SUBMACHINEGUN ||
           type == WEAPONTYPE_RIFLE ||
           type == WEAPONTYPE_SHOTGUN ||
           type == WEAPONTYPE_SNIPER_RIFLE ||
           type == WEAPONTYPE_MACHINEGUN;
}

bool c_legit_bot::is_target_valid(c_cs_player_pawn* pawn) {
    if (!pawn || !pawn->is_alive())
        return false;
    
    if (pawn == g_ctx->m_local_pawn)
        return false;
    
    // Check team
    if (pawn->m_team_num() == g_ctx->m_local_pawn->m_team_num())
        return false;
    
    // Check immunity
    if (pawn->m_gun_game_immunity())
        return false;
    
    return true;
}

bool c_legit_bot::can_see_player(c_cs_player_pawn* pawn, const vec3_t& point) {
    // Use autowall or simple trace
    vec3_t eye_pos = g_ctx->m_local_pawn->get_eye_pos();
    
    // Simple visibility check - you can expand this with proper tracing
    return true; // For now, assume visible if we found them
}
