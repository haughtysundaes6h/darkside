#include "binds.hpp"
#include "gui.hpp"
#include "../sdk/includes/json/json.h"

#define _HAS_STD_BYTE 0
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <vector>

namespace menu_binds {
namespace {
enum class value_type { boolean, integer, floating };
enum class bind_mode { always = 0, hold = 1, toggle = 2 };

struct bind_entry {
    std::uint64_t id = 0;
    int key = 0;
    bind_mode mode = bind_mode::hold;
    bool bool_value = true;
    int int_value = 0;
    float float_value = 0.f;
    bool toggled = false;
    bool was_down = false;
    bool active = false;
};

struct bind_target {
    target_id id = 0;
    std::string key;
    std::string label;
    value_type type = value_type::boolean;
    void* pointer = nullptr;
    bool pointer_seen = false;
    bool base_bool = false;
    int base_int = 0;
    float base_float = 0.f;
    int minimum_int = 0;
    int maximum_int = 100;
    float minimum_float = 0.f;
    float maximum_float = 1.f;
    bool was_overridden = false;
    std::vector<bind_entry> entries;
};

std::unordered_map<target_id, bind_target> g_targets;
std::unordered_map<void*, target_id> g_pointer_targets;
std::uint64_t g_next_entry_id = 1;
bool g_loaded = false;
bool g_dirty = false;
std::uint64_t g_capture_entry = 0;
std::array<bool, 0xA6> g_capture_down{};
constexpr const char* k_bind_file = "c:\\airflow\\binds.json";

std::uint64_t hash_key(const std::string& value) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const unsigned char c : value) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

bool belongs_to_main_menu(ImGuiWindow* window) {
    if (!window)
        return false;
    ImGuiWindow* main = ImGui::FindWindowByName("DARKSIDE##neverlose");
    return main && (window == main || window->RootWindow == main);
}

const char* mode_name(bind_mode mode) {
    switch (mode) {
        case bind_mode::always: return "Always";
        case bind_mode::hold: return "Hold";
        case bind_mode::toggle: return "Toggle";
    }
    return "Hold";
}

std::string key_name(int key) {
    switch (key) {
        case 0: return "None";
        case VK_LBUTTON: return "Mouse 1";
        case VK_RBUTTON: return "Mouse 2";
        case VK_MBUTTON: return "Mouse 3";
        case VK_XBUTTON1: return "Mouse 4";
        case VK_XBUTTON2: return "Mouse 5";
        case VK_SHIFT: return "Shift";
        case VK_CONTROL: return "Ctrl";
        case VK_MENU: return "Alt";
        case VK_SPACE: return "Space";
        case VK_INSERT: return "Insert";
        case VK_DELETE: return "Delete";
        case VK_HOME: return "Home";
        case VK_END: return "End";
        case VK_PRIOR: return "Page Up";
        case VK_NEXT: return "Page Down";
        case VK_ESCAPE: return "Escape";
    }

    UINT scan = MapVirtualKeyA(static_cast<UINT>(key), MAPVK_VK_TO_VSC);
    if (key == VK_LEFT || key == VK_UP || key == VK_RIGHT || key == VK_DOWN ||
        key == VK_RCONTROL || key == VK_RMENU || key == VK_DIVIDE || key == VK_NUMLOCK)
        scan |= 0x100;

    char name[64]{};
    if (GetKeyNameTextA(static_cast<LONG>(scan << 16), name, static_cast<int>(sizeof(name))) > 0)
        return name;
    return "VK " + std::to_string(key);
}

void mark_dirty() {
    g_dirty = true;
}

void load_once() {
    if (g_loaded)
        return;
    g_loaded = true;

    std::ifstream input(k_bind_file);
    if (!input)
        return;

    nlohmann::json data;
    try {
        input >> data;
    } catch (const nlohmann::json::exception&) {
        return;
    }
    if (!data.is_object() || !data.contains("targets") || !data["targets"].is_array())
        return;

    try {
        for (const auto& saved_target : data["targets"]) {
        if (!saved_target.is_object())
            continue;
        const std::string key = saved_target.value("key", std::string{});
        if (key.empty())
            continue;

        bind_target target{};
        target.key = key;
        target.id = hash_key(key);
        target.label = saved_target.value("label", key);
        const int saved_type = saved_target.value("type", 0);
        target.type = saved_type == 1 ? value_type::integer : saved_type == 2 ? value_type::floating : value_type::boolean;

        if (saved_target.contains("entries") && saved_target["entries"].is_array()) {
            for (const auto& saved_entry : saved_target["entries"]) {
                if (!saved_entry.is_object() || target.entries.size() >= 16)
                    continue;
                bind_entry entry{};
                entry.id = saved_entry.value("id", g_next_entry_id++);
                g_next_entry_id = (std::max)(g_next_entry_id, entry.id + 1);
                entry.key = std::clamp(saved_entry.value("key", 0), 0, 0xA5);
                entry.mode = static_cast<bind_mode>(std::clamp(saved_entry.value("mode", 1), 0, 2));
                entry.bool_value = saved_entry.value("bool_value", true);
                entry.int_value = saved_entry.value("int_value", 0);
                entry.float_value = saved_entry.value("float_value", 0.f);
                if (std::isfinite(entry.float_value))
                    target.entries.push_back(entry);
            }
        }
            if (!target.entries.empty())
                g_targets[target.id] = std::move(target);
        }
    } catch (const nlohmann::json::exception&) {
        // Keep valid entries already decoded and ignore the malformed remainder.
    }
}

void save_if_dirty() {
    if (!g_dirty)
        return;

    std::error_code error;
    std::filesystem::create_directories("c:\\airflow\\", error);
    if (error)
        return;

    nlohmann::json data;
    data["version"] = 1;
    data["targets"] = nlohmann::json::array();
    for (const auto& [id, target] : g_targets) {
        if (target.entries.empty() || !target.pointer_seen)
            continue;
        nlohmann::json saved_target;
        saved_target["key"] = target.key;
        saved_target["label"] = target.label;
        saved_target["type"] = target.type == value_type::boolean ? 0 : target.type == value_type::integer ? 1 : 2;
        saved_target["entries"] = nlohmann::json::array();
        for (const auto& entry : target.entries) {
            saved_target["entries"].push_back({
                {"id", entry.id}, {"key", entry.key}, {"mode", static_cast<int>(entry.mode)},
                {"bool_value", entry.bool_value}, {"int_value", entry.int_value},
                {"float_value", entry.float_value}
            });
        }
        data["targets"].push_back(std::move(saved_target));
    }

    std::ofstream output(k_bind_file, std::ios::trunc);
    if (!output)
        return;
    output << std::setw(2) << data;
    output.flush();
    if (output)
        g_dirty = false;
}

void begin_capture(bind_entry& entry) {
    g_capture_entry = entry.id;
    for (int key = 1; key <= 0xA5; ++key)
        g_capture_down[key] = (GetAsyncKeyState(key) & 0x8000) != 0;
}

bool update_capture(bind_entry& entry) {
    if (g_capture_entry != entry.id)
        return false;

    ImGui::SetNextFrameWantCaptureKeyboard(true);
    for (int key = 1; key <= 0xA5; ++key) {
        const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
        if (down && !g_capture_down[key]) {
            if (key == VK_INSERT || key == VK_END) {
                g_capture_down[key] = down;
                continue;
            }
            if (key == VK_ESCAPE) {
                g_capture_entry = 0;
                return false;
            }
            if (key == VK_BACK || key == VK_DELETE)
                entry.key = 0;
            else
                entry.key = key;
            entry.was_down = entry.key > 0 && (GetAsyncKeyState(entry.key) & 0x8000) != 0;
            entry.toggled = false;
            g_capture_entry = 0;
            mark_dirty();
            return true;
        }
        g_capture_down[key] = down;
    }
    return false;
}

void draw_bind_editor(bind_target& target, const ImRect& rect) {
    ImGui::PushID(target.key.c_str());
    const bool hovered = ImGui::IsMouseHoveringRect(rect.Min, rect.Max, true);
    const bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);

