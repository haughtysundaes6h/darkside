#pragma once

#include "../protobufs/protobufs.hpp"
#include "movement_math.hpp"

class c_movement {
    movement_math::yaw_history m_yaw_history;
    
    // Internal helpers
    void auto_strafe(c_user_cmd* user_cmd, float old_yaw);
    void limit_speed(c_user_cmd* user_cmd, c_cs_player_pawn* local_player, c_base_player_weapon* active_weapon, float max_speed);
    
    // Edge detection
    bool m_was_on_ground = false;
    bool m_edge_detected = false;
    vec3_t m_last_origin = {};
    
    // Jump bug state
    bool m_jumpbug_should_duck = false;
    int m_jumpbug_tick = 0;
    
    // Edge bug state
    bool m_edgebug_active = false;
    int m_edgebug_tick = 0;
    float m_edgebug_start_height = 0.0f;
    
public:
    // ========================================================================
    // BASIC MOVEMENT
    // ========================================================================
    bool bunnyhop(c_user_cmd* user_cmd, bool jump_held);
    void auto_stop(c_user_cmd* user_cmd, c_cs_player_pawn* local_player, c_base_player_weapon* active_weapon, bool no_spread, bool enabled);
    void movement_fix(c_user_cmd* user_cmd, vec3_t angle);
    void on_create_move(c_user_cmd* user_cmd);
    bool read_movement_analog(c_cs_player_pawn* pawn, float& forward, float& left) const;
    bool sync_subtick_movement(c_user_cmd* user_cmd, float prior_forward, float prior_left);
    
    // ========================================================================
    // ADVANCED MOVEMENT
    // ========================================================================
    
    // Edge Jump - jumps automatically when at the edge of a surface
    void edge_jump(c_user_cmd* user_cmd, c_cs_player_pawn* local_player);
    bool is_at_edge(c_cs_player_pawn* local_player);
    
    // Jump Bug - allows landing without taking fall damage (crouch on landing frame)
    void jump_bug(c_user_cmd* user_cmd, c_cs_player_pawn* local_player);
    bool predict_landing(c_cs_player_pawn* local_player, int& ticks_until_landing);
    
    // Edge Bug - sticks to edges for long falls
    void edge_bug(c_user_cmd* user_cmd, c_cs_player_pawn* local_player);
    bool is_on_edge(c_cs_player_pawn* local_player);
    
    // Long Jump - optimal lj technique
    void long_jump(c_user_cmd* user_cmd, c_cs_player_pawn* local_player);
    
    // Infinite Duck
    void infinite_duck(c_user_cmd* user_cmd, c_cs_player_pawn* local_player);
    
    // ========================================================================
    // STRAFE MODES
    // ========================================================================
    
    // Legit strafe - smooth directional strafe
    void legit_strafe(c_user_cmd* user_cmd, c_cs_player_pawn* local_player);
    
    // Directional strafe - strafe in movement direction
    void directional_strafe(c_user_cmd* user_cmd, c_cs_player_pawn* local_player, float old_yaw);
    
    // Rage strafe - maximum speed gain
    void rage_strafe(c_user_cmd* user_cmd, c_cs_player_pawn* local_player, float old_yaw);
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    float get_max_speed(c_cs_player_pawn* local_player);
    bool is_on_ground(c_cs_player_pawn* local_player);
    vec3_t predict_position(c_cs_player_pawn* local_player, int ticks);
    float get_fall_velocity(c_cs_player_pawn* local_player);
};

inline const auto g_movement = std::make_unique<c_movement>();
