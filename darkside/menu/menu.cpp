#define IMGUI_DEFINE_MATH_OPERATORS
#include "menu.hpp"
#include "../darkside.hpp"
#include "../sdk/config_system/config_system.hpp"
#include "../sdk/includes/imgui/imgui_internal.h"

void c_menu::init() {
    // Инициализация GUI системы Neverlose
    m_gui.apply_theme(0); // Dark theme
    m_gui.apply_style_colors();
}

void c_menu::draw() {
    render(); // Alias
}

void c_menu::draw_tabs() {
    // Используем tab систему из Neverlose GUI
    const char* tab_icons[] = {
        "R",   // Rage
        "L",   // Legit
        "A",   // Anti-Aim
        "V",   // Visuals
        "M",   // Misc
        "S",   // Skins
        "C"    // Config
    };
    
    const char* tab_labels[] = {
        "RAGE",
        "LEGIT",
        "ANTI-AIM",
        "VISUALS",
        "MISC",
        "SKINS",
        "CONFIG"
    };
    
    static int active_tab = 0;
    
    ImVec2 tab_pos = ImGui::GetCursorScreenPos();
    for (int i = 0; i < 7; i++) {
        bool selected = (active_tab == i);
        if (m_gui.tab(tab_icons[i], tab_labels[i], selected)) {
            active_tab = i;
        }
    }
}

void c_menu::draw_rage() {
    m_gui.begin_grid(2);
    
    // Left column
    m_gui.grid_col(0);
    
    m_gui.grid_box("GENERAL");
    m_gui.row_checkbox("Enabled", &g_cfg->rage_bot.m_enabled);
    m_gui.row_checkbox("Silent Aim", &g_cfg->rage_bot.m_silent);
    m_gui.end_group_box();
    
    m_gui.grid_box("HITBOXES");
    auto& cfg = g_cfg->rage_bot.m_weapon_config[0];
    m_gui.row_checkbox("Head", &cfg.m_hitboxes[0]);
    m_gui.row_checkbox("Chest", &cfg.m_hitboxes[1]);
    m_gui.row_checkbox("Stomach", &cfg.m_hitboxes[2]);
    m_gui.row_checkbox("Arms", &cfg.m_hitboxes[3]);
    m_gui.row_checkbox("Legs", &cfg.m_hitboxes[4]);
    m_gui.end_group_box();
    
    // Right column
    m_gui.grid_col(1);
    
    m_gui.grid_box("ACCURACY");
    ImGui::SliderInt("Hit Chance", &cfg.m_hit_chance, 0, 100, "%d%%");
    ImGui::SliderInt("Min Damage", &cfg.m_minimum_damage, 0, 120);
    m_gui.end_group_box();
    
    m_gui.grid_box("BEHAVIOR");
    m_gui.row_checkbox("Auto Fire", &cfg.m_auto_fire);
    m_gui.row_checkbox("Auto Stop", &cfg.m_auto_stop);
    m_gui.row_checkbox("Auto Scope", &cfg.m_auto_scope);
    m_gui.end_group_box();
    
    m_gui.end_grid();
}

void c_menu::draw_legit() {
    m_gui.begin_grid(2);
    
    m_gui.grid_col(0);
    m_gui.grid_box("AIMBOT");
    m_gui.row_checkbox("Enabled", &g_cfg->legit_bot.m_enabled);
    ImGui::SliderFloat("FOV", &g_cfg->legit_bot.m_fov, 0.f, 180.f, "%.1f°");
    ImGui::SliderFloat("Smooth", &g_cfg->legit_bot.m_smooth, 1.f, 20.f);
    m_gui.end_group_box();
    
    m_gui.grid_col(1);
    m_gui.grid_box("TRIGGERBOT");
    m_gui.row_checkbox("Enabled", &g_cfg->legit_bot.m_triggerbot_enabled);
    m_gui.end_group_box();
    
    m_gui.grid_box("BACKTRACK");
    m_gui.row_checkbox("Enabled", &g_cfg->legit_bot.m_backtrack_enabled);
    ImGui::SliderInt("Ticks", &g_cfg->legit_bot.m_backtrack_ticks, 0, 15);
    m_gui.end_group_box();
    
    m_gui.end_grid();
}

