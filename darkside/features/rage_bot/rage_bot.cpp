#include "rage_bot.hpp"
#include "../../entity_system/entity.hpp"
#include "../movement/movement.hpp"
#include "../../render/render.hpp"

#include <cfloat>

// ============================================================================
// ХРАНЕНИЕ ЗАПИСЕЙ (бектрек-хранилище)
// ============================================================================

void c_rage_bot::store_records()
{
	if (!g_interfaces->m_engine->is_in_game() || !g_interfaces->m_engine->is_connected())
		return;

	if (!g_ctx->m_local_pawn)
		return;

	static auto sv_maxunlag = g_interfaces->m_var->get_by_name("sv_maxunlag");
	if ( !sv_maxunlag )
		return;
	const int max_ticks = std::clamp(TIME_TO_TICKS(sv_maxunlag->get_float()), 1, 64);

	const auto& entity_list = g_entity_system->get("CCSPlayerController");
	for (int i = 0; i < entity_list.size(); ++i)
	{
		auto entity = entity_list[i];

		auto player_controller = reinterpret_cast<c_cs_player_controller*>(entity);
		if (!player_controller || player_controller == g_ctx->m_local_controller)
			continue;

		auto handle = player_controller->get_handle().to_int();

		if (!player_controller->m_pawn_is_alive())
		{
			auto player_iterator = m_lag_records.find(handle);
			if (player_iterator != m_lag_records.end())
				m_lag_records.erase(player_iterator);

			continue;
		}

		auto player_pawn = reinterpret_cast<c_cs_player_pawn*>(g_interfaces->m_entity_system->get_base_entity(player_controller->m_pawn().get_entry_index()));
		if (!player_pawn)
		{
			auto player_iterator = m_lag_records.find(handle);
			if (player_iterator != m_lag_records.end())
				m_lag_records.erase(player_iterator);

			continue;
		}

		if (player_pawn->m_team_num() == g_ctx->m_local_pawn->m_team_num())
			continue;

		if (player_pawn == g_ctx->m_local_pawn)
			continue;

		auto& records = m_lag_records[handle];
		const float sim_time = player_pawn->m_sim_time();
		if (!std::isfinite(sim_time) || ( !records.empty() && records.front().m_simulation_time == sim_time ))
			continue;
		lag_record_t record;
		record.store(player_pawn);
		if (record.m_pawn && record.m_stored_bone_count > 0)
			records.emplace_front(std::move(record));
		while (records.size() > static_cast<std::size_t>(max_ticks))
			records.pop_back();
	}
}

// ============================================================================
// ХИТБОКСЫ ИЗ МЕНЮ
// ============================================================================

void c_rage_bot::store_hitboxes()
{
	auto& weapon_cfg = get_current_weapon_config();
	const bool* enabled = weapon_cfg.m_hitboxes;

	if (enabled[0])
		m_hitboxes.emplace_back(HITBOX_HEAD);

	if (enabled[1])
	{
		m_hitboxes.emplace_back(HITBOX_CHEST);
		m_hitboxes.emplace_back(HITBOX_LOWER_CHEST);
		m_hitboxes.emplace_back(HITBOX_UPPER_CHEST);
	}

	if (enabled[2])
	{
		m_hitboxes.emplace_back(HITBOX_PELVIS);
		m_hitboxes.emplace_back(HITBOX_STOMACH);
	}

	if (enabled[3])
	{
		m_hitboxes.emplace_back(HITBOX_RIGHT_HAND);
		m_hitboxes.emplace_back(HITBOX_LEFT_HAND);
		m_hitboxes.emplace_back(HITBOX_RIGHT_UPPER_ARM);
		m_hitboxes.emplace_back(HITBOX_RIGHT_FOREARM);
		m_hitboxes.emplace_back(HITBOX_LEFT_UPPER_ARM);
		m_hitboxes.emplace_back(HITBOX_LEFT_FOREARM);
	}

	if (enabled[4])
	{
		m_hitboxes.emplace_back(HITBOX_RIGHT_THIGH);
		m_hitboxes.emplace_back(HITBOX_LEFT_THIGH);

		m_hitboxes.emplace_back(HITBOX_RIGHT_CALF);
		m_hitboxes.emplace_back(HITBOX_LEFT_CALF);
	}

	if (enabled[5])
	{
		m_hitboxes.emplace_back(HITBOX_RIGHT_FOOT);
		m_hitboxes.emplace_back(HITBOX_LEFT_FOOT);
	}
}

// ============================================================================
// ВЫБОР ЗАПИСИ (нормальный бектрек)
// ============================================================================

