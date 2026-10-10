#pragma once

#include "../../sdk/typedefs/vec_t.hpp"

class c_view_setup {
private:
    std::byte pad_0000[ 0x04A0 ];
public:
    vec3_t m_origin; // 0x04A0 in client build 0x6AB6D1DB
private:
    std::byte pad_04AC[ 0xC ];
public:
    vec3_t m_angles; // 0x04B8 in client build 0x6AB6D1DB
};

static_assert( offsetof( c_view_setup, m_origin ) == 0x4A0 );
static_assert( offsetof( c_view_setup, m_angles ) == 0x4B8 );
