#define IMGUI_DEFINE_MATH_OPERATORS
#include "menu.hpp"
#include "hashes.hpp"
#include "notification.hpp"
#include "shader_bg.hpp"
#include "../darkside.hpp"
#include "../sdk/config_system/config_system.hpp"
#include "../sdk/includes/imgui/imgui_internal.h"

namespace {
void switch_tab(int tab) {
    if (gui.m_tab == tab)
        return;
    gui.m_tab = tab;
    gui.m_anim = 0.f;
}

void draw_top_bar(ImGuiWindow* window) {
    ImDrawList* draw = window->DrawList;
    const ImVec2 pos = window->Pos;
    const ImVec2 size = window->Size;

    ImGui::SetCursorPos(ImVec2(190.f, 15.f));
    const ImVec2 button_pos = ImGui::GetCursorScreenPos();
    const float save_w = 35.f;
    const float drop_w = 145.f;
    const float total_w = save_w + drop_w;

    ImGui::SetCursorScreenPos(button_pos);
    const bool save_clicked = ImGui::InvisibleButton("##top_save", ImVec2(save_w, 30.f));
    const bool save_hovered = ImGui::IsItemHovered();

    ImGui::SetCursorScreenPos(button_pos + ImVec2(save_w, 0.f));
    const bool config_clicked = ImGui::InvisibleButton("##top_config", ImVec2(drop_w, 30.f));
    const bool config_hovered = ImGui::IsItemHovered();

    static float hover = 0.f;
    hover = ImLerp(hover, save_hovered || config_hovered ? 1.f : 0.f,
                   ImClamp(ImGui::GetIO().DeltaTime * 15.f, 0.f, 1.f));

    draw->AddRectFilled(button_pos, button_pos + ImVec2(total_w, 30.f),
                        gui.frame_inactive.to_im_color(0.5f + hover * 0.3f), 6.f);
    draw->AddLine(button_pos + ImVec2(save_w, 0.f), button_pos + ImVec2(save_w, 30.f),
                  gui.divider.to_im_color(1.f, false));

    const ImVec2 save_size = ImGui::CalcTextSize(ICON_FA_SAVE);
    draw->AddText(button_pos + ImVec2((save_w - save_size.x) * 0.5f, (30.f - save_size.y) * 0.5f),
                  save_hovered ? gui.accent_color.to_im_color() : gui.text.to_im_color(), ICON_FA_SAVE);

    const char* current_name = "Default";
    if (!g_config_system->config_files.empty())
        current_name = g_config_system->config_files.front().c_str();
    const ImVec2 name_size = ImGui::CalcTextSize(current_name);
    draw->AddText(button_pos + ImVec2(save_w + 12.f, (30.f - name_size.y) * 0.5f),
                  gui.text.to_im_color(), current_name);

    const ImVec2 arrow_size = ImGui::CalcTextSize(ICON_FA_CHEVRON_DOWN);
    draw->AddText(button_pos + ImVec2(total_w - arrow_size.x - 12.f, (30.f - arrow_size.y) * 0.5f),
                  gui.text_disabled.to_im_color(), ICON_FA_CHEVRON_DOWN);

    ImGui::SetCursorScreenPos(button_pos + ImVec2(total_w + 8.f, 0.f));
    ImGui::Button("DARKSIDE", ImVec2(88.f, 30.f));

    if (save_clicked && !g_config_system->config_files.empty()) {
        g_config_system->save(g_config_system->config_files.front());
        PushNotification("Config Saved", "Saved current configuration");
    }
    if (config_clicked)
        switch_tab(6);

    const int search_tab = gui.search_bar(pos + ImVec2(size.x - 15.f * gui.m_scale, 15.f));
    if (search_tab >= 0)
        switch_tab(search_tab);
}
}

void c_menu::init() {
    gui.apply_theme(0);
    gui.apply_style_colors();
    gui.user_name = "darkside";
    gui.user_status = "Till: Lifetime";
    gui.user_till = "Till: Lifetime";
    g_config_system->refresh();
}

