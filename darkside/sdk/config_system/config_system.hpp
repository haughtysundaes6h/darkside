#pragma once
#include "../typedefs/c_color.hpp"

#include <vector>
#include <string>
#include <memory>

class c_dumped_item;

#define g_cfg g_config_system

template <typename T>
struct item_t {
    T* pointer;
    std::string category;
    std::string name;
    T default_value;
};

class c_config_system {
private:
    std::vector<item_t<bool>> booleans;
    std::vector<item_t<int>> ints;
    std::vector<item_t<float>> floats;
    std::vector<item_t<int>> keybinds;
    std::vector<item_t<std::string>> strings;
    std::vector<item_t<c_color>> colors_rgb;
public:
    void push_item(bool* pointer, std::string category, std::string name, bool def_value);
    void push_item(int* pointer, std::string category, std::string name, int def_value);
    void push_item(float* pointer, std::string category, std::string name, float def_value);
    void push_item(c_color* pointer, std::string category, std::string name, c_color def_value);

    void setup_values();
    void read(std::string path);
    void save(std::string path);
    void load(std::string path);
    void remove(std::string path);
    void refresh();
    void reset();
    void menu();
    std::vector<std::string> config_files;

    // ========================================================================
    // RAGE BOT
    // ========================================================================
    struct rage_bot_t {
        bool m_enabled = false;
        bool m_hitboxes[6] = {true, true, true, false, false, false};
        bool m_multi_points[6] = {true, true, true, false, false, false};
        bool m_static_scale = true;
        bool m_silent = true;
        int m_hitbox_scale[6] = {50, 50, 50, 50, 50, 50};
        int m_minimum_damage = 50;
        int m_minimum_damage_override = 10;
        int m_hit_chance = 50;
        bool m_auto_fire = true;
        bool m_auto_stop = false;
        bool m_auto_scope = false;
        bool m_delay_shot = false;
        bool m_prefer_body = false;
        bool m_prefer_safe = false;

        int m_override_damage_key_bind = 0;
        int m_override_damage_key_bind_style = 0;

        struct weapon_settings_t {
            bool m_enabled = true;
            int m_hit_chance = 50;
            int m_min_damage = 50;
            int m_min_damage_override = 10;
            bool m_auto_stop = false;
            bool m_auto_scope = false;
        } weapon_group[7];
    } rage_bot;

    // ========================================================================
    // LEGIT BOT
    // ========================================================================
    struct legit_bot_t {
        bool m_enabled = false;
        int m_target_selection = 0; // 0 = closest to crosshair, 1 = lowest health, 2 = closest distance
        float m_fov = 5.0f;
        float m_smooth = 15.0f;
        bool m_hitboxes[4] = {true, true, true, true}; // head, neck, chest, stomach
        bool m_aim_backtrack = false;
        bool m_silent = false;
        
        // Triggerbot
        bool m_triggerbot_enabled = false;
        int m_triggerbot_key = 0;
        int m_triggerbot_key_style = 1; // hold by default
        int m_triggerbot_delay = 0;
        int m_triggerbot_hitchance = 100;
        bool m_triggerbot_head_only = false;
        bool m_triggerbot_magnet = false;
        
        // Backtrack
        bool m_backtrack_enabled = false;
        int m_backtrack_ticks = 6;
        bool m_backtrack_visualize = false;
    } legit_bot;

    // ========================================================================
    // ANTI-AIM
    // ========================================================================
    struct anti_hit_t {
        bool m_enabled = false;
        
        // Pitch
        int m_pitch_mode = 0; // 0=off, 1=down, 2=up, 3=zero, 4=custom
        bool m_override_pitch = false;
        int m_pitch_amount = 0;
        
        // Yaw
        int m_yaw_mode = 0;
        bool m_override_yaw = false;
        bool m_at_target = false;
        int m_yaw_amount = 0;
        
        // Jitter
        bool m_jitter_enabled = false;
        int m_jitter_amount = 0;
        
        // Desync
        bool m_desync_enabled = false;
        int m_desync_mode = 0; // 0=static, 1=jitter, 2=switch
        int m_desync_left = 58;
        int m_desync_right = 58;
        int m_flip_key = 0;
        int m_flip_key_style = 2; // toggle
        
        // Fakelag
        bool m_fakelag_enabled = false;
        int m_fakelag_mode = 0;
        int m_fakelag_limit = 7;
        bool m_fakelag_on_peek = false;
    } anti_hit;

