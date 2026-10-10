#pragma once

#include <cstdint>

// Windows x64 client.dll installed on 2026-10-09. RVAs are checked against
// RIP-relative references and function bodies in this exact binary.
namespace cs2_build {
    inline constexpr std::uint32_t client_timestamp = 0x6AC7FFFA;
    inline constexpr std::uint32_t client_image_size = 0x2994000;
    inline constexpr std::uintptr_t csgo_input = 0x2572460;
    inline constexpr std::uintptr_t global_vars = 0x2228090;
    inline constexpr std::uintptr_t game_entity_system = 0x2542928;
    inline constexpr std::uintptr_t local_player_controller = 0x25338A8;
    inline constexpr std::uintptr_t get_field_of_view = 0x889770;
}