void c_menu::draw_antiaim() {
    m_gui.begin_grid(2);
    
    m_gui.grid_col(0);
    
    m_gui.grid_box("PITCH");
    static const char* pitch_modes[] = {"Off", "Down", "Up", "Zero", "Custom"};
    ImGui::Combo("Mode", &g_cfg->anti_hit.m_pitch_mode, pitch_modes, IM_ARRAYSIZE(pitch_modes));
    if (g_cfg->anti_hit.m_pitch_mode == 4) {
        ImGui::SliderInt("Amount", &g_cfg->anti_hit.m_pitch_amount, -89, 89);
    }
    m_gui.end_group_box();
    
    m_gui.grid_box("YAW");
    static const char* yaw_modes[] = {"Off", "Backward", "Sideways", "Spin", "At Target"};
    ImGui::Combo("Mode", &g_cfg->anti_hit.m_yaw_mode, yaw_modes, IM_ARRAYSIZE(yaw_modes));
    ImGui::SliderInt("Add", &g_cfg->anti_hit.m_yaw_amount, -180, 180);
    m_gui.row_checkbox("At Target", &g_cfg->anti_hit.m_at_target);
    m_gui.end_group_box();
    
    m_gui.grid_col(1);
    
    m_gui.grid_box("DESYNC");
    m_gui.row_checkbox("Enabled", &g_cfg->anti_hit.m_desync_enabled);
    static const char* desync_modes[] = {"Static", "Jitter", "Switch"};
    ImGui::Combo("Mode", &g_cfg->anti_hit.m_desync_mode, desync_modes, IM_ARRAYSIZE(desync_modes));
    ImGui::SliderInt("Left", &g_cfg->anti_hit.m_desync_left, 0, 60);
    ImGui::SliderInt("Right", &g_cfg->anti_hit.m_desync_right, 0, 60);
    m_gui.end_group_box();
    
    m_gui.grid_box("FAKELAG");
    m_gui.row_checkbox("Enabled", &g_cfg->anti_hit.m_fakelag_enabled);
    static const char* fl_modes[] = {"Static", "Random", "Adaptive"};
    ImGui::Combo("Mode", &g_cfg->anti_hit.m_fakelag_mode, fl_modes, IM_ARRAYSIZE(fl_modes));
    ImGui::SliderInt("Limit", &g_cfg->anti_hit.m_fakelag_limit, 1, 15);
    m_gui.end_group_box();
    
    m_gui.end_grid();
}

void c_menu::draw_visuals() {
    m_gui.begin_grid(2);
    
    m_gui.grid_col(0);
    m_gui.grid_box("PLAYER ESP");
    m_gui.row_checkbox("Enabled", &g_cfg->visuals.m_player_esp.m_enabled);
    m_gui.row_checkbox("Teammates", &g_cfg->visuals.m_player_esp.m_teammates);
    m_gui.row_checkbox("Box", &g_cfg->visuals.m_player_esp.m_bounding_box);
    m_gui.row_checkbox("Name", &g_cfg->visuals.m_player_esp.m_name);
    m_gui.row_checkbox("Health", &g_cfg->visuals.m_player_esp.m_health_bar);
    m_gui.row_checkbox("Weapon", &g_cfg->visuals.m_player_esp.m_weapon);
    m_gui.row_checkbox("Skeleton", &g_cfg->visuals.m_player_esp.m_skeleton);
    m_gui.end_group_box();
    
    m_gui.grid_col(1);
    m_gui.grid_box("WORLD");
    ImGui::SliderInt("Exposure", &g_cfg->world.m_exposure, 0, 100);
    ImGui::SliderFloat("Brightness", &g_cfg->world.m_brightness, 0.f, 1.f, "%.2f");
    m_gui.row_checkbox("Remove Fog", &g_cfg->world.m_render_fog);
    m_gui.end_group_box();
    
    m_gui.grid_box("THIRDPERSON");
    m_gui.row_checkbox("Enabled", &g_cfg->world_esp.m_enable_thirdperson);
    ImGui::SliderFloat("Distance", &g_cfg->world_esp.m_distance, 50.f, 300.f, "%.0f");
    m_gui.end_group_box();
    
    m_gui.end_grid();
}