float c_rage_bot::quick_score_record(lag_record_t* record, vec3_t shoot_pos, bool is_taser)
{
	if (!record || !record->m_pawn || record->m_stored_bone_count <= 0)
		return -1.f;

	c_base_player_weapon* weapon = g_ctx->m_local_pawn->get_active_weapon();
	if (!weapon)
		return -1.f;

	c_cs_weapon_base_v_data* weapon_data = weapon->get_weapon_data();
	if (!weapon_data)
		return -1.f;

	auto* pawn = record->m_pawn;

	float best = -1.f;
	const int hitboxes_to_test[] = { HITBOX_HEAD, HITBOX_CHEST };

	record->apply(pawn);

	for (const int hitbox : hitboxes_to_test)
	{
		hitbox_data_t data = get_hitbox_data(pawn, hitbox, record);
		if (data.m_invalid_data)
			continue;

		vec3_t center = (data.m_mins + data.m_maxs) * 0.5f;
		vec3_t world_center{};
		g_math->vector_transform(center, data.m_matrix, world_center);

		penetration_data_t pen_data{};
		if (g_auto_wall->fire_bullet(shoot_pos, world_center, pawn, weapon_data, pen_data, is_taser))
			best = (std::max)(best, pen_data.m_damage);
	}

	record->reset(pawn);
	return best;
}

lag_record_t* c_rage_bot::select_record(int handle, int min_damage)
{
	if (!g_ctx->m_local_pawn->is_alive())
		return nullptr;

	auto records_it = m_lag_records.find(handle);
	if (records_it == m_lag_records.end())
		return nullptr;
	auto& records = records_it->second;

	if (records.empty())
		return nullptr;

	auto& weapon_cfg = get_current_weapon_config();
	const bool backtrack = weapon_cfg.m_backtrack;
	int scan_depth = backtrack ? std::clamp(weapon_cfg.m_backtrack_ticks, 0, 64) : 1;
	if (scan_depth == 0)
		scan_depth = static_cast<int>(records.size());

	const auto* local_data = g_prediction->get_local_data();
	if (!local_data)
		return nullptr;

	const vec3_t shoot_pos = local_data->m_eye_pos;
	const bool is_taser = [&]() {
		auto* weapon = g_ctx->m_local_pawn->get_active_weapon();
		auto* data = weapon ? weapon->get_weapon_data() : nullptr;
		return data && data->m_weapon_type() == WEAPONTYPE_TASER;
	}();

	lag_record_t* newest_valid = nullptr;
	lag_record_t* best = nullptr;
	float best_score = -1.f;
	int checked = 0;

	for (auto& record : records)
	{
		if (!record.m_pawn || record.m_stored_bone_count <= 0 || !record.is_valid())
			continue;

		if (!newest_valid)
			newest_valid = &record;

		if (++checked > scan_depth)
			break;

		const float score = quick_score_record(&record, shoot_pos, is_taser);
		if (score > best_score)
		{
			best_score = score;
			best = &record;
		}

		if (score >= static_cast<float>(record.m_pawn->m_health()))
			break;
	}

	if (best && best_score >= static_cast<float>(min_damage))
		return best;

	return newest_valid;
}

// ============================================================================
// ЦЕЛИ
// ============================================================================

void c_rage_bot::find_targets()
{
	m_aim_targets.clear();
	if (!g_ctx->m_local_pawn->is_alive())
		return;

	auto entities = g_entity_system->get("CCSPlayerController");

	for (auto entity : entities)
	{
		if (!entity)
			continue;

		auto player_controller = reinterpret_cast<c_cs_player_controller*>(entity);
		if (!player_controller || player_controller == g_ctx->m_local_controller)
			continue;

		int handle = player_controller->get_handle().to_int();

		lag_record_t* best_record = this->select_record(handle, 1);

		if (!best_record)
			continue;

		m_aim_targets.insert_or_assign(handle, aim_target_t(best_record));
	}
}

// ============================================================================
// МУЛЬТИПОИНТЫ
// ============================================================================

float c_rage_bot::calc_point_scale(const vec3_t& point, float hitbox_radius)
{
	const auto* local_data = g_prediction->get_local_data();
	if (!local_data || hitbox_radius <= 0.f)
		return 0.f;

	float spread = local_data->m_spread + local_data->m_inaccuracy;
	if (!(spread > 0.f))
		return 95.f;

	vec3_t pt = point;
	float distance = pt.dist(local_data->m_eye_pos);
	if (distance <= 1.f)
		return 95.f;

	float cone_radius = distance * std::tan(spread);
	float scale = ((hitbox_radius - cone_radius) / hitbox_radius) * 100.f;

	return std::clamp(scale, 0.f, 95.f);
}

vec3_t c_rage_bot::transform_point(matrix3x4_t matrix, vec3_t point)
{
	vec3_t result{ };
	g_math->vector_transform(point, matrix, result);
	return result;
}

std::vector<vec3_t> c_rage_bot::calculate_sphere_points(float radius, int num_points)
{
	std::vector<vec3_t> points;
	points.reserve(num_points);

	if (num_points < 2)
		return { vec3_t(0.f, 0.f, 0.f) };

	const float phi = _pi * (3.0f - std::sqrt(5.0f));

	for (int i = 0; i < num_points; ++i)
	{
		float y = 1.f - (i / float(num_points - 1)) * 2.f;
		const float r_y = std::sqrt((std::max)(0.f, 1.f - y * y));

		float theta = phi * i;

		float x = std::cos(theta) * r_y;
		float z = std::sin(theta) * r_y;

		points.push_back({ x * radius, y * radius, z * radius });
	}

	return points;
}

