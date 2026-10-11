#include "rage_bot.hpp"

#include "../../entity_system/entity.hpp"
#include "../movement/movement.hpp"
#include "../../render/render.hpp"

#include <cfloat>
#include <cmath>

// ============================================================================
// Accuracy model
//
// Inaccuracy and spread are not a single "speed blob". The game keeps them
// separate: get_inaccuracy() is the cone from stance/movement/air/penalty,
// get_spread() is the weapon pattern, and CalculateSpread (client.dll) turns
// a seed + recoil index + bullet count into the actual offset. Hitchance
// fires those offsets at the hitbox we are aiming at — not at every hitbox,
// and not through RandomFloat (that desyncs the game RNG and does not match
// revolver / negev / shotgun patterns).
// ============================================================================

namespace {

constexpr float k_tick = INTERVAL_PER_TICK;

float clampf(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

bool sane(float v, float max_v) {
	return std::isfinite(v) && v >= 0.f && v <= max_v;
}

float read_firing(float (&values)[2], int mode) {
	const float v = values[mode & 1];
	return sane(v, 2.f) ? v : -1.f;
}

float air_inaccuracy(float vertical_speed, float jump_initial, float jump_apex) {
	constexpr float k_sqrt = 17.37795666f;
	const float value = ((std::sqrt(std::fabs(vertical_speed)) - k_sqrt * 0.25f) * (jump_initial - jump_apex))
		/ (k_sqrt * 0.75f) + jump_apex;
	const float cap = jump_initial * 2.f;
	return clampf(value, 0.f, cap > 0.f ? cap : 1.f);
}

float convar_float(const char* name, float fallback, float max_v) {
	auto* var = g_interfaces->m_var ? g_interfaces->m_var->get_by_name(name) : nullptr;
	if (!var)
		return fallback;
	const float value = var->get_float();
	return sane(value, max_v) ? value : fallback;
}

bool hull_trace(c_cs_player_pawn* skip, const vec3_t& from, const vec3_t& to,
	const vec3_t& mins, const vec3_t& maxs, game_trace_t* hit) {
	if (!g_interfaces->m_trace || !hit)
		return false;
	trace_filter_t filter(0x1C3003ull, skip, nullptr, 4);
	if (!filter.m_vtable)
		return false;
	ray_t ray{};
	ray.m_mins = mins;
	ray.m_maxs = maxs;
	ray.type = 2;
	hit->m_fraction = -1.f;
	g_interfaces->m_trace->trace_shape(&ray, from, to, &filter, hit);
	return std::isfinite(hit->m_fraction) && hit->m_fraction >= 0.f && hit->m_fraction <= 1.f;
}

struct motion_step_t {
	vec3_t origin{};
	vec3_t velocity{};
	int flags{};
	vec3_t mins{};
	vec3_t maxs{};
	float direction{};
};

void slide_along(motion_step_t& step, c_cs_player_pawn* pawn, game_trace_t& trace) {
	for (int clip = 0; clip < 2; ++clip) {
		const float into = step.velocity.dot(trace.m_normal);
		step.velocity = step.velocity - trace.m_normal * into;
		const float still_into = step.velocity.dot(trace.m_normal);
		if (still_into < 0.f)
			step.velocity = step.velocity - trace.m_normal * still_into;

		const float remaining = (1.f - trace.m_fraction) * k_tick;
		vec3_t start = trace.m_end;
		vec3_t end = start + step.velocity * remaining;
		if (!hull_trace(pawn, start, end, step.mins, step.maxs, &trace) || trace.m_fraction >= 1.f) {
			step.origin = end;
			return;
		}
	}
	step.origin = trace.m_end;
}

void step_movement(motion_step_t& step, c_cs_player_pawn* pawn) {
	const float gravity = convar_float("sv_gravity", 800.f, 4000.f);
	const bool grounded = (step.flags & FL_ONGROUND) != 0;
	if (grounded)
		step.velocity.z = 0.f;
	else
		step.velocity.z -= gravity * k_tick;

	vec3_t end = step.origin + step.velocity * k_tick;
	game_trace_t trace{};
	if (!hull_trace(pawn, step.origin, end, step.mins, step.maxs, &trace) || trace.m_fraction >= 1.f)
		step.origin = end;
	else
		slide_along(step, pawn, trace);

	vec3_t ground_end = step.origin;
	ground_end.z -= 2.f;
	game_trace_t ground{};
	step.flags &= ~FL_ONGROUND;
	if (hull_trace(pawn, step.origin, ground_end, step.mins, step.maxs, &ground)
		&& ground.m_fraction < 1.f && ground.m_normal.z > 0.7f) {
		step.flags |= FL_ONGROUND;
		step.velocity.z = 0.f;
		step.origin.z = ground.m_end.z;
	}
}

struct motion_model_t {
	float direction{};
	float yaw_per_tick{};
	float speed{};
	float speed_per_tick{};
	float confidence{};
	int reliable_ticks{};
};

motion_model_t analyze_motion(const std::deque<lag_record_t>& records, int max_ticks) {
	motion_model_t model{};
	if (records.empty() || max_ticks <= 0)
		return model;

	const auto& newest = records.front();
	vec3_t velocity = newest.m_velocity;
	model.speed = velocity.length_2d();
	model.direction = rad2deg(std::atan2(velocity.y, velocity.x));
	model.confidence = model.speed > 8.f ? 0.4f : 0.f;
	model.reliable_ticks = model.speed > 8.f ? 1 : 0;
	if (records.size() < 2 || model.speed <= 8.f)
		return model;

	const auto& previous = records[1];
	const float dt = newest.m_simulation_time - previous.m_simulation_time;
	if (!(dt > 0.001f && dt < 0.4f)) {
		model.confidence = 0.25f;
		model.reliable_ticks = 1;
		return model;
	}

	vec3_t delta = newest.m_origin - previous.m_origin;
	const float path_speed = delta.length_2d() / dt;
	const float path_dir = rad2deg(std::atan2(delta.y, delta.x));
	const float turn = g_math->angle_diff(path_dir, model.direction);
	const float ticks = (std::max)(dt / k_tick, 1.f);
	model.yaw_per_tick = turn / ticks;
	model.speed_per_tick = (model.speed - path_speed) / ticks;

	// A hard direction change is a juke. Predicting 8-16 ticks through it misses.
	if (std::fabs(turn) > 40.f) {
		model.yaw_per_tick = 0.f;
		model.confidence = 0.12f;
		model.reliable_ticks = 1;
		return model;
	}
	if (std::fabs(model.yaw_per_tick) > 7.f)
		model.yaw_per_tick = 0.f;

	const float speed_error = std::fabs(model.speed - path_speed) / (std::max)(model.speed, 40.f);
	model.confidence = clampf(1.f - speed_error, 0.2f, 1.f);
	if (std::fabs(turn) > 16.f)
		model.confidence *= 0.65f;

	model.reliable_ticks = std::clamp(static_cast<int>(model.confidence * static_cast<float>(max_ticks) + 0.5f), 1, max_ticks);
	return model;
}

int ticks_worth_predicting(const lag_record_t& newest, const motion_model_t& model, int max_ticks) {
	if (max_ticks <= 0 || model.reliable_ticks <= 0 || model.speed <= 8.f)
		return 0;

	int stale = 0;
	if (g_interfaces->m_global_vars && std::isfinite(newest.m_simulation_time)) {
		const float behind = g_interfaces->m_global_vars->m_current_time - newest.m_simulation_time;
		if (behind > 0.f)
			stale = TIME_TO_TICKS(behind);
	}
	stale = std::clamp(stale, 0, max_ticks);

	// Lead only when the path is stable. Fast straight runs get one extra tick
	// so the shot meets them; a strafe does not get a blind 16-tick launch.
	int lead = 0;
	if (model.confidence > 0.72f && model.speed > 80.f && std::fabs(model.yaw_per_tick) < 3.5f)
		lead = 1;
	if (model.confidence > 0.9f && model.speed > 210.f && std::fabs(model.yaw_per_tick) < 1.5f)
		lead = 2;

	return std::clamp((std::min)(stale + lead, model.reliable_ticks), 0, max_ticks);
}

} // namespace

// ============================================================================
// RECORDS
// ============================================================================

void lag_record_t::store(c_cs_player_pawn* pawn) {
	if (!pawn || !pawn->is_player_pawn() || !pawn->is_alive())
		return;

	auto* scene = pawn->m_scene_node();
	auto* skeleton = scene ? scene->get_skeleton_instance() : nullptr;
	auto* collision = pawn->m_collision();
	if (!skeleton || !collision)
		return;

	auto& model_state = skeleton->m_model_state();
	auto* bones = model_state.get_bone_data();
	m_stored_bone_count = std::clamp(model_state.get_bone_count(), 0, 128);
	if (!bones || m_stored_bone_count <= 0)
		return;

	std::memcpy(m_bone_data, bones, sizeof(c_bone_data) * m_stored_bone_count);
	m_pawn = pawn;
	m_origin = scene->m_abs_origin();
	m_simulation_time = pawn->m_sim_time();
	m_mins = collision->m_mins();
	m_maxs = collision->m_maxs();
	m_velocity = pawn->m_vec_velocity();
	m_flags = pawn->m_flags();
	m_throwing = pawn->is_throwing();

	vec3_t eye = pawn->m_eye_angles();
	m_eye_angles = (std::isfinite(eye.x) && std::isfinite(eye.y) && std::fabs(eye.y) <= 1080.f) ? eye : vec3_t{};

	m_duck_amount = (m_flags & FL_DUCKING) ? 1.f : 0.f;
	if (auto* move = pawn->m_movement_services()) {
		const float duck = move->m_duck_amount();
		if (std::isfinite(duck) && duck >= 0.f && duck <= 1.f)
			m_duck_amount = duck;
	}
}

void lag_record_t::apply(c_cs_player_pawn* pawn) {
	if (!pawn || !pawn->is_alive())
		return;
	auto* scene = pawn->m_scene_node();
	auto* skeleton = scene ? scene->get_skeleton_instance() : nullptr;
	if (!skeleton)
		return;
	auto& model_state = skeleton->m_model_state();
	auto* bones = model_state.get_bone_data();
	const int count = (std::min)(m_stored_bone_count, std::clamp(model_state.get_bone_count(), 0, 128));
	if (!bones || count <= 0)
		return;
	std::memcpy(m_bone_data_backup, bones, sizeof(c_bone_data) * count);
	std::memcpy(bones, m_bone_data, sizeof(c_bone_data) * count);
}

void lag_record_t::reset(c_cs_player_pawn* pawn) {
	if (!pawn || !pawn->is_alive())
		return;
	auto* scene = pawn->m_scene_node();
	auto* skeleton = scene ? scene->get_skeleton_instance() : nullptr;
	if (!skeleton)
		return;
	auto& model_state = skeleton->m_model_state();
	auto* bones = model_state.get_bone_data();
	const int count = (std::min)(m_stored_bone_count, std::clamp(model_state.get_bone_count(), 0, 128));
	if (!bones || count <= 0)
		return;
	std::memcpy(bones, m_bone_data_backup, sizeof(c_bone_data) * count);
}

void lag_record_t::shift(const vec3_t& delta) {
	m_origin = m_origin + delta;
	for (int i = 0; i < m_stored_bone_count; ++i)
		m_bone_data[i].m_pos = m_bone_data[i].m_pos + delta;
}

bool lag_record_t::is_valid() const {
	const auto* local = g_prediction->get_local_data();
	if (!local || !std::isfinite(m_simulation_time))
		return false;
	const float dead = (local->m_tick_base * k_tick) - m_simulation_time;
	if (!std::isfinite(dead) || dead < -0.2f || dead > 0.2f)
		return false;
	vec3_t velocity = m_velocity;
	return velocity.length() < 4500.f;
}

void c_rage_bot::store_records() {
	if (!g_interfaces->m_engine->is_in_game() || !g_interfaces->m_engine->is_connected() || !g_ctx->m_local_pawn)
		return;

	static auto* sv_maxunlag = g_interfaces->m_var->get_by_name("sv_maxunlag");
	if (!sv_maxunlag)
		return;
	const int max_ticks = std::clamp(TIME_TO_TICKS(sv_maxunlag->get_float()), 1, 64);

	const auto& entities = g_entity_system->get("CCSPlayerController");
	for (int i = 0; i < static_cast<int>(entities.size()); ++i) {
		auto* controller = reinterpret_cast<c_cs_player_controller*>(entities[i]);
		if (!controller || controller == g_ctx->m_local_controller)
			continue;

		const int handle = controller->get_handle().to_int();
		if (!controller->m_pawn_is_alive()) {
			m_lag_records.erase(handle);
			m_resolver.erase(handle);
			continue;
		}

		auto* pawn = reinterpret_cast<c_cs_player_pawn*>(
			g_interfaces->m_entity_system->get_base_entity(controller->m_pawn().get_entry_index()));
		if (!pawn || pawn == g_ctx->m_local_pawn || pawn->m_team_num() == g_ctx->m_local_pawn->m_team_num()) {
			if (!pawn)
				m_lag_records.erase(handle);
			continue;
		}

		static const auto immunity_off = schema_get_offset("C_CSPlayerPawn", "m_bGunGameImmunity");
		if (immunity_off && *reinterpret_cast<bool*>(reinterpret_cast<std::uintptr_t>(pawn) + immunity_off)) {
			m_lag_records.erase(handle);
			continue;
		}

		auto& records = m_lag_records[handle];
		const float sim = pawn->m_sim_time();
		if (!std::isfinite(sim) || (!records.empty() && records.front().m_simulation_time == sim))
			continue;

		lag_record_t record;
		record.store(pawn);
		if (!record.m_pawn || record.m_stored_bone_count <= 0)
			continue;

		if (!records.empty()) {
			const float dt = sim - records.front().m_simulation_time;
			vec3_t delta = record.m_origin - records.front().m_origin;
			if (dt > 0.001f && delta.length() > 4000.f * dt)
				continue;
		}

		records.emplace_front(std::move(record));
		while (records.size() > static_cast<std::size_t>(max_ticks))
			records.pop_back();
	}
}

// ============================================================================
// HITBOXES / POINTS
// ============================================================================

int c_rage_bot::get_hitbox_from_menu(int hitbox) const {
	switch (hitbox) {
	case HITBOX_HEAD: return 0;
	case HITBOX_CHEST:
	case HITBOX_LOWER_CHEST:
	case HITBOX_UPPER_CHEST: return 1;
	case HITBOX_PELVIS:
	case HITBOX_STOMACH: return 2;
	case HITBOX_RIGHT_HAND:
	case HITBOX_LEFT_HAND:
	case HITBOX_RIGHT_UPPER_ARM:
	case HITBOX_RIGHT_FOREARM:
	case HITBOX_LEFT_UPPER_ARM:
	case HITBOX_LEFT_FOREARM: return 3;
	case HITBOX_RIGHT_THIGH:
	case HITBOX_LEFT_THIGH:
	case HITBOX_RIGHT_CALF:
	case HITBOX_LEFT_CALF: return 4;
	case HITBOX_RIGHT_FOOT:
	case HITBOX_LEFT_FOOT: return 5;
	default: return -1;
	}
}

c_config_system::weapon_config_t& c_rage_bot::weapon_config() const {
	auto* pawn = g_ctx->m_local_pawn;
	auto* weapon = pawn ? pawn->get_active_weapon() : nullptr;
	auto* data = weapon ? weapon->get_weapon_data() : nullptr;
	if (!data)
		return g_cfg->rage_bot.m_weapon_config[WEAPON_GROUP_GENERAL];
	const auto group = c_config_system::get_weapon_group(data->m_weapon_type(), weapon->get_item_def_index());
	return g_cfg->rage_bot.get_weapon_config(static_cast<int>(group));
}

void c_rage_bot::store_hitboxes() {
	const bool* enabled = weapon_config().m_hitboxes;
	if (enabled[0]) m_hitboxes.push_back(HITBOX_HEAD);
	if (enabled[1]) {
		m_hitboxes.push_back(HITBOX_UPPER_CHEST);
		m_hitboxes.push_back(HITBOX_CHEST);
		m_hitboxes.push_back(HITBOX_LOWER_CHEST);
	}
	if (enabled[2]) {
		m_hitboxes.push_back(HITBOX_STOMACH);
		m_hitboxes.push_back(HITBOX_PELVIS);
	}
	if (enabled[3]) {
		m_hitboxes.push_back(HITBOX_RIGHT_UPPER_ARM);
		m_hitboxes.push_back(HITBOX_LEFT_UPPER_ARM);
		m_hitboxes.push_back(HITBOX_RIGHT_FOREARM);
		m_hitboxes.push_back(HITBOX_LEFT_FOREARM);
		m_hitboxes.push_back(HITBOX_RIGHT_HAND);
		m_hitboxes.push_back(HITBOX_LEFT_HAND);
	}
	if (enabled[4]) {
		m_hitboxes.push_back(HITBOX_RIGHT_THIGH);
		m_hitboxes.push_back(HITBOX_LEFT_THIGH);
		m_hitboxes.push_back(HITBOX_RIGHT_CALF);
		m_hitboxes.push_back(HITBOX_LEFT_CALF);
	}
	if (enabled[5]) {
		m_hitboxes.push_back(HITBOX_RIGHT_FOOT);
		m_hitboxes.push_back(HITBOX_LEFT_FOOT);
	}
}

hitbox_data_t c_rage_bot::get_hitbox_data(c_cs_player_pawn* pawn, int hitbox_id, const lag_record_t* record) const {
	hitbox_data_t out{};
	if (!pawn)
		return out;
	auto* scene = pawn->m_scene_node();
	auto* skeleton = scene ? scene->get_skeleton_instance() : nullptr;
	if (!skeleton)
		return out;
	auto& model_state = skeleton->m_model_state();
	auto& model = model_state.m_model();
	if (!model)
		return out;
	auto* hitbox = model->get_hitbox(hitbox_id);
	if (!hitbox)
		return out;
	const int bone = pawn->get_bone_index(hitbox->m_bone_name);
	if (bone < 0 || bone >= 128)
		return out;

	out.m_num_hitbox = hitbox_id;
	out.m_num_bone = bone;
	out.m_num_hitgroup = hitbox->m_hitgroup;
	out.m_radius = hitbox->m_shape_radius;
	out.m_mins = hitbox->m_vec_min;
	out.m_maxs = hitbox->m_vec_maxs;
	out.m_invalid_data = false;

	if (record && bone < record->m_stored_bone_count)
		out.m_matrix = g_math->transform_to_matrix(const_cast<c_bone_data&>(record->m_bone_data[bone]));
	else if (auto* bones = model_state.get_bone_data())
		out.m_matrix = g_math->transform_to_matrix(bones[bone]);
	else
		out.m_invalid_data = true;
	return out;
}

vec3_t c_rage_bot::get_removed_aim_punch_angle(c_cs_player_pawn* local_player) const {
	using fn_t = void(__fastcall*)(void*, vec3_t*, bool);
	static fn_t fn = reinterpret_cast<fn_t>(g_opcodes->scan_absolute(
		g_modules->m_modules.client_dll.get_name(), andromeda::get_removed_aim_punch, 0x1));
	vec3_t punch{};
	if (fn)
		fn(local_player, &punch, true);
	return punch;
}

float c_rage_bot::point_scale(const vec3_t& point, float radius, float inaccuracy, float spread) const {
	const auto* local = g_prediction->get_local_data();
	if (!local || radius <= 0.1f)
		return 0.f;
	const float cone = inaccuracy + spread;
	vec3_t sample = point;
	const float distance = sample.dist(local->m_eye_pos);
	if (!(cone > 0.00005f) || distance < 1.f)
		return 90.f;
	const float cone_radius = distance * std::tan(cone);
	return clampf(((radius - cone_radius) / radius) * 100.f, 0.f, 92.f);
}

bool c_rage_bot::multi_points(lag_record_t* record, int hitbox, std::vector<aim_point_t>& points,
	float inaccuracy, float spread, int resolver_side) {
	if (!record || !record->m_pawn)
		return false;
	hitbox_data_t data = get_hitbox_data(record->m_pawn, hitbox, record);
	if (data.m_invalid_data)
		return false;

	const int menu = get_hitbox_from_menu(hitbox);
	if (menu < 0)
		return false;

	vec3_t mins = data.m_mins;
	vec3_t maxs = data.m_maxs;
	vec3_t center = (mins + maxs) * 0.5f;
	vec3_t world{};
	g_math->vector_transform(center, data.m_matrix, world);

	if (hitbox == HITBOX_HEAD && resolver_side != 0 && data.m_radius > 0.f) {
		vec3_t lateral{ data.m_matrix[0][1], data.m_matrix[1][1], data.m_matrix[2][1] };
		if (lateral.length() > 0.01f) {
			lateral = lateral.normalize();
			world = world + lateral * (data.m_radius * 0.45f * static_cast<float>(resolver_side));
		}
	}
	points.emplace_back(world, hitbox, true);

	auto& cfg = weapon_config();
	if (!cfg.m_multi_points[menu])
		return true;

	float scale = hitbox == HITBOX_HEAD ? static_cast<float>(cfg.m_head_scale) : static_cast<float>(cfg.m_body_scale);
	if (scale < 0.f)
		scale = point_scale(world, data.m_radius, inaccuracy, spread);
	const float radius = data.m_radius * (scale / 100.f);
	if (radius <= 0.05f)
		return true;

	const auto* local = g_prediction->get_local_data();
	if (!local)
		return true;

	// Camera basis: +forward is the surface facing the player. That is the
	// point you can actually shoot (back of the head on a turned/crouched bot),
	// not a far-side offset that only exists to satisfy the scan.
	vec3_t to_cam{
		local->m_eye_pos.x - world.x,
		local->m_eye_pos.y - world.y,
		local->m_eye_pos.z - world.z
	};
	const float to_len = std::sqrt(to_cam.x * to_cam.x + to_cam.y * to_cam.y + to_cam.z * to_cam.z);
	if (to_len < 1.f)
		return true;
	vec3_t forward{ to_cam.x / to_len, to_cam.y / to_len, to_cam.z / to_len };
	vec3_t world_up{ 0.f, 0.f, 1.f };
	vec3_t right{
		world_up.y * forward.z - world_up.z * forward.y,
		world_up.z * forward.x - world_up.x * forward.z,
		world_up.x * forward.y - world_up.y * forward.x
	};
	float right_len = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
	if (right_len < 0.01f) {
		right = { 1.f, 0.f, 0.f };
		right_len = 1.f;
	}
	right.x /= right_len; right.y /= right_len; right.z /= right_len;
	vec3_t up{
		right.y * forward.z - right.z * forward.y,
		right.z * forward.x - right.x * forward.z,
		right.x * forward.y - right.y * forward.x
	};

	auto push = [&](float fx, float rx, float ux) {
		points.emplace_back(vec3_t{
			world.x + forward.x * fx + right.x * rx + up.x * ux,
			world.y + forward.y * fx + right.y * rx + up.y * ux,
			world.z + forward.z * fx + right.z * rx + up.z * ux
		}, hitbox, false);
	};

	// Near face first: crouched occiput / exposed shell at scale 100.
	push(radius, 0.f, 0.f);
	push(radius * 0.72f, 0.f, radius * 0.62f);
	push(radius * 0.55f, 0.f, radius * 0.9f);
	push(radius * 0.7f, radius * 0.62f, radius * 0.25f);
	push(radius * 0.7f, -radius * 0.62f, radius * 0.25f);
	push(radius * 0.45f, radius * 0.85f, 0.f);
	push(radius * 0.45f, -radius * 0.85f, 0.f);
	if (hitbox == HITBOX_HEAD) {
		push(radius * 0.8f, 0.f, -radius * 0.35f);
		return true;
	}

	if (menu == 1 || menu == 2)
		push(radius * 0.65f, 0.f, -radius * 0.4f);
	vec3_t cap_a{}, cap_b{};
	g_math->vector_transform(data.m_mins, data.m_matrix, cap_a);
	g_math->vector_transform(data.m_maxs, data.m_matrix, cap_b);
	auto toward_cam = [&](const vec3_t& end) {
		const float dx = end.x - world.x;
		const float dy = end.y - world.y;
		const float dz = end.z - world.z;
		return dx * forward.x + dy * forward.y + dz * forward.z > 0.f;
	};
	if (toward_cam(cap_a))
		points.emplace_back(cap_a, hitbox, false);
	if (toward_cam(cap_b))
		points.emplace_back(cap_b, hitbox, false);
	return true;
}


namespace {

float capsule_enter(const vec3_t& start, const vec3_t& dir, const vec3_t& a, const vec3_t& b, float radius, float max_t) {
	const float abx = b.x - a.x;
	const float aby = b.y - a.y;
	const float abz = b.z - a.z;
	const float ab_len2 = abx * abx + aby * aby + abz * abz;

	auto sphere = [&](float cx, float cy, float cz) -> float {
		const float ox = start.x - cx;
		const float oy = start.y - cy;
		const float oz = start.z - cz;
		const float A = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
		const float B = 2.f * (ox * dir.x + oy * dir.y + oz * dir.z);
		const float C = ox * ox + oy * oy + oz * oz - radius * radius;
		const float disc = B * B - 4.f * A * C;
		if (disc < 0.f || A < 1e-8f)
			return -1.f;
		const float root = std::sqrt(disc);
		float t = (-B - root) / (2.f * A);
		if (t < 0.f)
			t = (-B + root) / (2.f * A);
		if (t < 0.f || t > max_t)
			return -1.f;
		return t;
	};

	if (ab_len2 < 1e-4f)
		return sphere(a.x, a.y, a.z);

	const float aox = start.x - a.x;
	const float aoy = start.y - a.y;
	const float aoz = start.z - a.z;
	const float d_ab = dir.x * abx + dir.y * aby + dir.z * abz;
	const float ao_ab = aox * abx + aoy * aby + aoz * abz;
	const float dir_len2 = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
	const float A = ab_len2 * dir_len2 - d_ab * d_ab;
	const float B = 2.f * (ab_len2 * (aox * dir.x + aoy * dir.y + aoz * dir.z) - d_ab * ao_ab);
	const float C = ab_len2 * (aox * aox + aoy * aoy + aoz * aoz) - ao_ab * ao_ab - radius * radius * ab_len2;

	float best = -1.f;
	auto take = [&](float t) {
		if (t >= 0.f && t <= max_t && (best < 0.f || t < best))
			best = t;
	};
	if (std::fabs(A) > 1e-5f) {
		const float disc = B * B - 4.f * A * C;
		if (disc >= 0.f) {
			const float root = std::sqrt(disc);
			const float t0 = (-B - root) / (2.f * A);
			const float t1 = (-B + root) / (2.f * A);
			for (float t : { t0, t1 }) {
				if (t < 0.f || t > max_t)
					continue;
				const float along = (ao_ab + t * d_ab) / ab_len2;
				if (along >= 0.f && along <= 1.f)
					take(t);
			}
		}
	}
	take(sphere(a.x, a.y, a.z));
	take(sphere(b.x, b.y, b.z));
	return best;
}

int scan_rank(int hitbox) {
	switch (hitbox) {
	case HITBOX_STOMACH:
	case HITBOX_PELVIS: return 0;
	case HITBOX_CHEST:
	case HITBOX_UPPER_CHEST:
	case HITBOX_LOWER_CHEST: return 1;
	case HITBOX_RIGHT_UPPER_ARM:
	case HITBOX_LEFT_UPPER_ARM:
	case HITBOX_RIGHT_FOREARM:
	case HITBOX_LEFT_FOREARM:
	case HITBOX_RIGHT_HAND:
	case HITBOX_LEFT_HAND: return 2;
	case HITBOX_RIGHT_THIGH:
	case HITBOX_LEFT_THIGH:
	case HITBOX_RIGHT_CALF:
	case HITBOX_LEFT_CALF: return 3;
	case HITBOX_RIGHT_FOOT:
	case HITBOX_LEFT_FOOT: return 4;
	case HITBOX_HEAD: return 5;
	default: return 6;
	}
}

float aim_margin(const vec3_t& eye, const vec3_t& point, const vec3_t& cap_a, const vec3_t& cap_b, float radius,
	float inaccuracy, float spread) {
	const vec3_t center{
		(cap_a.x + cap_b.x) * 0.5f,
		(cap_a.y + cap_b.y) * 0.5f,
		(cap_a.z + cap_b.z) * 0.5f
	};
	const float pdx = point.x - eye.x, pdy = point.y - eye.y, pdz = point.z - eye.z;
	const float cdx = center.x - eye.x, cdy = center.y - eye.y, cdz = center.z - eye.z;
	const float dp = std::sqrt(pdx * pdx + pdy * pdy + pdz * pdz);
	const float dc = std::sqrt(cdx * cdx + cdy * cdy + cdz * cdz);
	if (dp < 1.f || dc < 1.f)
		return 0.f;
	const float cosang = clampf((pdx * cdx + pdy * cdy + pdz * cdz) / (dp * dc), -1.f, 1.f);
	const float ang = std::acos(cosang);
	const float ang_radius = std::asin(clampf(radius / dc, 0.f, 1.f));
	const float cone = (std::max)(inaccuracy + spread, 0.0002f);
	return (ang_radius - ang) / cone;
}

bool better_shot(int hitbox, float damage, bool center, float margin, int best_hitbox, float best_damage, bool best_center,
	float best_margin, int health, bool prefer_body) {
	if (best_hitbox < 0)
		return true;
	const bool body = hitbox != HITBOX_HEAD;
	const bool best_body = best_hitbox != HITBOX_HEAD;
	const bool lethal = damage >= static_cast<float>(health);
	const bool best_lethal = best_damage >= static_cast<float>(health);
	const int rank = (body && lethal) ? 0 : (prefer_body && body) ? 1 : lethal ? 2 : 3;
	const int best_rank = (best_body && best_lethal) ? 0 : (prefer_body && best_body) ? 1 : best_lethal ? 2 : 3;
	if (rank != best_rank)
		return rank < best_rank;
	if (body && best_body && lethal && best_lethal && scan_rank(hitbox) != scan_rank(best_hitbox))
		return scan_rank(hitbox) < scan_rank(best_hitbox);
	// A multipoint on the exposed shell that the cone can cover beats a center
	// we can only graze. Otherwise 100% points never get shot.
	if (margin > best_margin + 0.2f)
		return true;
	if (best_margin > margin + 0.2f)
		return false;
	if (center != best_center)
		return center;
	return damage > best_damage + 0.05f;
}

} // namespace

int c_rage_bot::minimum_for(c_cs_player_pawn* target, c_cs_weapon_base_v_data* data) const {
	const int health = target ? (std::max)(target->m_health(), 1) : 100;
	const bool taser = data && data->m_weapon_type() == WEAPONTYPE_TASER;
	const bool override_damage = g_key_handler->is_pressed(
		g_cfg->rage_bot.m_override_damage_key_bind, g_cfg->rage_bot.m_override_damage_key_bind_style);
	int minimum = weapon_config().m_minimum_damage < 0 ? health : weapon_config().m_minimum_damage;
	if (override_damage)
		minimum = 1;
	minimum = (std::min)((std::max)(minimum, 1), health);
	if (taser)
		minimum = health;
	return minimum;
}

int c_rage_bot::hitgroup_of(const hitbox_data_t& data, int hitbox) const {
	if (data.m_num_hitgroup >= HITGROUP_HEAD && data.m_num_hitgroup <= HITGROUP_NECK)
		return data.m_num_hitgroup;
	switch (hitbox) {
	case HITBOX_HEAD: return HITGROUP_HEAD;
	case HITBOX_NECK: return HITGROUP_NECK;
	case HITBOX_PELVIS:
	case HITBOX_STOMACH: return HITGROUP_STOMACH;
	case HITBOX_CHEST:
	case HITBOX_LOWER_CHEST:
	case HITBOX_UPPER_CHEST: return HITGROUP_CHEST;
	case HITBOX_RIGHT_HAND:
	case HITBOX_RIGHT_UPPER_ARM:
	case HITBOX_RIGHT_FOREARM: return HITGROUP_RIGHTARM;
	case HITBOX_LEFT_HAND:
	case HITBOX_LEFT_UPPER_ARM:
	case HITBOX_LEFT_FOREARM: return HITGROUP_LEFTARM;
	case HITBOX_RIGHT_THIGH:
	case HITBOX_RIGHT_CALF:
	case HITBOX_RIGHT_FOOT: return HITGROUP_RIGHTLEG;
	case HITBOX_LEFT_THIGH:
	case HITBOX_LEFT_CALF:
	case HITBOX_LEFT_FOOT: return HITGROUP_LEFTLEG;
	default: return HITGROUP_CHEST;
	}
}

float c_rage_bot::unscaled_damage(c_cs_weapon_base_v_data* data, float distance) const {
	if (!data)
		return 0.f;
	float damage = static_cast<float>(data->m_damage());
	if (!(damage > 0.f))
		return 0.f;
	const float range = data->m_range();
	if (range > 1.f && distance > range)
		return 0.f;
	const float modifier = data->m_range_modifier();
	if (modifier > 0.f && modifier < 1.f && distance > 0.f)
		damage *= std::pow(modifier, distance / 500.f);
	return damage;
}

float c_rage_bot::scaled_damage(c_cs_weapon_base_v_data* data, c_cs_player_pawn* pawn, int hitgroup,
	float distance, float pen_ratio) const {
	float damage = unscaled_damage(data, distance) * clampf(pen_ratio, 0.f, 1.f);
	if (damage < 1.f || !pawn)
		return 0.f;
	g_auto_wall->scale_damage(hitgroup, pawn, data, damage);
	return damage;
}

bool c_rage_bot::hitbox_can_meet(c_cs_weapon_base_v_data* data, c_cs_player_pawn* pawn, int hitbox,
	float distance, float minimum, bool taser) const {
	if (taser)
		return true;
	hitbox_data_t box{};
	box.m_num_hitgroup = 0;
	const int group = hitgroup_of(box, hitbox);
	// Optimistic distance already subtracted by the caller. This is the ceiling,
	// not the shot we will take — if even this is under min damage, don't scan.
	return scaled_damage(data, pawn, group, distance, 1.f) + 0.05f >= minimum;
}

void c_rage_bot::build_capsules(lag_record_t* record, rage_capsule_t* caps, int count) const {
	if (!caps || count <= 0)
		return;
	for (int i = 0; i < count; ++i)
		caps[i] = {};
	if (!record || !record->m_pawn)
		return;
	const int limit = (std::min)(count, static_cast<int>(HITBOX_MAX));
	for (int hitbox = 0; hitbox < limit; ++hitbox) {
		hitbox_data_t data = get_hitbox_data(record->m_pawn, hitbox, record);
		if (data.m_invalid_data)
			continue;
		rage_capsule_t& cap = caps[hitbox];
		g_math->vector_transform(data.m_mins, data.m_matrix, cap.m_a);
		g_math->vector_transform(data.m_maxs, data.m_matrix, cap.m_b);
		cap.m_radius = (std::max)(data.m_radius, 0.1f);
		cap.m_hitbox = hitbox;
		cap.m_hitgroup = hitgroup_of(data, hitbox);
		cap.m_valid = true;
	}
}

int c_rage_bot::capsule_hit(const rage_capsule_t* caps, int count, const vec3_t& eye, const vec3_t& point, float& entry, int prefer) const {
	entry = 0.f;
	if (!caps || count <= 0)
		return -1;
	const float dx = point.x - eye.x;
	const float dy = point.y - eye.y;
	const float dz = point.z - eye.z;
	const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
	if (dist < 1.f)
		return -1;
	const vec3_t dir{ dx / dist, dy / dist, dz / dist };
	const float max_t = dist + 8.f;
	int first = -1;
	float first_t = max_t + 1.f;
	float prefer_t = -1.f;
	for (int i = 0; i < count; ++i) {
		if (!caps[i].m_valid)
			continue;
		const float t = capsule_enter(eye, dir, caps[i].m_a, caps[i].m_b, caps[i].m_radius + 1.25f, max_t);
		if (t < 0.f)
			continue;
		if (caps[i].m_hitbox == prefer && (prefer_t < 0.f || t < prefer_t))
			prefer_t = t;
		if (t < first_t) {
			first_t = t;
			first = caps[i].m_hitbox;
		}
	}
	// Overlapping capsules (chest/stomach) can be a fraction closer. An arm
	// actually in front is several units closer and must still win.
	if (prefer >= 0 && prefer_t >= 0.f && prefer_t <= first_t + 1.5f) {
		entry = prefer_t;
		return prefer;
	}
	entry = first >= 0 ? first_t : dist;
	return first;
}

float c_rage_bot::score_centers(lag_record_t* record, const vec3_t& eye, c_cs_weapon_base_v_data* data,
	bool allow_pen, bool taser, int minimum) const {
	if (!record || !record->m_pawn || !data)
		return -1.f;
	rage_capsule_t caps[HITBOX_MAX]{};
	build_capsules(record, caps, HITBOX_MAX);
	const float optimistic = (std::max)(1.f, eye.dist(record->m_origin) - 48.f);
	float best = -1.f;
	for (int hitbox : m_hitboxes) {
		if (hitbox < 0 || hitbox >= HITBOX_MAX || !caps[hitbox].m_valid)
			continue;
		if (!hitbox_can_meet(data, record->m_pawn, hitbox, optimistic, static_cast<float>(minimum), taser))
			continue;
		vec3_t center{
			(caps[hitbox].m_a.x + caps[hitbox].m_b.x) * 0.5f,
			(caps[hitbox].m_a.y + caps[hitbox].m_b.y) * 0.5f,
			(caps[hitbox].m_a.z + caps[hitbox].m_b.z) * 0.5f
		};
		float entry = 0.f;
		if (capsule_hit(caps, HITBOX_MAX, eye, center, entry, hitbox) != hitbox)
			continue;
		penetration_data_t pen{};
		if (!g_auto_wall->fire_bullet(eye, center, record->m_pawn, data, pen, taser))
			continue;
		if (!allow_pen && pen.m_penetrated)
			continue;
		float ratio = 1.f;
		const float expected = unscaled_damage(data, entry);
		if (expected > 1.f && pen.m_damage > 0.f && pen.m_damage <= expected * 1.15f)
			ratio = clampf(pen.m_damage / expected, 0.05f, 1.f);
		else if (pen.m_penetrated && expected > 1.f && pen.m_damage > expected * 1.15f)
			ratio = clampf(expected / pen.m_damage, 0.05f, 1.f);
		best = (std::max)(best, scaled_damage(data, record->m_pawn, caps[hitbox].m_hitgroup, entry, ratio));
	}
	return best;
}

void c_rage_bot::press_scope(c_user_cmd* cmd) {
	if (!cmd || !cmd->pb.has_base())
		return;
	cmd->m_button_state.m_button_state |= IN_ATTACK2;
	cmd->m_button_state.m_button_state2 |= IN_ATTACK2;
	cmd->m_button_state.m_button_state3 |= IN_ATTACK2;
	if (cmd->pb.input_history_size() > 0)
		cmd->pb.set_attack2_start_history_index(0);
	if (cmd->pb.mutable_base()->subtick_moves_size() >= 12)
		return;
	auto* press = g_protobuf->add_subtick_move_step(cmd);
	if (!press)
		return;
	press->set_button(IN_ATTACK2);
	press->set_pressed(true);
	press->set_when(0.f);
	press->clear_analog_forward_delta();
	press->clear_analog_left_delta();
	press->clear_pitch_delta();
	press->clear_yaw_delta();
}

aim_point_t c_rage_bot::select_points(lag_record_t* record, float& damage, float inaccuracy, float spread, int resolver_side, int minimum) {
	aim_point_t best{};
	damage = 0.f;
	if (!record || !record->m_pawn || !g_ctx->m_local_pawn)
		return best;

	auto* weapon = g_ctx->m_local_pawn->get_active_weapon();
	auto* weapon_data = weapon ? weapon->get_weapon_data() : nullptr;
	const auto* local = g_prediction->get_local_data();
	if (!weapon_data || !local)
		return best;

	const bool taser = weapon_data->m_weapon_type() == WEAPONTYPE_TASER;
	const bool allow_pen = weapon_config().m_penetration;
	const bool prefer_body = weapon_config().m_prefer_body_aim;
	const int health = (std::max)(record->m_pawn->m_health(), 1);
	const float need = static_cast<float>(minimum);
	vec3_t eye = local->m_eye_pos;

	rage_capsule_t caps[HITBOX_MAX]{};
	build_capsules(record, caps, HITBOX_MAX);

	const float optimistic = (std::max)(1.f, eye.dist(record->m_origin) - 48.f);
	int scan[HITBOX_MAX]{};
	int scan_count = 0;
	for (int hitbox : m_hitboxes) {
		if (hitbox < 0 || hitbox >= HITBOX_MAX || !caps[hitbox].m_valid)
			continue;
		if (!hitbox_can_meet(weapon_data, record->m_pawn, hitbox, optimistic, need, taser))
			continue;
		scan[scan_count++] = hitbox;
	}
	for (int i = 1; i < scan_count; ++i) {
		const int key = scan[i];
		int j = i;
		while (j > 0 && scan_rank(scan[j - 1]) > scan_rank(key)) {
			scan[j] = scan[j - 1];
			--j;
		}
		scan[j] = key;
	}

	float best_damage = 0.f;
	float best_margin = -100.f;
	bool lethal_body = false;

	record->apply(record->m_pawn);
	for (int i = 0; i < scan_count; ++i) {
		const int hitbox = scan[i];
		// A lethal stomach/chest already beats the head. Don't spend traces on it.
		if (lethal_body && hitbox == HITBOX_HEAD)
			continue;
		if (lethal_body && best.m_center && scan_rank(hitbox) > scan_rank(best.m_hitbox))
			continue;

		std::vector<aim_point_t> points;
		if (!multi_points(record, hitbox, points, inaccuracy, spread, resolver_side))
			continue;

		for (auto& point : points) {
			float entry = 0.f;
			const int struck = capsule_hit(caps, HITBOX_MAX, eye, point.m_point, entry, hitbox);
			// The bullet hits whatever capsule it enters first. Aiming at stomach
			// while an arm is in front is a 72 shot, not a 100 stomach shot.
			if (struck != hitbox)
				continue;

			penetration_data_t pen{};
			if (!g_auto_wall->fire_bullet(eye, point.m_point, record->m_pawn, weapon_data, pen, taser))
				continue;
			if (!allow_pen && pen.m_penetrated)
				continue;

			float ratio = 1.f;
			const float expected = unscaled_damage(weapon_data, entry);
			if (expected > 1.f && pen.m_damage > 0.f && pen.m_damage <= expected * 1.15f)
				ratio = clampf(pen.m_damage / expected, 0.05f, 1.f);
			else if (pen.m_penetrated && expected > 1.f && pen.m_damage > expected * 1.15f)
				ratio = clampf(expected / pen.m_damage, 0.05f, 1.f);
			const float dealt = scaled_damage(weapon_data, record->m_pawn, caps[hitbox].m_hitgroup, entry, ratio);
			if (dealt + 0.05f < need)
				continue;

			point.m_damage = dealt;
			point.m_penetrated = pen.m_penetrated;
			const float margin = caps[hitbox].m_valid
				? aim_margin(eye, point.m_point, caps[hitbox].m_a, caps[hitbox].m_b, caps[hitbox].m_radius, inaccuracy, spread)
				: 0.f;
			if (!better_shot(hitbox, dealt, point.m_center, margin, best.m_hitbox, best_damage, best.m_center, best_margin, health, prefer_body))
				continue;
			best = point;
			best_damage = dealt;
			best_margin = margin;
			if (hitbox != HITBOX_HEAD && dealt >= static_cast<float>(health))
				lethal_body = true;
		}
	}
	record->reset(record->m_pawn);

	damage = best_damage;
	return best;
}

float c_rage_bot::predict_inaccuracy(c_base_player_weapon* weapon, c_cs_weapon_base_v_data* data,
	float speed, float vertical_speed, bool on_ground, bool ducking) const {
	if (!weapon)
		return 1.f;

	const bool scoped = g_ctx->m_local_pawn && g_ctx->m_local_pawn->m_scoped();
	int mode = weapon->m_weapon_mode();
	if (mode != 0 && mode != 1)
		mode = scoped ? 1 : 0;

	float penalty = weapon->m_accuracy_penalty();
	if (!sane(penalty, 2.f))
		penalty = 0.f;

	if (!data)
		return weapon->get_inaccuracy();

	if (!on_ground) {
		const float initial = read_firing(data->m_inaccuracy_jump_initial(), mode);
		const float apex = read_firing(data->m_inaccuracy_jump_apex(), mode);
		if (initial >= 0.f && apex >= 0.f)
			return air_inaccuracy(vertical_speed, initial, apex) + penalty;
		return weapon->get_inaccuracy();
	}

	const float base = ducking
		? read_firing(data->m_inaccuracy_crouch(), mode)
		: read_firing(data->m_inaccuracy_stand(), mode);
	float move = read_firing(data->m_inaccuracy_move(), mode);
	if (base < 0.f)
		return weapon->get_inaccuracy();
	if (move < 0.f)
		move = 0.f;

	float max_speed = weapon->get_max_speed();
	if (!sane(max_speed, 500.f))
		max_speed = 250.f;
	const float lo = max_speed * 0.34f;
	const float hi = max_speed * 0.95f;
	float blend = 0.f;
	if (speed > lo && hi > lo)
		blend = clampf((speed - lo) / (hi - lo), 0.f, 1.f);
	return base + move * blend + penalty;
}

bool c_rage_bot::at_max_accuracy(c_cs_weapon_base_v_data* data, c_base_player_weapon* weapon, float inaccuracy) const {
	auto* pawn = g_ctx->m_local_pawn;
	if (!pawn || !data)
		return false;
	const bool ground = (pawn->m_flags() & FL_ONGROUND) != 0;
	const float speed = pawn->m_vec_abs_velocity().length_2d();
	if (!ground) {
		const float apex = read_firing(data->m_inaccuracy_jump_apex(), pawn->m_scoped() ? 1 : 0);
		float penalty = weapon ? weapon->m_accuracy_penalty() : 0.f;
		if (!sane(penalty, 2.f))
			penalty = 0.f;
		return apex >= 0.f && inaccuracy <= apex + penalty + 0.001f;
	}
	if (data->m_weapon_type() == WEAPONTYPE_SNIPER_RIFLE) {
		if (!pawn->m_scoped())
			return false;
		const bool duck = (pawn->m_flags() & FL_DUCKING) != 0;
		const float scale = duck ? 300.f : 170.f;
		const float rounded = std::floor(inaccuracy * scale) / scale;
		return speed <= (duck ? 10.f : 0.1f) && rounded < inaccuracy;
	}
	const float max_speed = weapon ? weapon->get_max_speed() : 250.f;
	return speed <= max_speed * 0.34f;
}

// ============================================================================
// SPREAD / HITCHANCE
// ============================================================================

bool c_rage_bot::game_spread(c_base_player_weapon* weapon, int seed, float inaccuracy, float spread, float& out_x, float& out_y) const {
	using fn_t = void(__fastcall*)(std::int16_t, int, int, std::uint32_t, float, float, float, float*, float*);
	static fn_t fn = reinterpret_cast<fn_t>(g_opcodes->scan(
		g_modules->m_modules.client_dll.get_name(), andromeda::calc_spread));
	if (!weapon)
		return false;

	auto* data = weapon->get_weapon_data();
	int bullets = data ? static_cast<int>(data->m_bullets()) : 1;
	if (bullets < 1)
		bullets = 1;
	float recoil = weapon->m_recoil_index();
	if (!sane(recoil, 32.f))
		recoil = 0.f;

	if (fn) {
		fn(static_cast<std::int16_t>(weapon->get_item_def_index()), bullets, 0,
			static_cast<std::uint32_t>(seed + 1), inaccuracy, spread, recoil, &out_x, &out_y);
		return std::isfinite(out_x) && std::isfinite(out_y);
	}

	// Pattern miss only. Same split the game uses (inaccuracy disk + spread disk),
	// but a private LCG so we never touch the client's RandomFloat state.
	auto lcg = [](std::uint32_t& s) { s = s * 1664525u + 1013904223u; return s; };
	std::uint32_t state = static_cast<std::uint32_t>(seed + 1) * 747796405u + static_cast<std::uint32_t>(bullets);
	auto unit = [&]() { return static_cast<float>(lcg(state) & 0xFFFFFF) / static_cast<float>(0x1000000); };
	const float r1 = unit();
	const float r2 = unit() * _pi2;
	const float r3 = unit();
	const float r4 = unit() * _pi2;
	out_x = std::cos(r2) * r1 * inaccuracy + std::cos(r4) * r3 * spread;
	out_y = std::sin(r2) * r1 * inaccuracy + std::sin(r4) * r3 * spread;
	return true;
}

bool c_rage_bot::ray_capsule(const vec3_t& start, const vec3_t& dir, const vec3_t& cap_a, const vec3_t& cap_b,
	float radius, float max_dist) const {
	vec3_t axis = cap_b - cap_a;
	vec3_t rel = cap_a - start;
	const float a = axis.dot(axis);
	const float e = dir.dot(dir);
	const float b = axis.dot(dir);
	const float c = axis.dot(rel);
	const float f = dir.dot(rel);
	const float denom = a * e - b * b;

	float s = 0.f;
	float t = 0.f;
	if (a < 1e-6f || denom < 1e-6f)
		t = e > 1e-6f ? f / e : 0.f;
	else {
		t = (f * a - c * b) / denom;
		s = a > 1e-6f ? (t * b - c) / a : 0.f;
		s = clampf(s, 0.f, 1.f);
	}
	t = clampf(t, 0.f, max_dist);
	if (a > 1e-6f)
		s = clampf((t * b - c) / a, 0.f, 1.f);

	vec3_t on_ray = start + dir * t;
	vec3_t on_cap = cap_a + axis * s;
	return on_ray.dist(on_cap) <= radius;
}

int c_rage_bot::hitchance(const vec3_t& eye, const vec3_t& angles, lag_record_t* record, int hitbox,
	c_base_player_weapon* weapon, float inaccuracy, float spread, int samples, int required) const {
	if (!record || !record->m_pawn || !weapon || samples <= 0)
		return 0;
	if (inaccuracy + spread <= 0.00005f)
		return 100;

	hitbox_data_t data = get_hitbox_data(record->m_pawn, hitbox, record);
	if (data.m_invalid_data || data.m_radius <= 0.f)
		return 0;

	vec3_t cap_a{}, cap_b{};
	g_math->vector_transform(data.m_mins, data.m_matrix, cap_a);
	g_math->vector_transform(data.m_maxs, data.m_matrix, cap_b);

	vec3_t forward, right, up;
	g_math->angle_vectors(angles, forward, right, up);

	auto* weapon_data = weapon->get_weapon_data();
	const float range = weapon_data && weapon_data->m_range() > 1.f ? weapon_data->m_range() : 8192.f;
	int hits = 0;
	int tested = 0;
	for (int seed = 0; seed < samples; ++seed) {
		float sx = 0.f, sy = 0.f;
		if (!game_spread(weapon, seed, inaccuracy, spread, sx, sy))
			continue;
		vec3_t direction{
			forward.x + right.x * sx + up.x * sy,
			forward.y + right.y * sx + up.y * sy,
			forward.z + right.z * sx + up.z * sy
		};
		const float dir_len = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
		if (dir_len < 0.001f)
			continue;
		direction.x /= dir_len; direction.y /= dir_len; direction.z /= dir_len;
		const float entry = capsule_enter(eye, direction, cap_a, cap_b, data.m_radius, range);
		++tested;
		if (entry >= 0.f)
			++hits;

		// Percent is hits/tested, never hits/total-after-an-early-abort.
		// Aborting and dividing by the full sample count turned a 70% shot into ~40.
		if (tested >= 32 && tested >= samples / 2) {
			const int pct = hits * 100 / tested;
			if (pct >= required)
				return std::clamp(pct, 0, 100);
			const int possible = (hits + (samples - tested)) * 100 / samples;
			if (possible < required)
				return std::clamp(pct, 0, 100);
		}
	}
	if (tested <= 0)
		return 0;
	return std::clamp(hits * 100 / tested, 0, 100);
}

float c_rage_bot::required_hitchance(int configured, float inaccuracy, float spread, float distance, float radius,
	int health, float damage, bool taser) const {
	if (taser)
		return 70.f;
	if (configured >= 0)
		return static_cast<float>(configured);

	const float cone = inaccuracy + spread;
	if (cone <= 0.00005f)
		return 0.f;
	const float cover = radius / (std::max)(distance, 1.f) / cone;
	float required = clampf(cover * 85.f, 28.f, 78.f);
	if (damage >= static_cast<float>(health))
		required = (std::min)(required, 42.f);
	return required;
}

// ============================================================================
// TARGETING
// ============================================================================

lag_record_t* c_rage_bot::select_record(int handle) {
	auto it = m_lag_records.find(handle);
	if (it == m_lag_records.end() || it->second.empty())
		return nullptr;

	auto& cfg = weapon_config();
	int depth = cfg.m_backtrack ? std::clamp(cfg.m_backtrack_ticks, 0, 64) : 1;
	if (!cfg.m_backtrack)
		depth = 1;
	else if (depth == 0)
		depth = static_cast<int>(it->second.size());

	const auto* local = g_prediction->get_local_data();
	if (!local)
		return nullptr;

	auto* weapon = g_ctx->m_local_pawn->get_active_weapon();
	auto* data = weapon ? weapon->get_weapon_data() : nullptr;
	if (!data)
		return nullptr;
	const bool taser = data->m_weapon_type() == WEAPONTYPE_TASER;
	const bool allow_pen = cfg.m_penetration;

	lag_record_t* best = nullptr;
	float best_damage = -1.f;
	int seen = 0;
	const lag_record_t* previous = nullptr;

	for (auto& record : it->second) {
		if (!record.m_pawn || record.m_stored_bone_count <= 0 || !record.is_valid())
			continue;
		if (previous && std::isfinite(previous->m_simulation_time)) {
			const float dt = previous->m_simulation_time - record.m_simulation_time;
			vec3_t delta = previous->m_origin - record.m_origin;
			if (dt > 0.001f && delta.length() > 4000.f * dt)
				continue;
		}
		if (!best)
			best = &record;
		if (++seen > depth)
			break;

		record.apply(record.m_pawn);
		const int minimum = minimum_for(record.m_pawn, data);
		const float damage = score_centers(&record, local->m_eye_pos, data, allow_pen, taser, minimum);
		record.reset(record.m_pawn);

		if (damage > best_damage) {
			best_damage = damage;
			best = &record;
		}
		if (record.m_pawn && damage >= static_cast<float>(record.m_pawn->m_health()))
			break;
		previous = &record;
	}
	return best;
}

void c_rage_bot::find_targets() {
	m_aim_targets.clear();
	if (!g_ctx->m_local_pawn || !g_ctx->m_local_pawn->is_alive())
		return;

	for (auto entity : g_entity_system->get("CCSPlayerController")) {
		auto* controller = reinterpret_cast<c_cs_player_controller*>(entity);
		if (!controller || controller == g_ctx->m_local_controller)
			continue;
		const int handle = controller->get_handle().to_int();
		lag_record_t* record = select_record(handle);
		if (!record)
			continue;
		m_aim_targets.insert_or_assign(handle, aim_target_t(record));
	}
}

bool c_rage_bot::build_prediction(int handle, lag_record_t& out) const {
	auto it = m_lag_records.find(handle);
	if (it == m_lag_records.end() || it->second.empty())
		return false;

	const int max_ticks = std::clamp(weapon_config().m_extrapolate, 0, 16);
	const auto& newest = it->second.front();
	const motion_model_t model = analyze_motion(it->second, max_ticks);
	const int ticks = ticks_worth_predicting(newest, model, max_ticks);
	if (ticks <= 0)
		return false;

	const auto* local = g_prediction->get_local_data();
	auto* weapon = g_ctx->m_local_pawn ? g_ctx->m_local_pawn->get_active_weapon() : nullptr;
	auto* data = weapon ? weapon->get_weapon_data() : nullptr;
	if (!local || !data)
		return false;
	const bool taser = data->m_weapon_type() == WEAPONTYPE_TASER;
	const bool allow_pen = weapon_config().m_penetration;

	auto score = [&](lag_record_t& record) {
		if (!record.m_pawn)
			return -1.f;
		record.apply(record.m_pawn);
		const int minimum = minimum_for(record.m_pawn, data);
		const float best = score_centers(&record, local->m_eye_pos, data, allow_pen, taser, minimum);
		record.reset(record.m_pawn);
		return best;
	};

	lag_record_t baseline = newest;
	const float base_damage = score(baseline);
	float best_weighted = base_damage;
	bool found = false;

	motion_step_t step{};
	step.origin = newest.m_origin;
	step.velocity = newest.m_velocity;
	step.flags = newest.m_flags;
	step.mins = newest.m_mins;
	step.maxs = newest.m_maxs;
	if (step.maxs.z - step.mins.z < 8.f) {
		step.mins = { -16.f, -16.f, 0.f };
		step.maxs = { 16.f, 16.f, 72.f };
	}
	step.direction = model.direction;

	for (int tick = 1; tick <= ticks; ++tick) {
		step.direction += model.yaw_per_tick;
		float speed = step.velocity.length_2d() + model.speed_per_tick;
		if (speed < 0.f)
			speed = 0.f;
		const float rad = deg2rad(step.direction);
		step.velocity.x = std::cos(rad) * speed;
		step.velocity.y = std::sin(rad) * speed;
		step_movement(step, newest.m_pawn);

		lag_record_t candidate = newest;
		candidate.shift(step.origin - newest.m_origin);
		candidate.m_velocity = step.velocity;
		candidate.m_flags = step.flags;
		candidate.m_extrapolated = true;
		candidate.m_extrapolate_ticks = tick;
		candidate.m_confidence = model.confidence;

		const float damage = score(candidate);
		const float weighted = damage * (1.f - static_cast<float>(tick) * 0.035f) * model.confidence;
		if (damage >= 0.f && weighted > best_weighted) {
			best_weighted = weighted;
			out = candidate;
			found = true;
		}
	}

	// Current networked position is already good enough — don't lead a shot we can hit now.
	if (found && base_damage >= 0.f && base_damage >= best_weighted / (std::max)(model.confidence, 0.2f) * 0.92f)
		return false;
	return found;
}


// Camera angles are where the player looks. Command angles are already the
// anti-aim yaw by the time rage runs, so an FOV cone on those follows the
// character model instead of the face.
vec3_t player_camera_angles() {
	if (g_interfaces && g_interfaces->m_csgo_input) {
		vec3_t camera = g_interfaces->m_csgo_input->get_view_angles();
		if (std::isfinite(camera.x) && std::isfinite(camera.y))
			return camera;
	}
	if (g_ctx->m_user_cmd && g_ctx->m_user_cmd->pb.has_base()) {
		const auto view = g_ctx->m_user_cmd->pb.base().viewangles();
		return { view.x(), view.y(), view.z() };
	}
	return {};
}

void c_rage_bot::select_target(float inaccuracy, float spread) {
	m_best_target = nullptr;
	if (m_aim_targets.empty() || !g_ctx->m_local_pawn)
		return;

	auto* weapon = g_ctx->m_local_pawn->get_active_weapon();
	auto* weapon_data = weapon ? weapon->get_weapon_data() : nullptr;
	const auto* local = g_prediction->get_local_data();
	if (!weapon_data || !local)
		return;

	float cos_fov = -2.f;
	vec3_t view_dir{};
	if (g_cfg->rage_bot.m_fov > 0 && g_cfg->rage_bot.m_fov < 180) {
		vec3_t angles = player_camera_angles();
		g_math->angle_vectors(angles, view_dir);
		view_dir = view_dir.normalize();
		cos_fov = std::cos(deg2rad(static_cast<float>(g_cfg->rage_bot.m_fov)));
	}

	float best_damage = -1.f;
	float best_distance = FLT_MAX;
	bool best_lethal = false;

	for (auto& [handle, target] : m_aim_targets) {
		if (!target.m_lag_record || !target.m_pawn || !target.m_pawn->is_alive())
			continue;

		const int minimum = minimum_for(target.m_pawn, weapon_data);

		lag_record_t predicted{};
		const bool have_prediction = build_prediction(handle, predicted);
		lag_record_t candidate = have_prediction ? predicted : *target.m_lag_record;

		const int side = resolver_side(handle, target.m_pawn, &candidate);
		float damage = 0.f;
		aim_point_t point = select_points(&candidate, damage, inaccuracy, spread, side, minimum);
		if (point.m_hitbox < 0 || damage < static_cast<float>(minimum))
			continue;

		if (cos_fov > -1.5f) {
			vec3_t to = point.m_point - local->m_eye_pos;
			const float len = to.length();
			if (len > 1.f && (to / len).dot(view_dir) < cos_fov)
				continue;
		}

		const float distance = point.m_point.dist(local->m_eye_pos);
		const bool kills = damage >= static_cast<float>(target.m_pawn->m_health());
		const bool better = kills != best_lethal ? kills
			: (damage > best_damage || (damage == best_damage && distance < best_distance));
		if (better) {
			best_damage = damage;
			best_distance = distance;
			best_lethal = kills;
			m_active_record = candidate;
			m_active_handle = handle;
			m_active_side = side;
			m_best_target = &target;
			m_best_target->m_best_point = std::make_unique<aim_point_t>(point);
		}
	}
}

// ============================================================================
// RESOLVER
// ============================================================================

int c_rage_bot::resolver_side(int handle, c_cs_player_pawn* pawn, const lag_record_t* record) {
	if (!g_cfg->rage_bot.m_resolver_enabled || !pawn || !record)
		return 0;

	auto& state = m_resolver[handle];
	const bool override_on = g_cfg->rage_bot.m_resolver_override && (g_cfg->rage_bot.m_resolver_override_key <= 0
		|| g_key_handler->is_pressed(g_cfg->rage_bot.m_resolver_override_key, g_cfg->rage_bot.m_resolver_override_key_style));
	if (override_on)
		return state.m_side;

	vec3_t velocity = record->m_velocity;
	const float speed = velocity.length_2d();
	float eye_yaw = record->m_eye_angles.y;
	if (!std::isfinite(eye_yaw))
		return state.m_misses > 0 ? state.m_side : 0;

	if (state.m_has_eye) {
		const float jitter = std::fabs(g_math->angle_diff(state.m_last_eye_yaw, eye_yaw));
		if (jitter > 20.f && jitter < 160.f && speed < 30.f)
			return state.m_side;
	}
	state.m_last_eye_yaw = eye_yaw;
	state.m_has_eye = true;

	if (speed > 40.f) {
		const float move_yaw = rad2deg(std::atan2(velocity.y, velocity.x));
		const float delta = g_math->angle_diff(move_yaw, eye_yaw);
		if (std::fabs(delta) > 28.f)
			state.m_side = delta > 0.f ? 1 : -1;
		else if (state.m_misses == 0)
			return 0;
	}
	return state.m_misses > 0 || speed > 40.f ? state.m_side : 0;
}

void c_rage_bot::note_shot(int handle, c_cs_player_pawn* pawn, int side) {
	m_last_shot.m_handle = handle;
	m_last_shot.m_health = pawn ? pawn->m_health() : 0;
	m_last_shot.m_tick = g_ctx->m_local_controller ? g_ctx->m_local_controller->m_tick_base() : 0;
	m_last_shot.m_side = side == 0 ? 1 : side;
	m_last_shot.m_pending = true;
	m_resolver[handle].m_side = m_last_shot.m_side;
	m_resolver[handle].m_last_shot_tick = m_last_shot.m_tick;
	m_resolver[handle].m_health_at_shot = m_last_shot.m_health;
}

void c_rage_bot::resolve_misses() {
	if (!m_last_shot.m_pending || !g_ctx->m_local_controller)
		return;
	const int tick = g_ctx->m_local_controller->m_tick_base();
	if (tick - m_last_shot.m_tick < 6)
		return;

	auto it = m_lag_records.find(m_last_shot.m_handle);
	const int health = (it != m_lag_records.end() && !it->second.empty() && it->second.front().m_pawn)
		? it->second.front().m_pawn->m_health() : -1;
	auto& state = m_resolver[m_last_shot.m_handle];
	if (health >= 0 && health < m_last_shot.m_health) {
		state.m_misses = 0;
	} else {
		state.m_side = -m_last_shot.m_side;
		++state.m_misses;
	}
	m_last_shot.m_pending = false;
}

// ============================================================================
// SHOOT / STOP / PEEK
// ============================================================================

void c_rage_bot::process_backtrack(const lag_record_t* record) {
	if (!record || record->m_extrapolated || !weapon_config().m_backtrack || !g_ctx->m_user_cmd)
		return;
	const int tick = TIME_TO_TICKS(record->m_simulation_time) + 2;
	auto* cmd = g_ctx->m_user_cmd;
	for (int i = 0; i < cmd->pb.input_history_size(); ++i) {
		auto* entry = cmd->pb.mutable_input_history(i);
		if (!entry)
			continue;
		if (entry->has_cl_interp())
			entry->mutable_cl_interp()->set_frac(0.f);
		if (entry->has_sv_interp0()) {
			auto* interp = entry->mutable_sv_interp0();
			interp->set_src_tick(tick);
			interp->set_dst_tick(tick);
			interp->set_frac(0.f);
		}
		if (entry->has_sv_interp1()) {
			auto* interp = entry->mutable_sv_interp1();
			interp->set_src_tick(tick);
			interp->set_dst_tick(tick);
			interp->set_frac(0.f);
		}
		entry->set_render_tick_count(tick);
		entry->set_player_tick_count(g_ctx->m_local_controller->m_tick_base());
		if (auto* local = g_prediction->get_local_data())
			entry->set_player_tick_fraction(local->m_player_tick_fraction);
	}
	cmd->pb.mutable_base()->set_client_tick(tick);
}

bool c_rage_bot::can_shoot(c_base_player_weapon* weapon) const {
	if (!weapon || !g_ctx->m_local_controller || weapon->m_in_reload() || weapon->m_clip1() <= 0)
		return false;
	const int tick = g_ctx->m_local_controller->m_tick_base();
	if (m_last_fire_tick == tick)
		return false;
	return weapon->m_next_primary_attack() <= tick;
}

void c_rage_bot::process_attack(c_user_cmd* cmd, const vec3_t& angle, bool backtrack) {
	cmd->pb.mutable_base()->mutable_viewangles()->set_x(angle.x);
	cmd->pb.mutable_base()->mutable_viewangles()->set_y(angle.y);
	cmd->pb.mutable_base()->mutable_viewangles()->set_z(angle.z);

	const int shoot_tick = g_prediction->get_local_data() ? g_prediction->get_local_data()->m_shoot_tick
		: g_ctx->m_local_controller->m_tick_base();
	for (int i = 0; i < cmd->pb.input_history_size(); ++i) {
		auto* entry = cmd->pb.mutable_input_history(i);
		if (!entry)
			continue;
		entry->set_player_tick_count(shoot_tick);
		entry->mutable_view_angles()->set_x(angle.x);
		entry->mutable_view_angles()->set_y(angle.y);
		entry->mutable_view_angles()->set_z(angle.z);
	}
	if (cmd->pb.input_history_size() > 0)
		cmd->pb.set_attack1_start_history_index(0);
	if (backtrack)
		process_backtrack(&m_active_record);
	cmd->m_button_state.m_button_state |= IN_ATTACK;
	cmd->m_button_state.m_button_state2 |= IN_ATTACK;
	m_last_fire_tick = g_ctx->m_local_controller->m_tick_base();
}

void c_rage_bot::process_exploit(c_user_cmd* cmd, const vec3_t& angle, bool dt, bool hide) {
	auto* weapon = g_ctx->m_local_pawn ? g_ctx->m_local_pawn->get_active_weapon() : nullptr;
	if (!weapon || !cmd->pb.has_base())
		return;

	for (int i = 0; i < cmd->pb.input_history_size(); ++i) {
		auto* entry = cmd->pb.mutable_input_history(i);
		if (!entry)
			continue;
		entry->mutable_view_angles()->set_x(angle.x);
		entry->mutable_view_angles()->set_y(angle.y);
		entry->mutable_view_angles()->set_z(angle.z);
		const int shoot_tick = g_prediction->get_local_data()
			? g_prediction->get_local_data()->m_shoot_tick
			: g_ctx->m_local_controller->m_tick_base();
		entry->set_player_tick_count(dt ? weapon->m_next_primary_attack() : shoot_tick);
	}

	const bool ready = can_shoot(weapon);
	const bool shoot = ready && (!dt || !m_dt_fired);
	if (shoot) {
		cmd->m_button_state.m_button_state |= IN_ATTACK;
		cmd->m_button_state.m_button_state2 |= IN_ATTACK;
		if (cmd->pb.input_history_size() > 0)
			cmd->pb.set_attack1_start_history_index(0);
		if (!m_active_record.m_extrapolated)
			process_backtrack(&m_active_record);
		m_last_fire_tick = g_ctx->m_local_controller->m_tick_base();
	}
	if (shoot && cmd->pb.mutable_base()->subtick_moves_size() < 12) {
		auto* press = g_protobuf->add_subtick_move_step(cmd);
		auto* release = g_protobuf->add_subtick_move_step(cmd);
		const float when = hide ? 0.99f : 0.f;
		if (press) {
			press->set_button(IN_ATTACK);
			press->set_pressed(true);
			press->set_when(when);
			press->clear_analog_forward_delta();
			press->clear_analog_left_delta();
			press->clear_pitch_delta();
			press->clear_yaw_delta();
		}
		if (release) {
			release->set_button(IN_ATTACK);
			release->set_pressed(false);
			release->set_when(0.999f);
			release->clear_analog_forward_delta();
			release->clear_analog_left_delta();
			release->clear_pitch_delta();
			release->clear_yaw_delta();
		}
	} else if (!shoot) {
		cmd->m_button_state.m_button_state &= ~IN_ATTACK;
		cmd->m_button_state.m_button_state2 &= ~IN_ATTACK;
		cmd->m_button_state.m_button_state3 &= ~IN_ATTACK;
	}
	m_dt_fired = dt && shoot && !m_dt_fired;
	if (!shoot)
		cmd->pb.set_attack1_start_history_index(-1);
	cmd->pb.mutable_base()->mutable_viewangles()->set_x(angle.x);
	cmd->pb.mutable_base()->mutable_viewangles()->set_y(angle.y);
	cmd->pb.mutable_base()->mutable_viewangles()->set_z(angle.z);
}

void c_rage_bot::apply_autostop(c_user_cmd* cmd, c_cs_player_pawn* local, c_base_player_weapon* weapon,
	bool no_spread, bool early, bool full, bool between, bool air) {
	auto_stop_options_t options{};
	options.m_enabled = true;
	options.m_early = early && !full;
	options.m_full = full || between;
	options.m_between = between;
	options.m_air = air;
	options.m_duck = false;
	options.m_no_spread = no_spread;
	g_movement->auto_stop(cmd, local, weapon, options);
}

void c_rage_bot::update_peek(c_user_cmd* cmd, c_cs_player_pawn* local, bool fired, bool have_target) {
	auto& cfg = g_cfg->rage_bot;
	if (!cfg.m_peek_assist || !local || !cmd || !cmd->pb.has_base()) {
		m_peek = {};
		std::lock_guard lock(m_peek_mutex);
		m_peek_draw = {};
		return;
	}

	auto* scene = local->m_scene_node();
	if (!scene)
		return;
	vec3_t origin = scene->m_abs_origin();
	if (!m_peek.m_active) {
		m_peek.m_active = true;
		m_peek.m_anchor = origin;
		m_peek.m_health_at_start = local->m_health();
	}

	const float max_units = cfg.m_peek_assist_max_distance >= 100
		? FLT_MAX : static_cast<float>(cfg.m_peek_assist_max_distance) * 39.37f;
	vec3_t away = origin - m_peek.m_anchor;
	const float distance = away.length_2d();

	if (fired && cfg.m_peek_assist_retreat == 1)
		m_peek.m_retreat_after_shot = true;
	if (cfg.m_peek_assist_retreat == 2 && local->m_health() < m_peek.m_health_at_start)
		m_peek.m_retreating = true;
	if (m_peek.m_retreat_after_shot)
		m_peek.m_retreating = true;
	if (distance > max_units)
		m_peek.m_retreating = true;
	if (m_peek.m_retreating && distance < 10.f) {
		m_peek.m_retreating = false;
		m_peek.m_retreat_after_shot = false;
		m_peek.m_anchor = origin;
		m_peek.m_health_at_start = local->m_health();
	}

	if (m_peek.m_retreating && distance > 8.f) {
		const float world_yaw = rad2deg(std::atan2(-away.y, -away.x));
		const float rel = deg2rad(g_math->angle_diff(m_move_yaw, world_yaw));
		cmd->pb.mutable_base()->set_forwardmove(std::cos(rel));
		cmd->pb.mutable_base()->set_leftmove(std::sin(rel));
	} else if (have_target && cfg.m_peek_assist_auto_stop != 0) {
		auto* weapon = local->get_active_weapon();
		if (weapon)
			apply_autostop(cmd, local, weapon, false, cfg.m_peek_assist_auto_stop == 1, cfg.m_peek_assist_auto_stop == 2, false, false);
	}

	std::lock_guard lock(m_peek_mutex);
	m_peek_draw.m_active = true;
	m_peek_draw.m_retreating = m_peek.m_retreating;
	m_peek_draw.m_anchor = m_peek.m_anchor;
	m_peek_draw.m_player = origin;
	m_peek_draw.m_max_distance = max_units;
	m_peek_draw.m_style = cfg.m_peek_assist_style;
}

void c_rage_bot::on_present() {
	peek_draw_t draw{};
	{
		std::lock_guard lock(m_peek_mutex);
		draw = m_peek_draw;
	}
	if (!draw.m_active || !g_render || !g_render->m_background_draw_list)
		return;

	const float radius = draw.m_style == 2 && draw.m_max_distance < 100000.f
		? (std::min)(draw.m_max_distance, 80.f) : 18.f;
	vec3_t previous{};
	bool have_previous = false;
	for (int i = 0; i <= 24; ++i) {
		const float ang = deg2rad(static_cast<float>(i) / 24.f * 360.f);
		vec3_t world = draw.m_anchor;
		world.x += std::cos(ang) * radius;
		world.y += std::sin(ang) * radius;
		vec3_t screen{};
		if (!g_render->world_to_screen(world, screen)) {
			have_previous = false;
			continue;
		}
		if (have_previous) {
			float alpha = 0.85f;
			c_color color{ 0.45f, 0.75f, 1.f, alpha };
			if (draw.m_style == 1) {
				vec3_t delta = draw.m_player - draw.m_anchor;
				const float dist = delta.length_2d();
				const float t = draw.m_max_distance > 1.f && draw.m_max_distance < 100000.f
					? clampf(dist / draw.m_max_distance, 0.f, 1.f) : clampf(dist / 400.f, 0.f, 1.f);
				color = { t, 1.f - t, 0.35f, 0.9f };
			} else if (draw.m_retreating) {
				color = { 1.f, 0.45f, 0.2f, 0.9f };
			}
			g_render->line(previous, screen, color);
		}
		previous = screen;
		have_previous = true;
	}
}

// ============================================================================
// CREATE MOVE
// ============================================================================

void c_rage_bot::on_create_move() {
	resolve_misses();
	if (!g_cfg->rage_bot.m_enabled || !g_interfaces->m_engine->is_in_game())
		return;

	m_hitboxes.clear();
	m_best_target = nullptr;

	c_user_cmd* cmd = g_ctx->m_user_cmd;
	c_cs_player_pawn* local = g_ctx->m_local_pawn;
	if (!cmd || !cmd->pb.has_base() || !local || !local->is_alive())
		return;

	m_move_yaw = cmd->pb.base().viewangles().y();

	c_base_player_weapon* weapon = local->get_active_weapon();
	c_cs_weapon_base_v_data* data = weapon ? weapon->get_weapon_data() : nullptr;
	if (!weapon || !data)
		return;
	const int weapon_type = data->m_weapon_type();
	if (weapon_type == WEAPONTYPE_KNIFE || weapon_type == WEAPONTYPE_GRENADE
		|| weapon_type == WEAPONTYPE_C4 || weapon_type == WEAPONTYPE_EQUIPMENT) {
		update_peek(cmd, local, false, false);
		return;
	}

	if (!g_cfg->rage_bot.m_double_tap)
		m_dt_fired = false;

	store_hitboxes();
	if (m_hitboxes.empty())
		return;

	weapon->update_accuracy_penality();
	const float spread = weapon->get_spread();
	const float inaccuracy = weapon->get_inaccuracy();
	const auto no_spread_var = g_interfaces->m_var->get_by_name(xorstr_("weapon_accuracy_nospread"));
	const bool no_spread = no_spread_var && no_spread_var->get_bool();

	m_active_handle = -1;
	m_active_side = 0;
	find_targets();
	select_target(no_spread ? 0.f : inaccuracy, no_spread ? 0.f : spread);

	auto& cfg = weapon_config();
	const bool have_target = m_best_target && m_best_target->m_best_point && m_best_target->m_best_point->m_hitbox >= 0;
	const bool taser = weapon_type == WEAPONTYPE_TASER;
	const bool on_ground = (local->m_flags() & FL_ONGROUND) != 0;
	const float speed = local->m_vec_abs_velocity().length_2d();
	const float max_speed = weapon->get_max_speed() > 1.f ? weapon->get_max_speed() : 250.f;
	if (weapon->m_next_primary_attack() > (g_ctx->m_local_controller ? g_ctx->m_local_controller->m_tick_base() : 0))
		m_dt_fired = false;
	const bool ready = can_shoot(weapon);

	const bool sniper = weapon_type == WEAPONTYPE_SNIPER_RIFLE;
	const int scope_tick = g_ctx->m_local_controller ? g_ctx->m_local_controller->m_tick_base() : 0;
	const bool unscoped = !local->m_scoped() && weapon->m_zoom_level() <= 0;
	bool enemy_for_scope = have_target;
	if (!enemy_for_scope && !m_aim_targets.empty() && g_ctx->m_user_cmd) {
		const int fov = g_cfg->rage_bot.m_fov;
		if (fov <= 0 || fov >= 180)
			enemy_for_scope = true;
		else {
			vec3_t view_angles = player_camera_angles();
			vec3_t forward{};
			g_math->angle_vectors(view_angles, forward);
			forward = forward.normalize();
			const float cos_fov = std::cos(deg2rad(static_cast<float>(fov)));
			const auto* local_data = g_prediction->get_local_data();
			vec3_t eye = local_data ? local_data->m_eye_pos : local->get_eye_pos();
			for (const auto& [handle, target] : m_aim_targets) {
				(void)handle;
				if (!target.m_lag_record)
					continue;
				vec3_t to{
					target.m_lag_record->m_origin.x - eye.x,
					target.m_lag_record->m_origin.y - eye.y,
					target.m_lag_record->m_origin.z + 48.f - eye.z
				};
				const float len = std::sqrt(to.x * to.x + to.y * to.y + to.z * to.z);
				if (len > 1.f && (to.x * forward.x + to.y * forward.y + to.z * forward.z) / len >= cos_fov) {
					enemy_for_scope = true;
					break;
				}
			}
		}
	}
	if (cfg.m_auto_scope && sniper && unscoped && enemy_for_scope) {
		const bool secondary_ready = weapon->m_next_secondary_attack() <= scope_tick;
		const bool cooled = m_last_scope_tick < 0 || scope_tick - m_last_scope_tick >= 12;
		if (secondary_ready && cooled) {
			press_scope(cmd);
			m_last_scope_tick = scope_tick;
		}
	}
	if (!unscoped)
		m_last_scope_tick = -1;

	const int tick = g_ctx->m_local_controller ? g_ctx->m_local_controller->m_tick_base() : 0;
	const int until_attack = weapon->m_next_primary_attack() - tick;
	const bool automatic = weapon_type == WEAPONTYPE_SUBMACHINEGUN || weapon_type == WEAPONTYPE_RIFLE || weapon_type == WEAPONTYPE_MACHINEGUN;
	const bool between = cfg.m_auto_stop && cfg.m_auto_stop_between && automatic && !ready && until_attack > 0 && until_attack <= 3;
	const bool want_stop = have_target && cfg.m_auto_stop && (ready || between);
	if (want_stop && (on_ground || cfg.m_auto_stop_air))
		apply_autostop(cmd, local, weapon, no_spread, cfg.m_auto_stop_early && !between, cfg.m_auto_stop_full || between, between, cfg.m_auto_stop_air);

	bool fired = false;
	if (have_target && ready) {
		const auto* local_data = g_prediction->get_local_data();
		vec3_t eye = local_data ? local_data->m_eye_pos : local->get_eye_pos();
		vec3_t aim = g_math->aim_direction(eye, m_best_target->m_best_point->m_point);
		vec3_t punch = get_removed_aim_punch_angle(local);
		vec3_t angle = aim - punch;

		const bool jump_scout = weapon->get_item_def_index() == 40 && !on_ground && local->m_scoped();
		static int ticks_since_land = 8;
		auto landing = g_prediction->landing(local, cmd);
		if (landing.valid && landing.grounded && landing.will_land && landing.fraction < 0.1f)
			ticks_since_land = 0;
		else if (on_ground)
			++ticks_since_land;
		else
			ticks_since_land = 0;

		const bool just_landed = ticks_since_land < 2 && !jump_scout && !taser;
		const float distance = eye.dist(m_best_target->m_best_point->m_point);
		hitbox_data_t aimed = get_hitbox_data(m_best_target->m_pawn, m_best_target->m_best_point->m_hitbox, &m_active_record);
		const float radius = aimed.m_invalid_data ? 4.f : aimed.m_radius;

		float needed = required_hitchance(cfg.m_hit_chance, inaccuracy, spread, distance, radius,
			m_best_target->m_pawn->m_health(), m_best_target->m_best_point->m_damage, taser);
		if (cfg.m_delay_shot && !at_max_accuracy(data, weapon, inaccuracy))
			needed = (std::min)(needed + 18.f, 100.f);

		int samples = cfg.m_hit_chance_seed <= 0 ? 128 : std::clamp(cfg.m_hit_chance_seed, 8, 256);
		const int bullets = (std::max)(1, static_cast<int>(data->m_bullets()));
		samples = (std::max)(samples, bullets * 4);
		samples = (std::min)(samples, 256);

		const float shot_inaccuracy = no_spread ? 0.f : inaccuracy;
		const float shot_spread = no_spread ? 0.f : spread;
		int chance = no_spread ? 100 : hitchance(eye, angle, &m_active_record, m_best_target->m_best_point->m_hitbox,
			weapon, shot_inaccuracy, shot_spread, samples, static_cast<int>(needed));

		bool duck_for_shot = false;
		if (!no_spread && static_cast<float>(chance) < needed && cfg.m_auto_stop && cfg.m_auto_stop_duck && on_ground) {
			const float ducked = predict_inaccuracy(weapon, data, speed, local->m_vec_abs_velocity().z, true, true);
			const int ducked_chance = hitchance(eye, angle, &m_active_record, m_best_target->m_best_point->m_hitbox,
				weapon, ducked, shot_spread, samples, static_cast<int>(needed));
			if (static_cast<float>(ducked_chance) >= needed) {
				chance = ducked_chance;
				duck_for_shot = true;
				cmd->m_button_state.m_button_state |= IN_DUCK;
				cmd->m_button_state.m_button_state2 |= IN_DUCK;
				cmd->m_button_state.m_button_state3 |= IN_DUCK;
			}
		}

		const bool accurate_now = no_spread || at_max_accuracy(data, weapon, inaccuracy)
			|| (cfg.m_auto_stop_early && !cfg.m_auto_stop_full && speed <= max_speed * 0.34f)
			|| speed <= 8.f;
		const bool wait_for_accuracy = cfg.m_delay_shot && !accurate_now && !duck_for_shot && !jump_scout;
		const bool scope_block = cfg.m_auto_scope && sniper && !local->m_scoped() && weapon->m_zoom_level() <= 0 && !no_spread;

		if (!just_landed && !wait_for_accuracy && !scope_block && static_cast<float>(chance) >= needed) {
			if (!g_cfg->rage_bot.m_silent)
				g_interfaces->m_csgo_input->set_view_angles(angle);

			const bool dt = g_cfg->rage_bot.m_double_tap && (g_cfg->rage_bot.m_double_tap_key <= 0
				|| g_key_handler->is_pressed(g_cfg->rage_bot.m_double_tap_key, g_cfg->rage_bot.m_double_tap_key_style));
			const bool hide = g_cfg->rage_bot.m_hide_shots && (g_cfg->rage_bot.m_hide_shots_key <= 0
				|| g_key_handler->is_pressed(g_cfg->rage_bot.m_hide_shots_key, g_cfg->rage_bot.m_hide_shots_key_style));

			if (dt || hide)
				process_exploit(cmd, angle, dt, hide);
			else
				process_attack(cmd, angle, cfg.m_backtrack && !m_active_record.m_extrapolated);

			if (m_active_handle != -1)
				note_shot(m_active_handle, m_best_target->m_pawn, m_active_side);
			fired = true;
		}
	}

	update_peek(cmd, local, fired, have_target);
}
