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
    if (!g_cfg->misc.m_bunny_hop || !user_cmd || !user_cmd->pb.has_base() || !g_ctx->m_local_pawn)
        return false;
    constexpr std::uint64_t jump = static_cast<std::uint64_t>(IN_JUMP);
    const bool pb_jump = user_cmd->pb.base().has_buttons_pb() &&
        (user_cmd->pb.base().buttons_pb().buttonstate1() & jump) != 0;
    const bool requested = (user_cmd->m_button_state.m_button_state & jump) != 0 || pb_jump || jump_held;
    if (!requested)
        return false;
    auto* pawn = g_ctx->m_local_pawn;
    if (!pawn->is_alive() || pawn->m_actual_move_type() != movetype_walk ||
        (pawn->m_flags() & (FL_INWATER | FL_WATERJUMP)))
        return false;

    // On the ground the held jump must stay, so the landing tick is a fresh press.
    if (pawn->m_flags() & FL_ONGROUND)
        return false;

    bool changed = false;
    const auto clear_jump = [&changed, jump](std::uint64_t& state) {
        const auto filtered = state & ~jump;
        changed |= filtered != state;
        state = filtered;
    };
    clear_jump(user_cmd->m_button_state.m_button_state);
    clear_jump(user_cmd->m_button_state.m_button_state2);
    clear_jump(user_cmd->m_button_state.m_button_state3);

    auto* base = user_cmd->pb.mutable_base();
    if (base->has_buttons_pb()) {
        auto* buttons = base->mutable_buttons_pb();
        auto value = buttons->buttonstate1(); clear_jump(value); buttons->set_buttonstate1(value);
        value = buttons->buttonstate2(); clear_jump(value); buttons->set_buttonstate2(value);
        value = buttons->buttonstate3(); clear_jump(value); buttons->set_buttonstate3(value);
    }

    const int count = std::clamp(base->subtick_moves_size(), 0, 64);
    for (int i = 0; i < count; ++i) {
        auto* step = base->mutable_subtick_moves(i);
        if (!step || !(step->button() & jump))
            continue;
        step->set_button(step->button() & ~jump);
        if (!step->button())
            step->set_pressed(false);
        changed = true;
    }
    return changed;
}

// ============================================================================
// CS2 AUTO STRAFE
// ============================================================================

// Один шаг source-подобного airaccelerate для предикта скорости.
// wish_dir_world уже в мировых координатах; возвращает реальный прирост.
static float air_accel_step(vec3_t& velocity, const vec3_t& wish_dir_world,
                            float air_max_wishspeed, float air_accel,
                            float frame_time, float surface_friction)
{
    // Создаем копию для вычислений (так как методы vec3_t не const)
    vec3_t wish_copy = wish_dir_world;
    const float wish_len = wish_copy.length();
    if (wish_len <= 0.001f)
        return 0.f;

    // CS2: в воздухе wishspeed капится sv_air_max_wishspeed (30)
    const float wish_speed = (std::min)(wish_len, air_max_wishspeed);
    vec3_t dir = wish_copy / wish_len;

    const float current_speed = velocity.dot(dir);
    const float adds = wish_speed - current_speed;
    if (adds <= 0.f)
        return 0.f;

    const float accelspeed = (std::min)(air_accel * frame_time * surface_friction * wish_speed, adds);
    velocity = velocity + dir * accelspeed;
    return accelspeed;
}

// Лучший борт стрейфа по предикту прироста (запускает air_accel_step на
// копии веля). side: +1 = left, -1 = right.
static float best_side_gain(vec3_t velocity, float yaw, float side,
                            float air_max_wishspeed, float air_accel,
                            float frame_time, float surface_friction)
{
    const auto wish_in = movement_math::rotate({0.f, side}, 0.f, yaw);
    const vec3_t wish_world = { wish_in.forward, wish_in.left, 0.f };
    return air_accel_step(velocity, wish_world, air_max_wishspeed, air_accel, frame_time, surface_friction);
}

