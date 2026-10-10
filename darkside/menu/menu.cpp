#include "menu.hpp"
#include "../darkside.hpp"
#include "../sdk/config_system/config_system.hpp"
#include "neverlose_style.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "../sdk/includes/imgui/imgui_internal.h"

void c_menu::init() {
    apply_style();
}

void c_menu::apply_style() {
    neverlose_style::apply_style();
}

// Lerp helper
static ImVec4 lerp_color(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

bool c_menu::tab_button(const char* label, int index, ImVec2 size) {
    bool selected = (m_selected_tab == index);
    
    // Smooth animation
    float target = selected ? 1.f : 0.f;
    m_tab_anim[index] += (target - m_tab_anim[index]) * ImGui::GetIO().DeltaTime * 8.f;
    
    ImVec4 bg_color = lerp_color(bg_light, accent, m_tab_anim[index]);
    ImVec4 text_color = lerp_color(text_dark, text, m_tab_anim[index]);
    
    ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, lerp_color(bg_color, accent, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
    ImGui::PushStyleColor(ImGuiCol_Text, text_color);
    
    bool clicked = ImGui::Button(label, size);
    
    ImGui::PopStyleColor(4);
    
    if (clicked) {
        m_selected_tab = index;
        m_selected_subtab = 0;
    }
    
    return clicked;
}

void c_menu::begin_child_window(const char* id, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bg_medium);
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
    ImGui::BeginChild(id, size, true);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void c_menu::group_box(const char* title, std::function<void()> content) {
    ImGui::BeginGroup();
    
    // Title
    ImGui::PushStyleColor(ImGuiCol_Text, text_dark);
    ImGui::Text("%s", title);
    ImGui::PopStyleColor();
    
    ImGui::Spacing();
    
    // Content
    ImGui::Indent(10.f);
    content();
    ImGui::Unindent(10.f);
    
    ImGui::EndGroup();
    ImGui::Spacing();
}

void c_menu::tab_rage() {
    ImGui::Columns(2, nullptr, false);
    
    // Left column
    begin_child_window("##rage_left", ImVec2(0, -1));
    
    group_box("GENERAL", []() {
        ImGui::Checkbox("Enabled", &g_cfg->rage_bot.m_enabled);
        ImGui::Checkbox("Silent Aim", &g_cfg->rage_bot.m_silent);
    });
    
    group_box("HITBOXES", []() {
        auto& cfg = g_cfg->rage_bot.m_weapon_config[0];
        ImGui::Checkbox("Head", &cfg.m_hitboxes[0]);
        ImGui::Checkbox("Chest", &cfg.m_hitboxes[1]);
        ImGui::Checkbox("Stomach", &cfg.m_hitboxes[2]);
        ImGui::Checkbox("Arms", &cfg.m_hitboxes[3]);
        ImGui::Checkbox("Legs", &cfg.m_hitboxes[4]);
    });
    
    group_box("ACCURACY", []() {
        auto& cfg = g_cfg->rage_bot.m_weapon_config[0];
        ImGui::SliderInt("Hit Chance", &cfg.m_hit_chance, 0, 100, "%d%%");
        ImGui::SliderInt("Min Damage", &cfg.m_minimum_damage, 0, 120);
    });
    
    ImGui::EndChild();
    
    ImGui::NextColumn();
    
    // Right column
    begin_child_window("##rage_right", ImVec2(0, -1));
    
    group_box("MULTIPOINT", []() {
        auto& cfg = g_cfg->rage_bot.m_weapon_config[0];
        for (int i = 0; i < 5; i++) {
            ImGui::PushID(i);
            ImGui::Checkbox("Enable##mp", &cfg.m_multi_points[i]);
            ImGui::SameLine();
            ImGui::SliderInt("Scale", &cfg.m_hitbox_scale[i], 0, 100, "%d%%");
            ImGui::PopID();
        }
    });
    
    group_box("BEHAVIOR", []() {
        auto& cfg = g_cfg->rage_bot.m_weapon_config[0];
        ImGui::Checkbox("Auto Fire", &cfg.m_auto_fire);
        ImGui::Checkbox("Auto Stop", &cfg.m_auto_stop);
        ImGui::Checkbox("Auto Scope", &cfg.m_auto_scope);
    });
    
    ImGui::EndChild();
    
    ImGui::Columns(1);
}

void c_menu::tab_legit() {
    ImGui::Columns(2, nullptr, false);
    
    begin_child_window("##legit_left", ImVec2(0, -1));
    
    group_box("AIMBOT", []() {
        ImGui::Checkbox("Enabled", &g_cfg->legit_bot.m_enabled);
        ImGui::SliderFloat("FOV", &g_cfg->legit_bot.m_fov, 0.f, 180.f, "%.1f°");
        ImGui::SliderFloat("Smooth", &g_cfg->legit_bot.m_smooth, 1.f, 20.f, "%.1f");
    });
    
    group_box("BACKTRACK", []() {
        ImGui::Checkbox("Enabled", &g_cfg->legit_bot.m_backtrack_enabled);
        ImGui::SliderInt("Ticks", &g_cfg->legit_bot.m_backtrack_ticks, 0, 15);
    });
    
    ImGui::EndChild();
    
    ImGui::NextColumn();
    
    begin_child_window("##legit_right", ImVec2(0, -1));
    
    group_box("TRIGGERBOT", []() {
        ImGui::Checkbox("Enabled", &g_cfg->legit_bot.m_triggerbot_enabled);
    });
    
    ImGui::EndChild();
    
    ImGui::Columns(1);
}

void c_menu::tab_antiaim() {
    ImGui::Columns(2, nullptr, false);
    
    begin_child_window("##aa_left", ImVec2(0, -1));
    
    group_box("PITCH", []() {
        static const char* pitch_modes[] = {"Off", "Down", "Up", "Zero", "Custom"};
        ImGui::Combo("Mode", &g_cfg->anti_hit.m_pitch_mode, pitch_modes, IM_ARRAYSIZE(pitch_modes));
        if (g_cfg->anti_hit.m_pitch_mode == 4) {
            ImGui::SliderInt("Amount", &g_cfg->anti_hit.m_pitch_amount, -89, 89);
        }
    });
    
    group_box("YAW", []() {
        static const char* yaw_modes[] = {"Off", "Backward", "Sideways", "Spin", "At Target"};
        ImGui::Combo("Mode", &g_cfg->anti_hit.m_yaw_mode, yaw_modes, IM_ARRAYSIZE(yaw_modes));
        ImGui::SliderInt("Add", &g_cfg->anti_hit.m_yaw_amount, -180, 180);
        ImGui::Checkbox("At Target", &g_cfg->anti_hit.m_at_target);
    });
    
    group_box("JITTER", []() {
        ImGui::Checkbox("Enabled", &g_cfg->anti_hit.m_jitter_enabled);
        static const char* jitter_modes[] = {"Offset", "Center", "Random"};
        ImGui::Combo("Mode", &g_cfg->anti_hit.m_jitter_mode, jitter_modes, IM_ARRAYSIZE(jitter_modes));
        ImGui::SliderInt("Amount", &g_cfg->anti_hit.m_jitter_amount, 0, 180);
    });
    
    ImGui::EndChild();
    
    ImGui::NextColumn();
    
    begin_child_window("##aa_right", ImVec2(0, -1));
    
    group_box("DESYNC", []() {
        ImGui::Checkbox("Enabled", &g_cfg->anti_hit.m_desync_enabled);
        static const char* desync_modes[] = {"Static", "Jitter", "Switch"};
        ImGui::Combo("Mode", &g_cfg->anti_hit.m_desync_mode, desync_modes, IM_ARRAYSIZE(desync_modes));
        ImGui::SliderInt("Left Amount", &g_cfg->anti_hit.m_desync_left, 0, 60);
        ImGui::SliderInt("Right Amount", &g_cfg->anti_hit.m_desync_right, 0, 60);
    });
    
    group_box("FAKELAG", []() {
        ImGui::Checkbox("Enabled", &g_cfg->anti_hit.m_fakelag_enabled);
        static const char* fl_modes[] = {"Static", "Random", "Adaptive"};
        ImGui::Combo("Mode", &g_cfg->anti_hit.m_fakelag_mode, fl_modes, IM_ARRAYSIZE(fl_modes));
        ImGui::SliderInt("Limit", &g_cfg->anti_hit.m_fakelag_limit, 1, 15);
        ImGui::Checkbox("On Peek Only", &g_cfg->anti_hit.m_fakelag_on_peek);
    });
    
    ImGui::EndChild();
    
    ImGui::Columns(1);
}

void c_menu::tab_visuals() {
    ImGui::Columns(2, nullptr, false);
    
    begin_child_window("##vis_left", ImVec2(0, -1));
    
    group_box("PLAYER ESP", []() {
        ImGui::Checkbox("Enabled", &g_cfg->visuals.m_player_esp.m_enabled);
        ImGui::Checkbox("Teammates", &g_cfg->visuals.m_player_esp.m_teammates);
        ImGui::Checkbox("Bounding Box", &g_cfg->visuals.m_player_esp.m_bounding_box);
        ImGui::Checkbox("Name", &g_cfg->visuals.m_player_esp.m_name);
        ImGui::Checkbox("Health Bar", &g_cfg->visuals.m_player_esp.m_health_bar);
        ImGui::Checkbox("Weapon", &g_cfg->visuals.m_player_esp.m_weapon);
        ImGui::Checkbox("Skeleton", &g_cfg->visuals.m_player_esp.m_skeleton);
    });
    
    ImGui::EndChild();
    
    ImGui::NextColumn();
    
    begin_child_window("##vis_right", ImVec2(0, -1));
    
    group_box("WORLD", []() {
        ImGui::SliderFloat("Exposure", &g_cfg->world.m_exposure, 0.f, 2.f, "%.2f");
        ImGui::SliderFloat("Lighting", &g_cfg->world.m_lighting, 0.f, 1.f, "%.2f");
        ImGui::Checkbox("Remove Fog", &g_cfg->world.m_render_fog);
    });
    
    group_box("THIRDPERSON", []() {
        ImGui::Checkbox("Enabled", &g_cfg->world_esp.m_enable_thirdperson);
        ImGui::SliderFloat("Distance", &g_cfg->world_esp.m_distance, 50.f, 300.f, "%.0f");
    });
    
    ImGui::EndChild();
    
    ImGui::Columns(1);
}

void c_menu::tab_misc() {
    ImGui::Columns(2, nullptr, false);
    
    begin_child_window("##misc_left", ImVec2(0, -1));
    
    group_box("MOVEMENT", []() {
        ImGui::Checkbox("Bunny Hop", &g_cfg->misc.m_bunny_hop);
        ImGui::Checkbox("Auto Strafe", &g_cfg->misc.m_auto_strafe);
        
        static const char* strafe_types[] = {"Legit", "Directional", "Rage"};
        ImGui::Combo("Type", &g_cfg->misc.m_strafe_type, strafe_types, IM_ARRAYSIZE(strafe_types));
        ImGui::SliderFloat("Smooth", &g_cfg->misc.m_strafe_smooth, 0.f, 100.f, "%.1f%%");
    });
    
    ImGui::EndChild();
    
    ImGui::NextColumn();
    
    begin_child_window("##misc_right", ImVec2(0, -1));
    
    group_box("OTHER", []() {
        ImGui::Text("Miscellaneous settings");
    });
    
    ImGui::EndChild();
    
    ImGui::Columns(1);
}

void c_menu::tab_skins() {
    ImGui::Columns(2, nullptr, false);
    
    begin_child_window("##skins_left", ImVec2(0, -1));
    
    group_box("KNIFE", []() {
        ImGui::SliderInt("Selected", &g_cfg->skins.m_knives.m_selected, 0, 20);
    });
    
    group_box("GLOVES", []() {
        ImGui::SliderInt("Selected", &g_cfg->skins.m_gloves.m_selected, 0, 20);
    });
    
    ImGui::EndChild();
    
    ImGui::NextColumn();
    
    begin_child_window("##skins_right", ImVec2(0, -1));
    
    group_box("AGENTS", []() {
        ImGui::SliderInt("CT Agent", &g_cfg->skins.m_agents.m_selected_ct, 0, 20);
        ImGui::SliderInt("T Agent", &g_cfg->skins.m_agents.m_selected, 0, 20);
    });
    
    ImGui::EndChild();
    
    ImGui::Columns(1);
}

void c_menu::tab_configs() {
    static char config_name[64] = "";
    
    ImGui::Columns(2, nullptr, false);
    
    begin_child_window("##cfg_left", ImVec2(0, -1));
    
    ImGui::Text("CONFIG MANAGER");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Config list
    g_config_system->refresh();
    for (const auto& cfg : g_config_system->config_files) {
        if (ImGui::Selectable(cfg.c_str())) {
            strncpy_s(config_name, cfg.c_str(), sizeof(config_name) - 1);
        }
    }
    
    ImGui::EndChild();
    
    ImGui::NextColumn();
    
    begin_child_window("##cfg_right", ImVec2(0, -1));
    
    ImGui::InputText("Name", config_name, sizeof(config_name));
    ImGui::Spacing();
    
    if (ImGui::Button("Create", ImVec2(-1, 0))) {
        if (strlen(config_name) > 0) {
            g_config_system->save(config_name);
            g_config_system->refresh();
        }
    }
    
    if (ImGui::Button("Load", ImVec2(-1, 0))) {
        if (strlen(config_name) > 0) {
            g_config_system->read(config_name);
        }
    }
    
    if (ImGui::Button("Save", ImVec2(-1, 0))) {
        if (strlen(config_name) > 0) {
            g_config_system->save(config_name);
        }
    }
    
    if (ImGui::Button("Delete", ImVec2(-1, 0))) {
        if (strlen(config_name) > 0) {
            g_config_system->remove(config_name);
            g_config_system->refresh();
            config_name[0] = '\0';
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    if (ImGui::Button("Reset to Default", ImVec2(-1, 0))) {
        g_config_system->reset();
    }
    
    ImGui::EndChild();
    
    ImGui::Columns(1);
}

void c_menu::render() {
    if (!m_opened)
        return;
    
    // Smooth fade in/out
    float target_alpha = m_opened ? 1.f : 0.f;
    m_alpha += (target_alpha - m_alpha) * ImGui::GetIO().DeltaTime * 10.f;
    
    if (m_alpha < 0.01f)
        return;
    
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_alpha);
    
    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(800, 500), ImVec2(FLT_MAX, FLT_MAX));
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg_dark);
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    
    if (ImGui::Begin("DARKSIDE", &m_opened, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {
        ImVec2 window_size = ImGui::GetWindowSize();
        
        // Header
        ImGui::PushStyleColor(ImGuiCol_Text, accent);
        ImGui::Text("DARKSIDE");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, text_dark);
        ImGui::Text("| CS2 Premium");
        ImGui::PopStyleColor();
        
        ImGui::Separator();
        ImGui::Spacing();
        
        // Tabs sidebar
        ImGui::BeginGroup();
        {
            float tab_width = 120.f;
            ImVec2 tab_size(tab_width, 35.f);
            
            tab_button("RAGE", 0, tab_size);
            tab_button("LEGIT", 1, tab_size);
            tab_button("ANTI-AIM", 2, tab_size);
            tab_button("VISUALS", 3, tab_size);
            tab_button("MISC", 4, tab_size);
            tab_button("SKINS", 5, tab_size);
            tab_button("CONFIGS", 6, tab_size);
        }
        ImGui::EndGroup();
        
        ImGui::SameLine();
        
        // Content area
        ImGui::BeginGroup();
        {
            ImVec2 content_size(window_size.x - 150.f, window_size.y - 80.f);
            ImGui::BeginChild("##content", content_size, false);
            
            switch (m_selected_tab) {
                case 0: tab_rage(); break;
                case 1: tab_legit(); break;
                case 2: tab_antiaim(); break;
                case 3: tab_visuals(); break;
                case 4: tab_misc(); break;
                case 5: tab_skins(); break;
                case 6: tab_configs(); break;
            }
            
            ImGui::EndChild();
        }
        ImGui::EndGroup();
        
        ImGui::End();
    }
    
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}
