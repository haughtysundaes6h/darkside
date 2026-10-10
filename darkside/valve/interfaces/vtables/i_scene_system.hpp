#pragma once

#include <cstddef>

class c_light_data_queue
{
public:
    char pad_0000[24]; // 0x0000
    void* light_data; // 0x0018
};
static_assert(offsetof(c_light_data_queue, light_data) == 0x18);

class i_scene_system
{
};
