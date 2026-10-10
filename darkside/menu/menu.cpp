#define IMGUI_DEFINE_MATH_OPERATORS
#include "menu.hpp"
#include "hashes.hpp"
#include "notification.hpp"
#include "shader_bg.hpp"
#include "binds.hpp"
#include "../darkside.hpp"
#include "../sdk/config_system/config_system.hpp"
#include "../sdk/includes/imgui/imgui_internal.h"

namespace {
std::string g_selected_config;

void refresh_config_selection() {
    g_config_system->refresh();
    const auto found = std::find(g_config_system->config_files.begin(),
                                 g_config_system->config_files.end(), g_selected_config);
    if (found == g_config_system->config_files.end())
        g_selected_config = g_config_system->config_files.empty() ? std::string{} : g_config_system->config_files.front();
}

void switch_tab(int tab) {
    if (gui.m_tab == tab)
        return;
    gui.m_tab = tab;
    gui.m_anim = 0.f;
}

void save_config(const std::string& name) {
    menu_binds::before_config_save();
    g_config_system->save(name);
    menu_binds::after_config_save();
}

void load_config(const std::string& name) {
    menu_binds::before_config_save();
    g_config_system->read(name);
    menu_binds::on_config_loaded();
}

void register_bind_targets() {
    auto& rage = g_cfg->rage_bot;
    auto& weapon = rage.m_weapon_config[0];
    menu_binds::register_bool("rage.enabled", "Enabled", &rage.m_enabled);
    menu_binds::register_bool("rage.silent", "Silent Aim", &rage.m_silent);
    menu_binds::register_bool("rage.auto_fire", "Automatic Fire", &weapon.m_auto_fire);
    menu_binds::register_bool("rage.auto_stop", "Automatic Stop", &weapon.m_auto_stop);
    menu_binds::register_bool("rage.auto_scope", "Automatic Scope", &weapon.m_auto_scope);
    for (int i = 0; i < 5; ++i) {
        static const char* names[] = { "Head", "Chest", "Stomach", "Arms", "Legs" };
        const std::string key = "rage.hitbox." + std::to_string(i);
        menu_binds::register_bool(key.c_str(), names[i], &weapon.m_hitboxes[i]);
    }
    menu_binds::register_int("rage.hit_chance", "Hit Chance", &weapon.m_hit_chance, 0, 100);
    menu_binds::register_int("rage.minimum_damage", "Minimum Damage", &weapon.m_minimum_damage, 0, 120);

    auto& legit = g_cfg->legit_bot;
    menu_binds::register_bool("legit.enabled", "Enabled", &legit.m_enabled);
    menu_binds::register_float("legit.fov", "Field Of View", &legit.m_fov, 0.f, 180.f);
    menu_binds::register_float("legit.smooth", "Smooth", &legit.m_smooth, 1.f, 20.f);
    menu_binds::register_bool("legit.triggerbot", "Triggerbot", &legit.m_triggerbot_enabled);
    menu_binds::register_bool("legit.backtrack", "Backtrack", &legit.m_backtrack_enabled);
    menu_binds::register_int("legit.backtrack_ticks", "Backtrack Ticks", &legit.m_backtrack_ticks, 0, 15);

    auto& anti = g_cfg->anti_hit;
    menu_binds::register_int("antiaim.pitch_mode", "Pitch", &anti.m_pitch_mode, 0, 4);
    menu_binds::register_int("antiaim.pitch_amount", "Pitch Amount", &anti.m_pitch_amount, -89, 89);
    menu_binds::register_int("antiaim.yaw_mode", "Yaw", &anti.m_yaw_mode, 0, 4);
    menu_binds::register_int("antiaim.yaw_amount", "Yaw Add", &anti.m_yaw_amount, -180, 180);
    menu_binds::register_bool("antiaim.at_target", "At Target", &anti.m_at_target);

    auto& esp = g_cfg->visuals.m_player_esp;
    menu_binds::register_bool("visuals.enabled", "Player ESP", &esp.m_enabled);
    menu_binds::register_bool("visuals.teammates", "Teammates", &esp.m_teammates);
    menu_binds::register_bool("visuals.box", "Bounding Box", &esp.m_bounding_box);
    menu_binds::register_bool("visuals.name", "Name", &esp.m_name);
    menu_binds::register_bool("visuals.health", "Health", &esp.m_health_bar);
    menu_binds::register_bool("visuals.weapon", "Weapon", &esp.m_weapon);
    menu_binds::register_bool("visuals.skeleton", "Skeleton", &esp.m_skeleton);
    menu_binds::register_int("world.exposure", "Exposure", &g_cfg->world.m_exposure, 0, 100);
    menu_binds::register_float("world.brightness", "Brightness", &g_cfg->world.m_brightness, 0.f, 1.f);
    menu_binds::register_bool("world.remove_fog", "Remove Fog", &g_cfg->world.m_render_fog);
    menu_binds::register_bool("thirdperson.enabled", "Thirdperson", &g_cfg->world_esp.m_enable_thirdperson);
    menu_binds::register_int("thirdperson.distance", "Thirdperson Distance", &g_cfg->world_esp.m_distance, 50, 300);

    menu_binds::register_bool("misc.bunny_hop", "Bunny Hop", &g_cfg->misc.m_bunny_hop);
    menu_binds::register_bool("misc.auto_strafe", "Automatic Strafe", &g_cfg->misc.m_auto_strafe);
    menu_binds::register_int("misc.strafe_type", "Strafe Type", &g_cfg->misc.m_strafe_type, 0, 2);
    menu_binds::register_int("misc.strafe_smooth", "Strafe Smooth", &g_cfg->misc.m_strafe_smooth, 0, 100);
    menu_binds::register_bool("menu.shader", "Menu Shader", &shader_bg::state().enabled);
    menu_binds::register_float("menu.shader_opacity", "Shader Opacity", &shader_bg::state().opacity, 0.f, 1.f);

    menu_binds::register_int("skins.knife", "Knife Model", &g_cfg->skins.m_knives.m_selected, 0, 20);
    menu_binds::register_int("skins.gloves", "Glove Model", &g_cfg->skins.m_gloves.m_selected, 0, 20);
    menu_binds::register_int("skins.agent_ct", "Counter-Terrorist", &g_cfg->skins.m_agents.m_selected_ct, 0, 20);
    menu_binds::register_int("skins.agent_t", "Terrorist", &g_cfg->skins.m_agents.m_selected, 0, 20);
}

void draw_animated_row(const char* id, bool visible, const std::function<void()>& draw_row) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImGuiID animation_id = window->GetID(id);
    ImGuiStorage* storage = window->DC.StateStorage;
    float amount = storage->GetFloat(animation_id, visible ? 1.f : 0.f);
    amount = ImLerp(amount, visible ? 1.f : 0.f,
                    1.f - expf(-14.f * ImMin(ImGui::GetIO().DeltaTime, 0.05f)));
    if (ImAbs(amount - (visible ? 1.f : 0.f)) < 0.002f)
        amount = visible ? 1.f : 0.f;
    storage->SetFloat(animation_id, amount);
    if (amount <= 0.002f)
        return;

    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float row_height = ImMax(ImGui::GetFontSize(), 19.f * gui.m_scale) +
        ImGui::GetStyle().FramePadding.y * 2.f + ImGui::GetStyle().ItemSpacing.y;
    const float animated_height = row_height * amount;
    ImGui::PushClipRect(start, start + ImVec2(ImGui::GetContentRegionAvail().x, animated_height), true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * amount);
    ImGui::BeginDisabled(amount < 0.98f);
    ImGui::SetCursorScreenPos(start - ImVec2(0.f, (1.f - amount) * 5.f * gui.m_scale));
    gui.row_begin();
    draw_row();
    ImGui::EndDisabled();
    ImGui::PopStyleVar();
    ImGui::PopClipRect();
    ImGui::SetCursorScreenPos(start + ImVec2(0.f, animated_height));
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