    if (!target.entries.empty()) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddCircleFilled(ImVec2(rect.Min.x + 3.f * gui.m_scale, rect.GetCenter().y),
                              2.f * gui.m_scale, gui.accent_color.to_im_color());
    }

    bool wants_open = false;
    const float popup_anim = gui.popup_animation("##bind_editor", clicked, wants_open);
    const ImVec2 anchor(rect.Max.x + 8.f * gui.m_scale, rect.GetCenter().y);
    const bool editor_open = gui.begin_popup_card_at(
        "##bind_editor", wants_open, popup_anim, anchor, ImVec2(0.f, 0.5f), 310.f);
    if (editor_open) {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 4 ? ImGui::GetIO().Fonts->Fonts[4] : ImGui::GetFont());
        ImGui::TextUnformatted(target.label.c_str());
        ImGui::PopFont();
        ImGui::TextDisabled("Right-click the item to edit binds");
        ImGui::TextDisabled("Last active bind wins");
        ImGui::Separator();

        int remove_index = -1;
        static const char* modes[] = { "Always", "Hold", "Toggle" };
        for (int index = 0; index < static_cast<int>(target.entries.size()); ++index) {
            bind_entry& entry = target.entries[index];
            ImGui::PushID(static_cast<int>(entry.id));
            ImGui::Text("Bind %d", index + 1);

            const bool capturing = g_capture_entry == entry.id;
            const std::string button_text = capturing ? "Press a key..." : key_name(entry.key);
            if (ImGui::Button(button_text.c_str(), ImVec2(125.f * gui.m_scale, 28.f * gui.m_scale)))
                begin_capture(entry);
            update_capture(entry);

            ImGui::SameLine();
            int mode = static_cast<int>(entry.mode);
            ImGui::SetNextItemWidth(105.f * gui.m_scale);
            if (ImGui::Combo("##mode", &mode, modes, IM_ARRAYSIZE(modes))) {
                entry.mode = static_cast<bind_mode>(mode);
                entry.toggled = false;
                entry.was_down = entry.key > 0 && (GetAsyncKeyState(entry.key) & 0x8000) != 0;
                mark_dirty();
            }

            if (target.type == value_type::boolean) {
                if (ImGui::Checkbox("Value", &entry.bool_value))
                    mark_dirty();
            } else if (target.type == value_type::integer) {
                if (ImGui::SliderInt("Value", &entry.int_value, target.minimum_int, target.maximum_int))
                    mark_dirty();
            } else {
                if (ImGui::SliderFloat("Value", &entry.float_value, target.minimum_float, target.maximum_float, "%.2f"))
                    mark_dirty();
            }

            if (ImGui::Button("Remove", ImVec2(-1.f, 26.f * gui.m_scale)))
                remove_index = index;
            ImGui::Separator();
            ImGui::PopID();
        }

        if (remove_index >= 0) {
            if (g_capture_entry == target.entries[remove_index].id)
                g_capture_entry = 0;
            target.entries.erase(target.entries.begin() + remove_index);
            mark_dirty();
        }

        if (target.entries.size() < 16 && ImGui::Button("Add bind", ImVec2(-1.f, 30.f * gui.m_scale))) {
            bind_entry entry{};
            entry.id = g_next_entry_id++;
            entry.mode = bind_mode::hold;
            entry.bool_value = target.type == value_type::boolean ? target.base_bool : true;
            entry.int_value = std::clamp(target.base_int, target.minimum_int, target.maximum_int);
            entry.float_value = std::clamp(target.base_float, target.minimum_float, target.maximum_float);
            target.entries.push_back(entry);
            mark_dirty();
        }

        gui.end_popup_card();
    } else if (!wants_open && g_capture_entry != 0) {
        const auto capture = std::find_if(target.entries.begin(), target.entries.end(),
            [](const bind_entry& entry) { return entry.id == g_capture_entry; });
        if (capture != target.entries.end())
            g_capture_entry = 0;
    }
    ImGui::PopID();
}