std::vector<vec3_t> c_rage_bot::calculate_capsule_points(float radius, matrix3x4_t matrix, hitbox_data_t hitbox, int rings, int per_ring)
{
	std::vector<vec3_t> points;
	points.reserve(rings * per_ring);

	vec3_t axis = hitbox.m_maxs - hitbox.m_mins;
	float axis_len = axis.length();
	vec3_t axis_dir = axis_len > 0.001f ? axis / axis_len : vec3_t(1.f, 0.f, 0.f);

	vec3_t ref = std::fabs(axis_dir.y) < 0.9f ? vec3_t(0.f, 1.f, 0.f) : vec3_t(1.f, 0.f, 0.f);
	vec3_t temp = axis_dir * axis_dir.dot(ref);
	vec3_t ring_u = ref - temp;
	float u_len = ring_u.length();
	ring_u = u_len < 0.0001f ? vec3_t(0.f, 0.f, 1.f) : ring_u / u_len;
	vec3_t ring_v = {
		axis_dir.y * ring_u.z - axis_dir.z * ring_u.y,
		axis_dir.z * ring_u.x - axis_dir.x * ring_u.z,
		axis_dir.x * ring_u.y - axis_dir.y * ring_u.x
	};

	vec3_t ring_start = hitbox.m_mins + axis_dir * hitbox.m_radius;
	vec3_t ring_end = hitbox.m_maxs - axis_dir * hitbox.m_radius;

	for (int ring = 0; ring < rings; ++ring)
	{
		float t_val = rings == 1 ? 0.5f : float(ring) / float(rings - 1);
		vec3_t diff = ring_end - ring_start;
		vec3_t ring_center = ring_start + diff * t_val;

		for (int p = 0; p < per_ring; ++p)
		{
			float theta = _pi2 * float(p) / float(per_ring);
			float cu = std::cos(theta) * radius;
			float su = std::sin(theta) * radius;

			vec3_t point_local = {
				ring_center.x + ring_u.x * cu + ring_v.x * su,
				ring_center.y + ring_u.y * cu + ring_v.y * su,
				ring_center.z + ring_u.z * cu + ring_v.z * su
			};

			vec3_t world{};
			g_math->vector_transform(point_local, matrix, world);
			points.push_back(world);
		}
	}

	return points;
}

bool c_rage_bot::multi_points(lag_record_t* record, int hitbox, std::vector<aim_point_t>& points)
{
	if (!record)
		return false;

	c_cs_player_pawn* pawn = record->m_pawn;

	if (!pawn || !pawn->is_player_pawn() || !pawn->is_alive())
		return false;

	hitbox_data_t hitbox_data = get_hitbox_data(pawn, hitbox, record);

	if (hitbox_data.m_invalid_data)
		return false;

	auto* local_data = g_prediction->get_local_data();
	if (!local_data)
		return false;

	const int hitbox_from_menu = this->get_hitbox_from_menu(hitbox);
	if (hitbox_from_menu == -1)
		return false;

	const matrix3x4_t matrix = g_math->transform_to_matrix(record->m_bone_data[hitbox_data.m_num_bone]);

	const vec3_t center = (hitbox_data.m_mins + hitbox_data.m_maxs) * 0.5f;
	const vec3_t world_center = transform_point(matrix, center);
	points.emplace_back(world_center, hitbox, true);

	auto& weapon_cfg = get_current_weapon_config();
	if (!weapon_cfg.m_multi_points[hitbox_from_menu])
		return true;

	float scale = (hitbox == HITBOX_HEAD) ? static_cast<float>(weapon_cfg.m_head_scale)
		: static_cast<float>(weapon_cfg.m_body_scale);
	if (scale < 0.f)
		scale = calc_point_scale(world_center, hitbox_data.m_radius);

	const float radius = hitbox_data.m_radius * (scale / 100.f);
	if (radius <= 0.01f)
		return true;

	if (hitbox == HITBOX_HEAD)
	{
		const auto sphere = calculate_sphere_points(radius, 15);
		for (const auto& offset : sphere)
			points.emplace_back(world_center + offset, hitbox);

		return true;
	}

	const bool is_torso = hitbox_from_menu == 1 || hitbox_from_menu == 2;
	const int rings = is_torso ? 3 : 2;
	auto capsule = calculate_capsule_points(radius, matrix, hitbox_data, rings, 6);
	for (const auto& point : capsule)
		points.emplace_back(point, hitbox);

	return true;
}

// ============================================================================
// ВЫБОР ТОЧКИ
// ============================================================================

