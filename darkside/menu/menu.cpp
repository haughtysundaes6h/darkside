#include "../features/protobufs/command_input.hpp"
#include "menu.hpp"
#include "../darkside.hpp"
#include "../features/skins/skins.hpp"

// ============================================================================
// NEVERLOSE-STYLE MENU IMPLEMENTATION
// ============================================================================

static const char* g_tab_icons[] = {
    "R",  // Rage
    "L",  // Legit  
    "A",  // Anti-Aim
    "V",  // Visuals
    "M",  // Misc
    "S",  // Skins
    "C",  // Configs
    "JS"  // Scripts
};

static const char* g_tab_names[] = {
    "RAGE",
    "LEGIT",
    "ANTI-AIM",
    "VISUALS",
    "MISC",
    "SKINS",
    "CONFIGS",
    "SCRIPTS"
};

void c_menu::apply_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
    
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(8, 6);
    style.ItemSpacing = ImVec2(8, 8);
    style.ItemInnerSpacing = ImVec2(6, 6);
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 8.0f;
    
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    
    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg] = m_colors.background;
    colors[ImGuiCol_ChildBg] = m_colors.background_child;
    colors[ImGuiCol_PopupBg] = m_colors.popup_bg;
    colors[ImGuiCol_Border] = m_colors.border;
    colors[ImGuiCol_FrameBg] = m_colors.slider_bg;
    colors[ImGuiCol_FrameBgHovered] = m_colors.button_hover;
    colors[ImGuiCol_FrameBgActive] = m_colors.accent;
    colors[ImGuiCol_TitleBg] = m_colors.header;
    colors[ImGuiCol_TitleBgActive] = m_colors.header;
    colors[ImGuiCol_TitleBgCollapsed] = m_colors.header;
    colors[ImGuiCol_MenuBarBg] = m_colors.header;
    colors[ImGuiCol_ScrollbarBg] = m_colors.slider_bg;
    colors[ImGuiCol_ScrollbarGrab] = m_colors.button;
    colors[ImGuiCol_ScrollbarGrabHovered] = m_colors.button_hover;
    colors[ImGuiCol_ScrollbarGrabActive] = m_colors.accent;
    colors[ImGuiCol_CheckMark] = m_colors.accent;
    colors[ImGuiCol_SliderGrab] = m_colors.accent;
    colors[ImGuiCol_SliderGrabActive] = m_colors.accent_hover;
    colors[ImGuiCol_Button] = m_colors.button;
    colors[ImGuiCol_ButtonHovered] = m_colors.button_hover;
    colors[ImGuiCol_ButtonActive] = m_colors.accent;
    colors[ImGuiCol_Header] = m_colors.button;
    colors[ImGuiCol_HeaderHovered] = m_colors.button_hover;
    colors[ImGuiCol_HeaderActive] = m_colors.accent;
    colors[ImGuiCol_Separator] = m_colors.border;
    colors[ImGuiCol_SeparatorHovered] = m_colors.accent;
    colors[ImGuiCol_SeparatorActive] = m_colors.accent;
    colors[ImGuiCol_Text] = m_colors.text;
    colors[ImGuiCol_TextDisabled] = m_colors.text_disabled;
    colors[ImGuiCol_Tab] = m_colors.button;
    colors[ImGuiCol_TabHovered] = m_colors.accent_hover;
    colors[ImGuiCol_TabActive] = m_colors.accent;
}

void c_menu::draw() {
    if (!m_opened)
        return;

    apply_style();
    
    // Main window
    ImGui::SetNextWindowPos(m_window_pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(m_window_size);
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
                            ImGuiWindowFlags_NoBackground;
    
    ImGui::Begin("##darkside_menu", nullptr, flags);
    
    auto draw_list = ImGui::GetWindowDrawList();
    auto pos = ImGui::GetWindowPos();
    auto size = m_window_size;
    
    // Background with gradient
    draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
        ImGui::ColorConvertFloat4ToU32(m_colors.background), 8.0f);
    
    // Accent line at top
    draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + 3), 
        ImGui::ColorConvertFloat4ToU32(m_colors.accent), 8.0f, ImDrawFlags_RoundCornersTop);
    
    // Border
    draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
        ImGui::ColorConvertFloat4ToU32(m_colors.border), 8.0f);
    
    draw_header();
    draw_sidebar();
    draw_content();
    
    ImGui::End();
    
    // External overlays
    draw_watermark();
    draw_spectator_list();
    draw_keybinds_list();
}

void c_menu::draw_header() {
    auto draw_list = ImGui::GetWindowDrawList();
    auto pos = ImGui::GetWindowPos();
    
    // Header background
    draw_list->AddRectFilled(
        ImVec2(pos.x, pos.y + 3),
        ImVec2(pos.x + m_window_size.x, pos.y + 50),
        ImGui::ColorConvertFloat4ToU32(m_colors.header),
        8.0f, ImDrawFlags_RoundCornersTop
    );
    
    // Logo/Title
    ImGui::SetCursorPos(ImVec2(20, 15));
    ImGui::PushStyleColor(ImGuiCol_Text, m_colors.accent);
    ImGui::Text("DARKSIDE");
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    ImGui::SetCursorPosY(15);
    ImGui::PushStyleColor(ImGuiCol_Text, m_colors.text_disabled);
    ImGui::Text("v2.0 | CS2");
    ImGui::PopStyleColor();
    
    // User info on right
    ImGui::SameLine(m_window_size.x - 150);
    ImGui::SetCursorPosY(15);
    ImGui::PushStyleColor(ImGuiCol_Text, m_colors.success);
    ImGui::Text("User: premium");
    ImGui::PopStyleColor();
}

