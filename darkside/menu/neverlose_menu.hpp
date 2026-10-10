#pragma once

#include "../sdk/includes/imgui/imgui.h"
#include "../sdk/typedefs/c_color.hpp"
#include <string>
#include <vector>
#include <functional>

namespace neverlose {

// Цветовая схема Neverlose
struct colors_t {
    ImVec4 accent = ImVec4(0.318f, 0.486f, 0.992f, 1.f);     // синий акцент
    ImVec4 text = ImVec4(0.922f, 0.922f, 0.941f, 1.f);        // белый текст
    ImVec4 text_disabled = ImVec4(0.51f, 0.52f, 0.56f, 1.f);  // серый текст
    
    ImVec4 bg_dark = ImVec4(0.043f, 0.055f, 0.086f, 0.90f);   // темный фон
    ImVec4 bg_medium = ImVec4(0.071f, 0.078f, 0.114f, 1.f);   // средний фон
    ImVec4 bg_light = ImVec4(0.102f, 0.110f, 0.149f, 1.f);    // светлый фон
    
    ImVec4 border = ImVec4(0.118f, 0.129f, 0.173f, 1.f);      // граница
    ImVec4 separator = ImVec4(1.f, 1.f, 1.f, 0.117f);         // разделитель
    
    ImVec4 frame_bg = ImVec4(0.098f, 0.110f, 0.149f, 1.f);    // фон элемента
    ImVec4 frame_active = ImVec4(0.043f, 0.07f, 0.137f, 1.f); // активный элемент
    
    ImVec4 button = ImVec4(0.102f, 0.102f, 0.133f, 1.f);
    ImVec4 button_hovered = ImVec4(0.050f, 0.054f, 0.078f, 1.f);
    ImVec4 button_active = ImVec4(0.07f, 0.074f, 0.098f, 1.f);
};

class menu_t {
public:
    colors_t colors;
    float scale = 1.0f;
    int active_tab = 0;
    int active_subtab = 0;
    
    // Применить стиль Neverlose
    void apply_style();
    
    // Главное окно меню
    void render();
    
    // Tabs
    bool tab(const char* icon, const char* label, int index);
    bool subtab(const char* label, int index);
    
    // Group box
    void begin_group(const char* title, float height = 0.f);
    void end_group();
    
    // Custom widgets
    bool checkbox(const char* label, bool* v);
    bool slider_int(const char* label, int* v, int min, int max);
    bool slider_float(const char* label, float* v, float min, float max);
    bool combo(const char* label, int* current, const char* const items[], int count);
    bool color_edit(const char* label, float col[4]);
    
    // Keybind
    bool keybind(const char* label, int* key);
    
    // Multi-column layout
    void begin_columns(int count);
    void next_column();
    void end_columns();
    
private:
    bool m_in_group = false;
    ImVec2 m_group_start_pos;
    float m_group_height = 0.f;
    
    // Анимации
    std::vector<float> m_tab_anims;
    
    void draw_tab_selector(ImVec2 pos, ImVec2 size);
    void draw_rounded_rect(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float rounding);
};

extern menu_t g_menu;

} // namespace neverlose
