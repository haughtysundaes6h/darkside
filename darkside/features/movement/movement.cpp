#include "movement.hpp"
#include "../eng_pred/eng_pred.hpp"
#include "command_movement.hpp"

// ============================================================================
// CS2 MOVEMENT SYSTEM v2.1 - SUBTICK BASED
// ============================================================================

void c_movement::on_create_move(c_user_cmd* user_cmd) {
    if (!user_cmd || !user_cmd->pb.has_base())
        return;
    
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || !pawn->is_alive())
        return;
    
    // Не работаем если не ходим (лестница, вода и т.д.)
    if (pawn->m_actual_move_type() != movetype_walk)
        return;
    
    // Сохраняем предыдущий yaw для стрейфа
    m_previous_yaw = m_yaw_history.sample(
        pawn, 
        user_cmd->m_command_number, 
        user_cmd->pb.base().viewangles().y()
    );
    
    // Bunnyhop с subtick
    if (g_cfg->misc.m_bunny_hop) {
        bool jump_held = (user_cmd->m_button_state.m_button_state & IN_JUMP) != 0;
        bunnyhop(user_cmd, jump_held);
    }
    
    // Auto strafe
    if (g_cfg->misc.m_auto_strafe) {
        auto_strafe(user_cmd, m_previous_yaw);
    }
    
    // Edge Jump
    if (g_cfg->misc.m_edge_jump) {
        if (g_key_handler->is_pressed(g_cfg->misc.m_edge_jump_key, g_cfg->misc.m_edge_jump_key_style)) {
            edge_jump(user_cmd);
        }
    }
    
    // Jump Bug
    if (g_cfg->misc.m_jump_bug) {
        if (g_key_handler->is_pressed(g_cfg->misc.m_jump_bug_key, g_cfg->misc.m_jump_bug_key_style)) {
            jump_bug(user_cmd);
        }
    }
    
    // Infinite Duck
    if (g_cfg->misc.m_infinite_duck) {
        infinite_duck(user_cmd);
    }
    
    // Slow Walk
    if (g_cfg->misc.m_slow_walk) {
        if (g_key_handler->is_pressed(g_cfg->misc.m_slow_walk_key, g_cfg->misc.m_slow_walk_key_style)) {
            slow_walk(user_cmd, static_cast<float>(g_cfg->misc.m_slow_walk_speed) / 100.0f);
        }
    }
    
    // Сохраняем состояние
    m_previous_flags = pawn->m_flags();
    m_previous_velocity = pawn->m_vec_abs_velocity();
}

// ============================================================================
// CS2 BHOP - ИСПОЛЬЗУЕТ SUBTICK LANDING PREDICTION
// ============================================================================

bool c_movement::bunnyhop(c_user_cmd* user_cmd, bool jump_held) {
    if (!user_cmd || !user_cmd->pb.has_base() || !g_ctx->m_local_pawn)
        return false;
    
    // Если прыжок не нажат - ничего не делаем
    if (!jump_held)
        return false;
    
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn->is_alive() || pawn->m_actual_move_type() != movetype_walk)
        return false;
    
    // В воде не работает
    if (pawn->m_flags() & (FL_INWATER | FL_WATERJUMP))
        return false;
    
    // Получаем landing prediction от CS2 engine
    auto landing = g_prediction->landing(pawn, user_cmd);
    
    // Проверяем на земле ли мы
    const bool grounded = landing.valid ? landing.grounded : (pawn->m_flags() & FL_ONGROUND) != 0;
    
    // Функция для создания subtick шага
    auto allocate = [&]() { 
        return g_protobuf->add_subtick_move_step(user_cmd); 
    };
    
    // Если скоро приземлимся - делаем subtick прыжок на момент приземления
    if (!grounded && landing.will_land && landing.fraction > 0.0f && landing.fraction < 1.0f) {
        // Landing jump - прыжок на фракции приземления
        return command_movement::landing_jump(*user_cmd, landing.fraction, allocate);
    }
    
    // Стандартный прыжок если на земле
    return command_movement::set_jump(*user_cmd, grounded, 0.0f, allocate);
}

// ============================================================================
// CS2 AUTO STRAFE
// ============================================================================

