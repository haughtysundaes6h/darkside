#include "protobufs.hpp"
#include <cstring>

bool c_protobuf::update_move_crc( c_user_cmd* user_cmd ) {
	if ( !user_cmd )
		return false;
	auto* base = user_cmd->pb.mutable_base( );
	if ( !base )
		return false;

	using string_copy_t = void( __fastcall* )( void*, const void*, int );
	using serialize_crc_t = void( __fastcall* )( void*, void*, void* );
	static auto string_copy = reinterpret_cast<string_copy_t>( g_opcodes->scan_absolute( g_modules->m_modules.client_dll.get_name( ),
		xorstr_( "E8 ? ? ? ? 0F 10 45 88" ), 0x1 ) );
	static auto serialize_crc = reinterpret_cast<serialize_crc_t>( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ),
		xorstr_( "48 89 5C 24 ? 55 56 57 48 83 EC 30 49 8B C0 48 8B FA 48 8B F1 48 8B 09 F6 C1 03" ) ) );
	if ( !string_copy || !serialize_crc )
		return false;

	std::uint8_t bytes[ 64 ]{};
	auto* out = bytes;
	if ( base->has_buttons_pb( ) ) {
		const auto& buttons = base->buttons_pb( );
		const std::uint64_t states[] = { buttons.buttonstate1( ), buttons.buttonstate2( ), buttons.buttonstate3( ) };
		std::uint8_t size = 0;
		for ( const auto state : states )
			if ( state ) size += 9;
		if ( size ) {
			*out++ = 0x1a;
			*out++ = size;
			for ( int i = 0; i < 3; ++i ) {
				if ( !states[ i ] ) continue;
				*out++ = static_cast<std::uint8_t>( 0x09 + i * 8 );
				std::memcpy( out, &states[ i ], 8 );
				out += 8;
			}
		}
	}
	if ( base->has_viewangles( ) ) {
		const auto& angles = base->viewangles( );
		const float values[] = { angles.x( ), angles.y( ), angles.z( ) };
		std::uint8_t size = 0;
		for ( const float value : values )
			if ( value != 0.f ) size += 5;
		if ( size ) {
			*out++ = 0x22;
			*out++ = size;
			for ( int i = 0; i < 3; ++i ) {
				if ( values[ i ] == 0.f ) continue;
				*out++ = static_cast<std::uint8_t>( 0x0d + i * 8 );
				std::memcpy( out, &values[ i ], 4 );
				out += 4;
			}
		}
	}

	auto* raw_base = reinterpret_cast<std::uint8_t*>( base );
	auto arena_bits = *reinterpret_cast<std::uintptr_t*>( raw_base + 0x08 );
	auto arena = arena_bits & ~std::uintptr_t{ 3 };
	if ( arena_bits & 1 ) {
		if ( !arena ) return false;
		arena = *reinterpret_cast<std::uintptr_t*>( arena );
	}
	alignas( 8 ) std::uint8_t message[ 0x20 ]{};
	string_copy( message, bytes, static_cast<int>( out - bytes ) );
	*reinterpret_cast<std::uint32_t*>( raw_base + 0x10 ) |= 1u;
	serialize_crc( raw_base + 0x30, message, reinterpret_cast<void*>( arena ) );
	// The game string helper allocates from g_pMemAlloc when the CRC payload
	// exceeds its 15-byte inline buffer.
	if ( *reinterpret_cast<const std::size_t*>( message + 0x18 ) > 15 ) {
		const auto allocation = *reinterpret_cast<void* const*>( message );
		if ( allocation && g_interfaces->m_mem_alloc )
			g_interfaces->m_mem_alloc->free( allocation );
	}
	return true;
}

CSubtickMoveStep* c_protobuf::add_subtick_move_step( c_user_cmd* user_cmd ) {
	if ( !user_cmd || !user_cmd->pb.has_base( ) )
		return nullptr;
	auto rept_field_move_steps = reinterpret_cast<google::protobuf::repeated_ptr_field_t<CSubtickMoveStep>*>( (PBYTE) user_cmd->pb.mutable_base( ) + 0x18 );
	if ( rept_field_move_steps->m_current_size < 0 || rept_field_move_steps->m_current_size >= 64 )
		return nullptr;

	if ( rept_field_move_steps->m_rep && rept_field_move_steps->m_current_size < rept_field_move_steps->m_rep->m_allocated_size ) {
		subtick_move_step = rept_field_move_steps->m_rep->m_elements[ rept_field_move_steps->m_current_size ];
		if ( !subtick_move_step )
			return nullptr;
		++rept_field_move_steps->m_current_size;
	}
	else
		subtick_move_step = create_new_subtick_move_step( rept_field_move_steps, rept_field_move_steps->m_arena );

	if (subtick_move_step) {
		subtick_move_step->clear_button();
		subtick_move_step->clear_pressed();
		subtick_move_step->clear_when();
		subtick_move_step->clear_analog_forward_delta();
		subtick_move_step->clear_analog_left_delta();
		subtick_move_step->clear_pitch_delta();
		subtick_move_step->clear_yaw_delta();
	}
	return subtick_move_step;
}

