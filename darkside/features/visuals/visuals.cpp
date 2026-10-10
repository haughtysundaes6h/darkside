#include "visuals.hpp"
#include "../../render/render.hpp"
#include "../../entity_system/entity.hpp"
#include "../rage_bot/rage_bot.hpp"

bbox_t c_visuals::calculate_bbox( vec3_t bottom, vec3_t top )
{
	bbox_t out{};

	vec3_t screen_points[ 2 ]{};

	if ( g_render->world_to_screen( top, screen_points[ 1 ] ) && g_render->world_to_screen( bottom, screen_points[ 0 ] ) )
	{
		out.height = std::abs( ( screen_points[ 1 ] - screen_points[ 0 ] ).y + 6 );
		out.width = out.height / 2.0f;

		out.x = screen_points[ 1 ].x - out.width / 2.f;
		out.y = screen_points[ 1 ].y;

		out.m_found = true;
		return out;
	}

	return {};
}

void c_visuals::store_players() {
    if (!g_interfaces->m_engine->is_in_game() || !g_ctx->m_local_pawn) { clear(); return; }
    decltype(m_player_map) players;
    auto* local = g_ctx->m_local_pawn;
    for (auto* instance : g_entity_system->get("CCSPlayerController")) {
        auto* controller = reinterpret_cast<c_cs_player_controller*>(instance);
        if (!controller || controller == g_ctx->m_local_controller || !controller->m_pawn_is_alive()) continue;
        const auto handle = controller->m_pawn();
        if (!handle.is_valid()) continue;
        auto* pawn = g_interfaces->m_entity_system->get_base_entity<c_cs_player_pawn>(handle.get_entry_index());
        if (!pawn || !pawn->is_alive() || pawn == local ||
            (!g_cfg->visuals.m_player_esp.m_teammates && pawn->m_team_num() == local->m_team_num())) continue;
        auto* scene = pawn->m_scene_node();
        auto* collision = pawn->m_collision();
        if (!scene || !collision) continue;
        player_info_t info{};
        info.m_valid = true;
        info.m_handle = handle.to_int();
        info.m_bottom = scene->m_abs_origin() - vec3_t{0, 0, 1};
        info.m_top = info.m_bottom + vec3_t{0, 0, collision->m_maxs().z};
        info.m_health = pawn->m_health();
        const auto* name = controller->m_player_name();
        info.m_name = name ? name : "";
        if (auto* weapon = pawn->get_active_weapon()) {
            if (auto* data = weapon->get_weapon_data()) {
                info.m_ammo = weapon->m_clip1();
                info.m_max_ammo = data->m_max_clip1();
                if (const auto* name = data->m_name()) {
                    const auto* prefix = strstr(name, "weapon_");
                    info.m_weapon_name = prefix ? prefix + 7 : name;
                    std::transform(info.m_weapon_name.begin(), info.m_weapon_name.end(), info.m_weapon_name.begin(),
                        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                }
            }
        }
        if (g_cfg->visuals.m_player_esp.m_flags) {
            auto* items = pawn->m_item_services();
            if (pawn->m_armor_value() > 0) info.m_flags.emplace_back(items && items->m_has_helmet() ? "HK" : "K");
            if (pawn->m_scoped()) info.m_flags.emplace_back("ZOOM");
        }
        players.emplace(info.m_handle, std::move(info));
    }
    std::lock_guard lock(m_player_mutex);
    m_player_map.swap(players);
    m_local_scoped = local->is_alive() && local->m_scoped();
    m_snapshot_time = std::chrono::steady_clock::now();
}

void c_visuals::handle_players( ) {
	for ( auto it = m_player_map.begin( ); it != m_player_map.end( ); it = std::next( it ) ) {
		player_info_t& player_info = it->second;
		if ( !player_info.m_valid )
			continue;

		const bbox_t bbox = calculate_bbox(player_info.m_bottom, player_info.m_top);
		if (bbox.m_found) {

			static const c_color outline_color = c_color{ 0.f, 0.f, 0.f, 1.f };

			if ( g_cfg->visuals.m_player_esp.m_bounding_box ) {
				const c_color main_color = c_color{ 1.f, 1.f, 1.f, 1.f };

				g_render->rect_outline( { bbox.x - 1, bbox.y - 1 }, { bbox.x + bbox.width + 1, bbox.y + bbox.height + 1 }, outline_color );
				g_render->rect_outline( { bbox.x, bbox.y }, { bbox.x + bbox.width, bbox.y + bbox.height }, main_color );
				g_render->rect_outline( { bbox.x + 1, bbox.y + 1 }, { bbox.x + bbox.width - 1, bbox.y + bbox.height - 1 }, outline_color );
			}
			if (g_cfg->visuals.m_player_esp.m_flags) {
				float y = bbox.y;
				for (const auto& flag : player_info.m_flags) {
					g_render->text({bbox.x + bbox.width + 4.f, y}, {1.f,1.f,1.f,1.f},
						font_flags_outline, g_render->fonts.onetap_pixel, flag, g_render->fonts.onetap_pixel->FontSize);
					y += 10.f;
				}
			}
			if ( g_cfg->visuals.m_player_esp.m_health_bar ) {
				c_color full_hp_color = c_color{ 0.69f, 1.f, 0.31f, 1.f };
				c_color low_hp_color = c_color{ 1.f, 0.2f, 0.31f, 1.f };

				int esp_health = std::clamp( player_info.m_health, 0, 100 );
				float esp_step = std::clamp( static_cast<float>( esp_health ) / 100.f, 0.f, 1.f );

				float height = ( bbox.height - ( ( bbox.height * static_cast<float>( esp_health ) ) / 100.f ) );
				height = std::max<float>( std::min<float>( height, bbox.height ), 0.f );

				vec3_t bb_outline_min{ bbox.x - 5.f, bbox.y - 1.f };
				vec3_t bb_outline_max{ 3.f, bbox.height + 2.f };

				vec3_t bb_min{ bbox.x - 4.f, bbox.y + height };
				vec3_t bb_max{ 2.f, bbox.height - height };

				g_render->rect( bb_outline_min, bb_outline_min + bb_outline_max, outline_color );
				g_render->rect( bb_min, bb_min + bb_max, low_hp_color.lerp( full_hp_color, esp_step ) );

				// TO-DO: fix position and color (@evj_k)
				/*if ( esp_health > 0 && esp_health < 100 ) {
					ImGui::PushFont( g_render->fonts.onetap_pixel );
					auto text_size = ImGui::CalcTextSize( std::to_string( esp_health ).data( ) );
					vec3_t text_pos{ bbox.x - 7.f, bbox.y + bbox.height / 2.f - height - text_size.y };

					g_render->text( text_pos, c_color{ 1.f, 1.f, 1.f, 1.f }, font_flags_center | font_flags_dropshadow,
						g_render->fonts.onetap_pixel, std::to_string( esp_health ).data( ), 1 );

					ImGui::PopFont( );
				}*/
			}

			if ( g_cfg->visuals.m_player_esp.m_name ) {
				const c_color main_color = g_cfg->visuals.m_player_esp.m_name_color;

				ImGui::PushFont( g_render->fonts.verdana_small );
				auto text_size = ImGui::CalcTextSize( player_info.m_name.c_str( ) );

				g_render->text( vec3_t( bbox.x + bbox.width / 2.f, bbox.y - text_size.y - 3.f ),
					main_color, font_flags_center | font_flags_dropshadow,
					g_render->fonts.verdana_small, player_info.m_name,
					g_render->fonts.verdana_small->FontSize );

				ImGui::PopFont( );
			}

			if ( g_cfg->visuals.m_player_esp.m_weapon ) {
				const auto main_color = c_color{ 1.f, 1.f, 1.f, 1.f };

				auto full_ammo_color = c_color{ 0.67f, 0.725f, 0.98f, 1.f };
				auto low_ammo_color = c_color{ 1.f, 33.f, 33.f, 1.f };

				float text_offset = 0.f;
				if ( player_info.m_ammo > 0 && player_info.m_max_ammo > 0 ) {
					float esp_step = std::clamp( static_cast<float>( player_info.m_ammo ) / static_cast<float>( player_info.m_max_ammo ), 0.f, 1.f );

					float width = ( ( bbox.width * static_cast<float>( player_info.m_ammo ) ) / static_cast<float>( player_info.m_max_ammo ) );
					width = std::max<float>( std::min<float>( width, bbox.width ), 0.f );

					vec3_t bb_outline_min{ bbox.x - 1.f, bbox.y + bbox.height + 2.f };
					vec3_t bb_outline_max{ bbox.width + 2.f, 2.f };

					vec3_t bb_min{ bbox.x, bbox.y + bbox.height + 2.f };
					vec3_t bb_max{ width, 2.f };

					g_render->rect( bb_outline_min, bb_outline_min + bb_outline_max, outline_color );
					g_render->rect( bb_min, bb_min + bb_max, low_ammo_color.lerp( full_ammo_color, esp_step ) );

					text_offset += 5.f;
				}

				g_render->text( vec3_t( bbox.x + bbox.width / 2.f, bbox.y + bbox.height + text_offset + 3.f ), main_color, font_flags_center | font_flags_outline,
					g_render->fonts.onetap_pixel, player_info.m_weapon_name, g_render->fonts.onetap_pixel->FontSize );
			}
		}
	}
}

void c_visuals::on_present( ) {
	std::lock_guard lock(m_player_mutex);
	if (std::chrono::steady_clock::now() - m_snapshot_time > std::chrono::milliseconds(500)) return;
	handle_players( );
}
void c_visuals::clear() { std::lock_guard lock(m_player_mutex); m_player_map.clear(); m_snapshot_time = {}; m_local_scoped = false; }
bool c_visuals::local_scoped() { std::lock_guard lock(m_player_mutex); return m_local_scoped && std::chrono::steady_clock::now() - m_snapshot_time < std::chrono::milliseconds(500); }
