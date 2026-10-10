#include "movement.hpp"
#include "../eng_pred/eng_pred.hpp"
#include "command_movement.hpp"

// ============================================================================
// MOVEMENT SYSTEM v2.0 - ПОЛНОСТЬЮ ПЕРЕПИСАН
// ============================================================================

void c_movement::on_create_move(c_user_cmd* user_cmd) {
    if (!user_cmd || !user_cmd->pb.has_base())
        return;
    
    if (!g_ctx->m_local_pawn || !g_ctx->m_local_pawn->is_alive())
        return;
    
    if (g_ctx->m_local_pawn->m_actual_move_type() != movetype_walk)
        return;
    
    // Сохраняем предыдущий yaw для стрейфа
    m_old_yaw = m_yaw_history.sample(
        g_ctx->m_local_pawn, 
        user_cmd->m_command_number, 
        user_cmd->pb.base().viewangles().y()
    );
    
    // Бхоп
    if (g_cfg->misc.m_bunny_hop) {
        bunnyhop(user_cmd);
    }
    
    // Автострейф
    if (g_cfg->misc.m_auto_strafe) {
        auto_strafe(user_cmd);
    }
    
    // Edge Jump
    if (g_cfg->misc.m_edge_jump && 
        g_key_handler->is_pressed(g_cfg->misc.m_edge_jump_key, g_cfg->misc.m_edge_jump_key_style)) {
        edge_jump(user_cmd);
    }
    
    // Jump Bug
    if (g_cfg->misc.m_jump_bug && 
        g_key_handler->is_pressed(g_cfg->misc.m_jump_bug_key, g_cfg->misc.m_jump_bug_key_style)) {
        jump_bug(user_cmd);
    }
    
    // Infinite Duck
    if (g_cfg->misc.m_infinite_duck) {
        infinite_duck(user_cmd);
    }
    
    // Обновляем состояние
    m_was_on_ground = is_on_ground(g_ctx->m_local_pawn);
    m_last_velocity = g_ctx->m_local_pawn->m_vec_abs_velocity();
}

// ============================================================================
// BHOP - ИСПРАВЛЕННЫЙ
// ============================================================================

bool c_movement::bunnyhop(c_user_cmd* user_cmd) {
    if (!user_cmd || !user_cmd->pb.has_base())
        return false;
    
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || !pawn->is_alive())
        return false;
    
    // Проверяем нажат ли прыжок
    bool jump_held = (user_cmd->m_button_state.m_button_state & IN_JUMP) != 0;
    if (!jump_held)
        return false;
    
    // Проверяем что мы на земле или приземляемся
    bool on_ground = (pawn->m_flags() & FL_ONGROUND) != 0;
    
    // В воде не работает
    if (pawn->m_flags() & (FL_INWATER | FL_WATERJUMP))
        return false;
    
    // Если в воздухе - убираем прыжок чтобы не зажимать
    if (!on_ground) {
        // Убираем флаг прыжка пока в воздухе
        user_cmd->m_button_state.m_button_state &= ~IN_JUMP;
        return false;
    }
    
    // На земле - разрешаем прыжок
    // Прыжок уже нажат, ничего не делаем
    return true;
}

// ============================================================================
// AUTO STRAFE - ИСПРАВЛЕННЫЙ
// ============================================================================

