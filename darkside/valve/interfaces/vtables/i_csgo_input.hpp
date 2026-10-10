#pragma once
#include "../../../sdk/typedefs/vec_t.hpp"
#include "../../../sdk/vfunc/vfunc.hpp"
#include "../../../utils/utils.hpp"

#include "usercmd.pb.h"
#include "cs_usercmd.pb.h"
#include "network_connection.pb.h"
#include "networkbasetypes.pb.h"

#define	FL_ONGROUND				(1 << 0)
#define FL_DUCKING				(1 << 1)
#define	FL_WATERJUMP			(1 << 3)
#define FL_ONTRAIN				(1 << 4)
#define FL_INRAIN				(1 << 5)
#define FL_FROZEN				(1 << 6)
#define FL_ATCONTROLS			(1 << 7)
#define	FL_CLIENT				(1 << 8)
#define FL_FAKECLIENT			(1 << 9)
#define	FL_INWATER				(1 << 10)
#define FL_HIDEHUD_SCOPE		(1 << 11)

enum e_button : std::uint32_t
{
    IN_ATTACK = (1 << 0),
    IN_JUMP = (1 << 1),
    IN_DUCK = (1 << 2),
    IN_FORWARD = (1 << 3),
    IN_BACK = (1 << 4),
    IN_USE = (1 << 5),
    IN_CANCEL = (1 << 6),
    IN_LEFT = (1 << 7),
    IN_RIGHT = (1 << 8),
    IN_MOVELEFT = (1 << 9),
    IN_MOVERIGHT = (1 << 10),
    IN_ATTACK2 = (1 << 11),
    IN_RUN = (1 << 12),
    IN_RELOAD = (1 << 13),
    IN_LEFT_ALT = (1 << 14),
    IN_RIGHT_ALT = (1 << 15),
    IN_SCORE = (1 << 16),
    IN_SPEED = (1 << 17),
    IN_WALK = (1 << 18),
    IN_ZOOM = (1 << 19),
    IN_FIRST_WEAPON = (1 << 20),
    IN_SECOND_WEAPON = (1 << 21),
    IN_BULLRUSH = (1 << 22),
    IN_FIRST_GRENADE = (1 << 23),
    IN_SECOND_GRENADE = (1 << 24),
    IN_MIDDLE_ATTACK = (1 << 25),
    IN_USE_OR_RELOAD = (1 << 26)
};

class c_in_button_state
{
public:
    void* __vfptr; //0x0000
    uint64_t m_button_state; //0x0008
    uint64_t m_button_state2; //0x0010
    uint64_t m_button_state3; //0x0018
}; //Size: 0x0020
static_assert(sizeof(c_in_button_state) == 0x20);

class c_user_cmd
{
public:
    void* __vfptr;
    std::int32_t m_command_number;
    std::int32_t pad_000C;
    CSGOUserCmdPB pb;
    c_in_button_state m_button_state;
    char pad_0078[8];
    double m_last_server_time;
    bool m_has_been_predicted;
    char pad_0089[3];
    std::int32_t m_cmd_flag;
    std::int32_t m_cmd_type;
    std::int32_t m_subtick_overwrite_type;
};
static_assert( sizeof( CSGOUserCmdPB ) == 0x48 );
static_assert( sizeof( c_user_cmd ) == 0x98 );
static_assert( offsetof( c_user_cmd, m_subtick_overwrite_type ) == 0x94 );

class i_csgo_input
{
public:
    std::uint64_t held_buttons() const {
        // B66720..B6677B reads current down at +258, previous at +250,
        // pressed/released at +260/+268; table 2230600 emits the masks.
        return *reinterpret_cast<const std::uint64_t*>(reinterpret_cast<const std::uint8_t*>(this)+0x258);
    }

    vec3_t get_view_angles()
    {
        using get_view_angles_t = void* (__fastcall*)(i_csgo_input*, int);
        static get_view_angles_t get_view_angles = reinterpret_cast<get_view_angles_t>(g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), "4C 8B C1 85 D2 74 ? 48 8D 05"));

        return *reinterpret_cast<vec3_t*>(get_view_angles(this, 0));
    }

    void set_view_angles(vec3_t& view_angles) 
    {
        using set_view_angles_t = void(__fastcall*)(i_csgo_input*, int, vec3_t&);
        static set_view_angles_t set_view_angles = reinterpret_cast<set_view_angles_t>(g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), "85 D2 75 ? 48 63 81"));

        set_view_angles(this, 0, view_angles);
    }

    c_user_cmd* get_user_cmd(void* local_controller, std::int32_t* sequence_out = nullptr)
    {
        if (sequence_out) *sequence_out = -1;
        if (!local_controller)
            return nullptr;

        using get_usercmd_base_fn = std::uint8_t*(__fastcall*)(void*);
        using get_usercmd_fn = c_user_cmd*(__fastcall*)(void*, std::int32_t);
        static auto get_usercmd_base = reinterpret_cast<get_usercmd_base_fn>(g_opcodes->scan_absolute(
            g_modules->m_modules.client_dll.get_name(), "48 83 EC 28 E8 ? ? ? ? 8B 80 10 59 00 00", 0x5));
        static auto get_usercmd = reinterpret_cast<get_usercmd_fn>(g_opcodes->scan(
            g_modules->m_modules.client_dll.get_name(), "40 53 48 83 EC 20 8B DA E8 ? ? ? ? 4C 8B C0"));

        if ( !get_usercmd_base || !get_usercmd )
            return nullptr;
        auto* cmd_base = get_usercmd_base(local_controller);
        if ( !cmd_base )
            return nullptr;
        const auto sequence = *reinterpret_cast<const std::int32_t*>(cmd_base + 0x5910);
        if (sequence_out) *sequence_out = sequence;
        if (sequence <= 0)
            return nullptr;
        return get_usercmd(local_controller, sequence);
    }
};