void c_menu::draw_sidebar() {
    auto draw_list = ImGui::GetWindowDrawList();
    auto pos = ImGui::GetWindowPos();
    
    // Sidebar background
    const float sidebar_width = 55.0f;
    draw_list->AddRectFilled(
        ImVec2(pos.x, pos.y + 50),
        ImVec2(pos.x + sidebar_width, pos.y + m_window_size.y),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.06f, 0.06f, 0.08f, 1.0f)),
        8.0f, ImDrawFlags_RoundCornersBottomLeft
    );
    
    // Tab buttons
    ImGui::SetCursorPos(ImVec2(5, 60));
    ImGui::BeginChild("##sidebar", ImVec2(sidebar_width - 5, m_window_size.y - 70), false);
    
    for (int i = 0; i < 8; i++) {
        menu::tab_id tab = static_cast<menu::tab_id>(i);
        bool is_selected = (m_current_tab == tab);
        
        // Animation
        m_tab_animation[i] = ImLerp(m_tab_animation[i], is_selected ? 1.0f : 0.0f, 0.15f);
        
        ImVec4 bg_color = ImLerp(m_colors.button, m_colors.accent, m_tab_animation[i]);
        
        ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImLerp(bg_color, m_colors.accent_hover, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, m_colors.accent);
        
        if (ImGui::Button(g_tab_icons[i], ImVec2(45, 40))) {
            m_current_tab = tab;
            // Reset subtab when changing tabs
            switch (tab) {
                case menu::tab_id::rage: m_current_subtab = menu::subtab_id::rage_main; break;
                case menu::tab_id::legit: m_current_subtab = menu::subtab_id::legit_main; break;
                case menu::tab_id::antiaim: m_current_subtab = menu::subtab_id::aa_angles; break;
                case menu::tab_id::visuals: m_current_subtab = menu::subtab_id::vis_players; break;
                case menu::tab_id::misc: m_current_subtab = menu::subtab_id::misc_movement; break;
                default: break;
            }
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", g_tab_names[i]);
            ImGui::EndTooltip();
        }
        
        ImGui::PopStyleColor(3);
    }
    
    ImGui::EndChild();
}

void c_menu::draw_content() {
    ImGui::SetCursorPos(ImVec2(65, 55));
    ImGui::BeginChild("##content", ImVec2(m_window_size.x - 75, m_window_size.y - 65), false);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
    
    switch (m_current_tab) {
        case menu::tab_id::rage: draw_rage_tab(); break;
        case menu::tab_id::legit: draw_legit_tab(); break;
        case menu::tab_id::antiaim: draw_antiaim_tab(); break;
        case menu::tab_id::visuals: draw_visuals_tab(); break;
        case menu::tab_id::misc: draw_misc_tab(); break;
        case menu::tab_id::skins: draw_skins_tab(); break;
        case menu::tab_id::configs: draw_configs_tab(); break;
        case menu::tab_id::scripts: draw_scripts_tab(); break;
    }
    
    ImGui::PopStyleVar();
    ImGui::EndChild();
}

// ============================================================================
// TAB IMPLEMENTATIONS
// ============================================================================