aim_point_t c_rage_bot::select_points(lag_record_t* record, float& damage)
{
	aim_point_t best_point{ vec3_t(0.f, 0.f, 0.f), -1 };

	if (!g_ctx->m_local_pawn->is_alive())
		return best_point;

	if (!record->m_pawn || !record->m_pawn->is_alive())
		return best_point;

	c_base_player_weapon* weapon = g_ctx->m_local_pawn->get_active_weapon();

	c_cs_weapon_base_v_data* weapon_data = weapon ? weapon->get_weapon_data() : nullptr;

	if (!weapon_data)
		return best_point;

	auto* local_data = g_prediction->get_local_data();

	if (!local_data)
		return best_point;

	const vec3_t shoot_pos = local_data->m_eye_pos;

	const bool is_taser = weapon_data->m_weapon_type() == WEAPONTYPE_TASER;

	auto& weapon_cfg = get_current_weapon_config();
	const bool prefer_body = weapon_cfg.m_prefer_body_aim;
	const bool allow_penetration = weapon_cfg.m_penetration;
	const int target_health = record->m_pawn->m_health();

	float best_damage = 0.f;
	float best_body_damage = 0.f;
	float best_center_damage = 0.f;
	int best_center_hitbox = -1;
	bool found_any = false;
	bool reached_lethal = false;

	for (auto& hitbox : m_hitboxes)
	{
		if (prefer_body && hitbox == HITBOX_HEAD && best_body_damage >= static_cast<float>(target_health) * 0.5f)
			continue;

		std::vector<aim_point_t> points;
		if (!multi_points(record, hitbox, points))
			continue;

		for (auto& point : points)
		{
			penetration_data_t pen_data{};

			if (!g_auto_wall->fire_bullet(shoot_pos, point.m_point, record->m_pawn, weapon_data, pen_data, is_taser))
				continue;

			if (!allow_penetration && pen_data.m_penetrated)
				continue;

			point.m_damage = pen_data.m_damage;
			point.m_penetrated = pen_data.m_penetrated;

			const bool is_head = point.m_hitbox == HITBOX_HEAD;

			if (point.m_center && pen_data.m_damage > best_center_damage)
			{
				best_center_damage = pen_data.m_damage;
				best_center_hitbox = point.m_hitbox;
			}

			if (point.m_damage > best_damage)
			{
				if (prefer_body && is_head && point.m_damage < best_body_damage * 1.25f)
					continue;

				best_point = point;
				best_damage = point.m_damage;
				found_any = true;
			}

			if (!is_head && point.m_damage > best_body_damage)
				best_body_damage = point.m_damage;

			if (best_damage >= static_cast<float>(target_health))
			{
				reached_lethal = true;
				break;
			}
		}

		if (reached_lethal)
			break;
	}

	if (found_any && best_center_hitbox != -1 && best_center_damage >= best_damage * 0.85f)
	{
		for (auto& hitbox : m_hitboxes)
		{
			hitbox_data_t data = get_hitbox_data(record->m_pawn, hitbox, record);
			if (data.m_invalid_data || data.m_num_hitbox != best_center_hitbox)
				continue;

			vec3_t center = (data.m_mins + data.m_maxs) * 0.5f;
			vec3_t world_center{};
			g_math->vector_transform(center, data.m_matrix, world_center);

			best_point = aim_point_t(world_center, best_center_hitbox, true);
			best_point.m_damage = best_center_damage;
			best_damage = best_center_damage;
			break;
		}
	}

	damage = best_damage;
	return best_point;
}

// ============================================================================
// ВЫБОР ЦЕЛИ
// ============================================================================

void c_rage_bot::select_target()
{
	if (!g_ctx->m_local_pawn->is_alive())
		return;

	if (m_aim_targets.empty())
		return;

	auto* weapon = g_ctx->m_local_pawn->get_active_weapon();
	auto* weapon_data = weapon ? weapon->get_weapon_data() : nullptr;
	if (!weapon_data)
		return;

	const bool is_taser = weapon_data->m_weapon_type() == WEAPONTYPE_TASER;
	const bool override_damage = g_key_handler->is_pressed(
		g_cfg->rage_bot.m_override_damage_key_bind, g_cfg->rage_bot.m_override_damage_key_bind_style);
	const auto* local_data = g_prediction->get_local_data();
	if (!local_data)
		return;

	auto& weapon_cfg = get_current_weapon_config();
	auto& rage_cfg = g_cfg->rage_bot;

	float cos_fov = -1.f;
	if (rage_cfg.m_fov > 0 && rage_cfg.m_fov < 180) {
		vec3_t view_dir{};
		auto view_angles_pb = g_ctx->m_user_cmd->pb.base().viewangles();
		vec3_t view_angles = { view_angles_pb.x(), view_angles_pb.y(), view_angles_pb.z() };
		g_math->angle_vectors(view_angles, view_dir);
		m_fov_dir = view_dir.normalize();
		cos_fov = std::cos(deg2rad(static_cast<float>(rage_cfg.m_fov)));
	}

	const int extrapolate_ticks = std::clamp(weapon_cfg.m_extrapolate, 0, 16);

	float highest_damage = -1.f;
	float closest_distance = FLT_MAX;
	aim_point_t chosen_point{ vec3_t{}, -1 };

	for (auto& [handle, target] : m_aim_targets)
	{
		auto* record = target.m_lag_record;
		auto* pawn = target.m_pawn;
		if (!record || !pawn || !pawn->is_alive() || !pawn->m_scene_node())
			continue;

		const int health = pawn->m_health();
		int min_damage_value = weapon_cfg.m_minimum_damage == -1 ? health : weapon_cfg.m_minimum_damage;
		int min_damage = (std::min)(override_damage ? min_damage_value : min_damage_value, health);
		if (is_taser)
			min_damage = health;

		m_active_record = *record;
		if (extrapolate_ticks > 0 && m_active_record.m_vec_velocity.length_2d() > 5.f)
			m_active_record.extrapolate(extrapolate_ticks);

		m_active_record.apply(pawn);
		float damage = 0.f;
		aim_point_t point = select_points(&m_active_record, damage);
		m_active_record.reset(pawn);

		if (point.m_hitbox == -1 || damage < static_cast<float>(min_damage))
			continue;

		if (cos_fov > -1.f)
		{
			vec3_t eye_pos = local_data->m_eye_pos;
			vec3_t to_point = point.m_point - eye_pos;
			float len = to_point.length();
			if (len > 1.f)
			{
				to_point = to_point / len;
				if (to_point.dot(m_fov_dir) < cos_fov)
					continue;
			}
		}

		float distance = point.m_point.dist(local_data->m_eye_pos);
		if (damage > highest_damage || (damage == highest_damage && distance < closest_distance))
		{
			highest_damage = damage;
			closest_distance = distance;
			chosen_point = point;
			m_best_target = &target;
		}
	}

	if (m_best_target)
		m_best_target->m_best_point = std::make_unique<aim_point_t>(chosen_point);
}

