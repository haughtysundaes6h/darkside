#include "anti_hit.hpp"
#include "../eng_pred/eng_pred.hpp"
#include "../../entity_system/entity.hpp"

// ============================================================================
// CS2 ANTI-AIM SYSTEM v2.1
// Работает с protobuf и subtick системой
// ============================================================================

void c_anti_hit::on_create_move(c_user_cmd* user_cmd) {
    if (!g_cfg->anti_hit.m_enabled || !user_cmd || !user_cmd->pb.has_base())
        return;

    if (!g_interfaces->m_engine->is_in_game())
        return;

    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || !pawn->is_alive() || pawn->m_actual_move_type() != movetype_walk)
        return;

    // Не применяем AA если атакуем
    auto buttons = user_cmd->m_button_state.m_button_state;
    if (buttons & (IN_ATTACK | IN_ATTACK2 | IN_USE))
        return;
    
    // Не применяем если кидаем гранату
    if (pawn->is_throwing())
        return;

    // Обновляем сторону desync
    update_side();
    
    // Pitch
    handle_pitch(user_cmd);
    
    // Yaw
    handle_yaw(user_cmd);
    
    // Desync
    if (g_cfg->anti_hit.m_desync_enabled) {
        handle_desync(user_cmd);
    }
}

// ============================================================================
// PITCH - CS2
// ============================================================================

void c_anti_hit::handle_pitch(c_user_cmd* user_cmd) {
    float pitch = get_pitch_angle();
    
    auto* base = user_cmd->pb.mutable_base();
    
    // Устанавливаем pitch в основной viewangles
    base->mutable_viewangles()->set_x(pitch);
    
    // CS2: Обновляем pitch во всех input_history записях
    for (int i = 0; i < user_cmd->pb.input_history_size(); i++) {
        auto* history = user_cmd->pb.mutable_input_history(i);
        if (history && history->has_view_angles()) {
            history->mutable_view_angles()->set_x(pitch);
        }
    }
}

float c_anti_hit::get_pitch_angle() {
    switch (g_cfg->anti_hit.m_pitch_mode) {
        case 0: // Off
            return g_interfaces->m_csgo_input->get_view_angles().x;
            
        case 1: // Down (emotion)
            return 89.0f;
            
        case 2: // Up
            return -89.0f;
            
        case 3: // Zero (fake up)
            return 0.0f;
            
        case 4: // Custom
            return static_cast<float>(std::clamp(g_cfg->anti_hit.m_pitch_amount, -89, 89));
            
        default:
            return 0.0f;
    }
}

// ============================================================================
// YAW - CS2
// ============================================================================

void c_anti_hit::handle_yaw(c_user_cmd* user_cmd) {
    float yaw = get_base_yaw(user_cmd);
    
    // Применяем модификаторы
    yaw = apply_yaw_modifier(yaw);
    
    // Применяем jitter
    if (g_cfg->anti_hit.m_jitter_enabled) {
        yaw = apply_jitter(yaw, user_cmd->m_command_number);
    }
    
    // Нормализуем
    yaw = std::remainder(yaw, 360.0f);
    
    // Сохраняем real yaw
    m_real_yaw = yaw;
    
    auto* base = user_cmd->pb.mutable_base();
    
    // Устанавливаем yaw
    base->mutable_viewangles()->set_y(yaw);
    
    // CS2: Обновляем yaw во всех input_history записях
    for (int i = 0; i < user_cmd->pb.input_history_size(); i++) {
        auto* history = user_cmd->pb.mutable_input_history(i);
        if (history && history->has_view_angles()) {
            history->mutable_view_angles()->set_y(yaw);
        }
    }
}

float c_anti_hit::get_base_yaw(c_user_cmd* user_cmd) {
    float yaw = g_interfaces->m_csgo_input->get_view_angles().y;
    
    // At Target - смотрим на ближайшего врага
    if (g_cfg->anti_hit.m_at_target) {
        float at_target_yaw = get_at_target_yaw();
        if (std::isfinite(at_target_yaw)) {
            yaw = at_target_yaw;
        }
    }
    
    switch (g_cfg->anti_hit.m_yaw_mode) {
        case 0: // Off
            break;
            
        case 1: // Backward
            yaw += 180.0f;
            break;
            
        case 2: // Sideways
            yaw += 90.0f * m_side;
            break;
            
        case 3: // Spin
        {
            static float spin_yaw = 0.0f;
            spin_yaw += 15.0f;
            if (spin_yaw > 360.0f) spin_yaw -= 360.0f;
            yaw = spin_yaw;
            break;
        }
            
        case 4: // At Target (уже обработан)
            yaw += 180.0f;
            break;
            
        case 5: // Custom - просто используем yaw add
            break;
    }
    
    return yaw;
}

float c_anti_hit::apply_yaw_modifier(float yaw) {
    yaw += static_cast<float>(g_cfg->anti_hit.m_yaw_amount);
    return yaw;
}

