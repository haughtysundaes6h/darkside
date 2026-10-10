// Minimal implementation of c_gui methods needed for menu
#include "gui.hpp"

void c_gui::apply_theme(int theme) {
    this->theme = theme;
    // Apply dark theme colors
    accent_color = color_t{ 0.318f, 0.486f, 0.992f, 1.f };
    text = color_t{ 0.922f, 0.922f, 0.941f, 1.f };
    text_disabled = color_t{ 0.51f, 0.52f, 0.56f, 1.f };
    border = color_t{ 0.118f, 0.129f, 0.173f, 1.f };
    group_box_bg = color_t{ 0.071f, 0.078f, 0.114f, 1.f };
}

void c_gui::apply_style_colors() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    colors[ImGuiCol_Text]                   = ImVec4(text.r, text.g, text.b, text.a);
    colors[ImGuiCol_TextDisabled]           = ImVec4(text_disabled.r, text_disabled.g, text_disabled.b, text_disabled.a);
    colors[ImGuiCol_WindowBg]               = ImVec4(panel_body.r, panel_body.g, panel_body.b, panel_body.a);
    colors[ImGuiCol_ChildBg]                = ImVec4(group_box_bg.r, group_box_bg.g, group_box_bg.b, group_box_bg.a);
    colors[ImGuiCol_Border]                 = ImVec4(border.r, border.g, border.b, border.a);
    colors[ImGuiCol_FrameBg]                = ImVec4(frame_inactive.r, frame_inactive.g, frame_inactive.b, frame_inactive.a);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(frame_active.r, frame_active.g, frame_active.b, frame_active.a);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(frame_active.r, frame_active.g, frame_active.b, frame_active.a);
    colors[ImGuiCol_Button]                 = ImVec4(button.r, button.g, button.b, button.a);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(button_hovered.r, button_hovered.g, button_hovered.b, button_hovered.a);
    colors[ImGuiCol_ButtonActive]           = ImVec4(button_active.r, button_active.g, button_active.b, button_active.a);
    colors[ImGuiCol_CheckMark]              = ImVec4(accent_color.r, accent_color.g, accent_color.b, accent_color.a);
    colors[ImGuiCol_SliderGrab]             = ImVec4(accent_color.r, accent_color.g, accent_color.b, accent_color.a);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(accent_color.r, accent_color.g, accent_color.b, accent_color.a);
}

bool c_gui::tab(const char* icon, const char* label, bool selected, float indent) {
    ImGui::PushID(label);
    
    ImVec2 size(120, 35);
    ImVec4 bg_color = selected ? 
        ImVec4(accent_color.r, accent_color.g, accent_color.b, accent_color.a) :
        ImVec4(button.r, button.g, button.b, button.a);
    
    ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(button_hovered.r, button_hovered.g, button_hovered.b, button_hovered.a));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(button_active.r, button_active.g, button_active.b, button_active.a));
    
    char buf[128];
    snprintf(buf, sizeof(buf), "%s  %s", icon, label);
    bool clicked = ImGui::Button(buf, size);
    
    ImGui::PopStyleColor(3);
    ImGui::PopID();
    
    return clicked;
}

bool c_gui::row_checkbox(const char* label, bool* v, const char* description) {
    return ImGui::Checkbox(label, v);
}

void c_gui::begin_grid(int columns, const float* weights) {
    m_grid_col_w.clear();
    float total = ImGui::GetContentRegionAvail().x;
    float col_width = total / columns;
    
    for (int i = 0; i < columns; i++) {
        m_grid_col_w.push_back(col_width);
    }
    
    m_grid_cursor.resize(columns, ImVec2(0, 0));
    m_grid_pos = ImGui::GetCursorScreenPos();
    m_grid_size = ImVec2(total, ImGui::GetContentRegionAvail().y);
    m_grid_active = 0;
}

void c_gui::grid_col(int index) {
    if (index < 0 || index >= (int)m_grid_col_w.size())
        return;
    
    m_grid_active = index;
    
    float x_offset = 0;
    for (int i = 0; i < index; i++) {
        x_offset += m_grid_col_w[i] + ImGui::GetStyle().ItemSpacing.x;
    }
    
    ImGui::SetCursorScreenPos(ImVec2(m_grid_pos.x + x_offset, m_grid_pos.y + m_grid_cursor[index].y));
}

void c_gui::grid_box(const char* name, float height, bool scrollable) {
    float box_height = height > 0 ? height * m_scale : 300.f;
    
    ImGui::BeginChild(name, ImVec2(m_grid_col_w[m_grid_active] - ImGui::GetStyle().ItemSpacing.x, box_height), true);
}

void c_gui::end_group_box() {
    m_grid_cursor[m_grid_active].y += ImGui::GetWindowSize().y + ImGui::GetStyle().ItemSpacing.y;
    ImGui::EndChild();
}

void c_gui::end_grid() {
    m_grid_col_w.clear();
    m_grid_cursor.clear();
}

void c_gui::user_profile(ImVec2 min, ImVec2 max, void* avatar_tex) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    
    // Simple user badge
    ImVec2 size = ImVec2(max.x - min.x, max.y - min.y);
    draw->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(ImVec4(0.1f, 0.1f, 0.12f, 1.f)), 6.f);
    draw->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(ImVec4(border.r, border.g, border.b, border.a)), 6.f);
    
    // Draw username
    ImGui::SetCursorScreenPos(ImVec2(min.x + 10, min.y + 10));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(text.r, text.g, text.b, text.a));
    ImGui::Text("User: Premium");
    ImGui::PopStyleColor();
}