// ============================================================================
// ГЕОМЕТРИЯ ДЛЯ ХИТШАНСА
// ============================================================================

bool c_rage_bot::ray_intersects_capsule(const vec3_t& start, const vec3_t& dir_normalized,
	const vec3_t& cap_a, const vec3_t& cap_b, float radius, float max_dist)
{
	vec3_t ca = cap_a;
	vec3_t cb = cap_b;
	vec3_t st = start;
	vec3_t dn = dir_normalized;
	
	vec3_t d1 = cb - ca;
	vec3_t r = ca - st;
	vec3_t d2 = dn;

	float a_val = d1.dot(d1);
	float e_val = d2.dot(d2);
	float b_val = d1.dot(d2);
	float c_val = d1.dot(r);
	float f_val = d2.dot(r);

	float denom = a_val * e_val - b_val * b_val;

	float s = 0.f;
	float t = 0.f;

	if (a_val < 1e-6f || denom < 1e-6f)
	{
		t = e_val > 1e-6f ? f_val / e_val : 0.f;
	}
	else
	{
		t = (f_val * a_val - c_val * b_val) / denom;
		s = a_val > 1e-6f ? (t * b_val - c_val) / a_val : 0.f;
		s = std::clamp(s, 0.f, 1.f);
	}

	t = std::clamp(t, 0.f, max_dist);

	if (a_val > 1e-6f)
		s = std::clamp((t * b_val - c_val) / a_val, 0.f, 1.f);

	vec3_t closest_ray = st + d2 * t;
	vec3_t closest_cap = ca + d1 * s;

	return closest_ray.dist(closest_cap) <= radius;
}

bool c_rage_bot::ray_hits_hitboxes(const vec3_t& start, const vec3_t& dir_normalized, lag_record_t* record, float max_dist)
{
	if (!record || !record->m_pawn || record->m_stored_bone_count <= 0)
		return false;

	c_cs_player_pawn* pawn = record->m_pawn;

	for (const int hitbox : m_hitboxes)
	{
		hitbox_data_t data = get_hitbox_data(pawn, hitbox, record);
		if (data.m_invalid_data)
			continue;

		vec3_t cap_a{}, cap_b{};
		g_math->vector_transform(data.m_mins, data.m_matrix, cap_a);
		g_math->vector_transform(data.m_maxs, data.m_matrix, cap_b);

		if (hitbox == HITBOX_HEAD)
		{
			vec3_t center = (cap_a + cap_b) * 0.5f;
			if (ray_intersects_capsule(start, dir_normalized, center, center, data.m_radius, max_dist))
				return true;
		}
		else
		{
			if (ray_intersects_capsule(start, dir_normalized, cap_a, cap_b, data.m_radius, max_dist))
				return true;
		}
	}

	return false;
}

// ============================================================================
// ХИТШАНС
// ============================================================================

bool c_rage_bot::weapon_is_at_max_accuracy(c_cs_weapon_base_v_data* weapon_data, float inaccuracy)
{
	auto* local_data = g_prediction->get_local_data();

	if (!local_data)
		return false;

	constexpr auto round_accuracy = [](float accuracy) { return floorf(accuracy * 170.f) / 170.f; };
	constexpr auto round_duck_accuracy = [](float accuracy) { return floorf(accuracy * 300.f) / 300.f; };

	float speed = g_ctx->m_local_pawn->m_vec_abs_velocity().length();

	bool is_scoped = ((weapon_data->m_weapon_type() == WEAPONTYPE_SNIPER_RIFLE)
		&& !(g_ctx->m_user_cmd->m_button_state.m_button_state & IN_ZOOM)
		&& !g_ctx->m_local_pawn->m_scoped());

	bool is_ducking = ((g_ctx->m_local_pawn->m_flags() & FL_DUCKING) || (g_ctx->m_user_cmd->m_button_state.m_button_state & IN_DUCK));

	float rounded_accuracy = round_accuracy(inaccuracy);
	float rounded_duck_accuracy = round_duck_accuracy(inaccuracy);

	if (is_ducking && is_scoped && (g_ctx->m_local_pawn->m_flags() & FL_ONGROUND) && (rounded_duck_accuracy < local_data->m_inaccuracy))
		return true;

	if (speed <= 0 && is_scoped && (g_ctx->m_local_pawn->m_flags() & FL_ONGROUND) && (rounded_accuracy < local_data->m_inaccuracy))
		return true;

	return false;
}