void c_menu::draw_misc() {
    m_gui.begin_grid(2);
    
    m_gui.grid_col(0);
    m_gui.grid_box("MOVEMENT");
    m_gui.row_checkbox("Bunny Hop", &g_cfg->misc.m_bunny_hop);
    m_gui.row_checkbox("Auto Strafe", &g_cfg->misc.m_auto_strafe);
    static const char* strafe_types[] = {"Legit", "Directional", "Rage"};
    ImGui::Combo("Type", &g_cfg->misc.m_strafe_type, strafe_types, IM_ARRAYSIZE(strafe_types));
    ImGui::SliderFloat("Smooth", &g_cfg->misc.m_strafe_smooth, 0.f, 100.f, "%.1f%%");
    m_gui.end_group_box();
    
    m_gui.grid_col(1);
    m_gui.grid_box("OTHER");
    ImGui::Text("Miscellaneous settings");
    m_gui.end_group_box();
    
    m_gui.end_grid();
}

void c_menu::draw_skins() {
    m_gui.begin_grid(2);
    
    m_gui.grid_col(0);
    m_gui.grid_box("KNIFE");
    ImGui::SliderInt("Model", &g_cfg->skins.m_knives.m_selected, 0, 20);
    m_gui.end_group_box();
    
    m_gui.grid_box("GLOVES");
    ImGui::SliderInt("Model", &g_cfg->skins.m_gloves.m_selected, 0, 20);
    m_gui.end_group_box();
    
    m_gui.grid_col(1);
    m_gui.grid_box("AGENTS");
    ImGui::SliderInt("CT Agent", &g_cfg->skins.m_agents.m_selected_ct, 0, 20);
    ImGui::SliderInt("T Agent", &g_cfg->skins.m_agents.m_selected, 0, 20);
    m_gui.end_group_box();
    
    m_gui.end_grid();
}

void c_menu::draw_configs() {
    static char config_name[64] = "";
    
    m_gui.begin_grid(2);
    
    m_gui.grid_col(0);
    m_gui.grid_box("CONFIGS");
    
    g_config_system->refresh();
    ImGui::BeginChild("##config_list", ImVec2(0, -100), true);
    for (const auto& cfg : g_config_system->config_files) {
        if (ImGui::Selectable(cfg.c_str())) {
            strncpy_s(config_name, cfg.c_str(), sizeof(config_name) - 1);
        }
    }
    ImGui::EndChild();
    
    m_gui.end_group_box();
    
    m_gui.grid_col(1);
    m_gui.grid_box("ACTIONS");
    
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
    if (ImGui::Button("Reset All", ImVec2(-1, 0))) {
        g_config_system->reset();
    }
    
    m_gui.end_group_box();
    
    m_gui.end_grid();
}

void c_menu::render() {
    if (!m_opened)
        return;
    
    // Применяем стиль Neverlose
    m_gui.apply_style_colors();
    
    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("DARKSIDE##main", &m_opened, 
        ImGuiWindowFlags_NoCollapse | 
        ImGuiWindowFlags_NoScrollbar | 
        ImGuiWindowFlags_NoResize)) {
        
        // Header
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        
        // User profile (top right)
        m_gui.user_profile(ImVec2(window_pos.x + window_size.x - 200, window_pos.y + 15), 
                          ImVec2(window_pos.x + window_size.x - 15, window_pos.y + 50), nullptr);
        
        // Sidebar with tabs
        ImGui::SetCursorPos(ImVec2(20, 80));
        ImGui::BeginGroup();
        {
            static int tab = 0;
            
            if (m_gui.tab("R", "RAGE", tab == 0, 0.f)) tab = 0;
            if (m_gui.tab("L", "LEGIT", tab == 1, 0.f)) tab = 1;
            if (m_gui.tab("A", "ANTI-AIM", tab == 2, 0.f)) tab = 2;
            if (m_gui.tab("V", "VISUALS", tab == 3, 0.f)) tab = 3;
            if (m_gui.tab("M", "MISC", tab == 4, 0.f)) tab = 4;
            if (m_gui.tab("S", "SKINS", tab == 5, 0.f)) tab = 5;
            if (m_gui.tab("C", "CONFIG", tab == 6, 0.f)) tab = 6;
            
            // Content area
            ImGui::SetCursorPos(ImVec2(180, 80));
            ImGui::BeginChild("##content", ImVec2(window_size.x - 200, window_size.y - 100), false);
            
            switch (tab) {
                case 0: draw_rage(); break;
                case 1: draw_legit(); break;
                case 2: draw_antiaim(); break;
                case 3: draw_visuals(); break;
                case 4: draw_misc(); break;
                case 5: draw_skins(); break;
                case 6: draw_configs(); break;
            }
            
            ImGui::EndChild();
        }
        ImGui::EndGroup();
        
        ImGui::End();
    }
}
