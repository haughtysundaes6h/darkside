#include "auto_wall.hpp"

void c_auto_wall::scale_damage(int hitgroup, c_cs_player_pawn* entity, c_cs_weapon_base_v_data* weapon_data, float& damage)
{
	static convar_t* mp_damage_scale_ct_head = g_interfaces->m_var->get_by_name("mp_damage_scale_ct_head");
	static convar_t* mp_damage_scale_t_head = g_interfaces->m_var->get_by_name("mp_damage_scale_t_head");
	static convar_t* mp_damage_scale_ct_body = g_interfaces->m_var->get_by_name("mp_damage_scale_ct_body");
	static convar_t* mp_damage_scale_t_body = g_interfaces->m_var->get_by_name("mp_damage_scale_t_body");
	if ( !mp_damage_scale_ct_head || !mp_damage_scale_t_head || !mp_damage_scale_ct_body || !mp_damage_scale_t_body )
		return;

	float damage_scale_ct_head = mp_damage_scale_ct_head->get_float();
	float damage_scale_t_head = mp_damage_scale_t_head->get_float();
	float damage_scale_ct_body = mp_damage_scale_ct_body->get_float();
	float damage_scale_t_body = mp_damage_scale_t_body->get_float();

	const bool is_ct = entity->m_team_num() == 3, is_t = entity->m_team_num() == 2;

	float head_damage_scale = is_ct ? damage_scale_ct_head : is_t ? damage_scale_t_head : 1.f;
	const float body_damage_scale = is_ct ? damage_scale_ct_body : is_t ? damage_scale_t_body : 1.f;

	switch (hitgroup)
	{
	case hitgroup_head:
		damage *= weapon_data->m_headshot_multiplier() * head_damage_scale;
		break;
	case hitgroup_chest:
	case hitgroup_left_hand:
	case hitgroup_right_hand:
	case hitgroup_neck:
		damage *= body_damage_scale;
		break;
	case hitgroup_stomach:
		damage *= 1.25f * body_damage_scale;
		break;
	case hitgroup_left_leg:
	case hitgroup_right_leg:
		damage *= .75f * body_damage_scale;
		break;
	default:
		break;
	}

	if (!entity->has_armor(hitgroup))
		return;

	float heavy_armor_bonus = 1.f, armor_bonus = .5f, armor_ratio = weapon_data->m_armor_ratio() * .5f;

	float damage_to_health = damage * armor_ratio;
	const float damage_to_armor = (damage - damage_to_health) * (heavy_armor_bonus * armor_bonus);

	if (damage_to_armor > static_cast<float>(entity->m_armor_value()))
		damage_to_health = damage - static_cast<float>(entity->m_armor_value()) / armor_bonus;

	damage = damage_to_health;
}

bool c_auto_wall::fire_bullet(vec3_t start, vec3_t end, c_cs_player_pawn* target, c_cs_weapon_base_v_data* weapon_data, penetration_data_t& pen_data, bool is_taser)
{
	c_cs_player_pawn* local_player = g_ctx->m_local_pawn;
	if (!local_player || !target || !weapon_data)
		return false;

	const float range = weapon_data->m_range();
	vec3_t direction = end - start;
	const float length = direction.length();
	if (!std::isfinite(length) || length <= 0.001f || !std::isfinite(range) || range <= 0.f)
		return false;
	vec3_t delta = direction * (range / length);

	trace_data_t trace_data{};
	trace_data.arr_pointer = &trace_data.arr;
	trace_data.pointer_update_value = &trace_data.hit_elements;

	const trace_filter_t filter(0x1C300B, local_player, nullptr, 3, 15);
	if (!g_interfaces->m_trace->create_trace(&trace_data, start, delta, filter, 4) ||
		trace_data.num_update <= 0 || !trace_data.pointer_update_value || !trace_data.arr_pointer)
		return false;

	using fn_trace_bullet_t = void(__fastcall*)(trace_data_t*, float, float, float, int, int, std::uintptr_t);
	static fn_trace_bullet_t trace_bullet = reinterpret_cast<fn_trace_bullet_t>(
		g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), "40 53 57 41 56 48 83 EC 50 8B 84 24"));
	if (!trace_bullet)
		return false;

	trace_bullet(&trace_data, static_cast<float>(weapon_data->m_damage()), weapon_data->m_penetration(),
		weapon_data->m_range_modifier(), 4, local_player->m_team_num(), 0);

	struct bullet_trace_record_t {
		float enter_fraction;
		float exit_fraction;
		float damage;
		int team;
		std::uint16_t enter_contact;
		std::uint16_t exit_contact;
		std::uint8_t can_penetrate;
		std::uint8_t pad[3];
	};
	static_assert(sizeof(bullet_trace_record_t) == 0x18);

	const auto* hits = static_cast<const bullet_trace_record_t*>(trace_data.pointer_update_value);
	const auto* surfaces = static_cast<const trace_array_element_t*>(trace_data.arr_pointer);
	bool penetrated = false;
	for (int i = 0; i < trace_data.num_update && i < 128; ++i) {
		const auto& hit = hits[i];
		if (!std::isfinite(hit.damage) || hit.damage < 1.f)
			break;
		if (hit.can_penetrate & 1) {
			penetrated = true;
			if (hit.exit_fraction >= 1.f)
				break;
			continue;
		}

		const auto contact = hit.enter_contact & 0x7FFF;
		if (contact >= 128)
			continue;
		game_trace_t game_trace{};
		g_interfaces->m_trace->init_trace_info(&game_trace);
		g_interfaces->m_trace->get_trace_info(&trace_data, &game_trace, 0.f,
			const_cast<trace_array_element_t*>(&surfaces[contact]));
		if (game_trace.m_hit_entity != target)
			continue;

		pen_data.m_damage = hit.damage;
		pen_data.m_hitbox = game_trace.get_hitbox_id();
		pen_data.m_penetrated = penetrated;
		scale_damage(game_trace.get_hit_group(), target, weapon_data, pen_data.m_damage);
		return !is_taser || !penetrated;
	}

	return false;
}