void c_movement::auto_strafe(c_user_cmd* user_cmd) {
    if (!user_cmd || !user_cmd->pb.has_base())
        return;
    
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || !pawn->is_alive())
        return;
    
    // Только в воздухе
    if (pawn->m_flags() & FL_ONGROUND)
        return;
    
    // В воде не работает
    if (pawn->m_flags() & (FL_INWATER | FL_WATERJUMP))
        return;
    
    auto* base = user_cmd->pb.mutable_base();
    vec3_t velocity = pawn->m_vec_abs_velocity();
    float speed = velocity.length_2d();
    
    // Если скорость маленькая - просто идём вперёд
    if (speed < 15.0f) {
        base->set_forwardmove(1.0f);
        base->set_leftmove(0.0f);
        return;
    }
    
    // Получаем текущий yaw и кнопки
    float current_yaw = base->viewangles().y();
    auto buttons = user_cmd->m_button_state.m_button_state;
    
    // Определяем направление движения от кнопок
    float key_forward = 0.0f, key_left = 0.0f;
    if (buttons & IN_FORWARD) key_forward += 1.0f;
    if (buttons & IN_BACK) key_forward -= 1.0f;
    if (buttons & IN_MOVELEFT) key_left += 1.0f;
    if (buttons & IN_MOVERIGHT) key_left -= 1.0f;
    
    // Вычисляем целевой угол на основе нажатых кнопок
    float wish_angle = current_yaw;
    if (std::abs(key_forward) > 0.01f || std::abs(key_left) > 0.01f) {
        wish_angle += rad2deg(std::atan2(key_left, key_forward));
    }
    
    // Вычисляем угол скорости
    float vel_angle = rad2deg(std::atan2(velocity.y, velocity.x));
    
    // Дельта между желаемым углом и текущей скоростью
    float delta = g_math->normalize_yaw(wish_angle - vel_angle);
    
    // Вычисляем идеальный угол стрейфа
    float ideal_angle = get_ideal_strafe_angle(speed);
    
    // Применяем сглаживание
    float smooth = g_cfg->misc.m_strafe_smooth / 100.0f;
    ideal_angle *= smooth;
    
    // Выбираем тип стрейфа
    switch (g_cfg->misc.m_strafe_type) {
        case 0: // Legit - следуем за мышкой
        {
            float yaw_delta = g_math->normalize_yaw(current_yaw - m_old_yaw);
            
            if (std::abs(yaw_delta) > 0.1f) {
                // Стрейфим в сторону поворота мыши
                if (yaw_delta > 0.0f) {
                    base->set_forwardmove(0.0f);
                    base->set_leftmove(1.0f);
                } else {
                    base->set_forwardmove(0.0f);
                    base->set_leftmove(-1.0f);
                }
            }
            break;
        }
        
        case 1: // Directional - в направлении движения
        {
            // Корректируем движение к желаемому углу
            if (delta > ideal_angle) {
                base->set_forwardmove(0.0f);
                base->set_leftmove(1.0f);
            } else if (delta < -ideal_angle) {
                base->set_forwardmove(0.0f);
                base->set_leftmove(-1.0f);
            } else {
                // Чередуем стороны для максимальной скорости
                m_strafe_index++;
                if (m_strafe_index % 2 == 0) {
                    base->set_forwardmove(0.0f);
                    base->set_leftmove(1.0f);
                } else {
                    base->set_forwardmove(0.0f);
                    base->set_leftmove(-1.0f);
                }
            }
            break;
        }
        
        case 2: // Rage - максимальная скорость
        {
            // Оптимальный стрейф с чередованием сторон
            m_strafe_index++;
            
            float strafe_angle = ideal_angle * ((m_strafe_index % 2) ? 1.0f : -1.0f);
            float target_yaw = vel_angle + strafe_angle;
            
            // Поворачиваем movement относительно view angles
            float rotation = deg2rad(target_yaw - current_yaw);
            float sin_rot = std::sin(rotation);
            float cos_rot = std::cos(rotation);
            
            // Идём влево/вправо в зависимости от стороны стрейфа
            if (m_strafe_index % 2 == 0) {
                base->set_forwardmove(0.0f);
                base->set_leftmove(-1.0f);
            } else {
                base->set_forwardmove(0.0f);
                base->set_leftmove(1.0f);
            }
            break;
        }
    }
}

// ============================================================================
// MOVEMENT FIX - ИСПРАВЛЕННЫЙ
// ============================================================================

void c_movement::movement_fix(c_user_cmd* user_cmd, vec3_t original_angles) {
    if (!user_cmd || !user_cmd->pb.has_base())
        return;
    
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || pawn->m_actual_move_type() != movetype_walk)
        return;
    
    auto* base = user_cmd->pb.mutable_base();
    
    float old_yaw = original_angles.y;
    float new_yaw = base->viewangles().y();
    
    // Получаем текущие значения движения
    float forward = base->forwardmove();
    float left = base->leftmove();
    
    // Вычисляем разницу углов
    float delta = deg2rad(new_yaw - old_yaw);
    float sin_delta = std::sin(delta);
    float cos_delta = std::cos(delta);
    
    // Поворачиваем вектор движения
    float new_forward = cos_delta * forward + sin_delta * left;
    float new_left = cos_delta * left - sin_delta * forward;
    
    // Clamp значения
    new_forward = std::clamp(new_forward, -1.0f, 1.0f);
    new_left = std::clamp(new_left, -1.0f, 1.0f);
    
    base->set_forwardmove(new_forward);
    base->set_leftmove(new_left);
}

