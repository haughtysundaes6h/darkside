#include "anti_hit.hpp"
#include "../eng_pred/eng_pred.hpp"
#include "../../entity_system/entity.hpp"

// ============================================================================
// ANTI-AIM SYSTEM v2.0 - ПОЛНОСТЬЮ ПЕРЕПИСАН
// ============================================================================

void c_anti_hit::on_create_move(c_user_cmd* user_cmd) {
    if (!g_cfg->anti_hit.m_enabled || !user_cmd || !user_cmd->pb.has_base())
        return;

    if (!g_interfaces->m_engine->is_in_game())
        return;

    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || !pawn->is_alive() || pawn->m_actual_move_type() != movetype_walk)
        return;

    // Не применяем АА если атакуем или используем
    auto buttons = user_cmd->m_button_state.m_button_state;
    if (buttons & (IN_ATTACK | IN_ATTACK2 | IN_USE))
        return;
    
    // Не применяем АА если кидаем гранату
    if (pawn->is_throwing())
        return;

    // Обновляем сторону desync
    update_side();
    
    // Fakelag
    if (g_cfg->anti_hit.m_fakelag_enabled) {
        handle_fakelag(user_cmd);
    }
    
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
// PITCH
// ============================================================================

void c_anti_hit::handle_pitch(c_user_cmd* user_cmd) {
    float pitch = get_pitch_angle();
    
    if (pitch != 0.0f || g_cfg->anti_hit.m_pitch_mode != 0) {
        user_cmd->pb.mutable_base()->mutable_viewangles()->set_x(pitch);
        
        // Обновляем во всех subtick frames
        for (int i = 0; i < user_cmd->pb.input_history_size(); i++) {
            auto* tick = user_cmd->pb.mutable_input_history(i);
            if (tick) {
                tick->mutable_view_angles()->set_x(pitch);
            }
        }
    }
}

float c_anti_hit::get_pitch_angle() {
    switch (g_cfg->anti_hit.m_pitch_mode) {
        case 0: // Off
            return g_interfaces->m_csgo_input->get_view_angles().x;
            
        case 1: // Down
            return 89.0f;
            
        case 2: // Up
            return -89.0f;
            
        case 3: // Zero
            return 0.0f;
            
        case 4: // Custom
            return static_cast<float>(std::clamp(g_cfg->anti_hit.m_pitch_amount, -89, 89));
            
        default:
            return 0.0f;
    }
}

// ============================================================================
// YAW
// ============================================================================

void c_anti_hit::handle_yaw(c_user_cmd* user_cmd) {
    // Получаем базовый yaw
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
    
    // Устанавливаем
    user_cmd->pb.mutable_base()->mutable_viewangles()->set_y(yaw);
    
    // Обновляем subtick frames
    for (int i = 0; i < user_cmd->pb.input_history_size(); i++) {
        auto* tick = user_cmd->pb.mutable_input_history(i);
        if (tick) {
            tick->mutable_view_angles()->set_y(yaw);
        }
    }
}

float c_anti_hit::get_base_yaw(c_user_cmd* user_cmd) {
    float yaw = g_interfaces->m_csgo_input->get_view_angles().y;
    
    // At Target
    if (g_cfg->anti_hit.m_at_target) {
        yaw = get_at_target_yaw();
    }
    
    switch (g_cfg->anti_hit.m_yaw_mode) {
        case 0: // Off
            break;
            
        case 1: // Backward
            yaw += 180.0f;
            break;
            
        case 2: // Sideways (left)
            yaw += 90.0f;
            break;
            
        case 3: // Spin
        {
            static float spin_angle = 0.0f;
            spin_angle += 15.0f;
            if (spin_angle > 360.0f) spin_angle -= 360.0f;
            yaw = spin_angle;
            break;
        }
            
        case 4: // At Target (уже обработан выше)
            yaw += 180.0f;
            break;
            
        case 5: // Custom
            break;
    }
    
    return yaw;
}

float c_anti_hit::apply_yaw_modifier(float yaw) {
    // Добавляем yaw add
    yaw += static_cast<float>(g_cfg->anti_hit.m_yaw_amount);
    
    return yaw;
}