bind_target& register_stable_target(const char* stable_key, const char* label,
                                  value_type type, void* pointer) {
    load_once();
    const std::string key = stable_key ? stable_key : "item";
    const target_id id = hash_key(key);
    auto [it, inserted] = g_targets.try_emplace(id);
    bind_target& target = it->second;
    if (inserted) {
        target.id = id;
        target.key = key;
        target.type = type;
    }
    target.label = label ? label : key;
    target.type = type;
    target.pointer = pointer;
    g_pointer_targets[pointer] = id;
    if (!target.pointer_seen) {
        target.pointer_seen = true;
        if (type == value_type::boolean) target.base_bool = *static_cast<bool*>(pointer);
        else if (type == value_type::integer) target.base_int = *static_cast<int*>(pointer);
        else target.base_float = *static_cast<float*>(pointer);
    }
    return target;
}

bind_target* find_registered_target(void* pointer) {
    const auto pointer_it = g_pointer_targets.find(pointer);
    if (pointer_it == g_pointer_targets.end())
        return nullptr;
    const auto target_it = g_targets.find(pointer_it->second);
    return target_it == g_targets.end() ? nullptr : &target_it->second;
}

void write_base(bind_target& target) {
    if (!target.pointer)
        return;
    if (target.type == value_type::boolean) *static_cast<bool*>(target.pointer) = target.base_bool;
    else if (target.type == value_type::integer) *static_cast<int*>(target.pointer) = target.base_int;
    else *static_cast<float*>(target.pointer) = target.base_float;
}