    const char* current_name = g_selected_config.empty() ? "None" : g_selected_config.c_str();
    const ImVec2 name_size = ImGui::CalcTextSize(current_name);
    draw->AddText(button_pos + ImVec2(save_w + 12.f, (30.f - name_size.y) * 0.5f),
                  gui.text.to_im_color(), current_name);

    const ImVec2 arrow_size = ImGui::CalcTextSize(ICON_FA_CHEVRON_DOWN);
    draw->AddText(button_pos + ImVec2(total_w - arrow_size.x - 12.f, (30.f - arrow_size.y) * 0.5f),
                  gui.text_disabled.to_im_color(), ICON_FA_CHEVRON_DOWN);

    if (save_clicked && !g_selected_config.empty()) {
        save_config(g_selected_config);
        PushNotification(gui.tr("Config Saved"), gui.trf("Saved changes to %s", g_selected_config.c_str()));
    }
    if (config_clicked)
        refresh_config_selection();

    bool config_open = false;
    const float config_anim = gui.popup_animation("##top_config_dropdown", config_clicked, config_open);
    const ImVec2 config_anchor(button_pos.x + save_w, button_pos.y + 36.f * gui.m_scale);
    if (gui.begin_popup_card_at("##top_config_dropdown", config_open, config_anim,
                                config_anchor, ImVec2(0.f, 0.f), 220.f)) {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 4 ? ImGui::GetIO().Fonts->Fonts[4] : ImGui::GetFont());
        ImGui::TextUnformatted(gui.tr("Config"));
        ImGui::PopFont();
        ImGui::Separator();
        if (g_config_system->config_files.empty()) {
            ImGui::TextDisabled("%s", gui.tr("No presets found"));
        } else {
            for (const auto& file : g_config_system->config_files) {
                if (ImGui::Selectable(file.c_str(), file == g_selected_config)) {
                    g_selected_config = file;
                    load_config(file);
                    PushNotification(gui.tr("Config Loaded"), gui.trf("Loaded config: %s", file.c_str()));
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        gui.end_popup_card();
    }

    const int search_tab = gui.search_bar(pos + ImVec2(size.x - 15.f * gui.m_scale, 15.f));
    if (search_tab >= 0)
        switch_tab(search_tab);
}
}

void c_menu::init() {
    gui.apply_theme(0);
    gui.apply_style_colors();
    gui.user_name = "PASTAWARE";
    gui.user_status = "Till: Lifetime";
    gui.user_till = "Till: Lifetime";
    refresh_config_selection();
    register_bind_targets();
}

void c_menu::draw() {
    menu_binds::update();
    render();
    menu_binds::draw_overlay(m_opened);
    menu_binds::flush();
}

void c_menu::draw_rage() {
    // Weapon group names for combo
    static const char* weapon_groups[] = {
        "General", "Pistols", "Heavy Pistol", "SMG", "Rifle", 
        "Auto Sniper", "Scout", "AWP", "Shotgun", "LMG"
    };
    
    // Hitbox names for multibox
    static const char* hitbox_names[] = { "Head", "Chest", "Stomach", "Arms", "Legs", "Feet" };
    static const char* multipoint_names[] = { "Head", "Chest", "Stomach", "Arms", "Legs", "Feet" };
    
    gui.begin_grid(2);

    // ========== LEFT COLUMN ==========
    gui.grid_col(0);
    
    // MAIN settings
    gui.grid_box("MAIN", k_auto_height);
    gui.row_checkbox("Enabled", &g_cfg->rage_bot.m_enabled);
    gui.row_checkbox("Silent Aim", &g_cfg->rage_bot.m_silent);
    gui.end_group_box();
    
    // WEAPON GROUP selector
    gui.grid_box("WEAPON", k_auto_height);
    gui.row_begin(); 
    ImGui::Combo("Group", &g_cfg->rage_bot.m_selected_group, weapon_groups, IM_ARRAYSIZE(weapon_groups));
    
    // Get current weapon config
    auto& cfg = g_cfg->rage_bot.m_weapon_config[g_cfg->rage_bot.m_selected_group];
    
    // Override toggle for non-general groups
    if (g_cfg->rage_bot.m_selected_group != WEAPON_GROUP_GENERAL) {
        gui.row_checkbox("Override General", &cfg.m_override_general);
    }
    gui.end_group_box();
    
    // HITBOXES multibox
    gui.grid_box("HITBOXES", k_auto_height);
    gui.row_multibox("Target", hitbox_names, cfg.m_hitboxes, 6, "Select which body parts to target.");
    gui.end_group_box();
    
    // MULTIPOINTS with gear icon
    gui.grid_box("MULTIPOINTS", k_auto_height);
    gui.row_begin();
    if (gui.row_options_begin("##multipoint_settings", 200.f)) {
        gui.popup_section_begin("##mp_sliders", true);
        ImGui::SliderInt("Head Scale", &cfg.m_hitbox_scale[0], 0, 100, "%d%%");
        ImGui::SliderInt("Body Scale", &cfg.m_hitbox_scale[1], 0, 100, "%d%%");
        gui.popup_section_end();
        gui.row_options_end();
    }
    gui.row_multibox("Points", multipoint_names, cfg.m_multi_points, 6, "Select multipoint locations.");
    
    gui.row_checkbox("Static Point Scale", &cfg.m_static_scale);
    gui.end_group_box();

    // ========== RIGHT COLUMN ==========
    gui.grid_col(1);
    
    // ACCURACY settings
    gui.grid_box("ACCURACY", k_auto_height);
    gui.row_begin(); ImGui::SliderInt("Hit Chance", &cfg.m_hit_chance, 0, 100, "%d%%");
    gui.row_begin(); ImGui::SliderInt("Minimum Damage", &cfg.m_minimum_damage, 0, 120);
    
    // Min damage override with gear
    gui.row_begin();
    if (gui.row_options_begin("##dmg_override", 220.f)) {
        gui.popup_section_begin("##dmg_settings", true);
        ImGui::SliderInt("Override Damage", &cfg.m_minimum_damage_override, 1, 120);
        ImGui::SliderInt("Auto (HP + X)", &cfg.m_minimum_damage_auto, 0, 50);
        gui.popup_section_end();
        gui.row_options_end();
    }
    ImGui::Checkbox("Damage Override", &cfg.m_enabled);
    gui.end_group_box();
    
    // BEHAVIOR settings
    gui.grid_box("BEHAVIOR", k_auto_height);
    gui.row_checkbox("Automatic Fire", &cfg.m_auto_fire);
    gui.row_checkbox("Automatic Stop", &cfg.m_auto_stop);
    
    // Auto stop mode selector
    draw_animated_row("##auto_stop_mode", cfg.m_auto_stop, [&cfg] {
        static const char* stop_modes[] = { "Full", "Minimal", "Between Shots" };
        ImGui::Combo("Stop Mode", &cfg.m_auto_stop_mode, stop_modes, IM_ARRAYSIZE(stop_modes));
    });
    
    gui.row_checkbox("Automatic Scope", &cfg.m_auto_scope);
    gui.row_checkbox("Delay Shot", &cfg.m_delay_shot);
    gui.end_group_box();
    
    // SAFETY settings
    gui.grid_box("SAFETY", k_auto_height);
    gui.row_checkbox("Prefer Safe Point", &cfg.m_prefer_safe);
    gui.row_checkbox("Prefer Body Aim", &cfg.m_prefer_body);
    gui.row_checkbox("Body Aim If Lethal", &cfg.m_prefer_baim_lethal);
    gui.row_begin(); ImGui::SliderInt("BAIM After Misses", &cfg.m_baim_after_x_shots, 0, 10);
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
    gui.begin_grid(1);

    gui.grid_col(0);
    gui.grid_box("ANGLES", k_auto_height);
    static const char* pitch_modes[] = { "Off", "Down", "Up", "Zero", "Custom" };
    gui.row_begin(); ImGui::Combo("Pitch", &g_cfg->anti_hit.m_pitch_mode, pitch_modes, IM_ARRAYSIZE(pitch_modes));
    draw_animated_row("##custom_pitch", g_cfg->anti_hit.m_pitch_mode == 4, [] {
        ImGui::SliderInt("Pitch Amount", &g_cfg->anti_hit.m_pitch_amount, -89, 89);
    });
    static const char* yaw_modes[] = { "Off", "Backward", "Sideways", "Spin", "At Target" };
    gui.row_begin(); ImGui::Combo("Yaw", &g_cfg->anti_hit.m_yaw_mode, yaw_modes, IM_ARRAYSIZE(yaw_modes));
    gui.row_begin(); ImGui::SliderInt("Yaw Add", &g_cfg->anti_hit.m_yaw_amount, -180, 180);
    gui.row_checkbox("At Target", &g_cfg->anti_hit.m_at_target);
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

    refresh_config_selection();

    gui.begin_grid(2);
    gui.grid_col(0);
    gui.grid_box("CONFIGS", 340.f, true);
    for (const auto& file : g_config_system->config_files) {
        if (ImGui::Selectable(file.c_str(), file == g_selected_config)) {
            g_selected_config = file;
            strncpy_s(config_name, file.c_str(), sizeof(config_name) - 1);
        }
    }
    gui.end_group_box();

    gui.grid_col(1);
    gui.grid_box("ACTIONS", k_auto_height);
    gui.row_begin(); ImGui::InputText("Name", config_name, sizeof(config_name));
    gui.row_begin();
    if (ImGui::Button("Create", ImVec2(-1.f, 30.f)) && config_name[0]) {
        save_config(config_name);
        refresh_config_selection();
        g_selected_config = config_name;
    }
    gui.row_begin();
    if (ImGui::Button("Load", ImVec2(-1.f, 30.f)) && config_name[0]) {
        load_config(config_name);
        g_selected_config = config_name;
    }
    gui.row_begin();
    if (ImGui::Button("Save", ImVec2(-1.f, 30.f)) && config_name[0]) {
        save_config(config_name);
        g_selected_config = config_name;
    }
    gui.row_begin();
    if (ImGui::Button("Delete", ImVec2(-1.f, 30.f)) && config_name[0]) {
        g_config_system->remove(config_name);
        if (g_selected_config == config_name)
            g_selected_config.clear();
        refresh_config_selection();
        config_name[0] = '\0';
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
                          1.f - expf(-36.f * ImMin(io.DeltaTime, 0.05f)));
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