void c_movement::auto_strafe(c_user_cmd* user_cmd, float previous_yaw) {
    (void)previous_yaw;
    if (!g_cfg->misc.m_auto_strafe || !user_cmd || !user_cmd->pb.has_base() || !g_ctx->m_local_pawn ||
        !g_ctx->m_local_pawn->is_alive() ||
        g_ctx->m_local_pawn->m_actual_move_type() != movetype_walk ||
        (g_ctx->m_local_pawn->m_flags() & (FL_ONGROUND | FL_INWATER | FL_WATERJUMP)))
        return;
    auto* base = user_cmd->pb.mutable_base();
    const float yaw = base->viewangles().y();
    if (!std::isfinite(yaw))
        return;
    const auto velocity = g_ctx->m_local_pawn->m_vec_abs_velocity();
    const float speed_2d = std::hypot(velocity.x, velocity.y);
    if (!std::isfinite(speed_2d) || speed_2d < 5.f)
        return;
    static std::uint64_t last_buttons = 0;
    static std::uint64_t last_pressed = 0;
    static int substep = 0;
    const std::uint64_t buttons = user_cmd->m_button_state.m_button_state;
    const auto check = [&](std::uint64_t button) {
        constexpr std::uint64_t l = std::uint64_t(IN_MOVELEFT);
        constexpr std::uint64_t r = std::uint64_t(IN_MOVERIGHT);
        constexpr std::uint64_t f = std::uint64_t(IN_FORWARD);
        constexpr std::uint64_t b = std::uint64_t(IN_BACK);
        if (!(buttons & button)) { last_pressed &= ~button; return; }
        const bool fresh = !(last_buttons & button);
        const bool free = ((button & l) && !(last_pressed & r)) ||
                          ((button & r) && !(last_pressed & l)) ||
                          ((button & f) && !(last_pressed & b)) ||
                          ((button & b) && !(last_pressed & f));
        if (!fresh && !free)
            return;
        if (button & l) last_pressed &= ~r;
        else if (button & r) last_pressed &= ~l;
        else if (button & f) last_pressed &= ~b;
        else if (button & b) last_pressed &= ~f;
        last_pressed |= button;
    };
    check(std::uint64_t(IN_MOVELEFT));
    check(std::uint64_t(IN_MOVERIGHT));
    check(std::uint64_t(IN_FORWARD));
    check(std::uint64_t(IN_BACK));
    last_buttons = buttons;
    float fwd = 0.f, left = 0.f;
    if (last_pressed & std::uint64_t(IN_FORWARD)) fwd = 1.f;
    else if (last_pressed & std::uint64_t(IN_BACK)) fwd = -1.f;
    if (last_pressed & std::uint64_t(IN_MOVELEFT)) left = -1.f;
    else if (last_pressed & std::uint64_t(IN_MOVERIGHT)) left = 1.f;
    if (fwd == 0.f && left == 0.f)
        return;
    const float base_offset = std::atan2(-left, fwd) * 180.f / std::numbers::pi_v<float>;
    const float target = movement_math::norm(yaw + base_offset);
    float start = 0.f;
    const int count = base->subtick_moves_size();
    for (int i = 0; i < count && i < 64; ++i) {
        const auto& s = base->subtick_moves(i);
        if (std::isfinite(s.when()) && s.when() > start)
            start = s.when();
    }
    if (start >= 0.99f)
        return;
    constexpr int n = 16;
    constexpr float tick = 1.f / 64.f;
    const float frame = tick / float(n);
    const float step_when = (1.f - start) / float(n + 1);
    float acc = yaw;
    float vx = velocity.x, vy = velocity.y;
    bool added = false;
    for (int i = 1; i <= n; ++i) {
        const bool side = ((substep + i) % 2) == 0;
        const float wish = movement_math::air_strafer(vx, vy, target, frame, side, 260.f);
        const float view = movement_math::norm(wish - base_offset);
        const float delta = movement_math::norm(view - acc);
        if (std::fabs(delta) <= 0.01f)
            break;
        auto* s = g_protobuf->add_subtick_move_step(user_cmd);
        if (!s)
            break;
        s->set_when(std::clamp(start + float(i) * step_when, 0.f, 1.f));
        s->set_yaw_delta(delta);
        acc = view;
        movement_math::air_accel(vx, vy, wish, frame, 260.f);
        added = true;
    }
    ++substep;
    // This command pipeline only resigns the CRC when buttons, angles, or
    // analog moves change. Yaw-only subticks still have to be signed.
    if (added) {
        g_protobuf->sort_subtick_move_steps(user_cmd);
        g_protobuf->update_move_crc(user_cmd);
    }
}

