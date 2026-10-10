#pragma once

#include "../../../sdk/vfunc/vfunc.hpp"
#include <cstddef>
#include <cstdint>

enum e_cvar_flags : int
{
	FCVAR_NONE = 0,
	FCVAR_UNREGISTERED = ( 1 << 0 ),
	FCVAR_DEVELOPMENTONLY = ( 1 << 1 ),
	FCVAR_GAMEDLL = ( 1 << 2 ),
	FCVAR_CLIENTDLL = ( 1 << 3 ),
	FCVAR_HIDDEN = ( 1 << 4 ),
	FCVAR_PROTECTED = ( 1 << 5 ),
	FCVAR_SPONLY = ( 1 << 6 ),
	FCVAR_ARCHIVE = ( 1 << 7 ),
	FCVAR_NOTIFY = ( 1 << 8 ),
	FCVAR_USERINFO = ( 1 << 9 ),
	FCVAR_CHEAT = ( 1 << 14 ),
	FCVAR_PRINTABLEONLY = ( 1 << 10 ),
	FCVAR_UNLOGGED = ( 1 << 11 ),
	FCVAR_NEVER_AS_STRING = ( 1 << 12 ),
	FCVAR_REPLICATED = ( 1 << 13 ),
	FCVAR_DEMO = ( 1 << 16 ),
	FCVAR_DONTRECORD = ( 1 << 17 ),
	FCVAR_RELOAD_MATERIALS = ( 1 << 20 ),
	FCVAR_RELOAD_TEXTURES = ( 1 << 21 ),
	FCVAR_NOT_CONNECTED = ( 1 << 22 ),
	FCVAR_MATERIAL_SYSTEM_THREAD = ( 1 << 23 ),
	FCVAR_ARCHIVE_XBOX = ( 1 << 24 ),
	FCVAR_ACCESSIBLE_FROM_THREADS = ( 1 << 25 ),
	FCVAR_SERVER_CAN_EXECUTE = ( 1 << 28 ),
	FCVAR_SERVER_CANNOT_QUERY = ( 1 << 29 ),
	FCVAR_CLIENTCMD_CAN_EXECUTE = ( 1 << 30 ),
	FCVAR_MATERIAL_THREAD_MASK = ( FCVAR_RELOAD_MATERIALS | FCVAR_RELOAD_TEXTURES | FCVAR_MATERIAL_SYSTEM_THREAD )
};

union convar_value_t
{
	bool m_i1;
	short m_i16;
	unsigned short m_u16;
	int m_i32;
	unsigned int m_u32;
	long long m_i64;
	unsigned long long m_u64;
	float m_fl;
	double m_db;
	const char* m_sz;
};

class convar_t {
public:
	const char* m_name;
	const void* m_default_value_ptr;
	const void* m_min_value;
	const void* m_max_value;
	const char* m_description;
	std::uint16_t m_type;
	char pad_002A[ 0x2 ];
	std::uint32_t m_change_count;
	std::uint64_t m_flags;
	char pad_0038[ 0x20 ];
	convar_value_t m_value;

	bool get_bool( ) const {
		return m_value.m_i1;
	}
	int get_int( ) const {
		return m_value.m_i32;
	}
	float get_float( ) const {
		return m_value.m_fl;
	}
};

static_assert( offsetof( convar_t, m_flags ) == 0x30 );
static_assert( offsetof( convar_t, m_value ) == 0x58 );

using var_iterator_t = unsigned long long;

template<typename T>
class c_utl_lean_vector {
public:
	T* data;
	std::uint16_t prev;
	std::uint16_t next;
};

struct convar_container_t {
	convar_t* data;
	std::uint16_t generation;
	std::uint16_t next;
	std::uint32_t links;
};

static_assert( sizeof( convar_container_t ) == 0x10 );

class i_cvar {
public:
	char pad_0000[ 0x4A ];
	std::uint16_t m_allocation_count;
	char pad_004C[ 0x4 ];
	convar_container_t* m_convars;
	std::uint16_t m_head;
	var_iterator_t get_first_var_iterator( );
	var_iterator_t get_next_var( var_iterator_t previous );

	convar_t* get_by_index( var_iterator_t idx );
	convar_t* get_by_name( const char* name );

	void unlock_hidden_vars( );
};

static_assert( offsetof( i_cvar, m_convars ) == 0x50 );
static_assert( offsetof( i_cvar, m_head ) == 0x58 );