void c_protobuf::sort_subtick_move_steps( c_user_cmd* user_cmd ) {
	if ( !user_cmd || !user_cmd->pb.has_base() )
		return;
	auto* field = reinterpret_cast<google::protobuf::repeated_ptr_field_t<CSubtickMoveStep>*>(
		reinterpret_cast<std::uint8_t*>( user_cmd->pb.mutable_base() ) + 0x18 );
	if ( !field->m_rep || field->m_current_size < 2 || field->m_current_size > 64 ||
		field->m_current_size > field->m_rep->m_allocated_size )
		return;
	auto* steps = field->m_rep->m_elements;
	for ( int i = 1; i < field->m_current_size; ++i ) {
		if ( !steps[i] )
			return;
		auto* current = steps[i];
		int j = i;
		while ( j > 0 && steps[j - 1] && steps[j - 1]->when() > current->when() ) {
			steps[j] = steps[j - 1];
			--j;
		}
		steps[j] = current;
	}
}

CSGOInputHistoryEntryPB* c_protobuf::add_input_history( c_user_cmd* user_cmd ) {
	if ( !user_cmd )
		return nullptr;
	auto rept_field_input_history = reinterpret_cast<google::protobuf::repeated_ptr_field_t<CSGOInputHistoryEntryPB>*>( reinterpret_cast<std::uint8_t*>( &user_cmd->pb ) + 0x18 );
	if ( rept_field_input_history->m_current_size < 0 || rept_field_input_history->m_current_size >= 64 )
		return nullptr;

	if ( rept_field_input_history->m_rep && rept_field_input_history->m_current_size < rept_field_input_history->m_rep->m_allocated_size )
		input_history_entry = rept_field_input_history->m_rep->m_elements[ rept_field_input_history->m_current_size++ ];
	else
		input_history_entry = create_new_input_history( rept_field_input_history, rept_field_input_history->m_arena );

	return input_history_entry;
}

CSGOInputHistoryEntryPB* c_protobuf::create_new_input_history( google::protobuf::repeated_ptr_field_t<CSGOInputHistoryEntryPB>* rept_ptr, void* arena ) {
	static auto fn_create_new_input_history = reinterpret_cast<CSGOInputHistoryEntryPB * ( __fastcall* )( void* )>( g_opcodes->scan_absolute( g_modules->m_modules.client_dll.get_name( ), xorstr_( "E8 ? ? ? ? 48 8B D0 48 8D 4E ? E8 ? ? ? ? 4C 8B F0" ), 0x1 ) );
	static auto fn_add_element_to_rep_field_container = reinterpret_cast<CSGOInputHistoryEntryPB * ( __fastcall* )( google::protobuf::repeated_ptr_field_t<CSGOInputHistoryEntryPB>*, void* )>( g_opcodes->scan_absolute( g_modules->m_modules.client_dll.get_name( ), xorstr_( "E8 ? ? ? ? 48 8B D0 48 8D 4E ? E8 ? ? ? ? 4C 8B F0" ), 0xD ) );

	if ( !fn_create_new_input_history || !fn_add_element_to_rep_field_container )
		return nullptr;
	auto input_history = fn_create_new_input_history( arena );
	if ( !input_history )
		return nullptr;

	return fn_add_element_to_rep_field_container( rept_ptr, input_history );
}

CSubtickMoveStep* c_protobuf::create_new_subtick_move_step( google::protobuf::repeated_ptr_field_t<CSubtickMoveStep>* rept_ptr, void* arena ) {
	static auto fn_create_new_subtick_move_step = reinterpret_cast<CSubtickMoveStep * ( __fastcall* )( void* )>( g_opcodes->scan_absolute( g_modules->m_modules.client_dll.get_name( ), xorstr_( "48 8B 54 CA 08 8D 41 01 89 47 08 EB 16 48 8B 0F E8 ? ? ? ? 48 8B D0 48 8B CF" ), 0x11 ) );
	static auto fn_add_element_to_rep_field_container = reinterpret_cast<CSubtickMoveStep * ( __fastcall* )( google::protobuf::repeated_ptr_field_t<CSubtickMoveStep>*, void* )>( g_opcodes->scan_absolute( g_modules->m_modules.client_dll.get_name( ), xorstr_( "E8 ? ? ? ? 4C 8B D0 45 8B 4A 10" ), 0x1 ) );

	if ( !fn_create_new_subtick_move_step || !fn_add_element_to_rep_field_container )
		return nullptr;
	auto subtick_move = fn_create_new_subtick_move_step( arena );
	if ( !subtick_move )
		return nullptr;

	return fn_add_element_to_rep_field_container( rept_ptr, subtick_move );
}