vec3_t c_rage_bot::calculate_spread_angles(vec3_t angle, int random_seed, float weapon_inaccuarcy, float weapon_spread)
{
	float r1, r2, r3, r4, s1, c1, s2, c2;

	g_interfaces->m_random_seed(random_seed + 1);

	r1 = g_interfaces->m_random_float(0.f, 1.f);
	r2 = g_interfaces->m_random_float(0.f, _pi2);
	r3 = g_interfaces->m_random_float(0.f, 1.f);
	r4 = g_interfaces->m_random_float(0.f, _pi2);

	c1 = std::cos(r2);
	c2 = std::cos(r4);
	s1 = std::sin(r2);
	s2 = std::sin(r4);

	vec3_t spread = {
		(c1 * (r1 * weapon_inaccuarcy)) + (c2 * (r3 * weapon_spread)),
		(s1 * (r1 * weapon_inaccuarcy)) + (s2 * (r3 * weapon_spread))
	};

	vec3_t vec_forward, vec_right, vec_up;
	g_math->angle_vectors(angle, vec_forward, vec_right, vec_up);

	return (vec_forward + (vec_right * spread.x) + (vec_up * spread.y)).normalize();
}

int c_rage_bot::calculate_hit_chance(c_cs_player_pawn* pawn, vec3_t angles, c_base_player_weapon* active_weapon, c_cs_weapon_base_v_data* weapon_data, bool no_spread)
{
	if (!g_ctx->m_local_pawn->is_alive())
		return 0;

	if (!pawn)
		return 0;

	auto* local_data = g_prediction->get_local_data();

	if (!local_data)
		return 0;

	if (no_spread)
		return 100;

	vec3_t shoot_pos = local_data->m_eye_pos;

	if (m_best_target && m_best_target->m_best_point &&
		shoot_pos.dist(m_best_target->m_best_point->m_point) > weapon_data->m_range())
		return 0;

	active_weapon->update_accuracy_penality();

	if (weapon_is_at_max_accuracy(weapon_data, active_weapon->get_inaccuracy()))
		return 100;

	const float spread = local_data->m_spread;
	const float inaccuracy = local_data->m_inaccuracy;

	if (spread + inaccuracy <= 0.f)
		return 100;

	auto& weapon_cfg = get_current_weapon_config();
	int seed_count = std::clamp(weapon_cfg.m_hit_chance_seed, 0, 256);
	if (seed_count == 0)
		seed_count = 128;

	int hits = 0;
	for (int seed = 0; seed < seed_count; seed++)
	{
		const vec3_t spread_dir = calculate_spread_angles(angles, seed, inaccuracy, spread);

		if (ray_hits_hitboxes(shoot_pos, spread_dir, &m_active_record, weapon_data->m_range()))
			hits++;
	}

	return static_cast<int>((static_cast<float>(hits) / static_cast<float>(seed_count)) * 100.f);
}

// ============================================================================
// БЕКТРЕК (интерполяция входов)
// ============================================================================

void c_rage_bot::process_backtrack(lag_record_t* record)
{
	if (!record)
		return;

	auto& weapon_cfg = get_current_weapon_config();
	if (!weapon_cfg.m_backtrack)
		return;

	const int tick_shoot = TIME_TO_TICKS(record->m_simulation_time) + 2;

	for (int i = 0; i < g_ctx->m_user_cmd->pb.input_history_size(); i++) {

		if (g_ctx->m_user_cmd->pb.attack1_start_history_index() == -1)
			continue;

		auto input_history = g_ctx->m_user_cmd->pb.mutable_input_history(i);
		if (!input_history)
			continue;

		if (input_history->has_cl_interp()) {
			auto interp_info = input_history->mutable_cl_interp();
			interp_info->set_frac(0.f);
		}

		if (input_history->has_sv_interp0()) {
			auto interp_info = input_history->mutable_sv_interp0();
			interp_info->set_src_tick(tick_shoot);
			interp_info->set_dst_tick(tick_shoot);
			interp_info->set_frac(0.f);
		}

		if (input_history->has_sv_interp1()) {
			auto interp_info = input_history->mutable_sv_interp1();
			interp_info->set_src_tick(tick_shoot);
			interp_info->set_dst_tick(tick_shoot);
			interp_info->set_frac(0.f);
		}

		input_history->set_render_tick_count(tick_shoot);

		g_ctx->m_user_cmd->pb.mutable_base()->set_client_tick(tick_shoot);

		input_history->set_player_tick_count(g_ctx->m_local_controller->m_tick_base());
		input_history->set_player_tick_fraction(g_prediction->get_local_data()->m_player_tick_fraction);
	}
}

