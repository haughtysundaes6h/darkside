#pragma once

#include <vector>
#include <memory>
#include <chrono>
#include "../../math/math.hpp"

class c_cs_player_pawn;
class c_base_player_weapon;
class c_cs_weapon_base_v_data;

// Target info for legit aimbot
struct legit_target_t {
    c_cs_player_pawn* m_pawn = nullptr;
    vec3_t m_aim_point = {};
    float m_fov = FLT_MAX;
    float m_distance = FLT_MAX;
    int m_health = 0;
    bool m_visible = false;
};

// Backtrack record for legit
struct backtrack_record_t {
    c_cs_player_pawn* m_pawn = nullptr;
    vec3_t m_head_position = {};
    vec3_t m_origin = {};
    float m_simulation_time = 0.0f;
    int m_tick_count = 0;
    bool m_valid = false;
    
    void store(c_cs_player_pawn* pawn);
    bool is_valid() const;
};

class c_legit_bot {
public:
    // Main functions
    void on_create_move();
    void on_present();
    void reset();
    
    // Aimbot
    void run_aimbot();
    void find_targets();
    legit_target_t* get_best_target();
    vec3_t get_aim_point(c_cs_player_pawn* pawn);
    float get_fov_to_player(c_cs_player_pawn* pawn, const vec3_t& aim_point);
    void smooth_aim(vec3_t& current_angles, const vec3_t& target_angles);
    
    // Triggerbot
    void run_triggerbot();
    bool should_shoot(c_cs_player_pawn* target);
    
    // Backtrack
    void store_records();
    void run_backtrack();
    void draw_backtrack();
    backtrack_record_t* get_best_record(c_cs_player_pawn* pawn);
    
    // Helpers
    bool is_weapon_valid(c_cs_weapon_base_v_data* weapon_data);
    bool is_target_valid(c_cs_player_pawn* pawn);
    bool can_see_player(c_cs_player_pawn* pawn, const vec3_t& point);
    
private:
    std::vector<legit_target_t> m_targets;
    std::unordered_map<int, std::deque<backtrack_record_t>> m_backtrack_records;
    legit_target_t* m_current_target = nullptr;
    
    // Triggerbot timing
    std::chrono::steady_clock::time_point m_triggerbot_time;
    bool m_triggerbot_ready = false;
    
    // Smoothing
    vec3_t m_last_aim_angles = {};
    float m_accumulated_mouse[2] = {0.0f, 0.0f};
};

inline const auto g_legit_bot = std::make_unique<c_legit_bot>();
