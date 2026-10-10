#pragma once

#include "../../modules/modules.hpp"
#include "../../../utils/utils.hpp"
#include "../../cs2_build.hpp"

class c_base_entity;
class c_cs_player_pawn;
class c_cs_player_controller;

class i_entity_system {
public:
	template <class C = c_base_entity>
	C* get_base_entity( int index ) {
		static auto get_client_entity = reinterpret_cast<C * ( __fastcall* )( i_entity_system*, int )>( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "4C 8D 49 ? 81 FA" ) );

		return get_client_entity( this, index );
	}

	c_cs_player_pawn* get_local_pawn( );

	c_cs_player_controller* get_local_controller( ) {
		const auto base = g_modules->m_modules.client_dll.get();
		return base ? *reinterpret_cast<c_cs_player_controller**>(base + cs2_build::local_player_controller) : nullptr;
	}

};