// ============================================================================
// EDGE JUMP
// ============================================================================

void c_movement::edge_jump(c_user_cmd* user_cmd) {
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || !pawn->is_alive())
        return;
    
    bool on_ground = is_on_ground(pawn);
    
    // Считаем тики на земле
    if (on_ground) {
        m_ground_ticks++;
    } else {
        m_ground_ticks = 0;
    }
    
    // Если были на земле и теперь падаем - прыгаем
    if (m_was_on_ground && !on_ground && m_ground_ticks == 0) {
        // Мы только что сошли с края - это наш шанс!
        // Уже поздно прыгать в этом тике
    }
    
    // Проверяем будем ли мы падать в следующем тике
    if (on_ground && is_at_edge(pawn)) {
        user_cmd->m_button_state.m_button_state |= IN_JUMP;
    }
}

// ============================================================================
// JUMP BUG
// ============================================================================

void c_movement::jump_bug(c_user_cmd* user_cmd) {
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || !pawn->is_alive())
        return;
    
    bool on_ground = is_on_ground(pawn);
    float fall_velocity = -pawn->m_vec_abs_velocity().z;
    
    // Если на земле - сбрасываем
    if (on_ground) {
        m_should_duck = false;
        return;
    }
    
    // Только если падаем
    if (fall_velocity < 100.0f)
        return;
    
    // Проверяем скоро ли приземлимся
    if (will_land_next_tick(pawn)) {
        // Duck + Jump на фрейме приземления
        user_cmd->m_button_state.m_button_state |= IN_DUCK;
        user_cmd->m_button_state.m_button_state |= IN_JUMP;
    }
}

// ============================================================================
// INFINITE DUCK
// ============================================================================

void c_movement::infinite_duck(c_user_cmd* user_cmd) {
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || !pawn->is_alive())
        return;
    
    // Проверяем duck stamina
    // Если duck зажат - чередуем duck каждые N тиков чтобы сбросить stamina
    if (user_cmd->m_button_state.m_button_state & IN_DUCK) {
        // Сбрасываем duck каждые 8 тиков на 1 тик
        if (user_cmd->m_command_number % 8 == 0) {
            user_cmd->m_button_state.m_button_state &= ~IN_DUCK;
        }
    }
}

// ============================================================================
// AUTO STOP (ДЛЯ RAGE)
// ============================================================================

void c_movement::auto_stop(c_user_cmd* user_cmd, c_cs_player_pawn* local_player, 
                           c_base_player_weapon* weapon, bool no_spread, bool enabled) {
    if (!enabled || no_spread)
        return;
    
    if (!user_cmd || !local_player || !weapon)
        return;
    
    if (!(local_player->m_flags() & FL_ONGROUND))
        return;
    
    // Убираем shift
    user_cmd->m_button_state.m_button_state &= ~IN_SPEED;
    
    // Получаем максимальную скорость для стрельбы
    float max_speed = weapon->get_max_speed() * 0.34f;
    
    limit_movement(user_cmd, max_speed);
}

void c_movement::slow_walk(c_user_cmd* user_cmd, float max_speed) {
    limit_movement(user_cmd, max_speed);
}

// ============================================================================
// HELPERS
// ============================================================================

bool c_movement::is_on_ground(c_cs_player_pawn* pawn) {
    if (!pawn)
        return false;
    return (pawn->m_flags() & FL_ONGROUND) != 0;
}

bool c_movement::will_land_next_tick(c_cs_player_pawn* pawn) {
    if (!pawn)
        return false;
    
    vec3_t origin = pawn->m_scene_node()->m_abs_origin();
    vec3_t velocity = pawn->m_vec_abs_velocity();
    
    // Простая проверка - если падаем быстро и близко к земле
    float tick_interval = g_interfaces->m_global_vars->m_interval_per_tick;
    float gravity = 800.0f;
    
    // Предсказываем позицию
    float next_z_vel = velocity.z - gravity * tick_interval;
    float next_z = origin.z + next_z_vel * tick_interval;
    
    // Проверяем collision bounds
    auto* collision = pawn->m_collision();
    if (!collision)
        return false;
    
    float feet_z = origin.z + collision->m_mins().z;
    float next_feet_z = next_z + collision->m_mins().z;
    
    // Если ноги уйдут ниже текущей позиции на значительное расстояние
    // и скорость падения большая - скоро приземлимся
    return velocity.z < -200.0f && (next_feet_z < feet_z - 5.0f);
}