// ============================================================================
// ВЫСТРЕЛ
// ============================================================================

bool c_rage_bot::can_shoot(c_cs_player_pawn* pawn, c_base_player_weapon* active_weapon)
{
	return (active_weapon->m_clip1() > 0) && (active_weapon->m_next_primary_attack() <= g_ctx->m_local_controller->m_tick_base() + 2);
}

void c_rage_bot::process_attack(c_user_cmd* user_cmd, vec3_t angle)
{
	user_cmd->pb.mutable_base()->mutable_viewangles()->set_x(angle.x);
	user_cmd->pb.mutable_base()->mutable_viewangles()->set_y(angle.y);
	user_cmd->pb.mutable_base()->mutable_viewangles()->set_z(angle.z);

	for (int i = 0; i < g_ctx->m_user_cmd->pb.input_history_size(); i++)
	{
		auto container = g_ctx->m_user_cmd->pb.mutable_input_history(i);
		if (container)
			container->set_player_tick_count(g_prediction->get_local_data()->m_shoot_tick);
	}

	if (user_cmd->pb.input_history_size() > 0)
		user_cmd->pb.set_attack1_start_history_index(0);
	process_backtrack(&m_active_record);

	user_cmd->m_button_state.m_button_state |= IN_ATTACK;
}

// ============================================================================
// DOUBLE TAP / HIDE SHOTS (CS2 subtick)
// ============================================================================

void c_rage_bot::strip_attack(c_user_cmd* user_cmd)
{
	auto& buttons = user_cmd->m_button_state;
	buttons.m_button_state &= ~IN_ATTACK;
	buttons.m_button_state2 &= ~IN_ATTACK;
	buttons.m_button_state3 &= ~IN_ATTACK;

	if (!user_cmd->pb.has_base())
		return;

	auto* base = user_cmd->pb.mutable_base();
	for (int i = 0; i < base->subtick_moves_size(); ++i) {
		auto* step = base->mutable_subtick_moves(i);
		if (step && (step->button() & IN_ATTACK)) {
			step->set_button(step->button() & ~IN_ATTACK);
			if (!step->button())
				step->clear_pressed();
		}
	}
}

void c_rage_bot::process_exploit_attack(c_user_cmd* user_cmd, vec3_t angle, bool dt_active, bool hs_active, bool want_shoot)
{
	auto* weapon = g_ctx->m_local_pawn ? g_ctx->m_local_pawn->get_active_weapon() : nullptr;
	if (!weapon || !user_cmd->pb.has_base())
		return;

	const int tick_base = g_ctx->m_local_controller->m_tick_base();
	const int next_attack_tick = weapon->m_next_primary_attack();
	const bool ready = (next_attack_tick <= tick_base) && (weapon->m_clip1() > 0);

	bool should_attack = want_shoot && ready;
	if (dt_active)
		should_attack = should_attack && !m_dt_fired;

	for (int i = 0; i < user_cmd->pb.input_history_size(); i++) {
		auto* entry = user_cmd->pb.mutable_input_history(i);
		if (entry)
			entry->set_player_tick_count(should_attack ? next_attack_tick : 0);
	}

	if (should_attack) {
		auto* base = user_cmd->pb.mutable_base();
		if (base->subtick_moves_size() < 64) {
			auto allocate = [&]() { return g_protobuf->add_subtick_move_step(user_cmd); };

			const auto make_step = [&](bool pressed, float when) -> CSubtickMoveStep* {
				auto* step = allocate();
				if (!step)
					return nullptr;
				step->set_button(IN_ATTACK);
				step->set_pressed(pressed);
				step->set_when(when);
				step->clear_analog_forward_delta();
				step->clear_analog_left_delta();
				step->clear_pitch_delta();
				step->clear_yaw_delta();
				return step;
			};

			make_step(true, 0.0f);
			make_step(false, 0.999f);
		}
	}
	else {
		strip_attack(user_cmd);
	}

	m_dt_fired = dt_active && should_attack && !m_dt_fired;

	user_cmd->pb.set_attack1_start_history_index(-1);
}

// ============================================================================
// ON CREATE MOVE
// ============================================================================