void c_menu::draw_rage_tab() {
    // Weapon group selector
    ImGui::BeginChild("##weapon_groups", ImVec2(0, 40), false);
    {
        for (int i = 0; i < WEAPON_GROUP_MAX; i++) {
            if (i > 0) ImGui::SameLine();
            
            bool selected = (g_cfg->rage_bot.m_selected_group == i);
            ImVec4 col = selected ? m_colors.accent : m_colors.button;
            
            ImGui::PushStyleColor(ImGuiCol_Button, col);
            
            // Короткие имена для кнопок
            const char* short_names[] = {"GEN", "PIST", "DEAG", "SMG", "RIFLE", "AUTO", "SCOUT", "AWP", "SHOT", "LMG"};
            
            if (ImGui::Button(short_names[i], ImVec2(55, 30))) {
                g_cfg->rage_bot.m_selected_group = i;
            }
            
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", weapon_group_names[i]);
                ImGui::EndTooltip();
            }
            
            ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // Получаем текущий конфиг группы
    int group = g_cfg->rage_bot.m_selected_group;
    auto& weapon_cfg = g_cfg->rage_bot.m_weapon_config[group];
    
    ImGui::BeginChild("##rage_content", ImVec2(0, 0), false);
    ImGui::Columns(2, nullptr, false);
    
    // ============ LEFT COLUMN ============
    
    // Global settings (только для General)
    if (group == WEAPON_GROUP_GENERAL) {
        group_box("Global", ImVec2(0, 100), [this]() {
            checkbox_ex("Enabled", &g_cfg->rage_bot.m_enabled);
            checkbox_ex("Silent Aim", &g_cfg->rage_bot.m_silent);
        });
    } else {
        // Override checkbox для других групп
        group_box("Weapon Config", ImVec2(0, 60), [&weapon_cfg]() {
            ImGui::Checkbox("Override General Settings", &weapon_cfg.m_override_general);
        });
    }
    
    group_box("Aimbot", ImVec2(0, 280), [this, &weapon_cfg]() {
        static const char* hitboxes[] = {"Head", "Chest", "Stomach", "Arms", "Legs", "Feet"};
        
        checkbox_ex("Enabled", &weapon_cfg.m_enabled);
        checkbox_ex("Auto Fire", &weapon_cfg.m_auto_fire);
        checkbox_ex("Auto Stop", &weapon_cfg.m_auto_stop);
        
        if (weapon_cfg.m_auto_stop) {
            static const char* stop_modes[] = {"Full Stop", "Minimal", "Between Shots"};
            combo_ex("Stop Mode", &weapon_cfg.m_auto_stop_mode, stop_modes, 3);
        }
        
        checkbox_ex("Auto Scope", &weapon_cfg.m_auto_scope);
        checkbox_ex("Delay Shot", &weapon_cfg.m_delay_shot);
        
        separator_ex("Hitboxes");
        
        multi_combo_ex("Hitboxes", weapon_cfg.m_hitboxes, hitboxes, 6);
        multi_combo_ex("Multi-Points", weapon_cfg.m_multi_points, hitboxes, 6);
    });
    
    ImGui::NextColumn();
    
    // ============ RIGHT COLUMN ============
    
    group_box("Accuracy", ImVec2(0, 220), [this, &weapon_cfg]() {
        slider_int_ex("Hit Chance", &weapon_cfg.m_hit_chance, 1, 100, "%d%%");
        slider_int_ex("Min Damage", &weapon_cfg.m_minimum_damage, 1, 100);
        slider_int_ex("Override Dmg", &weapon_cfg.m_minimum_damage_override, 1, 100);
        
        keybind_ex("Override Key", &g_cfg->rage_bot.m_override_damage_key_bind, 
                   &g_cfg->rage_bot.m_override_damage_key_bind_style);
        
        separator_ex();
        
        checkbox_ex("Static Point Scale", &weapon_cfg.m_static_scale);
        
        if (weapon_cfg.m_static_scale) {
            static const char* hitbox_names[] = {"Head", "Chest", "Stomach", "Arms", "Legs", "Feet"};
            for (int i = 0; i < 6; i++) {
                if (weapon_cfg.m_multi_points[i]) {
                    slider_int_ex(hitbox_names[i], &weapon_cfg.m_hitbox_scale[i], 1, 100, "%d%%");
                }
            }
        }
    });
    
    group_box("Safety", ImVec2(0, 180), [this, &weapon_cfg]() {
        checkbox_ex("Prefer Safe Point", &weapon_cfg.m_prefer_safe);
        checkbox_ex("Prefer Body Aim", &weapon_cfg.m_prefer_body);
        checkbox_ex("Body Aim if Lethal", &weapon_cfg.m_prefer_baim_lethal);
        slider_int_ex("BAIM After X Misses", &weapon_cfg.m_baim_after_x_shots, 0, 5);
    });
    
    group_box("Exploits", ImVec2(0, 150), [this]() {
        checkbox_ex("Double Tap", &g_cfg->rage_bot.m_double_tap);
        if (g_cfg->rage_bot.m_double_tap) {
            keybind_ex("DT Key", &g_cfg->rage_bot.m_double_tap_key, &g_cfg->rage_bot.m_double_tap_key_style);
        }
        
        checkbox_ex("Hide Shots", &g_cfg->rage_bot.m_hide_shots);
        if (g_cfg->rage_bot.m_hide_shots) {
            keybind_ex("HS Key", &g_cfg->rage_bot.m_hide_shots_key, &g_cfg->rage_bot.m_hide_shots_key_style);
        }
    });
    
    ImGui::Columns(1);
    ImGui::EndChild();
}

void c_menu::draw_legit_tab() {
    ImGui::Columns(2, nullptr, false);
    
    group_box("Aimbot", ImVec2(0, 300), [this]() {
        checkbox_ex("Enabled", &g_cfg->legit_bot.m_enabled);
        
        static const char* targets[] = {"Closest to Crosshair", "Lowest Health", "Closest Distance"};
        combo_ex("Target Selection", &g_cfg->legit_bot.m_target_selection, targets, IM_ARRAYSIZE(targets));
        
        slider_float_ex("FOV", &g_cfg->legit_bot.m_fov, 0.1f, 30.0f, "%.1f°");
        slider_float_ex("Smooth", &g_cfg->legit_bot.m_smooth, 1.0f, 50.0f);
        
        separator_ex();
        
        static const char* hitboxes[] = {"Head", "Neck", "Chest", "Stomach"};
        multi_combo_ex("Hitboxes", g_cfg->legit_bot.m_hitboxes, hitboxes, 4);
        
        checkbox_ex("Aim at Backtrack", &g_cfg->legit_bot.m_aim_backtrack);
        checkbox_ex("Silent (Risky)", &g_cfg->legit_bot.m_silent);
    });
    
    ImGui::NextColumn();
    
    group_box("Triggerbot", ImVec2(0, 200), [this]() {
        checkbox_ex("Enabled", &g_cfg->legit_bot.m_triggerbot_enabled);
        keybind_ex("Key", &g_cfg->legit_bot.m_triggerbot_key, &g_cfg->legit_bot.m_triggerbot_key_style);
        
        slider_int_ex("Delay", &g_cfg->legit_bot.m_triggerbot_delay, 0, 200, "%dms");
        slider_int_ex("Hit Chance", &g_cfg->legit_bot.m_triggerbot_hitchance, 1, 100, "%d%%");
        
        checkbox_ex("Head Only", &g_cfg->legit_bot.m_triggerbot_head_only);
        checkbox_ex("Magnet", &g_cfg->legit_bot.m_triggerbot_magnet);
    });
    
    group_box("Backtrack", ImVec2(0, 150), [this]() {
        checkbox_ex("Enabled", &g_cfg->legit_bot.m_backtrack_enabled);
        slider_int_ex("Max Ticks", &g_cfg->legit_bot.m_backtrack_ticks, 1, 12);
        checkbox_ex("Visualize", &g_cfg->legit_bot.m_backtrack_visualize);
    });
    
    ImGui::Columns(1);
}

void c_menu::draw_antiaim_tab() {
    // Subtabs
    const char* subtabs[] = {"Angles", "Desync", "Fakelag", "Conditions"};
    static int current_subtab = 0;
    
    ImGui::BeginChild("##aa_subtabs", ImVec2(0, 35), false);
    for (int i = 0; i < 4; i++) {
        if (i > 0) ImGui::SameLine();
        
        ImVec4 col = (current_subtab == i) ? m_colors.accent : m_colors.button;
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        
        if (ImGui::Button(subtabs[i], ImVec2(100, 28))) {
            current_subtab = i;
        }
        
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::BeginChild("##aa_content", ImVec2(0, 0), false);
    
    if (current_subtab == 0) {
        // ========== ANGLES ==========
        ImGui::Columns(2, nullptr, false);
        
        group_box("General", ImVec2(0, 80), [this]() {
            checkbox_ex("Enabled", &g_cfg->anti_hit.m_enabled);
        });
        
        group_box("Pitch", ImVec2(0, 150), [this]() {
            static const char* pitch_modes[] = {"Off", "Down", "Up", "Zero", "Custom"};
            combo_ex("Mode", &g_cfg->anti_hit.m_pitch_mode, pitch_modes, 5);
            
            if (g_cfg->anti_hit.m_pitch_mode == 4) {
                slider_int_ex("Custom Angle", &g_cfg->anti_hit.m_pitch_amount, -89, 89);
            }
        });
        
        group_box("Yaw Base", ImVec2(0, 180), [this]() {
            static const char* yaw_modes[] = {"Off", "Backward", "Sideways", "Spin", "At Target", "Custom"};
            combo_ex("Mode", &g_cfg->anti_hit.m_yaw_mode, yaw_modes, 6);
            
            checkbox_ex("At Target", &g_cfg->anti_hit.m_at_target);
            slider_int_ex("Yaw Add", &g_cfg->anti_hit.m_yaw_amount, -180, 180);
        });
        
        ImGui::NextColumn();
        
        group_box("Jitter", ImVec2(0, 180), [this]() {
            checkbox_ex("Enabled", &g_cfg->anti_hit.m_jitter_enabled);
            
            if (g_cfg->anti_hit.m_jitter_enabled) {
                static const char* jitter_modes[] = {"Offset", "Center", "Random"};
                combo_ex("Mode", &g_cfg->anti_hit.m_jitter_mode, jitter_modes, 3);
                slider_int_ex("Range", &g_cfg->anti_hit.m_jitter_amount, 0, 180);
            }
        });
        
        group_box("Manual AA", ImVec2(0, 150), [this]() {
            ImGui::TextColored(m_colors.text_disabled, "Manual anti-aim direction");
            ImGui::TextColored(m_colors.text_disabled, "Use arrow keys during game");
        });
        
        ImGui::Columns(1);
    }
    else if (current_subtab == 1) {
        // ========== DESYNC ==========
        ImGui::Columns(2, nullptr, false);
        
        group_box("Desync", ImVec2(0, 280), [this]() {
            checkbox_ex("Enabled", &g_cfg->anti_hit.m_desync_enabled);
            
            if (g_cfg->anti_hit.m_desync_enabled) {
                static const char* desync_modes[] = {"Static", "Jitter", "Switch"};
                combo_ex("Mode", &g_cfg->anti_hit.m_desync_mode, desync_modes, 3);
                
                separator_ex("Limits");
                
                slider_int_ex("Left Limit", &g_cfg->anti_hit.m_desync_left, 0, 58);
                slider_int_ex("Right Limit", &g_cfg->anti_hit.m_desync_right, 0, 58);
                
                separator_ex();
                
                keybind_ex("Flip Key", &g_cfg->anti_hit.m_flip_key, &g_cfg->anti_hit.m_flip_key_style);
            }
        });
        
        ImGui::NextColumn();
        
        group_box("Anti Bruteforce", ImVec2(0, 150), [this]() {
            checkbox_ex("Enabled", &g_cfg->anti_hit.m_anti_bruteforce);
            
            if (g_cfg->anti_hit.m_anti_bruteforce) {
                static const char* bf_modes[] = {"Switch Side", "Random"};
                combo_ex("Mode", &g_cfg->anti_hit.m_anti_bruteforce_mode, bf_modes, 2);
            }
        });
        
        group_box("Info", ImVec2(0, 100), [this]() {
            ImGui::TextColored(m_colors.accent, "Real: %.1f", g_anti_hit->get_real_yaw());
            ImGui::TextColored(m_colors.warning, "Fake: %.1f", g_anti_hit->get_fake_yaw());
            ImGui::TextColored(m_colors.success, "Desync: %.1f", g_anti_hit->get_desync_amount());
        });
        
        ImGui::Columns(1);
    }
    else if (current_subtab == 2) {
        // ========== FAKELAG ==========
        ImGui::Columns(2, nullptr, false);
        
        group_box("Fakelag", ImVec2(0, 250), [this]() {
            checkbox_ex("Enabled", &g_cfg->anti_hit.m_fakelag_enabled);
            
            if (g_cfg->anti_hit.m_fakelag_enabled) {
                static const char* fakelag_modes[] = {"Static", "Random", "Adaptive"};
                combo_ex("Mode", &g_cfg->anti_hit.m_fakelag_mode, fakelag_modes, 3);
                
                slider_int_ex("Limit", &g_cfg->anti_hit.m_fakelag_limit, 1, 14);
                
                separator_ex("Conditions");
                
                checkbox_ex("On Peek", &g_cfg->anti_hit.m_fakelag_on_peek);
                checkbox_ex("On Ground Only", &g_cfg->anti_hit.m_fakelag_on_ground_only);
            }
        });
        
        ImGui::NextColumn();
        
        group_box("Info", ImVec2(0, 150), [this]() {
            ImGui::TextColored(m_colors.text_disabled, "Fakelag chokes packets to");
            ImGui::TextColored(m_colors.text_disabled, "make you harder to hit.");
            ImGui::Spacing();
            ImGui::TextColored(m_colors.warning, "Max 14 ticks in CS2");
        });
        
        ImGui::Columns(1);
    }
    else if (current_subtab == 3) {
        // ========== CONDITIONS ==========
        static int selected_condition = 0;
        const char* conditions[] = {"Standing", "Moving", "In Air", "Slow Walk"};
        
        ImGui::BeginChild("##cond_select", ImVec2(150, 0), true);
        for (int i = 0; i < 4; i++) {
            if (ImGui::Selectable(conditions[i], selected_condition == i)) {
                selected_condition = i;
            }
        }
        ImGui::EndChild();
        
        ImGui::SameLine();
        
        ImGui::BeginChild("##cond_settings", ImVec2(0, 0), false);
        
        anti_hit_t::condition_t* cond = nullptr;
        switch (selected_condition) {
            case 0: cond = &g_cfg->anti_hit.m_standing; break;
            case 1: cond = &g_cfg->anti_hit.m_moving; break;
            case 2: cond = &g_cfg->anti_hit.m_air; break;
            case 3: cond = &g_cfg->anti_hit.m_slow_walk; break;
        }
        
        if (cond) {
            group_box(conditions[selected_condition], ImVec2(0, 300), [this, cond]() {
                checkbox_ex("Override Default", &cond->m_override);
                
                if (cond->m_override) {
                    separator_ex();
                    
                    static const char* pitch_modes[] = {"Off", "Down", "Up", "Zero", "Custom"};
                    combo_ex("Pitch", &cond->m_pitch_mode, pitch_modes, 5);
                    
                    static const char* yaw_modes[] = {"Off", "Backward", "Sideways", "Spin", "At Target"};
                    combo_ex("Yaw", &cond->m_yaw_mode, yaw_modes, 5);
                    
                    slider_int_ex("Yaw Add", &cond->m_yaw_amount, -180, 180);
                    
                    checkbox_ex("Jitter", &cond->m_jitter);
                    if (cond->m_jitter) {
                        slider_int_ex("Jitter Range", &cond->m_jitter_amount, 0, 180);
                    }
                }
            });
        }
        
        ImGui::EndChild();
    }
    
    ImGui::EndChild();
}

void c_menu::draw_visuals_tab() {
    const char* subtabs[] = {"Players", "World", "Effects", "Other"};
    
    ImGui::BeginChild("##vis_subtabs", ImVec2(0, 35), false);
    for (int i = 0; i < 4; i++) {
        if (i > 0) ImGui::SameLine();
        
        int subtab_idx = static_cast<int>(menu::subtab_id::vis_players) + i;
        bool selected = (static_cast<int>(m_current_subtab) == subtab_idx);
        
        ImVec4 col = selected ? m_colors.accent : m_colors.button;
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        
        if (ImGui::Button(subtabs[i], ImVec2(100, 28))) {
            m_current_subtab = static_cast<menu::subtab_id>(subtab_idx);
        }
        
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::BeginChild("##vis_content", ImVec2(0, 0), false);
    
    if (m_current_subtab == menu::subtab_id::vis_players) {
        ImGui::Columns(2, nullptr, false);
        
        group_box("Enemy ESP", ImVec2(0, 350), [this]() {
            checkbox_ex("Enabled", &g_cfg->visuals.m_player_esp.m_enabled);
            checkbox_ex("Show Teammates", &g_cfg->visuals.m_player_esp.m_teammates);
            
            separator_ex("Elements");
            
            checkbox_ex("Bounding Box", &g_cfg->visuals.m_player_esp.m_bounding_box);
            checkbox_ex("Health Bar", &g_cfg->visuals.m_player_esp.m_health_bar);
            checkbox_ex("Name", &g_cfg->visuals.m_player_esp.m_name);
            checkbox_ex("Weapon", &g_cfg->visuals.m_player_esp.m_weapon);
            checkbox_ex("Ammo Bar", &g_cfg->visuals.m_player_esp.m_ammo_bar);
            checkbox_ex("Flags", &g_cfg->visuals.m_player_esp.m_flags);
            checkbox_ex("Skeleton", &g_cfg->visuals.m_player_esp.m_skeleton);
            checkbox_ex("Distance", &g_cfg->visuals.m_player_esp.m_distance);
            
            separator_ex("Colors");
            
            color_picker_ex("Name Color", reinterpret_cast<float*>(&g_cfg->visuals.m_player_esp.m_name_color));
            color_picker_ex("Box Color", reinterpret_cast<float*>(&g_cfg->visuals.m_player_esp.m_box_color));
        });
        
        ImGui::NextColumn();
        
        group_box("Chams", ImVec2(0, 200), [this]() {
            checkbox_ex("Enabled", &g_cfg->visuals.m_chams.m_enabled);
            
            static const char* materials[] = {"Flat", "Textured", "Glow", "Metallic", "Crystal"};
            combo_ex("Material", &g_cfg->visuals.m_chams.m_material, materials, IM_ARRAYSIZE(materials));
            
            color_picker_ex("Visible Color", reinterpret_cast<float*>(&g_cfg->visuals.m_chams.m_visible_color));
            color_picker_ex("Hidden Color", reinterpret_cast<float*>(&g_cfg->visuals.m_chams.m_hidden_color));
            
            checkbox_ex("XQZ (Through Walls)", &g_cfg->visuals.m_chams.m_xqz);
        });
        
        group_box("Glow", ImVec2(0, 150), [this]() {
            checkbox_ex("Enabled", &g_cfg->visuals.m_glow.m_enabled);
            
            static const char* styles[] = {"Outer", "Inner", "Pulse"};
            combo_ex("Style", &g_cfg->visuals.m_glow.m_style, styles, IM_ARRAYSIZE(styles));
            
            color_picker_ex("Color", reinterpret_cast<float*>(&g_cfg->visuals.m_glow.m_color));
        });
        
        ImGui::Columns(1);
    }
    else if (m_current_subtab == menu::subtab_id::vis_world) {
        ImGui::Columns(2, nullptr, false);
        
        group_box("World", ImVec2(0, 300), [this]() {
            checkbox_ex("Custom Skybox", &g_cfg->world.m_custom_skybox);
            color_picker_ex("Sky Color", reinterpret_cast<float*>(&g_cfg->world.m_sky));
            
            checkbox_ex("Night Mode", &g_cfg->world.m_night_mode);
            slider_float_ex("Brightness", &g_cfg->world.m_brightness, 0.0f, 1.0f);
            
            checkbox_ex("Remove Fog", &g_cfg->world.m_render_fog);
            checkbox_ex("Remove Smoke", &g_cfg->world.m_remove_smoke);
            checkbox_ex("Remove Flash", &g_cfg->world.m_remove_flash);
            
            color_picker_ex("Wall Color", reinterpret_cast<float*>(&g_cfg->world.m_wall));
        });
        
        ImGui::NextColumn();
        
        group_box("View", ImVec2(0, 250), [this]() {
            checkbox_ex("Third Person", &g_cfg->world_esp.m_enable_thirdperson);
            keybind_ex("Key", &g_cfg->world_esp.m_thirdperson_key_bind, &g_cfg->world_esp.m_thirdperson_key_bind_style);
            slider_int_ex("Distance", &g_cfg->world_esp.m_distance, 50, 300);
            
            separator_ex();
            
            slider_int_ex("FOV", &g_cfg->world.m_fov, 0, 40);
            slider_int_ex("Viewmodel FOV", &g_cfg->world.m_viewmodel_fov, 0, 40);
        });
        
        ImGui::Columns(1);
    }
    else if (m_current_subtab == menu::subtab_id::vis_effects) {
        ImGui::Columns(2, nullptr, false);
        
        group_box("Effects", ImVec2(0, 250), [this]() {
            checkbox_ex("Hitmarker", &g_cfg->visuals.m_effects.m_hitmarker);
            checkbox_ex("Hitmarker Sound", &g_cfg->visuals.m_effects.m_hitmarker_sound);
            checkbox_ex("Kill Effect", &g_cfg->visuals.m_effects.m_kill_effect);
            
            separator_ex();
            
            checkbox_ex("Bullet Tracers", &g_cfg->visuals.m_effects.m_bullet_tracers);
            color_picker_ex("Tracer Color", reinterpret_cast<float*>(&g_cfg->visuals.m_effects.m_tracer_color));
            
            checkbox_ex("Bullet Impacts", &g_cfg->visuals.m_effects.m_bullet_impacts);
        });
        
        ImGui::NextColumn();
        
        group_box("Indicators", ImVec2(0, 200), [this]() {
            checkbox_ex("Crosshair", &g_cfg->visuals.m_indicators.m_crosshair);
            checkbox_ex("Spread Circle", &g_cfg->visuals.m_indicators.m_spread_circle);
            checkbox_ex("Anti-Aim Lines", &g_cfg->visuals.m_indicators.m_aa_lines);
            checkbox_ex("Desync Indicator", &g_cfg->visuals.m_indicators.m_desync);
        });
        
        ImGui::Columns(1);
    }
    
    ImGui::EndChild();
}

void c_menu::draw_misc_tab() {
    // Subtabs
    const char* subtabs[] = {"Movement", "Other", "Buy Bot"};
    static int current_subtab = 0;
    
    ImGui::BeginChild("##misc_subtabs", ImVec2(0, 35), false);
    for (int i = 0; i < 3; i++) {
        if (i > 0) ImGui::SameLine();
        
        ImVec4 col = (current_subtab == i) ? m_colors.accent : m_colors.button;
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        
        if (ImGui::Button(subtabs[i], ImVec2(100, 28))) {
            current_subtab = i;
        }
        
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::BeginChild("##misc_content", ImVec2(0, 0), false);
    
    if (current_subtab == 0) {
        // ========== MOVEMENT ==========
        ImGui::Columns(2, nullptr, false);
        
        group_box("Basic Movement", ImVec2(0, 200), [this]() {
            checkbox_ex("Bunny Hop", &g_cfg->misc.m_bunny_hop);
            
            checkbox_ex("Auto Strafe", &g_cfg->misc.m_auto_strafe);
            
            if (g_cfg->misc.m_auto_strafe) {
                static const char* strafe_types[] = {"Legit", "Directional", "Rage"};
                combo_ex("Type", &g_cfg->misc.m_strafe_type, strafe_types, 3);
                slider_int_ex("Smooth", &g_cfg->misc.m_strafe_smooth, 1, 99, "%d%%");
            }
        });
        
        group_box("Advanced", ImVec2(0, 250), [this]() {
            checkbox_ex("Edge Jump", &g_cfg->misc.m_edge_jump);
            if (g_cfg->misc.m_edge_jump) {
                keybind_ex("Key##ej", &g_cfg->misc.m_edge_jump_key, &g_cfg->misc.m_edge_jump_key_style);
            }
            
            separator_ex();
            
            checkbox_ex("Jump Bug", &g_cfg->misc.m_jump_bug);
            if (g_cfg->misc.m_jump_bug) {
                keybind_ex("Key##jb", &g_cfg->misc.m_jump_bug_key, &g_cfg->misc.m_jump_bug_key_style);
            }
            
            separator_ex();
            
            checkbox_ex("Edge Bug", &g_cfg->misc.m_edge_bug);
            if (g_cfg->misc.m_edge_bug) {
                keybind_ex("Key##eb", &g_cfg->misc.m_edge_bug_key, &g_cfg->misc.m_edge_bug_key_style);
            }
            
            separator_ex();
            
            checkbox_ex("Long Jump", &g_cfg->misc.m_long_jump);
            if (g_cfg->misc.m_long_jump) {
                keybind_ex("Key##lj", &g_cfg->misc.m_long_jump_key, &g_cfg->misc.m_long_jump_key_style);
            }
        });
        
        ImGui::NextColumn();
        
        group_box("Other Movement", ImVec2(0, 180), [this]() {
            checkbox_ex("Infinite Duck", &g_cfg->misc.m_infinite_duck);
            checkbox_ex("Fast Duck", &g_cfg->misc.m_fast_duck);
            checkbox_ex("Slide Walk", &g_cfg->misc.m_slide_walk);
            
            separator_ex();
            
            checkbox_ex("Slow Walk", &g_cfg->misc.m_slow_walk);
            if (g_cfg->misc.m_slow_walk) {
                keybind_ex("Key##sw", &g_cfg->misc.m_slow_walk_key, &g_cfg->misc.m_slow_walk_key_style);
                slider_int_ex("Speed", &g_cfg->misc.m_slow_walk_speed, 1, 100, "%d%%");
            }
        });
        
        group_box("Info", ImVec2(0, 150), [this]() {
            ImGui::TextColored(m_colors.text_disabled, "Movement Tips:");
            ImGui::BulletText("Edge Jump: Auto-jump at edges");
            ImGui::BulletText("Jump Bug: No fall damage");
            ImGui::BulletText("Edge Bug: Stick to edges");
            ImGui::BulletText("LJ: Optimal long jump");
        });
        
        ImGui::Columns(1);
    }
    else if (current_subtab == 1) {
        // ========== OTHER ==========
        ImGui::Columns(2, nullptr, false);
        
        group_box("Features", ImVec2(0, 200), [this]() {
            checkbox_ex("Reveal Ranks", &g_cfg->misc.m_reveal_ranks);
            checkbox_ex("Clan Tag", &g_cfg->misc.m_clan_tag);
            checkbox_ex("Auto Accept", &g_cfg->misc.m_auto_accept);
        });
        
        group_box("HUD", ImVec2(0, 150), [this]() {
            checkbox_ex("Watermark", &g_cfg->misc.m_watermark);
            checkbox_ex("Spectator List", &g_cfg->misc.m_spectator_list);
            checkbox_ex("Keybinds List", &g_cfg->misc.m_keybinds_list);
        });
        
        ImGui::NextColumn();
        
        group_box("Menu", ImVec2(0, 150), [this]() {
            ImGui::TextColored(m_colors.text_disabled, "Menu Key: INSERT");
            ImGui::TextColored(m_colors.text_disabled, "Panic Key: END");
        });
        
        ImGui::Columns(1);
    }
    else if (current_subtab == 2) {
        // ========== BUY BOT ==========
        ImGui::Columns(2, nullptr, false);
        
        group_box("Buy Bot", ImVec2(0, 300), [this]() {
            checkbox_ex("Enabled", &g_cfg->misc.m_buy_bot_enabled);
            
            if (g_cfg->misc.m_buy_bot_enabled) {
                separator_ex("Weapons");
                
                static const char* primary[] = {"Off", "Auto", "AWP", "Scout", "AK/M4", "Galil/Famas"};
                combo_ex("Primary", &g_cfg->misc.m_buy_bot_primary, primary, 6);
                
                static const char* secondary[] = {"Off", "Deagle", "Dualies", "Five-Seven/Tec9", "P250"};
                combo_ex("Secondary", &g_cfg->misc.m_buy_bot_secondary, secondary, 5);
                
                separator_ex("Equipment");
                
                checkbox_ex("Armor + Helmet", &g_cfg->misc.m_buy_bot_armor);
                checkbox_ex("Defuse Kit", &g_cfg->misc.m_buy_bot_defuser);
                checkbox_ex("Grenades", &g_cfg->misc.m_buy_bot_grenades);
            }
        });
        
        ImGui::NextColumn();
        
        group_box("Info", ImVec2(0, 150), [this]() {
            ImGui::TextColored(m_colors.text_disabled, "Buy bot will automatically");
            ImGui::TextColored(m_colors.text_disabled, "purchase items at round start.");
        });
        
        ImGui::Columns(1);
    }
    
    ImGui::EndChild();
}

void c_menu::draw_skins_tab() {
    ImGui::TextColored(m_colors.warning, "Skin Changer - Coming Soon");
    ImGui::TextColored(m_colors.text_disabled, "This feature is under development");
}

void c_menu::draw_configs_tab() {
    ImGui::Columns(2, nullptr, false);
    
    group_box("Configs", ImVec2(0, 350), [this]() {
        static int selected_config = 0;
        static char config_name[64] = "";
        
        ImGui::InputText("##config_name", config_name, 64);
        ImGui::SameLine();
        if (button_ex("Create")) {
            // Create config
        }
        
        ImGui::BeginChild("##config_list", ImVec2(-1, 200), true);
        
        const char* configs[] = {"default", "legit", "rage", "hvh"};
        for (int i = 0; i < 4; i++) {
            if (ImGui::Selectable(configs[i], selected_config == i)) {
                selected_config = i;
            }
        }
        
        ImGui::EndChild();
        
        if (button_ex("Load", ImVec2(100, 30))) {
            g_config_system->load(configs[selected_config]);
        }
        ImGui::SameLine();
        if (button_ex("Save", ImVec2(100, 30))) {
            g_config_system->save(configs[selected_config]);
        }
        ImGui::SameLine();
        if (button_ex("Delete", ImVec2(100, 30))) {
            // Delete config
        }
    });
    
    ImGui::NextColumn();
    
    group_box("Import/Export", ImVec2(0, 150), [this]() {
        if (button_ex("Import from Clipboard", ImVec2(-1, 30))) {
            // Import
        }
        if (button_ex("Export to Clipboard", ImVec2(-1, 30))) {
            // Export
        }
    });
    
    ImGui::Columns(1);
}

void c_menu::draw_scripts_tab() {
    ImGui::TextColored(m_colors.warning, "Lua Scripts - Coming Soon");
    ImGui::TextColored(m_colors.text_disabled, "JavaScript/Lua scripting support planned");
}

// ============================================================================
// CUSTOM WIDGETS
// ============================================================================

bool c_menu::checkbox_ex(const char* label, bool* v) {
    ImGui::PushID(label);
    
    bool changed = false;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    float height = ImGui::GetFrameHeight();
    float width = height * 0.55f;
    
    if (ImGui::InvisibleButton("##cb", ImVec2(ImGui::GetContentRegionAvail().x, height))) {
        *v = !*v;
        changed = true;
    }
    
    bool hovered = ImGui::IsItemHovered();
    
    // Checkbox background
    ImU32 bg_color = ImGui::ColorConvertFloat4ToU32(*v ? m_colors.accent : m_colors.checkbox_bg);
    draw_list->AddRectFilled(
        ImVec2(pos.x, pos.y + (height - width) * 0.5f),
        ImVec2(pos.x + width, pos.y + (height + width) * 0.5f),
        bg_color, 3.0f
    );
    
    // Checkmark
    if (*v) {
        float pad = 3.0f;
        draw_list->AddLine(
            ImVec2(pos.x + pad, pos.y + height * 0.5f),
            ImVec2(pos.x + width * 0.4f, pos.y + height * 0.5f + pad),
            IM_COL32(255, 255, 255, 255), 2.0f
        );
        draw_list->AddLine(
            ImVec2(pos.x + width * 0.4f, pos.y + height * 0.5f + pad),
            ImVec2(pos.x + width - pad, pos.y + height * 0.5f - pad),
            IM_COL32(255, 255, 255, 255), 2.0f
        );
    }
    
    // Label
    ImGui::SameLine(0, width + 8);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (height - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::TextColored(hovered ? m_colors.text : m_colors.text_disabled, "%s", label);
    
    ImGui::PopID();
    return changed;
}

bool c_menu::slider_int_ex(const char* label, int* v, int v_min, int v_max, const char* format) {
    ImGui::PushID(label);
    
    ImGui::Text("%s", label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150);
    
    char buf[64];
    snprintf(buf, 64, format, *v);
    
    ImGui::SetNextItemWidth(120);
    bool changed = ImGui::SliderInt("##slider", v, v_min, v_max, buf);
    
    ImGui::PopID();
    return changed;
}

bool c_menu::slider_float_ex(const char* label, float* v, float v_min, float v_max, const char* format) {
    ImGui::PushID(label);
    
    ImGui::Text("%s", label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150);
    
    ImGui::SetNextItemWidth(120);
    bool changed = ImGui::SliderFloat("##slider", v, v_min, v_max, format);
    
    ImGui::PopID();
    return changed;
}

bool c_menu::combo_ex(const char* label, int* current_item, const char* const items[], int items_count) {
    ImGui::PushID(label);
    
    ImGui::Text("%s", label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150);
    
    ImGui::SetNextItemWidth(120);
    bool changed = ImGui::Combo("##combo", current_item, items, items_count);
    
    ImGui::PopID();
    return changed;
}

bool c_menu::multi_combo_ex(const char* label, bool* values, const char* const items[], int items_count) {
    ImGui::PushID(label);
    
    ImGui::Text("%s", label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150);
    
    // Build preview string
    std::string preview;
    int count = 0;
    for (int i = 0; i < items_count; i++) {
        if (values[i]) {
            if (count > 0) preview += ", ";
            preview += items[i];
            count++;
        }
    }
    if (preview.empty()) preview = "None";
    if (preview.length() > 15) preview = preview.substr(0, 12) + "...";
    
    ImGui::SetNextItemWidth(120);
    if (ImGui::BeginCombo("##combo", preview.c_str())) {
        for (int i = 0; i < items_count; i++) {
            ImGui::Selectable(items[i], &values[i], ImGuiSelectableFlags_DontClosePopups);
        }
        ImGui::EndCombo();
    }
    
    ImGui::PopID();
    return false;
}

bool c_menu::keybind_ex(const char* label, int* key, int* mode) {
    ImGui::PushID(label);
    
    ImGui::Text("%s", label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150);
    
    static std::unordered_map<void*, bool> waiting_map;
    bool& waiting = waiting_map[key];
    
    const char* key_names[] = {
        "None", "Mouse1", "Mouse2", "Cancel", "Mouse3", "Mouse4", "Mouse5",
        "?", "Backspace", "Tab", "?", "?", "Clear", "Enter", "?", "?",
        "Shift", "Ctrl", "Alt", "Pause", "Caps", "?", "?", "?", "?", "?", "?",
        "Esc", "?", "?", "?", "?", "Space", "PgUp", "PgDn", "End", "Home",
        "Left", "Up", "Right", "Down", "?", "?", "?", "?", "Ins", "Del"
    };
    
    const char* display_name = "None";
    if (*key > 0 && *key < sizeof(key_names)/sizeof(key_names[0])) {
        display_name = key_names[*key];
    } else if (*key >= 'A' && *key <= 'Z') {
        static char buf[2];
        buf[0] = static_cast<char>(*key);
        buf[1] = '\0';
        display_name = buf;
    }
    
    char button_label[64];
    snprintf(button_label, 64, "%s##btn", waiting ? "..." : display_name);
    
    ImGui::SetNextItemWidth(80);
    if (ImGui::Button(button_label, ImVec2(80, 0))) {
        waiting = true;
    }
    
    if (waiting) {
        for (int i = 0; i < 256; i++) {
            if (GetAsyncKeyState(i) & 0x8000) {
                if (i == VK_ESCAPE) {
                    *key = 0;
                } else {
                    *key = i;
                }
                waiting = false;
                break;
            }
        }
    }
    
    ImGui::SameLine();
    
    const char* modes[] = {"Always", "Hold", "Toggle", "Off"};
    ImGui::SetNextItemWidth(60);
    ImGui::Combo("##mode", mode, modes, 4);
    
    ImGui::PopID();
    return false;
}

bool c_menu::color_picker_ex(const char* label, float* col) {
    ImGui::PushID(label);
    
    ImGui::Text("%s", label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150);
    
    bool changed = ImGui::ColorEdit4("##color", col, 
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
    
    ImGui::PopID();
    return changed;
}

bool c_menu::button_ex(const char* label, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button, m_colors.accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, m_colors.accent_hover);
    bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(2);
    return pressed;
}

void c_menu::group_box(const char* label, const ImVec2& size, std::function<void()> content) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_colors.background_child);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    
    ImGui::BeginChild(label, size, true);
    
    // Header
    ImGui::PushStyleColor(ImGuiCol_Text, m_colors.accent);
    ImGui::Text("%s", label);
    ImGui::PopStyleColor();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    content();
    
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    
    ImGui::Spacing();
}

void c_menu::separator_ex(const char* label) {
    if (label) {
        ImGui::Spacing();
        ImGui::TextColored(m_colors.text_disabled, "%s", label);
    }
    ImGui::Separator();
    ImGui::Spacing();
}

void c_menu::tooltip(const char* text) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", text);
        ImGui::EndTooltip();
    }
}

// ============================================================================
// OVERLAYS
// ============================================================================

void c_menu::draw_watermark() {
    if (!g_cfg->misc.m_watermark)
        return;
    
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowBgAlpha(0.8f);
    
    ImGui::Begin("##watermark", nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);
    
    ImGui::PushStyleColor(ImGuiCol_Text, m_colors.accent);
    ImGui::Text("DARKSIDE");
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    ImGui::TextColored(m_colors.text_disabled, "|");
    ImGui::SameLine();
    
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time);
    
    char time_buf[16];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm);
    ImGui::Text("%s", time_buf);
    
    ImGui::SameLine();
    ImGui::TextColored(m_colors.text_disabled, "|");
    ImGui::SameLine();
    ImGui::TextColored(m_colors.success, "delay: 0ms");
    
    ImGui::End();
}

