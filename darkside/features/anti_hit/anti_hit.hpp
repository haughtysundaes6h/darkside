#pragma once

#include "../../darkside.hpp"

// ============================================================================
// ANTI-AIM SYSTEM v2.0
// Полностью переписан с desync, fakelag, jitter
// ============================================================================

class c_anti_hit {
public:
    void on_create_move(c_user_cmd* user_cmd);
    void on_pre_move(c_user_cmd* user_cmd);
    
    // Getters для индикаторов
    float get_real_yaw() const { return m_real_yaw; }
    float get_fake_yaw() const { return m_fake_yaw; }
    float get_desync_amount() const { return m_desync_amount; }
    bool is_side_left() const { return m_side == -1; }
    bool is_breaking_lby() const { return m_breaking_lby; }
    
private:
    // ========================================================================
    // PITCH
    // ========================================================================
    void handle_pitch(c_user_cmd* user_cmd);
    float get_pitch_angle();
    
    // ========================================================================
    // YAW
    // ========================================================================
    void handle_yaw(c_user_cmd* user_cmd);
    float get_base_yaw(c_user_cmd* user_cmd);
    float apply_yaw_modifier(float yaw);
    float apply_jitter(float yaw, int tick);
    
    // ========================================================================
    // DESYNC
    // ========================================================================
    void handle_desync(c_user_cmd* user_cmd);
    void micro_move(c_user_cmd* user_cmd);
    void update_side();
    
    // ========================================================================
    // FAKELAG
    // ========================================================================
    void handle_fakelag(c_user_cmd* user_cmd);
    bool should_choke();
    int get_choke_amount();
    
    // ========================================================================
    // AT TARGET
    // ========================================================================
    float get_at_target_yaw();
    c_cs_player_pawn* find_closest_enemy();
    
    // ========================================================================
    // STATE
    // ========================================================================
    
    // Yaw state
    float m_real_yaw = 0.0f;
    float m_fake_yaw = 0.0f;
    float m_desync_amount = 0.0f;
    
    // Side (-1 = left, 1 = right)
    int m_side = 1;
    int m_side_switch_tick = 0;
    
    // Jitter
    int m_jitter_tick = 0;
    float m_jitter_angle = 0.0f;
    
    // LBY
    float m_last_lby = 0.0f;
    bool m_breaking_lby = false;
    int m_lby_update_tick = 0;
    
    // Fakelag
    int m_choked_ticks = 0;
    bool m_should_choke = false;
    int m_fakelag_cycle = 0;
    
    // Micro movement
    bool m_micro_moved = false;
};

inline const auto g_anti_hit = std::make_unique<c_anti_hit>();
