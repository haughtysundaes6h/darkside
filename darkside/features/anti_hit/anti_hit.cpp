#include "anti_hit.hpp"
#include "../eng_pred/eng_pred.hpp"
#include "../../entity_system/entity.hpp"
#include "../movement/movement.hpp"

// ============================================================================
// CS2 ANTI-AIM SYSTEM v3.1
// Без десинка и фейклага — их в CS2 не существует. Только то, что реально
// работает против современных резолверов/анимфиксов.
// ============================================================================

namespace {

    // Детерминированный псевдорандом 0..1 по номеру команды — джиттер
    // одинаковый на клиенте/сервере и не "дрожит" между вызовами фикса.
    float tick_rand(int command_number, int salt = 0)
    {
        std::uint32_t x = static_cast<std::uint32_t>(command_number) * 747796405u
            + 2891336453u + static_cast<std::uint32_t>(salt) * 19349663u;
        x = ((x >> ((x >> 28) + 4)) ^ x) * 277803737u;
        x = (x >> 22) ^ x;
        return static_cast<float>(x & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
    }

    // sv_quantize_movement_input: сервер округляет дробные forward/left
    // до целых. Микро 0.015 при включённом квантайзе мертво.
    bool movement_quantized()
    {
        static auto quantize_cvar = g_interfaces->m_var->get_by_name("sv_quantize_movement_input");
        return quantize_cvar && quantize_cvar->get_bool();
    }

    void set_button(c_user_cmd* user_cmd, int button, bool state)
    {
        if (state) {
            user_cmd->m_button_state.m_button_state |= button;
            user_cmd->m_button_state.m_button_state2 |= button;
            user_cmd->m_button_state.m_button_state3 |= button;
        }
        else {
            user_cmd->m_button_state.m_button_state &= ~button;
            user_cmd->m_button_state.m_button_state2 &= ~button;
            user_cmd->m_button_state.m_button_state3 &= ~button;
        }
    }

} // namespace

void c_anti_hit::on_create_move(c_user_cmd* user_cmd) {
    if (!g_cfg->anti_hit.m_enabled || !user_cmd || !user_cmd->pb.has_base())
        return;

    if (!g_interfaces->m_engine->is_in_game())
        return;

    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn || !pawn->is_alive() || pawn->m_actual_move_type() != movetype_walk)
        return;

    // Не применяем AA если игрок вручную атакует/юзает
    auto buttons = user_cmd->m_button_state.m_button_state;
    if (buttons & (IN_ATTACK | IN_ATTACK2 | IN_USE))
        return;

    // Не применяем если кидаем гранату
    if (pawn->is_throwing())
        return;

    // Сторона для Sideways/Freestanding (флип по кейбинду)
    update_side();

    // Slow Walk из AA-меню: честный IN_WALK (движковый walk-режим).
    // Не режется квантайзом — это кнопка, а не дробные мувы.
    // + ограничение скорости по слайдеру
    if (g_cfg->anti_hit.m_aa_slow_walk && (pawn->m_flags() & FL_ONGROUND)) {
        set_button(user_cmd, IN_WALK, true);
        
        // Дополнительное ограничение скорости если слайдер < 100%
        const int speed_percent = std::clamp(g_cfg->anti_hit.m_aa_slow_walk_speed, 1, 100);
        if (speed_percent < 100) {
            auto* weapon = pawn->get_active_weapon();
            if (weapon) {
                float max_speed = weapon->get_max_speed() * (speed_percent / 100.0f);
                g_movement->slow_walk(user_cmd, speed_percent / 100.0f);
            }
        }
    }

    // Pitch
    handle_pitch(user_cmd);

    // Yaw
    handle_yaw(user_cmd);

    // Leg jitter — дёрганье анимаций ножек на месте (ломает чтение позы
    // и прицеливание по ногам). Работает и при квантайзе.
    if (g_cfg->anti_hit.m_leg_jitter) {
        leg_jitter(user_cmd);
    }
}

void c_anti_hit::on_pre_move(c_user_cmd* user_cmd) {
    // Зарезервировано: pre-move стадия
}

