#pragma once

#include "../protobufs/protobufs.hpp"
#include "movement_math.hpp"

// ============================================================================
// CS2 MOVEMENT SYSTEM v2.1
// Subtick-based movement for Counter-Strike 2
// ============================================================================

class c_movement {
private:
    // Yaw history for strafe calculations
    movement_math::yaw_history m_yaw_history;
    
    // Previous frame data
    float m_previous_yaw = 0.0f;
    int m_previous_flags = 0;
    vec3_t m_previous_velocity = {};
    
    // Strafe state
    int m_strafe_tick = 0;
    bool m_strafe_direction = false;
    
    // Edge detection
    int m_ground_ticks = 0;
    bool m_edge_jump_triggered = false;
    
public:
    // ========================================================================
    // MAIN ENTRY POINT
    // ========================================================================
    void on_create_move(c_user_cmd* user_cmd);
    
    // ========================================================================
    // CS2 BHOP - SUBTICK BASED
    // ========================================================================
    bool bunnyhop(c_user_cmd* user_cmd, bool jump_held);
    
    // ========================================================================
    // CS2 AUTO STRAFE
    // ========================================================================
    void auto_strafe(c_user_cmd* user_cmd, float previous_yaw);
    
    // ========================================================================
    // MOVEMENT FIX (для silent aim и AA)
    // ========================================================================
    void movement_fix(c_user_cmd* user_cmd, vec3_t original_angles);
    
    // ========================================================================
    // RAGE FEATURES
    // ========================================================================
    void auto_stop(c_user_cmd* user_cmd, c_cs_player_pawn* local_player, 
                   c_base_player_weapon* weapon, bool no_spread, bool enabled);
    void slow_walk(c_user_cmd* user_cmd, float speed_percent);
    
    // ========================================================================
    // ADVANCED MOVEMENT
    // ========================================================================
    void edge_jump(c_user_cmd* user_cmd);
    void jump_bug(c_user_cmd* user_cmd);
    void infinite_duck(c_user_cmd* user_cmd);
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    bool read_movement_analog(c_cs_player_pawn* pawn, float& forward, float& left) const;
    bool sync_subtick_movement(c_user_cmd* user_cmd, float prior_forward, float prior_left);
    
private:
    // Internal helpers
    void limit_speed(c_user_cmd* user_cmd, c_cs_player_pawn* pawn, 
                     c_base_player_weapon* weapon, float max_speed);
    bool is_on_ground(c_cs_player_pawn* pawn) const;
    float get_ideal_strafe_angle(float speed) const;
};

inline const auto g_movement = std::make_unique<c_movement>();
