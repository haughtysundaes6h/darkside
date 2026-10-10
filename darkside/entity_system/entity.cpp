#include "entity.hpp"

void c_entity_system::initialize( ) {
    if (!g_interfaces->m_entity_system)
        return;
    decltype(entities) current;

    // +0x2120 is the live entity count. Slots can be sparse, so it cannot
    // bound this initial scan; subsequent changes arrive through add/remove.
    for ( int i = 0; i < 0x7fff; i++ ) {
        c_entity_instance* entity = g_interfaces->m_entity_system->get_base_entity( i );

        if ( !entity )
            continue;

        const char* class_name = entity->get_entity_class_name( );
        if ( !class_name )
            continue;
        current[ fnv1a::hash_64( class_name ) ].emplace_back( entity );
    }
    entities.swap(current);
}

void c_entity_system::level_init( ) {
    entities.clear( );
    initialize( );
}

void c_entity_system::level_shutdown( ) {
    entities.clear( );
}

void c_entity_system::add_entity( c_entity_instance* entity, int index ) {
    if ( !entity )
        return;
    const char* className = entity->get_entity_class_name( );
    if ( !className )
        return;

	auto& group = entities[ fnv1a::hash_64( className ) ];
	if (std::find(group.begin(), group.end(), entity) == group.end())
		group.emplace_back(entity);
}

void c_entity_system::remove_entity( c_entity_instance* entity, int index ) {
    if ( !entity )
        return;
    const char* class_name = entity->get_entity_class_name( );
    if ( !class_name )
        return;

    auto it = entities.find( fnv1a::hash_64( class_name ) );
    if ( it == entities.end( ) )
        return;

    std::erase(it->second, entity);
}

std::vector<c_entity_instance*> c_entity_system::get( const char* name ) {
    return entities[ fnv1a::hash_64( name ) ];
}



std::vector<c_cs_player_pawn*> c_entity_system::get_players( int index ) {
    std::vector<c_cs_player_pawn*> enteties;

    c_cs_player_pawn* local_player = g_ctx->m_local_pawn;
    if ( !local_player )
        return enteties;

    int local_team_num = local_player->m_team_num( );

    std::vector<c_entity_instance*> entities = get( "C_CSPlayerPawn" );
    for ( c_entity_instance* entity : entities ) {
        c_cs_player_pawn* player_pawn = reinterpret_cast<c_cs_player_pawn*>( entity );

        int team_num = player_pawn->m_team_num( );

        if ( index == ENTITY_ALL || ( ( index == ENTITY_ENEMIES_ONLY && local_team_num != team_num ) || ( index == ENTITY_TEAMMATES_ONLY && local_team_num == team_num && local_player != player_pawn ) ) )
            enteties.emplace_back( player_pawn );
    }

    return enteties;
}

std::vector<player_controller_pair> c_entity_system::get_players_with_controller( int index ) {
    std::vector<player_controller_pair> return_enteties;

    c_cs_player_pawn* local_player = g_ctx->m_local_pawn;
    if ( !local_player )
        return return_enteties;

    int local_team_num = local_player->m_team_num( );

    std::vector<c_entity_instance*> entities = get( "CCSPlayerController" );
    for ( auto entity : entities )
    {
        c_cs_player_controller* controller = reinterpret_cast<c_cs_player_controller*>( entity );
        if ( !controller )
            continue;

        c_cs_player_pawn* player_pawn = reinterpret_cast<c_cs_player_pawn*>( g_interfaces->m_entity_system->get_base_entity( controller->m_pawn( ).get_entry_index( ) ) );
        if ( !player_pawn )
            continue;

        int team_num = player_pawn->m_team_num( );

        if ( index == ENTITY_ALL || ( ( index == ENTITY_ENEMIES_ONLY && local_team_num != team_num ) || ( index == ENTITY_TEAMMATES_ONLY && local_team_num == team_num && local_player != player_pawn ) ) )
            return_enteties.emplace_back( std::make_pair( controller, player_pawn ) );
    }

    return return_enteties;
}