void c_menu::draw_spectator_list() {
    if (!g_cfg->misc.m_spectator_list)
        return;
    
    // Placeholder - actual implementation needs game data
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 200, 10));
    ImGui::SetNextWindowBgAlpha(0.8f);
    
    ImGui::Begin("##spectators", nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoScrollbar);
    
    ImGui::PushStyleColor(ImGuiCol_Text, m_colors.accent);
    ImGui::Text("Spectators");
    ImGui::PopStyleColor();
    
    ImGui::Separator();
    ImGui::TextColored(m_colors.text_disabled, "No spectators");
    
    ImGui::End();
}

void c_menu::draw_keybinds_list() {
    if (!g_cfg->misc.m_keybinds_list)
        return;
    
    ImGui::SetNextWindowPos(ImVec2(10, ImGui::GetIO().DisplaySize.y - 150));
    ImGui::SetNextWindowBgAlpha(0.8f);
    
    ImGui::Begin("##keybinds", nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoScrollbar);
    
    ImGui::PushStyleColor(ImGuiCol_Text, m_colors.accent);
    ImGui::Text("Keybinds");
    ImGui::PopStyleColor();
    
    ImGui::Separator();
    
    // Show active keybinds
    if (g_key_handler->is_pressed(g_cfg->rage_bot.m_override_damage_key_bind, g_cfg->rage_bot.m_override_damage_key_bind_style)) {
        ImGui::Text("Damage Override [ON]");
    }
    if (g_key_handler->is_pressed(g_cfg->world_esp.m_thirdperson_key_bind, g_cfg->world_esp.m_thirdperson_key_bind_style)) {
        ImGui::Text("Third Person [ON]");
    }
    
    ImGui::End();
}

// ============================================================================
// UTILITY
// ============================================================================

bool c_menu::on_create_move() {
    if (m_opened)
        m_block_attack_until_release = true;
    else if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000) &&
             !(GetAsyncKeyState(VK_RBUTTON) & 0x8000) &&
             !(GetAsyncKeyState(VK_MBUTTON) & 0x8000))
        m_block_attack_until_release = false;
    return blocks_attacks() && g_ctx->m_user_cmd &&
        command_input::suppress_attacks(*g_ctx->m_user_cmd);
}