// ============================================================================
// PITCH
// ============================================================================

void c_anti_hit::handle_pitch(c_user_cmd* user_cmd) {
    float pitch = get_pitch_angle(user_cmd->m_command_number);

    auto* base = user_cmd->pb.mutable_base();

    base->mutable_viewangles()->set_x(pitch);

    // CS2: сервер строит подтик-движение по углам из истории — синчим всё
    for (int i = 0; i < user_cmd->pb.input_history_size(); i++) {
        auto* history = user_cmd->pb.mutable_input_history(i);
        if (history && history->has_view_angles()) {
            history->mutable_view_angles()->set_x(pitch);
        }
    }
}

float c_anti_hit::get_pitch_angle(int command_number) {
    switch (g_cfg->anti_hit.m_pitch_mode) {
        case 0: // Off
            return g_interfaces->m_csgo_input->get_view_angles().x;

        case 1: // Down (tuck) — база CS2 AA
            return 89.0f;

        case 2: // Up
            return -89.0f;

        case 3: // Zero
            return 0.0f;

        case 4: // Custom
            return static_cast<float>(std::clamp(g_cfg->anti_hit.m_pitch_amount, -89, 89));

        case 5: // Jitter down/up — каждый тик вверх ногами
        {
            const bool down = (command_number % 2) == 0;
            return down ? 89.0f : -89.0f;
        }

        case 6: // Random — детерминированный рандом по тику
        {
            return tick_rand(command_number, 7) * 178.0f - 89.0f;
        }

        default:
            return 0.0f;
    }
}

// ============================================================================
// YAW
// ============================================================================

void c_anti_hit::handle_yaw(c_user_cmd* user_cmd) {
    float yaw = get_base_yaw(user_cmd);

    // Фристендинг имеет приоритет: разворачивает базу от ближайшего врага
    if (g_cfg->anti_hit.m_freestanding)
        yaw = get_freestanding_yaw(user_cmd, yaw);

    yaw = apply_yaw_modifier(yaw);

    // Джиттер из комбобокса Yaw Jitter (Disabled/Low/Medium/High)
    yaw = apply_yaw_jitter(yaw, user_cmd->m_command_number);

    // Легаси-джиттер (старые поля конфига)
    if (g_cfg->anti_hit.m_jitter_enabled) {
        yaw = apply_legacy_jitter(yaw);
    }

    yaw = std::remainder(yaw, 360.0f);
    m_real_yaw = yaw;

    auto* base = user_cmd->pb.mutable_base();

    base->mutable_viewangles()->set_y(yaw);

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

        case 3: // Spin — детерминированный, ~768°/с на 64 тик
        {
            yaw = static_cast<float>((user_cmd->m_command_number * 12) % 360);
            break;
        }

        case 4: // At Target force — жёстко спиной к ближайшему врагу
        {
            const float at_yaw = get_at_target_yaw();
            if (std::isfinite(at_yaw))
                yaw = at_yaw + 180.0f;
            break;
        }

        case 5: // Custom - только yaw add
            break;
    }

    return yaw;
}

// Фристендинг: стоим к ближайшему врагу максимально неудобным боком.
// Auto — спиной к врагу, Left/Right — фикс бок.
float c_anti_hit::get_freestanding_yaw(c_user_cmd* user_cmd, float base_yaw) {
    auto* target = find_closest_enemy();
    if (!target || !target->m_scene_node())
        return base_yaw;

    auto* local = g_ctx->m_local_pawn;
    if (!local || !local->m_scene_node())
        return base_yaw;

    const vec3_t local_pos = local->m_scene_node()->m_abs_origin();
    const vec3_t target_pos = target->m_scene_node()->m_abs_origin();
    const vec3_t delta = target_pos - local_pos;

    if (std::fabs(delta.x) < 0.001f && std::fabs(delta.y) < 0.001f)
        return base_yaw;

    const float enemy_yaw = rad2deg(std::atan2(delta.y, delta.x));

    switch (g_cfg->anti_hit.m_freestanding_mode) {
        case 0: // Auto — спиной к врагу
            return enemy_yaw + 180.0f;
        case 1: // Left бок к врагу
            return enemy_yaw + 90.0f;
        case 2: // Right бок к врагу
            return enemy_yaw - 90.0f;
        default:
            return base_yaw;
    }
}