// ============================================================================
// CS2 SUBTICK STRAFE — per-subtick air acceleration
// ============================================================================
// По мотивам UC "[Release] strafe per subtick" (#673323): тик дробится до
// 12 сабтик-шагов, на каждом шаге — свой оптимальный борт по предикту
// airaccelerate (sv_airacceleration * frame_time * surface_friction),
// дельты кладутся в аналоговые поля сабтик-степов (when + analog deltas).
// Бонус: аналоговые дельты НЕ квантуются sv_quantize_movement_input
// (квантайз режет только базовые forward/left) — стрейф сохраняет точную
// траекторию даже на серверах с квантайзом. Финальная база = накопленный
// вектор (сервер применяет дельты кумулятивно).

void c_movement::subtick_strafe(c_user_cmd* user_cmd, c_cs_player_pawn* pawn, float previous_yaw) {
    if (!user_cmd || !user_cmd->pb.has_base() || !pawn)
        return;

    // В воде/на земле смысла нет
    if (pawn->m_flags() & (FL_ONGROUND | FL_INWATER | FL_WATERJUMP))
        return;

    auto* base = user_cmd->pb.mutable_base();

    // Сколько сабтик-шагов ещё влезает (обычно до 12 на тик)
    const int free_slots = std::clamp(12 - base->subtick_moves_size(), 0, 12);
    if (free_slots <= 0)
        return;

    const float frame_time = INTERVAL_PER_TICK / float(free_slots);

    static auto airaccel_var = g_interfaces->m_var->get_by_name("sv_airacceleration");
    static auto wishspeed_var = g_interfaces->m_var->get_by_name("sv_air_max_wishspeed");
    const float air_accel = airaccel_var ? airaccel_var->get_float() : 12.f;
    const float wish_cap = wishspeed_var ? wishspeed_var->get_float() : 30.f;

    float friction = 1.f;
    if (auto* ms = pawn->m_movement_services()) {
        const float f = ms->m_surface_friction();
        friction = (std::isfinite(f) && f > 0.f) ? (std::clamp)(f, 0.25f, 1.f) : 1.f;
    }

    const float yaw = base->viewangles().y();
    vec3_t velocity = pawn->m_vec_abs_velocity();

    const auto buttons = user_cmd->m_button_state.m_button_state;
    const movement_math::input keys{
        float(bool(buttons & IN_FORWARD)) - float(bool(buttons & IN_BACK)),
        float(bool(buttons & IN_MOVELEFT)) - float(bool(buttons & IN_MOVERIGHT))
    };
    const bool keys_pressed = std::hypot(keys.forward, keys.left) > 0.01f;

    // Поворот мыши за тик: стрейфим в сторону поворота (макс. гейн всегда
    // в сторону поворота, если там вообще есть прирост)
    const float turn = std::remainder(yaw - previous_yaw, 360.f);
    const int turn_side = turn > 0.05f ? 1 : (turn < -0.05f ? -1 : 0);

    float acc_fm = base->forwardmove();
    float acc_lm = base->leftmove();

    for (int i = 0; i < free_slots; ++i) {
        float new_fm = acc_fm;
        float new_lm = acc_lm;

        if (keys_pressed) {
            // WASD зажат — держим направление игрока (б_hop с управлением)
            new_fm = keys.forward;
            new_lm = keys.left;
        }
        else {
            // Свободный полёт: оптимальный борт на КАЖДЫЙ сабтик.
            // Поворот мыши приоритетнее (стрейф под камеру), иначе max-gain.
            if (turn_side != 0 && best_side_gain(velocity, yaw, float(turn_side),
                    wish_cap, air_accel, frame_time, friction) > 0.f) {
                new_fm = 0.f;
                new_lm = float(turn_side);
            }
            else {
                const float gain_left = best_side_gain(velocity, yaw, 1.f, wish_cap, air_accel, frame_time, friction);
                const float gain_right = best_side_gain(velocity, yaw, -1.f, wish_cap, air_accel, frame_time, friction);
                if ((std::max)(gain_left, gain_right) <= 0.f)
                    continue; // прироста нет — шаг не нужен

                new_fm = 0.f;
                new_lm = gain_left >= gain_right ? 1.f : -1.f;
            }
        }

        if (new_fm == acc_fm && new_lm == acc_lm)
            continue;

        // Предиктим скорость этим шагом (для следующих сабтиков)
        const auto wish_in = movement_math::rotate({new_fm, new_lm}, 0.f, yaw);
        const vec3_t wish_world = { wish_in.forward, wish_in.left, 0.f };
        air_accel_step(velocity, wish_world, wish_cap, air_accel, frame_time, friction);

        auto* step = g_protobuf->add_subtick_move_step(user_cmd);
        if (!step)
            break;

        step->set_when(float(i) / float(free_slots));
        step->set_analog_forward_delta(new_fm - acc_fm);
        step->set_analog_left_delta(new_lm - acc_lm);
        acc_fm = new_fm;
        acc_lm = new_lm;
    }

    // База = накопленный вектор (дельты кумулятивны поверх базы)
    base->set_forwardmove(acc_fm);
    base->set_leftmove(acc_lm);
}

