#pragma once

// ============================================================================
// ANDROMEDA SDK — fresh CS2 patterns & RVAs
// Source: https://andromeda-sdk.com  (/include/Patterns/patterns.json,
//                                 /include/offsets/offsets.hpp)
// Fetched 2026-10-11. Dump: 509 scanned / 504 found.
// Only rage-bot-relevant entries are kept here; the project sig-scans at
// runtime, so patterns stay valid across builds while raw RVAs (cs2_build.hpp)
// go stale.
// ============================================================================

#include <cstdint>

namespace andromeda {

    // ------------------------------------------------------------------------
    // PATTERNS (client.dll unless noted)
    // ------------------------------------------------------------------------

    // Aim punch removal (C_CSPlayerPawn -> QAngle). Used by rage bot.
    inline constexpr const char* get_removed_aim_punch =
        "E8 ? ? ? ? 4C 8B C0 48 8D 55 ? 48 8B CB E8 ? ? ? ? 48 8D 0D";

    // CCollisionProperty / trace bullet-penetration solver (v2).
    inline constexpr const char* handle_bullet_penetration_v2 =
        "48 8B C4 44 89 48 ? 48 89 50 ? 48 89 48 ? 55 57";

    // Weapon accuracy.
    inline constexpr const char* get_inaccuracy =
        "48 89 5C 24 ? 55 56 57 48 81 EC ? ? ? ? 44 0F 29 84 24";
    inline constexpr const char* get_spread =
        "48 63 91 ? ? ? ? 48 8B 81 ? ? ? ? 85 D2";
    inline constexpr const char* calc_spread =
        "48 8B C4 48 89 58 ? 48 89 68 ? 48 89 70 ? 57 41 54 41 55 41 56 41 57 48 81 EC ? ? ? ? 4C 63 EA";

    // CS2 deterministic spread RNG (also known as ComputeRandomSeed /
    // SpreadSeedGen — same body). We bind tier0 RandomSeed/RandomFloat instead,
    // but the pattern is kept for future no-spread work.
    inline constexpr const char* compute_random_seed =
        "48 89 5C 24 ? 57 48 81 EC ? ? ? ? ? ? ? ? 48 8D 8C 24";
    inline constexpr const char* shared_random_float =
        "4C 8B DC 49 89 5B 08 49 89 73 10 57 48 81 EC 00 01 00 00 8B 05 ? ? ? ? 48 8D 54 24 40";

    // Traces (scenesystem-independent game trace layer).
    inline constexpr const char* trace_shape =
        "48 89 54 24 ? 48 89 4C 24 ? 55 53 56 57 41 56 41 57 48 8D AC 24 ? ? ? ? B8";
    inline constexpr const char* game_trace_line =
        "4C 8B DC 49 89 5B 08 49 89 6B 10 49 89 73 18 57 48 81 EC ? ? ? ? 33 C0 0F 57 C0";
    inline constexpr const char* trace_get_info =
        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 81 EC ? ? ? ? 48 8B E9 0F 29 74 24";
    inline constexpr const char* game_trace_manager_ptr = // riprel global
        "48 8B 0D ? ? ? ? 48 8B D0 C7 44 24 ? 04 00 00 00 48 C7 44 24 ? 01 30 1C 00";

    // Hitboxes / bones.
    inline constexpr const char* get_hit_group =
        "40 53 48 83 EC 20 48 83 79 10 00 48 8B D9 74 16 E8 ? ? ? ? 84 C0 75 0D 48 8B 43 10 8B 40 38";
    inline constexpr const char* get_transforms_for_hitbox_list =
        "48 89 5C 24 20 55 56 57 41 54 41 55 48 81 EC ? ? ? ? 49 63 30 4D 8B E0";
    inline constexpr const char* get_hitbox_set =
        "48 89 5C 24 ? 48 89 74 24 ? 57 48 81 EC ? ? ? ? 8B DA 48 8B F9 E8 ? ? ? ? 48 8B F0";
    inline constexpr const char* lookup_bone =
        "40 53 48 83 EC ? 48 8B 89 ? ? ? ? 48 8B DA 48 8B 01 FF 50 ? 48 8B C8";

    // Input / angles.
    inline constexpr const char* set_view_angles =
        "85 D2 75 ? 48 63 81";
    inline constexpr const char* create_move =
        "48 8B C4 4C 89 40 18 48 89 48 08 55 53 41 54 41 55";

    // Movement.
    inline constexpr const char* process_movement = // engine-side, rel32 call
        "E8 ? ? ? ? 48 8B 06 48 8B CE FF 90 ? ? ? ? 48 85 DB";

    // ------------------------------------------------------------------------
    // INTERFACE RVAs (client.dll) — mirror of cs2_build.hpp values from the
    // same Andromeda dump. Kept for reference; the project resolves these via
    // its own checked RVAs (valve/cs2_build.hpp).
    // ------------------------------------------------------------------------
    namespace rva {
        inline constexpr std::uintptr_t csgo_input             = 0x23BEB20;
        inline constexpr std::uintptr_t input_ptr_global       = 0x20A4820;
        inline constexpr std::uintptr_t view_angles            = 0x23BF1A8;
        inline constexpr std::uintptr_t view_matrix            = 0x23AE550;
        inline constexpr std::uintptr_t global_vars            = 0x2094D38;
        inline constexpr std::uintptr_t entity_list            = 0x2554050;
        inline constexpr std::uintptr_t local_player_controller = 0x2383DB0;
        inline constexpr std::uintptr_t local_player_pawn      = 0x23A9118;
        inline constexpr std::uintptr_t prediction             = 0x23A9020;
        inline constexpr std::uintptr_t game_rules             = 0x1A55338;
        inline constexpr std::uintptr_t get_abs_origin         = 0x219F80;
    }
}