    // ========================================================================
    // VISUALS
    // ========================================================================
    struct visuals_t {
        struct player_esp_t {
            bool m_enabled = true;
            bool m_teammates = false;
            bool m_bounding_box = true;
            bool m_health_bar = true;
            bool m_name = true;
            bool m_weapon = true;
            bool m_ammo_bar = true;
            bool m_flags = true;
            bool m_skeleton = false;
            bool m_distance = false;
            
            c_color m_name_color{1.f, 1.f, 1.f, 1.f};
            c_color m_box_color{1.f, 1.f, 1.f, 1.f};
        } m_player_esp;
        
        struct chams_t {
            bool m_enabled = false;
            int m_material = 0; // 0=flat, 1=textured, 2=glow, 3=metallic, 4=crystal
            c_color m_visible_color{0.2f, 0.8f, 0.2f, 1.f};
            c_color m_hidden_color{0.8f, 0.2f, 0.2f, 1.f};
            bool m_xqz = true;
        } m_chams;
        
        struct glow_t {
            bool m_enabled = false;
            int m_style = 0;
            c_color m_color{1.f, 0.5f, 0.f, 1.f};
        } m_glow;
        
        struct effects_t {
            bool m_hitmarker = true;
            bool m_hitmarker_sound = true;
            bool m_kill_effect = false;
            bool m_bullet_tracers = false;
            c_color m_tracer_color{0.42f, 0.65f, 1.0f, 1.f};
            bool m_bullet_impacts = false;
        } m_effects;
        
        struct indicators_t {
            bool m_crosshair = false;
            bool m_spread_circle = false;
            bool m_aa_lines = false;
            bool m_desync = false;
        } m_indicators;
    } visuals;

    // ========================================================================
    // WORLD
    // ========================================================================
    struct world_esp_t {
        bool m_enable_thirdperson = false;
        bool m_enable_override_fov = false;
        int m_distance = 120;
        int m_override_fov = 0;
        int m_thirdperson_key_bind = 0;
        int m_thirdperson_key_bind_style = 2; // toggle
    } world_esp;

    struct world_modulation_t {
        bool m_custom_skybox = false;
        c_color m_sky{0.36f, 0.36f, 0.36f, 1.f};
        c_color m_sky_clouds{0.37f, 0.84f, 0.98f, 1.f};
        c_color m_wall{1.f, 1.f, 1.f, 1.f};
        c_color m_lighting{0.35f, 0.35f, 0.52f, 1.f};
        
        bool m_night_mode = false;
        float m_brightness = 0.5f;
        int m_exposure = 20;
        bool m_render_fog = false;
        bool m_remove_smoke = false;
        bool m_remove_flash = false;
        
        int m_fov = 0;
        int m_viewmodel_fov = 0;
        int m_removals = 0;
    } world;

    // ========================================================================
    // MISC
    // ========================================================================
    struct misc_t {
        // Movement
        bool m_bunny_hop = false;
        bool m_auto_strafe = false;
        int m_strafe_type = 0;
        int m_strafe_smooth = 70;
        
        bool m_edge_jump = false;
        int m_edge_jump_key = 0;
        int m_edge_jump_key_style = 1;
        
        bool m_jump_bug = false;
        int m_jump_bug_key = 0;
        int m_jump_bug_key_style = 1;
        
        bool m_edge_bug = false;
        int m_edge_bug_key = 0;
        int m_edge_bug_key_style = 1;
        
        // Other
        bool m_reveal_ranks = false;
        bool m_clan_tag = false;
        bool m_watermark = true;
        bool m_spectator_list = true;
        bool m_keybinds_list = true;
        bool m_auto_accept = false;
        bool m_infinite_duck = false;
        
        // Buy bot
        bool m_buy_bot_enabled = false;
        int m_buy_bot_primary = 0;
        int m_buy_bot_secondary = 0;
        bool m_buy_bot_armor = false;
        bool m_buy_bot_defuser = false;
    } misc;

    // ========================================================================
    // SKINS
    // ========================================================================
    struct skins_t {
        struct {
            int m_selected = 0;
            int m_selected_ct = 0;
        } m_agents;

        struct skin_data_t {
            int m_paint_kit = 0;
            int m_item_def_index = 0;
            int m_prev_skin = -1;
        } m_skin_settings[100];

        struct {
            int m_selected = 0;
        } m_knives;

        struct {
            int m_selected = 0;
        } m_gloves;

        int m_selected_weapon = 0;
        int m_selected_type = 0;
        c_dumped_item* m_selected_item = nullptr;
    } skins;
};

inline const auto g_config_system = std::make_unique<c_config_system>();
