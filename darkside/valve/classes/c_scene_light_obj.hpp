#pragma once

#include "../../sdk/typedefs/c_color.hpp"
#include <cstddef>

class c_scene_light_object
{
public:
	char pad_0000[ 0xE4 ]; // 0x0
	c_color m_color; // 0xE4
};

class c_aggregate_object_data
{
public:
    char pad_0000[4]; //0x0000
    int count; //0x0004
    char pad_0008[0x30]; //0x0008
    int index; //0x0038
}; //Size: 0x003C
static_assert(offsetof(c_aggregate_object_data, index) == 0x38);

class c_aggregate_object_array
{
public:
    void* object; //0x0000
    c_aggregate_object_data* data; //0x0008
};

class c_material_2
{
public:
    virtual const char* get_name( ) = 0;
    virtual const char* get_shared_name( ) = 0;
};

class c_scene_object
{
public:
    char pad_0000[ 184 ]; //0x0000
    uint8_t r; //0x00B8
    uint8_t g; //0x00B9
    uint8_t b; //0x00BA
    uint8_t a; //0x00BB
    char pad_00BC[ 196 ]; //0x00BC
}; //Size: 0x0180

class c_base_scene_data
{
public:
    char pad_0000[0x20]; //0x0000
    c_material_2* m_material; //0x0020
    c_material_2* m_material2; //0x0028
    char pad_0030[0x20]; //0x0030
    uint8_t r; //0x0050
    uint8_t g; //0x0051
    uint8_t b; //0x0052
    uint8_t a; //0x0053
    char pad_0054[0x1C]; //0x0054
}; //Size: 0x0070
static_assert(sizeof(c_base_scene_data) == 0x70);
static_assert(offsetof(c_base_scene_data, m_material) == 0x20);
static_assert(offsetof(c_base_scene_data, r) == 0x50);