void c_movement::auto_strafe(c_user_cmd* user_cmd, float previous_yaw) {
    if (!user_cmd || !user_cmd->pb.has_base() || !g_ctx->m_local_pawn)
        return;
    
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn->is_alive())
        return;
    
    // Только в воздухе
    if (pawn->m_flags() & FL_ONGROUND)
        return;
    
    // Не в воде
    if (pawn->m_flags() & (FL_INWATER | FL_WATERJUMP))
        return;
    
    auto* base = user_cmd->pb.mutable_base();
    
    // Получаем текущую скорость и кнопки
    const auto buttons = user_cmd->m_button_state.m_button_state;
    const vec3_t velocity = pawn->m_vec_abs_velocity();
    const float current_yaw = base->viewangles().y();
    
    // Определяем направление от нажатых кнопок
    movement_math::input keys {
        float(bool(buttons & IN_FORWARD)) - float(bool(buttons & IN_BACK)),
        float(bool(buttons & IN_MOVELEFT)) - float(bool(buttons & IN_MOVERIGHT))
    };
    
    // Вычисляем strafe через math
    float smooth = g_cfg->misc.m_strafe_smooth / 100.0f;
    
    // В зависимости от типа стрейфа
    switch (g_cfg->misc.m_strafe_type) {
        case 0: // Legit - следуем за мышкой
        {
            auto move = movement_math::strafe(
                current_yaw, 
                previous_yaw, 
                keys,
                velocity.x, 
                velocity.y, 
                user_cmd->m_command_number, 
                smooth
            );
            base->set_forwardmove(move.forward);
            base->set_leftmove(move.left);
            break;
        }
        
        case 1: // Directional
        case 2: // Rage
        {
            float speed = velocity.length_2d();
            
            if (speed < 15.0f) {
                // Если медленно - просто идём вперёд
                base->set_forwardmove(1.0f);
                base->set_leftmove(0.0f);
                return;
            }
            
            // Вычисляем идеальный угол
            float ideal_angle = get_ideal_strafe_angle(speed);
            
            // Угол скорости
            float vel_angle = rad2deg(std::atan2(velocity.y, velocity.x));
            
            // Дельта с целевым направлением
            float target_yaw = current_yaw;
            if (std::hypot(keys.forward, keys.left) > 0.01f) {
                target_yaw += rad2deg(std::atan2(keys.left, keys.forward));
            }
            
            float delta = std::remainder(target_yaw - vel_angle, 360.0f);
            
            // Rage strafe - чередуем стороны
            if (g_cfg->misc.m_strafe_type == 2) {
                m_strafe_tick++;
                m_strafe_direction = (m_strafe_tick % 2) == 0;
                
                if (m_strafe_direction) {
                    base->set_forwardmove(0.0f);
                    base->set_leftmove(1.0f);
                } else {
                    base->set_forwardmove(0.0f);
                    base->set_leftmove(-1.0f);
                }
            } else {
                // Directional - корректируем к цели
                if (delta > ideal_angle * smooth) {
                    base->set_forwardmove(0.0f);
                    base->set_leftmove(1.0f);
                } else if (delta < -ideal_angle * smooth) {
                    base->set_forwardmove(0.0f);
                    base->set_leftmove(-1.0f);
                } else {
                    // Чередуем для набора скорости
                    m_strafe_tick++;
                    if (m_strafe_tick % 2 == 0) {
                        base->set_leftmove(1.0f);
                    } else {
                        base->set_leftmove(-1.0f);
                    }
                    base->set_forwardmove(0.0f);
                }
            }
            break;
        }
    }
}

// ============================================================================
// MOVEMENT FIX - ДЛЯ SILENT AIM И ANTI-AIM
// ============================================================================

void c_movement::movement_fix(c_user_cmd* user_cmd, vec3_t original_angles) {
    if (!user_cmd || !user_cmd->pb.has_base() || !g_ctx->m_local_pawn)
        return;
    
    if (g_ctx->m_local_pawn->m_actual_move_type() != movetype_walk)
        return;
    
    auto* base = user_cmd->pb.mutable_base();
    
    // Используем встроенную ротацию из movement_math
    auto move = movement_math::bounded(movement_math::rotate(
        {base->forwardmove(), base->leftmove()}, 
        original_angles.y, 
        base->viewangles().y()
    ));
    
    base->set_forwardmove(move.forward);
    base->set_leftmove(move.left);
}

// ============================================================================
// AUTO STOP - ДЛЯ RAGE
// ============================================================================

void c_movement::auto_stop(c_user_cmd* user_cmd, c_cs_player_pawn* local_player, 
                           c_base_player_weapon* weapon, bool no_spread, bool enabled) {
    if (!enabled || no_spread)
        return;
    
    if (!user_cmd || !local_player || !weapon || !user_cmd->pb.has_base())
        return;
    
    // Только на земле
    if (!(local_player->m_flags() & FL_ONGROUND))
        return;
    
    // Убираем shift
    auto remove_button = [&](int button) {
        user_cmd->m_button_state.m_button_state &= ~button;
        user_cmd->m_button_state.m_button_state2 &= ~button;
        user_cmd->m_button_state.m_button_state3 &= ~button;
    };
    
    remove_button(IN_SPEED);
    
    // Максимальная скорость для попадания (33.34% от max speed)
    float wish_speed = weapon->get_max_speed() * 0.34f;
    
    limit_speed(user_cmd, local_player, weapon, wish_speed);
}

// ============================================================================
// SLOW WALK
// ============================================================================