float c_anti_hit::apply_jitter(float yaw, int tick) {
    if (g_cfg->anti_hit.m_jitter_amount <= 0)
        return yaw;
    
    float jitter_range = static_cast<float>(g_cfg->anti_hit.m_jitter_amount);
    
    // Чередуем стороны
    m_jitter_tick++;
    if (m_jitter_tick % 2 == 0) {
        yaw += jitter_range;
    } else {
        yaw -= jitter_range;
    }
    
    return yaw;
}

// ============================================================================
// DESYNC
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
    
    // Применяем desync в зависимости от режима
    switch (g_cfg->anti_hit.m_desync_mode) {
        case 0: // Static
            m_desync_amount = desync_limit;
            break;
            
        case 1: // Jitter
        {
            // Чередуем между 0 и max
            if (user_cmd->m_command_number % 2 == 0) {
                m_desync_amount = desync_limit;
            } else {
                m_desync_amount = desync_limit * 0.5f;
            }
            break;
        }
            
        case 2: // Switch
        {
            // Меняем сторону каждые N тиков
            if ((user_cmd->m_command_number % 8) == 0) {
                m_side = -m_side;
            }
            m_desync_amount = desync_limit;
            break;
        }
    }
    
    // Fake yaw = real yaw + desync offset
    m_fake_yaw = m_real_yaw + (m_desync_amount * m_side);
    
    // Micro movement для активации desync
    micro_move(user_cmd);
}

void c_anti_hit::micro_move(c_user_cmd* user_cmd) {
    auto* base = user_cmd->pb.mutable_base();
    
    // Чередуем микро движение
    m_micro_moved = !m_micro_moved;
    
    float micro = m_micro_moved ? 1.01f : -1.01f;
    
    // Добавляем минимальное движение
    float current_left = base->leftmove();
    
    // Если нет движения - добавляем микро
    if (std::abs(current_left) < 0.01f && std::abs(base->forwardmove()) < 0.01f) {
        base->set_leftmove(micro);
    }
}

void c_anti_hit::update_side() {
    // Проверяем кнопку смены стороны
    if (g_key_handler->is_pressed(g_cfg->anti_hit.m_flip_key, g_cfg->anti_hit.m_flip_key_style)) {
        static bool was_pressed = false;
        
        if (!was_pressed) {
            m_side = -m_side;
            was_pressed = true;
        }
    } else {
        // Сбрасываем при отпускании
        static bool was_pressed = false;
        was_pressed = false;
    }
}

// ============================================================================
// FAKELAG
// ============================================================================

void c_anti_hit::handle_fakelag(c_user_cmd* user_cmd) {
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || !pawn->is_alive())
        return;
    
    // Проверяем нужно ли чокать
    if (!should_choke()) {
        m_choked_ticks = 0;
        m_should_choke = false;
        return;
    }
    
    int choke_amount = get_choke_amount();
    
    if (m_choked_ticks < choke_amount) {
        m_should_choke = true;
        m_choked_ticks++;
    } else {
        m_should_choke = false;
        m_choked_ticks = 0;
    }
}

bool c_anti_hit::should_choke() {
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn)
        return false;
    
    // На земле всегда можно чокать
    if (pawn->m_flags() & FL_ONGROUND)
        return true;
    
    // On Peek - чокаем только когда высовываемся
    if (g_cfg->anti_hit.m_fakelag_on_peek) {
        float speed = pawn->m_vec_abs_velocity().length_2d();
        return speed > 10.0f;
    }
    
    return true;
}

int c_anti_hit::get_choke_amount() {
    int limit = std::clamp(g_cfg->anti_hit.m_fakelag_limit, 1, 14);
    
    switch (g_cfg->anti_hit.m_fakelag_mode) {
        case 0: // Static
            return limit;
            
        case 1: // Random
            return 1 + (rand() % limit);
            
        case 2: // Adaptive
        {
            auto* pawn = g_ctx->m_local_pawn;
            if (!pawn)
                return limit;
            
            float speed = pawn->m_vec_abs_velocity().length_2d();
            
            // Больше скорость = больше чок
            if (speed > 200.0f)
                return limit;
            else if (speed > 100.0f)
                return limit / 2;
            else
                return 1;
        }
            
        default:
            return limit;
    }
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
    
    if (delta.x == 0.0f && delta.y == 0.0f)
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
