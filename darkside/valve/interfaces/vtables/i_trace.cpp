#include "i_trace.hpp"

#include "../../classes/c_cs_player_pawn.hpp"

int game_trace_t::get_hitbox_id() 
{
	if (m_hitbox_data)
		return m_hitbox_data->m_hitbox_id;

	return 0;
}

int game_trace_t::get_hit_group()
{
	if (m_hitbox_data)
		return m_hitbox_data->m_hit_group;

	return 0;
}

trace_filter_t::trace_filter_t(std::uint64_t mask, c_cs_player_pawn* entity, c_cs_player_pawn* player, int layer, int type)
{
	static auto fn = reinterpret_cast<void(__fastcall*)(trace_filter_t*, void*, std::uint64_t, int)>(
		g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 0F B6 41 ? 33 FF 24"));
	if (fn)
		fn(this, entity ? entity : player, mask, layer);
	m_trace_type = static_cast<std::uint8_t>(type);
}