void c_menu::draw() {
    render();
}

void c_menu::draw_rage() {
    gui.begin_grid(2);

    gui.grid_col(0);
    gui.grid_box("MAIN", k_auto_height);
    gui.row_checkbox("Enabled", &g_cfg->rage_bot.m_enabled);
    gui.row_checkbox("Silent Aim", &g_cfg->rage_bot.m_silent);
    auto& cfg = g_cfg->rage_bot.m_weapon_config[0];
    gui.row_checkbox("Automatic Fire", &cfg.m_auto_fire);
    gui.end_group_box();

    gui.grid_box("HITBOXES", k_auto_height);
    gui.row_checkbox("Head", &cfg.m_hitboxes[0]);
    gui.row_checkbox("Chest", &cfg.m_hitboxes[1]);
    gui.row_checkbox("Stomach", &cfg.m_hitboxes[2]);
    gui.row_checkbox("Arms", &cfg.m_hitboxes[3]);
    gui.row_checkbox("Legs", &cfg.m_hitboxes[4]);
    gui.end_group_box();

    gui.grid_col(1);
    gui.grid_box("ACCURACY", k_auto_height);
    gui.row_begin(); ImGui::SliderInt("Hit Chance", &cfg.m_hit_chance, 0, 100, "%d%%");
    gui.row_begin(); ImGui::SliderInt("Minimum Damage", &cfg.m_minimum_damage, 0, 120);
    gui.end_group_box();

    gui.grid_box("BEHAVIOR", k_auto_height);
    gui.row_checkbox("Automatic Stop", &cfg.m_auto_stop);
    gui.row_checkbox("Automatic Scope", &cfg.m_auto_scope);
    gui.end_group_box();

    gui.end_grid();
}

void c_menu::draw_legit() {
    gui.begin_grid(2);

    gui.grid_col(0);
    gui.grid_box("AIMBOT", k_auto_height);
    gui.row_checkbox("Enabled", &g_cfg->legit_bot.m_enabled);
    gui.row_begin(); ImGui::SliderFloat("Field Of View", &g_cfg->legit_bot.m_fov, 0.f, 180.f, "%.1f");
    gui.row_begin(); ImGui::SliderFloat("Smooth", &g_cfg->legit_bot.m_smooth, 1.f, 20.f, "%.1f");
    gui.end_group_box();

    gui.grid_col(1);
    gui.grid_box("TRIGGERBOT", k_auto_height);
    gui.row_checkbox("Enabled", &g_cfg->legit_bot.m_triggerbot_enabled);
    gui.end_group_box();

    gui.grid_box("BACKTRACK", k_auto_height);
    gui.row_checkbox("Enabled", &g_cfg->legit_bot.m_backtrack_enabled);
    gui.row_begin(); ImGui::SliderInt("Ticks", &g_cfg->legit_bot.m_backtrack_ticks, 0, 15);
    gui.end_group_box();

    gui.end_grid();
}

