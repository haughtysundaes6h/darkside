#include <limits>
#include <cmath>
#include <algorithm>
#include "config_system.hpp"

#include "../../features/skins/skins.hpp"

#include <filesystem>
#include <fstream>

#include "../../sdk/includes/json/json.h"

void c_config_system::setup_values( )
{
	push_item( &g_cfg->rage_bot.m_enabled, "rage_bot", "enabled", g_cfg->rage_bot.m_enabled );
	push_item( &g_cfg->rage_bot.m_enabled, "rage_bot", "silent", g_cfg->rage_bot.m_silent );

	for ( auto hitbox = 0; hitbox < 6; hitbox++ ) {
		push_item( &g_cfg->rage_bot.m_hitboxes[ hitbox ], "rage_bot", std::format( "hitbox_{0}", hitbox ), g_cfg->rage_bot.m_hitboxes[ hitbox ] );
		push_item( &g_cfg->rage_bot.m_hitboxes[ hitbox ], "rage_bot", std::format( "multi_points_{0}", hitbox ), g_cfg->rage_bot.m_hitboxes[ hitbox ] );
		push_item( &g_cfg->rage_bot.m_hitbox_scale[ hitbox ], "rage_bot", std::format( "multi_points_scale_{0}", hitbox ), g_cfg->rage_bot.m_hitbox_scale[ hitbox ] );
	}

	push_item( &g_cfg->rage_bot.m_static_scale, "rage_bot", "static_scale", g_cfg->rage_bot.m_static_scale );		

	push_item( &g_cfg->rage_bot.m_minimum_damage, "rage_bot", "minimum_damage", g_cfg->rage_bot.m_minimum_damage );
	push_item( &g_cfg->rage_bot.m_minimum_damage_override, "rage_bot", "minimum_damage_override", g_cfg->rage_bot.m_minimum_damage_override );
	push_item( &g_cfg->rage_bot.m_override_damage_key_bind, "rage_bot", "override_damage_keybind", g_cfg->rage_bot.m_override_damage_key_bind );
	push_item( &g_cfg->rage_bot.m_override_damage_key_bind_style, "rage_bot", "override_damage_keybind_style", g_cfg->rage_bot.m_override_damage_key_bind_style );
	push_item( &g_cfg->rage_bot.m_hit_chance, "rage_bot", "hit_chance", g_cfg->rage_bot.m_hit_chance );
	push_item( &g_cfg->rage_bot.m_auto_stop, "rage_bot", "auto_stop", g_cfg->rage_bot.m_auto_stop );

	push_item( &g_cfg->anti_hit.m_enabled, "anti_aim", "enabled", g_cfg->anti_hit.m_enabled );
	push_item( &g_cfg->anti_hit.m_at_target, "anti_aim", "at_target", g_cfg->anti_hit.m_at_target );
	push_item( &g_cfg->anti_hit.m_override_pitch, "anti_aim", "override_pitch", g_cfg->anti_hit.m_override_pitch );
	push_item( &g_cfg->anti_hit.m_pitch_amount, "anti_aim", "pitch_amount", g_cfg->anti_hit.m_pitch_amount );
	push_item( &g_cfg->anti_hit.m_override_yaw, "anti_aim", "override_yaw", g_cfg->anti_hit.m_override_yaw );
	push_item( &g_cfg->anti_hit.m_yaw_amount, "anti_aim", "yaw_amount", g_cfg->anti_hit.m_yaw_amount );
	push_item( &g_cfg->anti_hit.m_jitter_amount, "anti_aim", "jitter_amount", g_cfg->anti_hit.m_jitter_amount );

	push_item( &g_cfg->visuals.m_player_esp.m_bounding_box, "player_esp", "bounding_box", g_cfg->visuals.m_player_esp.m_bounding_box );
	push_item( &g_cfg->visuals.m_player_esp.m_health_bar, "player_esp", "health_bar", g_cfg->visuals.m_player_esp.m_health_bar );

	push_item( &g_cfg->world_esp.m_enable_thirdperson, "thirdperson", "enabled", g_cfg->world_esp.m_enable_thirdperson );
	push_item( &g_cfg->world_esp.m_distance, "thirdperson", "distance", g_cfg->world_esp.m_distance );
	push_item( &g_cfg->world_esp.m_thirdperson_key_bind, "thirdperson", "thirdperson_keybind", g_cfg->world_esp.m_thirdperson_key_bind );
	push_item( &g_cfg->world_esp.m_thirdperson_key_bind_style, "thirdperson", "thirdperson_keybind_style", g_cfg->world_esp.m_thirdperson_key_bind_style );

	push_item( &g_cfg->skins.m_agents.m_selected_ct, "agent_changer", "selected_ct", g_cfg->skins.m_agents.m_selected_ct );
	push_item( &g_cfg->skins.m_agents.m_selected, "agent_changer", "selected_t", g_cfg->skins.m_agents.m_selected );

	push_item( &g_cfg->world.m_exposure, "world", "exposure", g_cfg->world.m_exposure );
	push_item( &g_cfg->world.m_lighting, "world", "lighting", g_cfg->world.m_lighting );
	push_item( &g_cfg->world.m_render_fog, "world", "render_fog", g_cfg->world.m_render_fog );
	push_item( &g_cfg->world.m_sky, "world", "sky_color", g_cfg->world.m_sky );
	push_item( &g_cfg->world.m_sky_clouds, "world", "sky_clouds_color", g_cfg->world.m_sky_clouds );
	push_item( &g_cfg->world.m_wall, "world", "m_wall_color", g_cfg->world.m_wall );

	static std::vector<const char*> weapons{};

	if ( weapons.size( ) < g_skins->m_dumped_items.size( ) )
		for ( auto& item : g_skins->m_dumped_items )
			weapons.emplace_back( item.m_name );

	push_item( &g_cfg->skins.m_selected_weapon, "skin_changer", "selected_weapon", g_cfg->skins.m_selected_weapon );

	for ( auto weapon = 0; weapon < weapons.size( ); weapon++ )
		push_item( &g_cfg->skins.m_skin_settings[ weapon ].m_paint_kit, "skin_changer", std::format( "paint_kit_{0}", weapon ), g_cfg->skins.m_skin_settings[ weapon ].m_paint_kit );

	push_item( &g_cfg->skins.m_knives.m_selected, "knife_changer", "selected", g_cfg->skins.m_knives.m_selected );
	push_item( &g_cfg->skins.m_gloves.m_selected, "glove_changer", "selected", g_cfg->skins.m_gloves.m_selected );
}

