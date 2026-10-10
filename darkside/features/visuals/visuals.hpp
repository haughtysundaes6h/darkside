#pragma once

#include "../../darkside.hpp"

struct bbox_t {
    bool m_found = false;
    float x;
    float y;
    float width;
    float height;
};

// Bone pair for skeleton drawing
struct bone_connection_t {
    int m_from;
    int m_to;
};

class c_visuals {
    // Player info structure
    struct player_info_t {
        bool m_valid = false;
        int m_handle;
        int m_health;
        int m_armor;
        int m_ammo;
        int m_max_ammo;
        float m_distance;
        bool m_visible;
        bool m_has_defuser;
        bool m_has_bomb;
        bool m_is_scoped;
        bool m_is_flashed;
        bool m_is_defusing;
        bool m_is_reloading;
        int m_team;
        
        vec3_t m_bottom{}, m_top{};
        vec3_t m_head_pos{};
        
        std::string m_name;
        std::string m_weapon_name;
        std::vector<std::string> m_flags;
        
        // Skeleton data
        std::vector<std::pair<vec3_t, vec3_t>> m_bones;
    };
    
    // Thread-safe data storage
    std::mutex m_player_mutex;
    std::chrono::steady_clock::time_point m_snapshot_time{};
    bool m_local_scoped = false;
    std::unordered_map<int, player_info_t> m_player_map;
    
    // Drawing helpers
    bbox_t calculate_bbox(vec3_t bottom, vec3_t top);
    void draw_box(const bbox_t& bbox, const c_color& color);
    void draw_health_bar(const bbox_t& bbox, int health);
    void draw_armor_bar(const bbox_t& bbox, int armor);
    void draw_ammo_bar(const bbox_t& bbox, int ammo, int max_ammo);
    void draw_name(const bbox_t& bbox, const std::string& name, const c_color& color);
    void draw_weapon(const bbox_t& bbox, const std::string& weapon);
    void draw_flags(const bbox_t& bbox, const std::vector<std::string>& flags);
    void draw_skeleton(const player_info_t& info);
    void draw_distance(const bbox_t& bbox, float distance);
    
    // Internal handlers
    void handle_players();
    void handle_world();
    
public:
    // Main functions
    void store_players();
    void clear();
    bool local_scoped();
    void on_present();
    
    // ========================================================================
    // EFFECTS
    // ========================================================================
    
    // Hitmarker
    void draw_hitmarker();
    void on_player_hurt();
    
    // Bullet tracers
    void draw_bullet_tracers();
    void add_bullet_tracer(const vec3_t& start, const vec3_t& end);
    
    // Bullet impacts
    void draw_bullet_impacts();
    void add_bullet_impact(const vec3_t& position);
    
    // ========================================================================
    // INDICATORS
    // ========================================================================
    
    void draw_crosshair();
    void draw_spread_circle();
    void draw_antiaim_indicators();
    void draw_desync_indicator();
    
    // ========================================================================
    // WORLD ESP
    // ========================================================================
    
    void draw_dropped_weapons();
    void draw_grenades();
    void draw_bomb();
    void draw_chicken(); // :)
    
private:
    // Hitmarker state
    float m_hitmarker_alpha = 0.0f;
    float m_hitmarker_time = 0.0f;
    bool m_hitmarker_kill = false;
    
    // Bullet tracer storage
    struct bullet_tracer_t {
        vec3_t m_start;
        vec3_t m_end;
        float m_time;
        c_color m_color;
    };
    std::vector<bullet_tracer_t> m_bullet_tracers;
    std::mutex m_tracer_mutex;
    
    // Bullet impact storage
    struct bullet_impact_t {
        vec3_t m_position;
        float m_time;
    };
    std::vector<bullet_impact_t> m_bullet_impacts;
    std::mutex m_impact_mutex;
};

inline const auto g_visuals = std::make_unique<c_visuals>();