void c_menu::draw_antiaim() {
    gui.begin_grid(2);

    gui.grid_col(0);
    gui.grid_box("ANGLES", k_auto_height);
    static const char* pitch_modes[] = { "Off", "Down", "Up", "Zero", "Custom" };
    gui.row_begin(); ImGui::Combo("Pitch", &g_cfg->anti_hit.m_pitch_mode, pitch_modes, IM_ARRAYSIZE(pitch_modes));
    if (g_cfg->anti_hit.m_pitch_mode == 4) {
        gui.row_begin(); ImGui::SliderInt("Pitch Amount", &g_cfg->anti_hit.m_pitch_amount, -89, 89);
    }
    static const char* yaw_modes[] = { "Off", "Backward", "Sideways", "Spin", "At Target" };
    gui.row_begin(); ImGui::Combo("Yaw", &g_cfg->anti_hit.m_yaw_mode, yaw_modes, IM_ARRAYSIZE(yaw_modes));
    gui.row_begin(); ImGui::SliderInt("Yaw Add", &g_cfg->anti_hit.m_yaw_amount, -180, 180);
    gui.row_checkbox("At Target", &g_cfg->anti_hit.m_at_target);
    gui.end_group_box();

    gui.grid_col(1);
    gui.grid_box("DESYNC", k_auto_height);
    gui.row_checkbox("Enabled", &g_cfg->anti_hit.m_desync_enabled);
    static const char* desync_modes[] = { "Static", "Jitter", "Switch" };
    gui.row_begin(); ImGui::Combo("Mode", &g_cfg->anti_hit.m_desync_mode, desync_modes, IM_ARRAYSIZE(desync_modes));
    gui.row_begin(); ImGui::SliderInt("Left", &g_cfg->anti_hit.m_desync_left, 0, 60);
    gui.row_begin(); ImGui::SliderInt("Right", &g_cfg->anti_hit.m_desync_right, 0, 60);
    gui.end_group_box();

    gui.grid_box("FAKELAG", k_auto_height);
    gui.row_checkbox("Enabled", &g_cfg->anti_hit.m_fakelag_enabled);
    static const char* lag_modes[] = { "Static", "Random", "Adaptive" };
    gui.row_begin(); ImGui::Combo("Mode", &g_cfg->anti_hit.m_fakelag_mode, lag_modes, IM_ARRAYSIZE(lag_modes));
    gui.row_begin(); ImGui::SliderInt("Limit", &g_cfg->anti_hit.m_fakelag_limit, 1, 15);
    gui.end_group_box();

    gui.end_grid();
}

void c_menu::draw_visuals() {
    gui.begin_grid(2);

    gui.grid_col(0);
    gui.grid_box("PLAYER ESP", k_auto_height);
    gui.row_checkbox("Enabled", &g_cfg->visuals.m_player_esp.m_enabled);
    gui.row_checkbox("Teammates", &g_cfg->visuals.m_player_esp.m_teammates);
    gui.row_checkbox("Bounding Box", &g_cfg->visuals.m_player_esp.m_bounding_box);
    gui.row_checkbox("Name", &g_cfg->visuals.m_player_esp.m_name);
    gui.row_checkbox("Health", &g_cfg->visuals.m_player_esp.m_health_bar);
    gui.row_checkbox("Weapon", &g_cfg->visuals.m_player_esp.m_weapon);
    gui.row_checkbox("Skeleton", &g_cfg->visuals.m_player_esp.m_skeleton);
    gui.end_group_box();

    gui.grid_col(1);
    gui.grid_box("WORLD", k_auto_height);
    gui.row_begin(); ImGui::SliderInt("Exposure", &g_cfg->world.m_exposure, 0, 100);
    gui.row_begin(); ImGui::SliderFloat("Brightness", &g_cfg->world.m_brightness, 0.f, 1.f, "%.2f");
    gui.row_checkbox("Remove Fog", &g_cfg->world.m_render_fog);
    gui.end_group_box();

    gui.grid_box("THIRDPERSON", k_auto_height);
    gui.row_checkbox("Enabled", &g_cfg->world_esp.m_enable_thirdperson);
    gui.row_begin(); ImGui::SliderInt("Distance", &g_cfg->world_esp.m_distance, 50, 300);
    gui.end_group_box();

    gui.end_grid();
}

