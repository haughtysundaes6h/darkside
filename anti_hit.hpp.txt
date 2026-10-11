#pragma once

#include "../../darkside.hpp"

// ============================================================================
// ANTI-AIM SYSTEM v3.1
// CS2-реальность: десинка НЕТ (Source 2 жёстко синхронит вьюанглы),
// фейклага НЕТ (чок команд имеет смысл только под тикшифт/даблтап эксплойты,
// и те тут не реализованы). Что реально работает: pitch tucking, yaw-режимы,
// jitter, freestanding, leg jitter (дёрганье анимаций ножек), slow walk.
// ============================================================================

class c_anti_hit {
public:
    void on_create_move(c_user_cmd* user_cmd);
    void on_pre_move(c_user_cmd* user_cmd);
    
    // Для индикаторов
    float get_real_yaw() const { return m_real_yaw; }
    bool is_side_left() const { return m_side == -1; }
    
private:
    // ========================================================================
    // PITCH
    // ========================================================================
    void handle_pitch(c_user_cmd* user_cmd);
    float get_pitch_angle(int command_number);
    
    // ========================================================================
    // YAW
    // ========================================================================
    void handle_yaw(c_user_cmd* user_cmd);
    float get_base_yaw(c_user_cmd* user_cmd);
    float get_freestanding_yaw(c_user_cmd* user_cmd, float base_yaw);
    float apply_yaw_modifier(float yaw);
    float apply_yaw_jitter(float yaw, int command_number);
    float apply_legacy_jitter(float yaw);
    
    // ========================================================================
    // LEG JITTER (дёрганье анимаций ножек, quantize-aware)
    // ========================================================================
    void leg_jitter(c_user_cmd* user_cmd);
    
    // ========================================================================
    // SIDE / TARGETS
    // ========================================================================
    void update_side();
    float get_at_target_yaw();
    c_cs_player_pawn* find_closest_enemy();
    
    // ========================================================================
    // STATE
    // ========================================================================
    
    float m_real_yaw = 0.0f;
    
    // Side (-1 = left, 1 = right) — для Sideways и Freestanding, флип по кейбинду
    int m_side = 1;
    
    // Jitter
    int m_jitter_tick = 0;
    float m_jitter_angle = 0.0f;
    
    // Leg jitter
    bool m_micro_moved = false;
};

inline const auto g_anti_hit = std::make_unique<c_anti_hit>();