void write_entry(bind_target& target, const bind_entry& entry) {
    if (!target.pointer)
        return;
    if (target.type == value_type::boolean) *static_cast<bool*>(target.pointer) = entry.bool_value;
    else if (target.type == value_type::integer) *static_cast<int*>(target.pointer) = std::clamp(entry.int_value, target.minimum_int, target.maximum_int);
    else *static_cast<float*>(target.pointer) = std::clamp(entry.float_value, target.minimum_float, target.maximum_float);
}
}

void register_bool(const char* key, const char* label, bool* value) {
    if (key && value)
        register_stable_target(key, label, value_type::boolean, value);
}

void register_int(const char* key, const char* label, int* value, int minimum, int maximum) {
    if (!key || !value)
        return;
    bind_target& target = register_stable_target(key, label, value_type::integer, value);
    target.minimum_int = minimum;
    target.maximum_int = maximum;
    for (auto& entry : target.entries)
        entry.int_value = std::clamp(entry.int_value, minimum, maximum);
}

void register_float(const char* key, const char* label, float* value, float minimum, float maximum) {
    if (!key || !value)
        return;
    bind_target& target = register_stable_target(key, label, value_type::floating, value);
    target.minimum_float = minimum;
    target.maximum_float = maximum;
    for (auto& entry : target.entries)
        entry.float_value = std::clamp(entry.float_value, minimum, maximum);
}

target_id checkbox_item(ImGuiWindow* window, ImGuiID item_id, const char* label,
                        const ImRect& rect, bool* value) {
    IM_UNUSED(item_id);
    if (!value || !belongs_to_main_menu(window) ||
        (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled))
        return 0;
    bind_target* target = find_registered_target(value);
    if (!target || target->type != value_type::boolean)
        return 0;
    draw_bind_editor(*target, rect);
    return target->id;
}

target_id slider_item(ImGuiWindow* window, ImGuiID item_id, const char* label,
                      const ImRect& rect, ImGuiDataType type, void* value,
                      const void* minimum, const void* maximum) {
    IM_UNUSED(item_id);
    if (!value || !minimum || !maximum || !belongs_to_main_menu(window) ||
        (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled))
        return 0;
    if (type != ImGuiDataType_S32 && type != ImGuiDataType_Float)
        return 0;

    const value_type target_type = type == ImGuiDataType_S32 ? value_type::integer : value_type::floating;
    bind_target* target = find_registered_target(value);
    if (!target || target->type != target_type)
        return 0;
    if (target_type == value_type::integer) {
        target->minimum_int = *static_cast<const int*>(minimum);
        target->maximum_int = *static_cast<const int*>(maximum);
        for (auto& entry : target->entries)
            entry.int_value = std::clamp(entry.int_value, target->minimum_int, target->maximum_int);
    } else {
        target->minimum_float = *static_cast<const float*>(minimum);
        target->maximum_float = *static_cast<const float*>(maximum);
        for (auto& entry : target->entries)
            entry.float_value = std::clamp(entry.float_value, target->minimum_float, target->maximum_float);
    }
    draw_bind_editor(*target, rect);
    return target->id;
}

void sync_bool(target_id id, bool value) {
    if (!id) return;
    auto it = g_targets.find(id);
    if (it != g_targets.end()) it->second.base_bool = value;
}

void sync_int(target_id id, int value) {
    if (!id) return;
    auto it = g_targets.find(id);
    if (it != g_targets.end()) it->second.base_int = value;
}

void sync_float(target_id id, float value) {
    if (!id || !std::isfinite(value)) return;
    auto it = g_targets.find(id);
    if (it != g_targets.end()) it->second.base_float = value;
}