void c_config_system::push_item( bool* pointer, std::string category, std::string name, bool def_value )
{
	this->booleans.push_back( { pointer, category, name, def_value } );
}

void c_config_system::push_item( int* pointer, std::string category, std::string name, int def_value )
{
	this->ints.push_back( { pointer, category, name, def_value } );
}

void c_config_system::push_item( float* pointer, std::string category, std::string name, float def_value )
{
	this->floats.push_back( { pointer, category, name, def_value } );
}

void c_config_system::push_item( c_color* pointer, std::string category, std::string name, c_color def_value )
{
	this->colors_rgb.push_back( { pointer, category, name, def_value } );
}

inline bool FileExists( const std::string& name ) {
	struct stat buffer;
	return ( stat( name.c_str( ), &buffer ) == 0 );
}

using nlohmann::json;
void c_config_system::read( std::string path ) {
	path = "c:\\airflow\\configs\\" + path + ".cfg";

	if ( !FileExists( path ) )
		return;

	std::ifstream reader( path );
	if ( !reader )
		return;

	json data;
	try {
		reader >> data;
	}
	catch ( const json::exception& ) {
		return;
	}
	if ( !data.is_object() )
		return;

	auto value_of = [&data]( const std::string& category, const std::string& name ) -> const json* {
		const auto group = data.find( category );
		if ( group == data.end() || !group->is_object() )
			return nullptr;
		const auto value = group->find( name );
		return value == group->end() ? nullptr : &*value;
	};
	auto read_int = []( const json& value, int& out ) -> bool {
		if ( !value.is_number_integer() )
			return false;
		if ( value.is_number_unsigned() ) {
			const auto number = value.get<std::uint64_t>();
			if ( number > static_cast<std::uint64_t>( (std::numeric_limits<int>::max)() ) )
				return false;
			out = static_cast<int>( number );
		}
		else {
			const auto number = value.get<std::int64_t>();
			if ( number < (std::numeric_limits<int>::min)() || number > (std::numeric_limits<int>::max)() )
				return false;
			out = static_cast<int>( number );
		}
		return true;
	};

	for ( auto& b : booleans )
		if ( const auto* value = value_of( b.category, b.name ); value && value->is_boolean() )
			*b.pointer = value->get<bool>();

	for ( auto& i : ints ) {
		int value = 0;
		if ( const auto* field = value_of( i.category, i.name ); field && read_int( *field, value ) ) {
			if ( i.name.find( "key_bind_style" ) != std::string::npos || i.name.find( "keybind_style" ) != std::string::npos )
				*i.pointer = value >= 0 && value <= 2 ? value : 0;
			else if ( i.name.find( "key_bind" ) != std::string::npos || i.name.find( "keybind" ) != std::string::npos )
				*i.pointer = value >= 0 && value <= 0xA5 ? value : 0;
			else
				*i.pointer = value;
		}
	}

	for ( auto& f : floats )
		if ( const auto* value = value_of( f.category, f.name ); value && value->is_number() ) {
			const float number = value->get<float>();
			if ( std::isfinite( number ) )
				*f.pointer = number;
		}

	for ( auto& c : colors_rgb ) {
		const auto* color = value_of( c.category, c.name );
		if ( !color || !color->is_object() )
			continue;
		for ( const auto& channel : { std::pair{ "r", &c.pointer->r }, std::pair{ "g", &c.pointer->g },
			std::pair{ "b", &c.pointer->b }, std::pair{ "a", &c.pointer->a } } ) {
			const auto value = color->find( channel.first );
			if ( value != color->end() && value->is_number() ) {
				const float number = value->get<float>();
				if ( std::isfinite( number ) )
					*channel.second = number;
			}
		}
	}

	for ( auto& i : keybinds )
		if ( const auto* value = value_of( i.category, i.name ); value ) {
			int key = 0;
			if ( read_int( *value, key ) )
				*i.pointer = key >= 0 && key <= 0xA5 ? key : 0;
		}
	// Older configs stored zero alpha for World colors whose tint is opaque.
	g_cfg->world.m_sky.a = 1.f;
	g_cfg->world.m_sky_clouds.a = 1.f;
	g_cfg->world.m_wall.a = 1.f;
	g_cfg->world.m_lighting.a = 1.f;
}