// ============================================================================
// MOVEMENT FIX - ДЛЯ SILENT AIM И ANTI-AIM (v2, quantize-aware)
// ============================================================================

// Компенсация sv_quantize_movement_input.
//
// В CS2 (обновление против snaptap, конец 2024) сервер округляет дробные
// forward/left до целых {-1,0,1}. Повернутый мув-фиксом вектор вида
// (0.94, 0.34) сервер округляет до (1,0) — направление уплывает на десятки
// градусов, в джиттере получается зигзаг и потеря скорости ("ходит боком").
//
// Решение ("bypass via math"): при включенном квантайзе шлём сразу
// нормализованные векторы 8-направленной решетки. Все ненулевые целые
// векторы (±1,0)/(0,±1)/(±1,±1) дают ПОЛНУЮ скорость — движок нормализует
// wishdir и клампит к maxspeed, значит решается только задача направления:
//  - желаемое направление ложим между двумя соседями решетки (45° сетка);
//  - чередуем их по тикам (тайм-даизеринг): среднее направление = желаемое,
//    ошибка <= ~11°, скорость всегда максимальная;
//  - если движемся слабее порога (контр-страйф автостопа) — шлем (0,0):
//    сервер сам округлит дробь в ноль, поведение идентично.
// Если квантайз выключен (community-сервера) — честная точная ротация.
static movement_math::input quantize_move_input(movement_math::input move, int command_number)
{
    const float len = std::hypot(move.forward, move.left);

    // Всё, что слабее полноценного нажатия клавиш (автостоп, контр-страйф,
    // микро) — честный ноль: сервер дробь всё равно округлит в 0. Клавиши
    // дают len >= 1 (bounded сохраняет длину), сюда они не попадают.
    if (len < 0.95f)
        return {};

    // Направление движения в пространстве (forward, left)
    const float target = rad2deg(std::atan2(move.left, move.forward));

    const float k_exact = target / 45.0f;
    const float k_floor = std::floor(k_exact);
    const float k_frac = k_exact - k_floor;

    const int k_a = static_cast<int>(k_floor);
    const int k_b = k_frac > 0.0001f ? k_a + 1 : k_a;

    const bool near_is_a = k_frac <= 0.5f;
    const int k_near = near_is_a ? k_a : k_b;
    const int k_far = near_is_a ? k_b : k_a;

    // Тайм-даизеринг: 3 тика ближний сосед, 1 тик дальний.
    // Порядок стабилен по номеру команды — без флика.
    const int k = (command_number % 4) == 0 ? k_far : k_near;

    const float angle = deg2rad(45.0f * static_cast<float>(k));
    return { std::cos(angle), std::sin(angle) };
}