float c_anti_hit::apply_yaw_modifier(float yaw) {
    yaw += static_cast<float>(g_cfg->anti_hit.m_yaw_amount);
    return yaw;
}

// Джиттер из меню: Disabled / Offset / Center / 3-Way / 5-Way
// Детерминированный по command_number для стабильности и совместимости с мув-фиксом
float c_anti_hit::apply_yaw_jitter(float yaw, int command_number) {
    const int mode = g_cfg->anti_hit.m_yaw_jitter;
    if (mode <= 0)
        return yaw;

    // Используем jitter_amount из конфига, если есть, иначе дефолтный
    const float amount = (g_cfg->anti_hit.m_jitter_amount > 0) 
        ? static_cast<float>(g_cfg->anti_hit.m_jitter_amount) 
        : 45.0f;

    switch (mode) {
        case 1: // Offset — чередование влево/вправо
        {
            const bool flip = (command_number % 2) != 0;
            yaw += flip ? amount : -amount;
            break;
        }

        case 2: // Center — лево / центр / право
        {
            const int phase = command_number % 3;
            if (phase == 0)
                yaw -= amount;
            else if (phase == 1)
                yaw += 0.0f; // центр
            else
                yaw += amount;
            break;
        }

        case 3: // 3-Way — три фиксированные позиции (-60°, 0°, +60°)
        {
            const int phase = command_number % 3;
            constexpr float offsets[3] = { -60.0f, 0.0f, 60.0f };
            yaw += offsets[phase];
            break;
        }

        case 4: // 5-Way — пять фиксированных позиций (-90°, -45°, 0°, +45°, +90°)
        {
            const int phase = command_number % 5;
            constexpr float offsets[5] = { -90.0f, -45.0f, 0.0f, 45.0f, 90.0f };
            yaw += offsets[phase];
            break;
        }
    }

    return yaw;
}

float c_anti_hit::apply_legacy_jitter(float yaw) {
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
            break;

        case 2: // Random - детерминированный по тику
            m_jitter_tick++;
            yaw += range * (tick_rand(m_jitter_tick, 11) * 2.0f - 1.0f);
            break;
    }

    return yaw;
}

// ============================================================================
// LEG JITTER
// ============================================================================

// Стоим на месте — дёргаем ножки анимацией: альтернирующий страйф.
//  - без квантайза: микро ±0.015 (анимация идёт, смещения почти нет);
//  - с квантайзом (sv_quantize_movement_input): микро округлится сервером
//    в 0, поэтому альтернируем ПОЛНЫЙ страйф ±1 — за 2 тика чистое
//    смещение ~0, а ножки дёргаются по полной.
// Пока игрок реально движется — не мешаем.
void c_anti_hit::leg_jitter(c_user_cmd* user_cmd) {
    auto* base = user_cmd->pb.mutable_base();

    m_micro_moved = !m_micro_moved;

    const float forward = base->forwardmove();
    const float left = base->leftmove();

    if (std::fabs(forward) > 0.01f || std::fabs(left) > 0.01f)
        return;

    if (movement_quantized()) {
        base->set_leftmove(m_micro_moved ? 1.0f : -1.0f);
    }
    else {
        base->set_leftmove(m_micro_moved ? 0.015f : -0.015f);
    }
}

// ============================================================================
// SIDE / TARGETS
// ============================================================================

void c_anti_hit::update_side() {
    static bool flip_pressed = false;

    bool is_pressed = g_key_handler->is_pressed(g_cfg->anti_hit.m_flip_key, g_cfg->anti_hit.m_flip_key_style);

    if (is_pressed && !flip_pressed) {
        m_side = -m_side;
    }

    flip_pressed = is_pressed;
}

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