void c_config_system::save( std::string path ) {
	std::error_code directory_error;
	std::filesystem::create_directories( "c:\\airflow\\configs\\", directory_error );
	if ( directory_error )
		return;
	path = "c:\\airflow\\configs\\" + path + ".cfg";

	json ret_data;
	auto sanity_checks = [&]( std::string category ) -> void {
		if ( ret_data.find( category ) == ret_data.end( ) )
			ret_data[ category ] = json( );
	};

	for ( auto& b : booleans ) {
		sanity_checks( b.category );
		ret_data[ b.category ][ b.name ] = *b.pointer;
	}

	for ( auto& i : ints ) {
		sanity_checks( i.category );
		ret_data[ i.category ][ i.name ] = *i.pointer;
	}

	for ( auto& f : floats ) {
		sanity_checks( f.category );
		ret_data[ f.category ][ f.name ] = *f.pointer;
	}

	for ( auto& c : colors_rgb ) {
		sanity_checks( c.category );
		json d;
		c_color clr = *c.pointer;
		d[ "r" ] = clr.r;
		d[ "g" ] = clr.g;
		d[ "b" ] = clr.b;
		d[ "a" ] = clr.a;
		ret_data[ c.category ][ c.name ] = d;
	}

	for ( auto& i : keybinds ) {
		sanity_checks( i.category );
		ret_data[ i.category ][ i.name ] = *i.pointer;
	}

	std::ofstream writer( path, std::ios::trunc );
	if ( writer )
		writer << std::setw( 4 ) << ret_data << std::endl;
}