void c_rage_bot::on_create_move()
{
	if (!g_cfg->rage_bot.m_enabled)
		return;

	if (!g_interfaces->m_engine->is_in_game())
		return;

	if (!m_hitboxes.empty())
		m_hitboxes.clear();

	m_best_target = nullptr;

	c_user_cmd* user_cmd = g_ctx->m_user_cmd;

	if (!user_cmd)
		return;

	c_cs_player_pawn* local_player = g_ctx->m_local_pawn;

	if (!local_player || !local_player->is_alive())
		return;

	c_base_player_weapon* active_weapon = local_player->get_active_weapon();

	if (!active_weapon)
		return;

	c_cs_weapon_base_v_data* weapon_data = active_weapon->get_weapon_data();

	if (!weapon_data
		|| weapon_data->m_weapon_type() == WEAPONTYPE_KNIFE
		|| weapon_data->m_weapon_type() == WEAPONTYPE_GRENADE)
		return;

	store_hitboxes();

	if (m_hitboxes.empty())
		return;

	find_targets();

	select_target();

	if (!m_best_target || !m_best_target->m_best_point)
		return;

	auto* local_data = g_prediction->get_local_data();

	if (!local_data)
		return;

	const auto no_spread_var = g_interfaces->m_var->get_by_name(xorstr_("weapon_accuracy_nospread"));
	bool no_spread = no_spread_var && no_spread_var->get_bool();

	auto& weapon_cfg = get_current_weapon_config();

	// Автоскоуп
	if (weapon_cfg.m_auto_scope
		&& (weapon_data->m_weapon_type() == WEAPONTYPE_SNIPER_RIFLE)
		&& !(user_cmd->m_button_state.m_button_state & IN_ZOOM)
		&& !local_player->m_scoped()
		&& (local_player->m_flags() & FL_ONGROUND)
		&& !no_spread) {
		user_cmd->m_button_state.m_button_state |= IN_ATTACK2;
	}

	// Автостоп
	{
		const int weapon_type = weapon_data->m_weapon_type();
		const bool is_automatic = weapon_type == WEAPONTYPE_SUBMACHINEGUN
			|| weapon_type == WEAPONTYPE_RIFLE
			|| weapon_type == WEAPONTYPE_MACHINEGUN;

		const bool between_window = is_automatic
			&& weapon_cfg.m_auto_stop_between
			&& active_weapon->m_next_primary_attack() > g_ctx->m_local_controller->m_tick_base();

		auto_stop_options_t stop_options{};
		stop_options.m_enabled = weapon_cfg.m_auto_stop;
		stop_options.m_early = weapon_cfg.m_auto_stop_early;
		stop_options.m_full = weapon_cfg.m_auto_stop_full;
		stop_options.m_between = between_window;
		stop_options.m_air = weapon_cfg.m_auto_stop_air;
		stop_options.m_duck = weapon_cfg.m_auto_stop_duck;
		stop_options.m_no_spread = no_spread;

		g_movement->auto_stop(user_cmd, local_player, active_weapon, stop_options);
	}

	if (!can_shoot(local_player, active_weapon))
		return;

	const bool is_taser = weapon_data->m_weapon_type() == WEAPONTYPE_TASER;

	m_active_record.apply(m_active_record.m_pawn);

	vec3_t angle = g_math->aim_direction(local_data->m_eye_pos, m_best_target->m_best_point->m_point);

	vec3_t punch = get_removed_aim_punch_angle(local_player);
	vec3_t best_angle = angle - punch;

	const int hit_chance = calculate_hit_chance(m_best_target->m_pawn, best_angle, active_weapon, weapon_data, no_spread);

	m_active_record.reset(m_active_record.m_pawn);

	float required_hc = weapon_cfg.m_hit_chance < 0
		? 0.f
		: static_cast<float>(is_taser ? 70 : weapon_cfg.m_hit_chance);

	if (weapon_cfg.m_delay_shot && hit_chance < 100
		&& m_best_target->m_best_point->m_damage < static_cast<float>(m_best_target->m_pawn->m_health())) {
		required_hc = (std::min)(required_hc + 25.f, 100.f);
	}

	// DT / HS: тумблер + кейбинд
	const bool dt_active = g_cfg->rage_bot.m_double_tap
		&& g_key_handler->is_pressed(g_cfg->rage_bot.m_double_tap_key, g_cfg->rage_bot.m_double_tap_key_style);
	const bool hs_active = g_cfg->rage_bot.m_hide_shots
		&& g_key_handler->is_pressed(g_cfg->rage_bot.m_hide_shots_key, g_cfg->rage_bot.m_hide_shots_key_style);

	const bool want_shoot = static_cast<float>(hit_chance) >= required_hc;

	if (dt_active || hs_active) {
		if (want_shoot) {
			for (int i = 0; i < user_cmd->pb.input_history_size(); i++) {
				auto tick = user_cmd->pb.mutable_input_history(i);
				if (tick) {
					tick->mutable_view_angles()->set_x(best_angle.x);
					tick->mutable_view_angles()->set_y(best_angle.y);
					tick->mutable_view_angles()->set_z(best_angle.z);
				}
			}

			if (!g_cfg->rage_bot.m_silent)
				g_interfaces->m_csgo_input->set_view_angles(best_angle);
		}

		process_exploit_attack(user_cmd, best_angle, dt_active, hs_active, want_shoot);
		return;
	}

	if (!want_shoot)
		return;

	for (int i = 0; i < user_cmd->pb.input_history_size(); i++) {
		auto tick = user_cmd->pb.mutable_input_history(i);
		if (tick) {
			tick->mutable_view_angles()->set_x(best_angle.x);
			tick->mutable_view_angles()->set_y(best_angle.y);
			tick->mutable_view_angles()->set_z(best_angle.z);
		}
	}

	if (!g_cfg->rage_bot.m_silent)
		g_interfaces->m_csgo_input->set_view_angles(best_angle);

	process_attack(user_cmd, best_angle);
}