void c_menu::draw_misc() {
    gui.begin_grid(2);

    gui.grid_col(0);
    gui.grid_box("MOVEMENT", k_auto_height);
    gui.row_checkbox("Bunny Hop", &g_cfg->misc.m_bunny_hop);
    gui.row_checkbox("Automatic Strafe", &g_cfg->misc.m_auto_strafe);
    static const char* strafe_modes[] = { "Legit", "Directional", "Rage" };
    gui.row_begin(); ImGui::Combo("Strafe Type", &g_cfg->misc.m_strafe_type, strafe_modes, IM_ARRAYSIZE(strafe_modes));
    gui.row_begin(); ImGui::SliderInt("Smooth", &g_cfg->misc.m_strafe_smooth, 0, 100);
    gui.end_group_box();

    gui.grid_col(1);
    gui.grid_box("OTHER", k_auto_height);
    gui.row_checkbox("Menu Shader", &shader_bg::state().enabled);
    gui.row_begin(); ImGui::SliderFloat("Shader Opacity", &shader_bg::state().opacity, 0.f, 1.f, "%.2f");
    gui.end_group_box();

    gui.end_grid();
}

void c_menu::draw_skins() {
    gui.begin_grid(2);

    gui.grid_col(0);
    gui.grid_box("KNIFE", k_auto_height);
    gui.row_begin(); ImGui::SliderInt("Model", &g_cfg->skins.m_knives.m_selected, 0, 20);
    gui.end_group_box();

    gui.grid_box("GLOVES", k_auto_height);
    gui.row_begin(); ImGui::SliderInt("Model", &g_cfg->skins.m_gloves.m_selected, 0, 20);
    gui.end_group_box();

    gui.grid_col(1);
    gui.grid_box("AGENTS", k_auto_height);
    gui.row_begin(); ImGui::SliderInt("Counter-Terrorist", &g_cfg->skins.m_agents.m_selected_ct, 0, 20);
    gui.row_begin(); ImGui::SliderInt("Terrorist", &g_cfg->skins.m_agents.m_selected, 0, 20);
    gui.end_group_box();

    gui.end_grid();
}

void c_menu::draw_configs() {
    static char config_name[64] = "default";
    static int selected = -1;

    g_config_system->refresh();

    gui.begin_grid(2);
    gui.grid_col(0);
    gui.grid_box("CONFIGS", 340.f, true);
    for (int i = 0; i < static_cast<int>(g_config_system->config_files.size()); ++i) {
        const auto& file = g_config_system->config_files[i];
        if (ImGui::Selectable(file.c_str(), selected == i)) {
            selected = i;
            strncpy_s(config_name, file.c_str(), sizeof(config_name) - 1);
        }
    }
    gui.end_group_box();

    gui.grid_col(1);
    gui.grid_box("ACTIONS", k_auto_height);
    gui.row_begin(); ImGui::InputText("Name", config_name, sizeof(config_name));
    gui.row_begin();
    if (ImGui::Button("Create", ImVec2(-1.f, 30.f)) && config_name[0]) {
        g_config_system->save(config_name);
        g_config_system->refresh();
    }
    gui.row_begin();
    if (ImGui::Button("Load", ImVec2(-1.f, 30.f)) && config_name[0])
        g_config_system->read(config_name);
    gui.row_begin();
    if (ImGui::Button("Save", ImVec2(-1.f, 30.f)) && config_name[0])
        g_config_system->save(config_name);
    gui.row_begin();
    if (ImGui::Button("Delete", ImVec2(-1.f, 30.f)) && config_name[0]) {
        g_config_system->remove(config_name);
        g_config_system->refresh();
        config_name[0] = '\0';
        selected = -1;
    }
    gui.end_group_box();
    gui.end_grid();
}

