#pragma once

#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#include "../sdk/includes/imgui/imgui.h"

// Neverlose-style modern menu system
namespace menu {
    // Color scheme
    struct color_scheme_t {
        ImVec4 accent = ImVec4(0.42f, 0.65f, 1.0f, 1.0f);      // Main accent (blue)
        ImVec4 accent_hover = ImVec4(0.52f, 0.75f, 1.0f, 1.0f);
        ImVec4 background = ImVec4(0.08f, 0.08f, 0.10f, 0.98f);
        ImVec4 background_child = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
        ImVec4 header = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
        ImVec4 text = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
        ImVec4 text_disabled = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        ImVec4 border = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
        ImVec4 slider_bg = ImVec4(0.15f, 0.15f, 0.17f, 1.0f);
        ImVec4 button = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
        ImVec4 button_hover = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
        ImVec4 checkbox_bg = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
        ImVec4 popup_bg = ImVec4(0.10f, 0.10f, 0.12f, 0.98f);
        ImVec4 success = ImVec4(0.35f, 0.85f, 0.45f, 1.0f);
        ImVec4 warning = ImVec4(0.95f, 0.75f, 0.25f, 1.0f);
        ImVec4 danger = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
    };

    enum class tab_id : int {
        rage = 0,
        legit,
        antiaim,
        visuals,
        misc,
        skins,
        configs,
        scripts
    };

    enum class subtab_id : int {
        // Rage subtabs
        rage_main = 0,
        rage_weapons,
        rage_exploits,
        
        // Legit subtabs
        legit_main,
        legit_triggerbot,
        legit_backtrack,
        
        // Antiaim subtabs
        aa_angles,
        aa_fakelag,
        aa_desync,
        
        // Visuals subtabs
        vis_players,
        vis_world,
        vis_effects,
        vis_misc,
        
        // Misc subtabs
        misc_movement,
        misc_other,
        misc_exploits
    };

    struct tab_t {
        const char* name;
        const char* icon;
        std::vector<std::pair<subtab_id, const char*>> subtabs;
    };
}

class c_menu {
public:
    // State
    std::atomic_bool m_opened{false};
    std::atomic_bool m_block_attack_until_release{false};
    
    // Animation
    float m_animation = 0.0f;
    float m_tab_animation[8] = {0.0f};
    float m_subtab_animation[20] = {0.0f};
    
    // Current selection
    menu::tab_id m_current_tab = menu::tab_id::rage;
    menu::subtab_id m_current_subtab = menu::subtab_id::rage_main;
    
    // Colors
    menu::color_scheme_t m_colors;
    
    // Drag state
    bool m_dragging = false;
    ImVec2 m_drag_offset;
    ImVec2 m_window_pos = ImVec2(100, 100);
    ImVec2 m_window_size = ImVec2(720, 540);
    
    // Methods
    void draw();
    void draw_header();
    void draw_sidebar();
    void draw_content();
    
    // Tab renderers
    void draw_rage_tab();
    void draw_legit_tab();
    void draw_antiaim_tab();
    void draw_visuals_tab();
    void draw_misc_tab();
    void draw_skins_tab();
    void draw_configs_tab();
    void draw_scripts_tab();
    
    // Custom widgets
    bool checkbox_ex(const char* label, bool* v);
    bool slider_int_ex(const char* label, int* v, int v_min, int v_max, const char* format = "%d");
    bool slider_float_ex(const char* label, float* v, float v_min, float v_max, const char* format = "%.2f");
    bool combo_ex(const char* label, int* current_item, const char* const items[], int items_count);
    bool multi_combo_ex(const char* label, bool* values, const char* const items[], int items_count);
    bool keybind_ex(const char* label, int* key, int* mode);
    bool color_picker_ex(const char* label, float* col);
    bool button_ex(const char* label, const ImVec2& size = ImVec2(0, 0));
    void group_box(const char* label, const ImVec2& size, std::function<void()> content);
    void separator_ex(const char* label = nullptr);
    void tooltip(const char* text);
    
    // Utility
    bool blocks_attacks() const { return m_opened.load() || m_block_attack_until_release.load(); }
    bool on_create_move();
    void apply_style();
    
    // Spectator list & watermark
    void draw_watermark();
    void draw_spectator_list();
    void draw_keybinds_list();
};

inline const auto g_menu = std::make_unique<c_menu>();