void c_movement::movement_fix(c_user_cmd* user_cmd, vec3_t original_angles) {
    if (!user_cmd || !user_cmd->pb.has_base() || !g_ctx->m_local_pawn)
        return;

    auto* base = user_cmd->pb.mutable_base();

    const float fm = base->forwardmove();
    const float sm = base->leftmove();

    // Есть ли аналоговые дельты в сабтик-степах (subtick strafe)?
    bool has_deltas = false;
    const int step_count = base->subtick_moves_size();
    for (int i = 0; i < step_count; ++i) {
        auto* step = base->mutable_subtick_moves(i);
        if (step && (step->analog_forward_delta() != 0.f || step->analog_left_delta() != 0.f)) {
            has_deltas = true;
            break;
        }
    }

    // Нет инпута ни в базе, ни в дельтах — ротировать нечего
    if (std::fabs(fm) <= 0.0001f && std::fabs(sm) <= 0.0001f && !has_deltas)
        return;

    // Лестница/ноклип — движок и так двигает по своим углам
    if (g_ctx->m_local_pawn->m_actual_move_type() != movetype_walk)
        return;

    const float old_yaw = original_angles.y;
    const float new_yaw = base->viewangles().y();

    if (!std::isfinite(old_yaw) || !std::isfinite(new_yaw))
        return;

    // Точная ротация вектора движения на дельту базовых углов.
    // bounded() клампит по Чебышёву (макс. компонент в [-1,1]) с сохранением
    // направления: длина вектора не падает ниже 1 => скорость не режется.
    movement_math::input move = movement_math::bounded(
        movement_math::rotate({fm, sm}, old_yaw, new_yaw));

    // Компенсация серверного квантайза
    static auto quantize_cvar = g_interfaces->m_var->get_by_name("sv_quantize_movement_input");
    const bool quantized = quantize_cvar && quantize_cvar->get_bool();
    if (quantized)
        move = quantize_move_input(move, user_cmd->m_command_number);

    base->set_forwardmove(move.forward);
    base->set_leftmove(move.left);

    // Ротируем и аналоговые дельты сабтик-степов — иначе сабтик-стрейфер
    // после поворота углов AA/silent-аимом уедет вбок (дельты применяются
    // сервером в системе координат ФИНАЛЬНЫХ углов).
    if (has_deltas) {
        for (int i = 0; i < step_count; ++i) {
            auto* step = base->mutable_subtick_moves(i);
            if (!step)
                continue;
            const float dF = step->analog_forward_delta();
            const float dL = step->analog_left_delta();
            if (dF == 0.f && dL == 0.f)
                continue;
            const auto rot = movement_math::rotate({dF, dL}, old_yaw, new_yaw);
            step->set_analog_forward_delta(rot.forward);
            step->set_analog_left_delta(rot.left);
        }

        // При квантайзе база заменена решёточным вектором — суммарный
        // вектор съехал. Компенсируем в ПОСЛЕДНЕЙ ненулевой дельте, чтобы
        // траектория осталась ровно той же.
        if (quantized && step_count > 0) {
            const auto pre = movement_math::rotate({fm, sm}, old_yaw, new_yaw);
            for (int i = step_count - 1; i >= 0; --i) {
                auto* step = base->mutable_subtick_moves(i);
                if (!step || (step->analog_forward_delta() == 0.f && step->analog_left_delta() == 0.f))
                    continue;
                step->set_analog_forward_delta(step->analog_forward_delta() + pre.forward - move.forward);
                step->set_analog_left_delta(step->analog_left_delta() + pre.left - move.left);
                break;
            }
        }
    }
}

// ============================================================================
// AUTO STOP - ДЛЯ RAGE
// ============================================================================

void c_movement::auto_stop(c_user_cmd* user_cmd, c_cs_player_pawn* local_player, 
                           c_base_player_weapon* weapon, const auto_stop_options_t& options) {
    if (!options.m_enabled || options.m_no_spread)
        return;
    
    if (!user_cmd || !local_player || !weapon || !user_cmd->pb.has_base())
        return;
    
    const bool on_ground = (local_player->m_flags() & FL_ONGROUND) != 0;
    
    // Air: по умолчанию стоп только на земле; с режимом Air — всегда.
    if (!on_ground && !options.m_air)
        return;
    
    // Убираем shift (влияет на инаккураси в движении)
    auto remove_button = [&](int button) {
        user_cmd->m_button_state.m_button_state &= ~button;
        user_cmd->m_button_state.m_button_state2 &= ~button;
        user_cmd->m_button_state.m_button_state3 &= ~button;
    };
    
    remove_button(IN_SPEED);
    
    const float weapon_speed = weapon->get_max_speed();
    if (!std::isfinite(weapon_speed) || weapon_speed <= 0.f)
        return;
    
    // Ни один режим не выбран (или Between вне окна кулдауна) — не мешаем движению
    if (!options.m_full && !options.m_early && !options.m_between)
        return;
    
    // Порог торможения:
    //  - Full Stop: тормоз в ноль (полный контр-страйф);
    //  - Early: 34% max speed — порог точности большинства стволов;
    //  - только Between: полный стоп, но rage_bot отдаёт флаг лишь в окне
    //    между выстрелами, вне окна мы сюда уже не дошли.
    const float wish_speed = options.m_early ? weapon_speed * 0.34f : 0.f;
    
    limit_speed(user_cmd, local_player, weapon, wish_speed);
    
    // Duck: приседаем на торможении (инаккараси ниже, хитбокс ниже)
    if (options.m_duck) {
        user_cmd->m_button_state.m_button_state |= IN_DUCK;
        user_cmd->m_button_state.m_button_state2 |= IN_DUCK;
        user_cmd->m_button_state.m_button_state3 |= IN_DUCK;
    }
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
    float ideal = std::clamp(static_cast<float>(rad2deg(std::atan2(15.0f, speed))), 0.0f, 45.0f);
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