void c_movement::slow_walk(c_user_cmd* user_cmd, float speed_percent) {
    if (!user_cmd || !user_cmd->pb.has_base() || !g_ctx->m_local_pawn)
        return;
    
    auto* pawn = g_ctx->m_local_pawn;
    auto* weapon = pawn->get_active_weapon();
    if (!weapon)
        return;
    
    float max_speed = weapon->get_max_speed() * speed_percent;
    limit_speed(user_cmd, pawn, weapon, max_speed);
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
        m_edge_jump_triggered = false;
    } else {
        // Только что упали с края и не прыгали
        if (m_ground_ticks > 0 && !m_edge_jump_triggered) {
            // Проверяем что мы не прыгали (скорость вниз)
            if (pawn->m_vec_abs_velocity().z < 0) {
                // Слишком поздно, но пробуем
            }
        }
        m_ground_ticks = 0;
    }
    
    // Проверяем landing prediction - скоро ли упадём с края
    auto landing = g_prediction->landing(pawn, user_cmd);
    
    if (on_ground && !landing.grounded && !m_edge_jump_triggered) {
        // Мы на земле, но prediction говорит что скоро не будем - это край!
        auto allocate = [&]() { return g_protobuf->add_subtick_move_step(user_cmd); };
        command_movement::set_jump(*user_cmd, true, 0.0f, allocate);
        m_edge_jump_triggered = true;
    }
}

// ============================================================================
// JUMP BUG - CROUCH + JUMP НА ПРИЗЕМЛЕНИИ
// ============================================================================

void c_movement::jump_bug(c_user_cmd* user_cmd) {
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || !pawn->is_alive())
        return;
    
    bool on_ground = is_on_ground(pawn);
    
    // Работает только в воздухе при падении
    if (on_ground)
        return;
    
    float fall_speed = -pawn->m_vec_abs_velocity().z;
    if (fall_speed < 200.0f)
        return;
    
    // Получаем prediction
    auto landing = g_prediction->landing(pawn, user_cmd);
    
    if (landing.will_land && landing.fraction > 0.0f && landing.fraction < 0.95f) {
        auto allocate = [&]() { return g_protobuf->add_subtick_move_step(user_cmd); };
        
        // Duck на фракцию приземления
        command_movement::landing_duck(*user_cmd, landing.fraction, allocate);
        
        // Jump сразу после
        command_movement::landing_jump(*user_cmd, landing.fraction + 0.01f, allocate);
    }
}

// ============================================================================
// INFINITE DUCK
// ============================================================================

void c_movement::infinite_duck(c_user_cmd* user_cmd) {
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || !pawn->is_alive())
        return;
    
    // Чередуем duck каждые 8 тиков чтобы сбрасывать stamina
    if (user_cmd->m_button_state.m_button_state & IN_DUCK) {
        if (user_cmd->m_command_number % 8 == 0) {
            user_cmd->m_button_state.m_button_state &= ~IN_DUCK;
        }
    }
}

// ============================================================================
// HELPERS
// ============================================================================

void c_movement::limit_speed(c_user_cmd* user_cmd, c_cs_player_pawn* pawn, 
                              c_base_player_weapon* weapon, float max_speed) {
    if (!user_cmd || !pawn || !weapon || !user_cmd->pb.has_base())
        return;
    
    auto* base = user_cmd->pb.mutable_base();
    
    float weapon_speed = weapon->get_max_speed();
    if (!std::isfinite(weapon_speed) || weapon_speed <= 0.0f)
        return;
    
    max_speed = std::clamp(max_speed, 0.0f, weapon_speed);
    
    vec3_t velocity = pawn->m_vec_abs_velocity();
    float speed = velocity.length_2d();
    
    if (!std::isfinite(speed))
        return;
    
    // Если превышаем скорость - тормозим
    if (speed > max_speed + 2.0f) {
        float yaw = deg2rad(base->viewangles().y());
        float sine = std::sin(yaw);
        float cosine = std::cos(yaw);
        
        float strength = std::clamp((speed - max_speed) / (weapon_speed * 0.12f), 0.0f, 1.0f);
        
        base->set_forwardmove(std::clamp(static_cast<float>(-(velocity.x * cosine + velocity.y * sine) / speed * strength), -1.0f, 1.0f));
        base->set_leftmove(std::clamp(static_cast<float>((velocity.x * sine - velocity.y * cosine) / speed * strength), -1.0f, 1.0f));
        return;
    }
    
    // Ограничиваем input
    float input_limit = std::clamp(max_speed / weapon_speed, 0.0f, 1.0f);
    
    float forward = base->forwardmove();
    float side = base->leftmove();
    float magnitude = std::hypot(forward, side);
    
    if (magnitude > input_limit && magnitude > 0.0f) {
        float scale = input_limit / magnitude;
        base->set_forwardmove(forward * scale);
        base->set_leftmove(side * scale);
    }
}

bool c_movement::is_on_ground(c_cs_player_pawn* pawn) const {
    if (!pawn)
        return false;
    return (pawn->m_flags() & FL_ONGROUND) != 0;
}

float c_movement::get_ideal_strafe_angle(float speed) const {
    if (speed <= 0.0f)
        return 0.0f;
    
    // Формула идеального угла стрейфа для CS2
    // angle = atan(airaccel / speed)
    // airaccel ≈ 15-30 в CS2
    float ideal = std::clamp(rad2deg(std::atan2(15.0f, speed)), 0.0f, 45.0f);
    return ideal;
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
        [&]() { return g_protobuf->add_subtick_move_step(user_cmd); }
    );
}