void c_menu::render() {
    if (!m_opened)
        return;

    ImGuiIO& io = ImGui::GetIO();
    gui.apply_style_colors();

    static bool menu_pos_ready = false;
    static bool menu_dragging = false;
    static ImVec2 menu_pos{};
    static ImVec2 menu_target{};
    static ImVec2 menu_grab{};

    if (menu_pos_ready) {
        if (menu_dragging) {
            if (io.MouseDown[0])
                menu_target = io.MousePos - menu_grab;
            else
                menu_dragging = false;
        }
        menu_pos = ImLerp(menu_pos, menu_target,
                          1.f - expf(-20.f * ImMin(io.DeltaTime, 0.05f)));
        if (ImLengthSqr(menu_target - menu_pos) < 0.25f)
            menu_pos = menu_target;
        ImGui::SetNextWindowPos(menu_pos, ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowPos(io.DisplaySize * 0.5f, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    }

    ImGui::SetNextWindowSize(ImVec2(800.f * gui.m_scale, 600.f * gui.m_scale), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                   ImGuiWindowFlags_NoBackground |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;

    const bool visible = ImGui::Begin("DARKSIDE##neverlose", &m_opened, flags);
    if (visible) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImDrawList* draw = window->DrawList;
        const ImVec2 pos = window->Pos;
        const ImVec2 size = window->Size;

        gui.m_anim = ImLerp(gui.m_anim, 1.f, 0.045f);
        gui.name_plate(pos, 170.f);
        shader_bg::update(size, pos);

        draw->AddLine(pos + ImVec2(185.f, 60.f), pos + ImVec2(size.x - 15.f, 60.f), gui.border.to_im_color());
        draw->AddLine(pos + ImVec2(10.f, 60.f), pos + ImVec2(160.f, 60.f), gui.border.to_im_color());
        gui.user_profile(pos + ImVec2(10.f, size.y - 48.f), pos + ImVec2(160.f, size.y - 8.f), nullptr);

        ImGui::SetCursorPos(ImVec2(10.f, 70.f));
        ImGui::BeginChild("##tabs", ImVec2(150.f, size.y - 120.f), false);
        gui.group_title("AIMBOT");
        if (gui.tab(ICON_FA_CROSSHAIRS, "Rage", gui.m_tab == 0)) switch_tab(0);
        if (gui.tab(ICON_FA_MOUSE, "Legit", gui.m_tab == 1)) switch_tab(1);
        if (gui.tab(ICON_FA_SHIELD_ALT, "Anti-Aim", gui.m_tab == 2)) switch_tab(2);

        ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
        gui.group_title("COMMON");
        if (gui.tab(ICON_FA_IMAGE, "Visuals", gui.m_tab == 3)) switch_tab(3);
        if (gui.tab(ICON_FA_SLIDERS_H, "Miscellaneous", gui.m_tab == 4)) switch_tab(4);

        ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
        gui.group_title("CUSTOMIZATION");
        if (gui.tab(ICON_FA_PAINT_BRUSH, "Skins", gui.m_tab == 5)) switch_tab(5);
        if (gui.tab(ICON_FA_COG, "Config", gui.m_tab == 6)) switch_tab(6);
        ImGui::EndChild();

        draw_top_bar(window);

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, gui.m_anim);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 8.f));
        ImGui::SetCursorPos(ImVec2(185.f, 81.f - 5.f * gui.m_anim));
        ImGui::BeginChild("##content", ImVec2(size.x - 200.f, size.y - 91.f), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        static const char* tab_names[] = {
            "Rage", "Legit", "Anti-Aim", "Visuals", "Miscellaneous", "Skins", "Config"
        };
        gui.search_scope_begin(gui.m_tab, tab_names[gui.m_tab]);
        switch (gui.m_tab) {
            case 0: draw_rage(); break;
            case 1: draw_legit(); break;
            case 2: draw_antiaim(); break;
            case 3: draw_visuals(); break;
            case 4: draw_misc(); break;
            case 5: draw_skins(); break;
            case 6: draw_configs(); break;
            default: switch_tab(0); break;
        }
        gui.search_scope_end();

        ImGui::EndChild();
        ImGui::PopStyleVar(2);

        if (!menu_pos_ready) {
            menu_pos = menu_target = window->Pos;
            menu_pos_ready = true;
        }
        if (!menu_dragging && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered() &&
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_NoPopupHierarchy)) {
            menu_dragging = true;
            menu_grab = io.MousePos - window->Pos;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();

    gui.spotify_widget_draw();
    DrawNotifications();
}