float c_anti_hit::apply_jitter(float yaw, int tick) {
    if (g_cfg->anti_hit.m_jitter_amount <= 0)
        return yaw;
    
    float range = static_cast<float>(g_cfg->anti_hit.m_jitter_amount);
    
    switch (g_cfg->anti_hit.m_jitter_mode) {
        case 0: // Offset - чередуем стороны
            m_jitter_tick++;
            yaw += range * ((m_jitter_tick % 2) ? 1.0f : -1.0f);
            break;
            
        case 1: // Center - возвращаемся в центр
            m_jitter_tick++;
            if (m_jitter_tick % 3 == 0) {
                yaw += range;
            } else if (m_jitter_tick % 3 == 1) {
                yaw -= range;
            }
            // На каждый 3й тик - центр
            break;
            
        case 2: // Random
            yaw += range * (static_cast<float>(rand() % 200 - 100) / 100.0f);
            break;
    }
    
    return yaw;
}

// ============================================================================
// DESYNC - CS2
// ============================================================================

void c_anti_hit::handle_desync(c_user_cmd* user_cmd) {
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || !pawn->is_alive())
        return;
    
    // Получаем лимиты
    float left_limit = static_cast<float>(g_cfg->anti_hit.m_desync_left);
    float right_limit = static_cast<float>(g_cfg->anti_hit.m_desync_right);
    
    // Выбираем лимит на основе стороны
    float desync_limit = (m_side == -1) ? left_limit : right_limit;
    
    // Режим desync
    switch (g_cfg->anti_hit.m_desync_mode) {
        case 0: // Static
            m_desync_amount = desync_limit;
            break;
            
        case 1: // Jitter
            if (user_cmd->m_command_number % 2 == 0) {
                m_desync_amount = desync_limit;
            } else {
                m_desync_amount = desync_limit * 0.5f;
            }
            break;
            
        case 2: // Switch
            if ((user_cmd->m_command_number % 8) == 0) {
                m_side = -m_side;
            }
            m_desync_amount = desync_limit;
            break;
    }
    
    // Вычисляем fake yaw
    m_fake_yaw = m_real_yaw + (m_desync_amount * m_side);
    
    // Микро движение для активации desync в CS2
    micro_move(user_cmd);
}

void c_anti_hit::micro_move(c_user_cmd* user_cmd) {
    auto* base = user_cmd->pb.mutable_base();
    
    // Чередуем направление
    m_micro_moved = !m_micro_moved;
    
    // CS2: Минимальное движение для активации анимаций
    float micro = m_micro_moved ? 0.015f : -0.015f;
    
    // Добавляем только если нет движения
    float forward = base->forwardmove();
    float left = base->leftmove();
    
    if (std::abs(forward) < 0.01f && std::abs(left) < 0.01f) {
        base->set_leftmove(micro);
    }
}

void c_anti_hit::update_side() {
    static bool flip_pressed = false;
    
    bool is_pressed = g_key_handler->is_pressed(g_cfg->anti_hit.m_flip_key, g_cfg->anti_hit.m_flip_key_style);
    
    if (is_pressed && !flip_pressed) {
        m_side = -m_side;
    }
    
    flip_pressed = is_pressed;
}

// ============================================================================
// AT TARGET
// ============================================================================

float c_anti_hit::get_at_target_yaw() {
    auto* local = g_ctx->m_local_pawn;
    if (!local || !local->m_scene_node())
        return g_interfaces->m_csgo_input->get_view_angles().y;
    
    auto* target = find_closest_enemy();
    if (!target || !target->m_scene_node())
        return g_interfaces->m_csgo_input->get_view_angles().y;
    
    vec3_t local_pos = local->m_scene_node()->m_abs_origin();
    vec3_t target_pos = target->m_scene_node()->m_abs_origin();
    
    vec3_t delta = target_pos - local_pos;
    
    if (std::abs(delta.x) < 0.001f && std::abs(delta.y) < 0.001f)
        return g_interfaces->m_csgo_input->get_view_angles().y;
    
    return rad2deg(std::atan2(delta.y, delta.x));
}

c_cs_player_pawn* c_anti_hit::find_closest_enemy() {
    auto* local = g_ctx->m_local_pawn;
    if (!local)
        return nullptr;
    
    auto local_data = g_prediction->get_local_data();
    if (!local_data)
        return nullptr;
    
    vec3_t eye_pos = local_data->m_eye_pos;
    
    c_cs_player_pawn* best_target = nullptr;
    float best_distance = FLT_MAX;
    
    for (auto* entity : g_entity_system->get_players(ENTITY_ENEMIES_ONLY)) {
        if (!entity || !entity->is_alive() || !entity->m_scene_node())
            continue;
        
        float distance = entity->m_scene_node()->m_abs_origin().dist(eye_pos);
        
        if (distance < best_distance) {
            best_distance = distance;
            best_target = entity;
        }
    }
    
    return best_target;
}