void c_config_system::refresh( ) {
	config_files.clear( );
	std::error_code error;
	if ( !std::filesystem::exists( "c:\\airflow\\configs\\", error ) )
		return;
	for ( const auto& p : std::filesystem::directory_iterator( "c:\\airflow\\configs\\", error ) ) {
		if ( !std::filesystem::is_directory( p ) &&
			p.path( ).extension( ).string( ) == ".cfg" ) {
			auto file = p.path( ).filename( ).string( );
			config_files.push_back( file.substr( 0, file.size( ) - 4 ) );
		}
	}
	std::sort(config_files.begin(), config_files.end());
}

void c_config_system::remove( std::string path ) {
	path = "c:\\airflow\\configs\\" + path + ".cfg";

	std::filesystem::remove( path );
	refresh( );
}

void c_config_system::reset( ) {
	for ( auto& b : booleans )
		*b.pointer = b.default_value;
	for ( auto& i : ints )
		*i.pointer = i.default_value;
	for ( auto& c : colors_rgb )
		*c.pointer = c.default_value;
	for ( auto& f : floats )
		*f.pointer = f.default_value;
	for ( auto& i : keybinds )
		*i.pointer = i.default_value;
}

static std::string selected_cfg = "";
bool confirm_opened = false;
bool confirmed = false;
bool delete_cfg = false;

void confirm_save( bool deleting )
{
	confirm_opened = true;
	delete_cfg = deleting;
}

void final_save( )
{
	if ( delete_cfg )
		g_config_system->remove( selected_cfg );
	else
		g_config_system->save( selected_cfg );


	confirm_opened = false;
	confirmed = true;
}

void no_confirm( )
{
	confirm_opened = false;
	confirmed = false;
}

void window( )
{
	if ( !confirm_opened )
		return;

	ImGui::Begin( "confirmation", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar );
	ImGui::SetWindowSize( ImVec2( 240, 145 ) );
	ImGui::SetCursorPosY( 39 );

	if ( confirm_opened )
		delete_cfg ? ImGui::Text( "are you sure want to delete this config?" ) : ImGui::Text( "are you sure want to save this config?" );

	ImGui::SetCursorPosX( 4 );
	ImGui::SetCursorPosY( 61 );

	if ( confirm_opened && ImGui::Button( delete_cfg ? "delete" : "save" ) )
		final_save( );

	ImGui::SetCursorPosY( 86 );
	ImGui::SetCursorPosX( 4 );

	if ( confirm_opened && ImGui::Button( "cancel" ) )
		no_confirm( );

	ImGui::End( );
}

void c_config_system::menu( ) {
	refresh( );

	static char cfg_name[ 26 ];

	if ( ImGui::BeginChild( "Settings" ) ) {
		ImGui::ListBoxHeader( "##cfglist", ImVec2( -1, -205 ) );

		for ( auto cfg : config_files )
			if ( ImGui::Selectable( cfg.c_str( ), cfg == selected_cfg ) )
				selected_cfg = cfg;

		ImGui::ListBoxFooter( );
		ImGui::SetCursorPosY( ImGui::GetWindowSize( ).y - 193 );
		if ( ImGui::InputText( " ", cfg_name, 26 ) ) selected_cfg = std::string( cfg_name );
		if ( std::string( cfg_name ) != "" )
		{
			ImGui::SetCursorPosX( 4 );
			if ( ImGui::Button( "create" ) ) {

				if ( !selected_cfg.empty( ) )
					save( selected_cfg );

				refresh( );
			}
		}
		ImGui::SetCursorPosX( 4 );
		if ( ImGui::Button( "load" ) ) {
			if ( !selected_cfg.empty( ) )
				read( selected_cfg );

			refresh( );
		}
		ImGui::SetCursorPosX( 4 );
		if ( ImGui::Button( "save" ) ) {
			if ( !selected_cfg.empty( ) )
				confirm_save( false );

			refresh( );
		}

		window( );

		ImGui::SetCursorPosX( 4 );
		if ( ImGui::Button( "delete") ) {
			if ( !selected_cfg.empty( ) )
				confirm_save( true );

			refresh( );
		}
		ImGui::SetCursorPosX( 4 );
		if ( ImGui::Button( "reset" ) )
			reset( );

		ImGui::SetCursorPosX( 4 );
		if ( ImGui::Button( "refresh" ) )
			refresh( );

		ImGui::EndChild( );
	}
}