bool c_movement::is_at_edge(c_cs_player_pawn* pawn) {
    if (!pawn)
        return false;
    
    vec3_t origin = pawn->m_scene_node()->m_abs_origin();
    vec3_t velocity = pawn->m_vec_abs_velocity();
    
    float speed = velocity.length_2d();
    if (speed < 50.0f)
        return false;
    
    // Направление движения
    vec3_t dir = velocity.normalize_2d();
    
    // Проверяем точку впереди
    vec3_t check_pos = origin + dir * 15.0f;
    
    // Для полной реализации нужен trace
    // Пока используем эвристику на основе скорости
    return speed > 150.0f;
}

float c_movement::get_ideal_strafe_angle(float speed) {
    if (speed <= 0.0f)
        return 0.0f;
    
    // Формула идеального угла стрейфа
    // angle = atan(30 / speed)
    float accel = 30.0f;
    float ideal = rad2deg(std::atan(accel / speed));
    
    return std::clamp(ideal, 0.0f, 45.0f);
}

void c_movement::limit_movement(c_user_cmd* user_cmd, float max_speed) {
    if (!user_cmd || !user_cmd->pb.has_base())
        return;
    
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn)
        return;
    
    auto* weapon = pawn->get_active_weapon();
    if (!weapon)
        return;
    
    float weapon_speed = weapon->get_max_speed();
    if (weapon_speed <= 0.0f)
        return;
    
    auto* base = user_cmd->pb.mutable_base();
    vec3_t velocity = pawn->m_vec_abs_velocity();
    float speed = velocity.length_2d();
    
    // Если скорость больше максимальной - тормозим
    if (speed > max_speed + 1.0f) {
        float yaw = deg2rad(base->viewangles().y());
        float sin_yaw = std::sin(yaw);
        float cos_yaw = std::cos(yaw);
        
        // Направление против скорости
        float stop_forward = -(velocity.x * cos_yaw + velocity.y * sin_yaw) / speed;
        float stop_left = (velocity.x * sin_yaw - velocity.y * cos_yaw) / speed;
        
        base->set_forwardmove(std::clamp(stop_forward, -1.0f, 1.0f));
        base->set_leftmove(std::clamp(stop_left, -1.0f, 1.0f));
        return;
    }
    
    // Ограничиваем инпут
    float limit = max_speed / weapon_speed;
    limit = std::clamp(limit, 0.0f, 1.0f);
    
    float forward = base->forwardmove();
    float left = base->leftmove();
    float magnitude = std::hypot(forward, left);
    
    if (magnitude > limit && magnitude > 0.0f) {
        float scale = limit / magnitude;
        base->set_forwardmove(forward * scale);
        base->set_leftmove(left * scale);
    }
}

bool c_movement::read_movement_analog(c_cs_player_pawn* pawn, float& forward, float& left) const {
    if (!pawn)
        return false;
    
    auto* movement = pawn->m_movement_services();
    if (!movement)
        return false;
    
    static const auto forward_offset = schema_get_offset("CPlayer_MovementServices", "m_flCmdForwardMove");
    static const auto left_offset = schema_get_offset("CPlayer_MovementServices", "m_flCmdLeftMove");
    
    if (!forward_offset || !left_offset)
        return false;
    
    const auto* raw = reinterpret_cast<const std::uint8_t*>(movement);
    forward = *reinterpret_cast<const float*>(raw + forward_offset);
    left = *reinterpret_cast<const float*>(raw + left_offset);
    
    return std::isfinite(forward) && std::isfinite(left);
}

bool c_movement::sync_subtick_movement(c_user_cmd* user_cmd, float prior_forward, float prior_left) {
    if (!user_cmd || !user_cmd->pb.has_base())
        return false;
    
    return command_movement::analog(
        *user_cmd->pb.mutable_base(), 
        prior_forward, 
        prior_left,
        [&] { return g_protobuf->add_subtick_move_step(user_cmd); }
    );
}
