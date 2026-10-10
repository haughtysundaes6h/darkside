#pragma once

#include "../sdk/includes/imgui/imgui.h"
#include "../sdk/typedefs/c_color.hpp"
#include <vector>
#include <string>
#include <functional>

// Neverlose-style menu
class c_menu {
public:
    void init();
    void render();
    void toggle() { m_opened = !m_opened; }
    bool is_opened() const { return m_opened; }
    
private:
    bool m_opened = false;
    int m_selected_tab = 0;
    int m_selected_subtab = 0;
    
    // Animation
    float m_alpha = 0.f;
    float m_tab_anim[8] = {0.f};
    
    // Neverlose colors
    ImVec4 accent = ImVec4(0.318f, 0.486f, 0.992f, 1.f);
    ImVec4 text = ImVec4(0.922f, 0.922f, 0.941f, 1.f);
    ImVec4 text_dark = ImVec4(0.51f, 0.52f, 0.56f, 1.f);
    ImVec4 bg_dark = ImVec4(0.043f, 0.055f, 0.086f, 0.95f);
    ImVec4 bg_medium = ImVec4(0.071f, 0.078f, 0.114f, 1.f);
    ImVec4 bg_light = ImVec4(0.098f, 0.110f, 0.149f, 1.f);
    ImVec4 border = ImVec4(0.118f, 0.129f, 0.173f, 1.f);
    
    // Helpers
    void apply_style();
    bool tab_button(const char* label, int index, ImVec2 size);
    void begin_child_window(const char* id, ImVec2 size);
    void group_box(const char* title, std::function<void()> content);
    
    // Tabs
    void tab_rage();
    void tab_legit();
    void tab_antiaim();
    void tab_visuals();
    void tab_misc();
    void tab_skins();
    void tab_configs();
};

inline auto g_menu = std::make_unique<c_menu>();
