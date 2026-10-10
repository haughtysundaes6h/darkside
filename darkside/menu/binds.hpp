#pragma once

#include "../sdk/includes/imgui/imgui.h"
#include "../sdk/includes/imgui/imgui_internal.h"
#include <cstdint>

struct ImGuiWindow;

namespace menu_binds {
using target_id = std::uint64_t;

void register_bool(const char* key, const char* label, bool* value);
void register_int(const char* key, const char* label, int* value, int minimum, int maximum);
void register_float(const char* key, const char* label, float* value, float minimum, float maximum);

target_id checkbox_item(ImGuiWindow* window, ImGuiID item_id, const char* label,
                        const ImRect& rect, bool* value);
target_id slider_item(ImGuiWindow* window, ImGuiID item_id, const char* label,
                      const ImRect& rect, ImGuiDataType type, void* value,
                      const void* minimum, const void* maximum);

void sync_bool(target_id id, bool value);
void sync_int(target_id id, int value);
void sync_float(target_id id, float value);

void update();
void flush();
void before_config_save();
void after_config_save();
void on_config_loaded();
bool is_capturing();
void draw_overlay(bool menu_open);
}
