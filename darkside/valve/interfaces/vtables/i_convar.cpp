#include "i_convar.hpp"
#include <cstdint>
#include "../../../sdk/includes/hash.hpp"

var_iterator_t i_cvar::get_first_var_iterator( ) {
    return m_head;
}

var_iterator_t i_cvar::get_next_var( var_iterator_t previous ) {
    const auto capacity = static_cast<std::uint16_t>( m_allocation_count & 0x7FFF );
    const auto index = static_cast<std::uint16_t>( previous );
    return m_convars && index < capacity ? m_convars[index].next : 0xFFFF;
}

convar_t* i_cvar::get_by_index(var_iterator_t idx)
{
    const auto capacity = static_cast<std::uint16_t>( m_allocation_count & 0x7FFF );
    const auto index = static_cast<std::uint16_t>( idx );
    return m_convars && index < capacity ? m_convars[index].data : nullptr;
}

convar_t* i_cvar::get_by_name(const char* name)
{
    if ( !name || !m_convars )
        return nullptr;

    const auto capacity = static_cast<std::uint16_t>( m_allocation_count & 0x7FFF );
    for (std::uint16_t current = m_head, visited = 0;
         current != 0xFFFF && current < capacity && visited < capacity;
         current = m_convars[current].next, ++visited) {
        convar_t* convar = m_convars[current].data;
        if (convar == nullptr || convar->m_name == nullptr)
            continue;

        if (fnv1a::hash_64(convar->m_name) == fnv1a::hash_64(name))
            return convar;
    }

    return nullptr;
}

void i_cvar::unlock_hidden_vars( ) {
    var_iterator_t it = get_first_var_iterator( );
	const auto capacity = static_cast<std::uint16_t>( m_allocation_count & 0x7FFF );

    for ( std::uint16_t visited = 0; it != 0xFFFF && visited < capacity; ++visited ) {
        convar_t* var = get_by_index( it );
		if ( var )
			var->m_flags &= ~( static_cast<std::uint64_t>( FCVAR_HIDDEN ) | FCVAR_DEVELOPMENTONLY );

        it = get_next_var( it );
    }
}