void update() {
    load_once();
    for (auto& [id, target] : g_targets) {
        if (!target.pointer_seen || !target.pointer)
            continue;

        if (!target.was_overridden) {
            if (target.type == value_type::boolean) target.base_bool = *static_cast<bool*>(target.pointer);
            else if (target.type == value_type::integer) target.base_int = *static_cast<int*>(target.pointer);
            else if (std::isfinite(*static_cast<float*>(target.pointer))) target.base_float = *static_cast<float*>(target.pointer);
        }

        bind_entry* winner = nullptr;
        for (auto& entry : target.entries) {
            const bool down = entry.key > 0 && (GetAsyncKeyState(entry.key) & 0x8000) != 0;
            if (g_capture_entry != 0) {
                entry.was_down = down;
                if (entry.active)
                    winner = &entry;
                continue;
            }
            if (entry.mode == bind_mode::always) {
                entry.active = true;
            } else if (entry.mode == bind_mode::hold) {
                entry.active = entry.key > 0 && down;
            } else {
                if (entry.key > 0 && down && !entry.was_down)
                    entry.toggled = !entry.toggled;
                entry.active = entry.key > 0 && entry.toggled;
            }
            entry.was_down = down;
            if (entry.active)
                winner = &entry;
        }

        if (winner)
            write_entry(target, *winner);
        else if (target.was_overridden)
            write_base(target);
        target.was_overridden = winner != nullptr;
    }
    save_if_dirty();
}

void flush() {
    save_if_dirty();
}

void before_config_save() {
    for (auto& [id, target] : g_targets)
        if (target.was_overridden)
            write_base(target);
}

void after_config_save() {
    for (auto& [id, target] : g_targets) {
        const bind_entry* winner = nullptr;
        for (const auto& entry : target.entries)
            if (entry.active)
                winner = &entry;
        if (winner)
            write_entry(target, *winner);
    }
}

void on_config_loaded() {
    for (auto& [id, target] : g_targets) {
        if (!target.pointer_seen || !target.pointer)
            continue;
        target.was_overridden = false;
        if (target.type == value_type::boolean) target.base_bool = *static_cast<bool*>(target.pointer);
        else if (target.type == value_type::integer) target.base_int = *static_cast<int*>(target.pointer);
        else if (std::isfinite(*static_cast<float*>(target.pointer))) target.base_float = *static_cast<float*>(target.pointer);

        const bind_entry* winner = nullptr;
        for (const auto& entry : target.entries)
            if (entry.active)
                winner = &entry;
        if (winner) {
            write_entry(target, *winner);
            target.was_overridden = true;
        }
    }
}

bool is_capturing() {
    return g_capture_entry != 0;
}

void draw_overlay(bool menu_open) {
    struct active_row { std::string label; std::string binding; };
    std::vector<active_row> rows;
    for (const auto& [id, target] : g_targets) {
        const bind_entry* winner = nullptr;
        for (const auto& entry : target.entries)
            if (entry.active)
                winner = &entry;
        if (!winner)
            continue;
        std::string binding = gui.tr(mode_name(winner->mode));
        if (winner->mode != bind_mode::always)
            binding += " · " + key_name(winner->key);
        rows.push_back({ gui.tr(target.label.c_str()), std::move(binding) });
    }
    std::sort(rows.begin(), rows.end(), [](const active_row& left, const active_row& right) {
        return left.label < right.label;
    });
    if (rows.empty())
        return;

    ImGui::SetNextWindowPos(ImVec2(20.f, 180.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(230.f * gui.m_scale, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 10.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, gui.panel_body.to_vec4(0.94f, false));
    ImGui::PushStyleColor(ImGuiCol_Border, gui.popup_border.to_vec4(0.9f, false));

    ImGuiWindowFlags overlay_flags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (!menu_open)
        overlay_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##pastaware_keybinds", nullptr, overlay_flags)) {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 4 ? ImGui::GetIO().Fonts->Fonts[4] : ImGui::GetFont());
        ImGui::TextUnformatted(gui.tr("KEYBINDS"));
        ImGui::PopFont();
        ImGui::Separator();
        for (const auto& row : rows) {
            ImGui::TextUnformatted(row.label.c_str());
            const float width = ImGui::CalcTextSize(row.binding.c_str()).x;
            ImGui::SameLine((std::max)(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - width - 12.f));
            ImGui::TextColored(gui.accent_color.to_vec4(), "%s", row.binding.c_str());
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}
}
