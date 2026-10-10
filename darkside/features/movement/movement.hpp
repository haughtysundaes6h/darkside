#pragma once

#include "../protobufs/protobufs.hpp"
#include "movement_math.hpp"

// ============================================================================
// MOVEMENT SYSTEM v2.0
// Fixed bhop, auto strafe, movement fix
// ============================================================================

class c_movement {
private:
    // Strafe history
    movement_math::yaw_history m_yaw_history;
    
    // Previous tick data
    float m_old_yaw = 0.0f;
    bool m_was_on_ground = false;
    bool m_was_jumping = false;
    vec3_t m_last_velocity = {};
    
    // Edge detection
    bool m_edge_detected = false;
    int m_ground_ticks = 0;
    
    // Jump bug state
    bool m_should_duck = false;
    int m_land_tick = 0;
    
    // Strafe counter for direction
    int m_strafe_index = 0;
    
public:
    // ========================================================================
    // CORE FUNCTIONS
    // ========================================================================
    
    void on_create_move(c_user_cmd* user_cmd);
    void movement_fix(c_user_cmd* user_cmd, vec3_t original_angles);
    
    // ========================================================================
    // BHOP & AUTO STRAFE (FIXED)
    // ========================================================================
    
    bool bunnyhop(c_user_cmd* user_cmd);
    void auto_strafe(c_user_cmd* user_cmd);
    
    // ========================================================================
    // ADVANCED MOVEMENT
    // ========================================================================
    
    void edge_jump(c_user_cmd* user_cmd);
    void jump_bug(c_user_cmd* user_cmd);
    void edge_bug(c_user_cmd* user_cmd);
    void long_jump(c_user_cmd* user_cmd);
    void infinite_duck(c_user_cmd* user_cmd);
    
    // ========================================================================
    // RAGE FUNCTIONS
    // ========================================================================
    
    void auto_stop(c_user_cmd* user_cmd, c_cs_player_pawn* local_player, 
                   c_base_player_weapon* weapon, bool no_spread, bool enabled);
    void slow_walk(c_user_cmd* user_cmd, float max_speed);
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    bool read_movement_analog(c_cs_player_pawn* pawn, float& forward, float& left) const;
    bool sync_subtick_movement(c_user_cmd* user_cmd, float prior_forward, float prior_left);
    
private:
    // Internal helpers
    bool is_on_ground(c_cs_player_pawn* pawn);
    bool will_land_next_tick(c_cs_player_pawn* pawn);
    bool is_at_edge(c_cs_player_pawn* pawn);
    float get_ideal_strafe_angle(float speed);
    void limit_movement(c_user_cmd* user_cmd, float max_speed);
};

inline const auto g_movement = std::make_unique<c_movement>();
