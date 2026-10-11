#pragma once

#include "../../darkside.hpp"
#include "../../valve/andromeda_patterns.hpp"
#include "../auto_wall/auto_wall.hpp"
#include "../eng_pred/eng_pred.hpp"

struct lag_record_t
{
	vec3_t m_origin{};
	c_cs_player_pawn* m_pawn{};
	c_skeleton_instace* m_skeleton = nullptr;
	c_bone_data m_bone_data[128];
	c_bone_data m_bone_data_backup[128];
	int m_stored_bone_count{};
	float m_simulation_time{};
	vec3_t m_vec_mins{};
	vec3_t m_vec_maxs{};
	vec3_t m_vec_velocity{};

	bool m_throwing{};

	lag_record_t() = default;

	void store(c_cs_player_pawn* pawn)
	{
		if (!pawn || !pawn->is_player_pawn() || !pawn->is_alive())
			return;

		auto scene_node = pawn->m_scene_node();
		if (!scene_node)
			return;

		auto skeleton_instance = scene_node->get_skeleton_instance();
		if (!skeleton_instance)
			return;

		auto collision = pawn->m_collision();
		if (!collision)
			return;

		m_skeleton = skeleton_instance;
		m_origin = scene_node->m_abs_origin();

		auto& model_state = skeleton_instance->m_model_state();
		auto* bones = model_state.get_bone_data();
		m_stored_bone_count = std::clamp(model_state.get_bone_count(), 0, 128);
		if (!bones || !m_stored_bone_count)
			return;
		std::memcpy(m_bone_data, bones, sizeof(c_bone_data) * m_stored_bone_count);
		m_pawn = pawn;

		m_simulation_time = pawn->m_sim_time();
		m_vec_mins = collision->m_mins();
		m_vec_maxs = collision->m_maxs();
		m_vec_velocity = pawn->m_vec_velocity();

		m_throwing = pawn->is_throwing();
	}

	void apply(c_cs_player_pawn* pawn)
	{
		if (!pawn || !pawn->is_player_pawn() || !pawn->is_alive())
			return;

		auto scene_node = pawn->m_scene_node();
		if (!scene_node)
			return;

		auto skeleton_instance = scene_node->get_skeleton_instance();
		if (!skeleton_instance)
			return;

		auto& model_state = skeleton_instance->m_model_state();
		auto* bones = model_state.get_bone_data();
		const int count = (std::min)(m_stored_bone_count, std::clamp(model_state.get_bone_count(), 0, 128));
		if (!bones || !count)
			return;
		std::memcpy(m_bone_data_backup, bones, sizeof(c_bone_data) * count);

		std::memcpy(bones, m_bone_data, sizeof(c_bone_data) * count);
	}

	void reset(c_cs_player_pawn* pawn)
	{
		if (!pawn || !pawn->is_player_pawn() || !pawn->is_alive())
			return;

		auto scene_node = pawn->m_scene_node();
		if (!scene_node)
			return;

		auto skeleton_instance = scene_node->get_skeleton_instance();
		if (!skeleton_instance)
			return;

		auto& model_state = skeleton_instance->m_model_state();
		auto* bones = model_state.get_bone_data();
		const int count = (std::min)(m_stored_bone_count, std::clamp(model_state.get_bone_count(), 0, 128));
		if (!bones || !count)
			return;
		std::memcpy(bones, m_bone_data_backup, sizeof(c_bone_data) * count);
	}

	// Экстраполяция: двигаем все кости/о origin вперёд по скорости на N тиков.
	// Кости в CS2 хранятся в мировых координатах, поэтому достаточно сдвига.
	void extrapolate(int ticks)
	{
		if (ticks <= 0 || !std::isfinite(m_simulation_time))
			return;

		const vec3_t velocity = m_vec_velocity;
		if (!std::isfinite(velocity.x) || !std::isfinite(velocity.y) || !std::isfinite(velocity.z))
			return;

		const float delta_time = TICKS_TO_TIME(ticks);
		const vec3_t shift = velocity * delta_time;

		m_origin = m_origin + shift;
		m_vec_mins = m_vec_mins + shift;
		m_vec_maxs = m_vec_maxs + shift;
		m_simulation_time += delta_time;

		for (int i = 0; i < m_stored_bone_count; ++i) {
			m_bone_data[i].m_pos = m_bone_data[i].m_pos + shift;
		}
	}

	bool is_valid()
	{
		const auto* local = g_prediction->get_local_data();
		if (!local || !std::isfinite(m_simulation_time))
			return false;
		const float dead_time = (local->m_tick_base * INTERVAL_PER_TICK) - m_simulation_time;
		return std::isfinite(dead_time) && dead_time >= -0.2f && dead_time < 0.2f;
	}
};

struct hitbox_data_t
{
	hitbox_data_t() = default;

	int m_num_hitbox;
	int m_num_bone;
	int m_num_hitgroup;
	c_hitbox* m_hitbox_data;
	float m_radius;
	matrix3x4_t m_matrix;
	vec3_t m_mins;
	vec3_t m_maxs;
	bool m_invalid_data{ true };
};

struct aim_point_t
{
	aim_point_t(vec3_t point, int hitbox, bool center = false)
	{
		this->m_point = point;
		this->m_hitbox = hitbox;
		this->m_center = center;
	}

	vec3_t m_point{};
	int m_hitbox{};
	bool m_center{};

	// заполняется при сканировании
	float m_damage{ 0.f };
	bool m_penetrated{ false };
};

struct aim_target_t
{
	lag_record_t* m_lag_record{};
	std::unique_ptr<aim_point_t> m_best_point{};
	c_cs_player_pawn* m_pawn{ nullptr };

	aim_target_t() = default;

	aim_target_t(lag_record_t* record) { this->store(record); }

	void store(lag_record_t* record)
	{
		if (!record || !record->m_pawn || !record->m_pawn->is_player_pawn())
			return;

		m_lag_record = record;
		m_pawn = record->m_pawn;
	}

	void reset()
	{
		m_lag_record = nullptr;
		m_pawn = nullptr;
		m_best_point.reset();
	}
};

// Auto-stop options structure
struct auto_stop_options_t {
	bool m_enabled = false;
	bool m_early = false;
	bool m_full = true;
	bool m_between = false;
	bool m_air = false;
	bool m_duck = false;
	bool m_no_spread = false;
};

class c_rage_bot
{
	std::vector<int> m_hitboxes;
	std::unordered_map<int, std::deque<lag_record_t>> m_lag_records{};
	std::unordered_map<int, aim_target_t> m_aim_targets{};
	aim_target_t* m_best_target{};

	// Активная запись этого тика (копия с экстраполяцией, если включена) —
	// используется в скане точек, хитшансе и бектрек-интерполяции
	lag_record_t m_active_record{};

	// Направление взгляда для FOV-фильтра (заполняется в select_target)
	vec3_t m_fov_dir{};

	// Состояние DT-цикла: true = этот тик discharge (пропускаем выстрел)
	bool m_dt_fired{};

	int get_hitbox_from_menu(int hitbox)
	{
		switch (hitbox) {
		case HITBOX_HEAD:
			return 0;
		case HITBOX_CHEST:
		case HITBOX_LOWER_CHEST:
		case HITBOX_UPPER_CHEST:
			return 1;
		case HITBOX_PELVIS:
		case HITBOX_STOMACH:
			return 2;
		case HITBOX_RIGHT_HAND:
		case HITBOX_LEFT_HAND:
		case HITBOX_RIGHT_UPPER_ARM:
		case HITBOX_RIGHT_FOREARM:
		case HITBOX_LEFT_UPPER_ARM:
		case HITBOX_LEFT_FOREARM:
			return 3;
		case HITBOX_RIGHT_THIGH:
		case HITBOX_LEFT_THIGH:
		case HITBOX_RIGHT_CALF:
		case HITBOX_LEFT_CALF:
			return 4;
		case HITBOX_RIGHT_FOOT:
		case HITBOX_LEFT_FOOT:
			return 5;
		}

		return -1;
	}

	hitbox_data_t get_hitbox_data(c_cs_player_pawn* player_pawn, const int hitbox_id, const lag_record_t* record = nullptr)
	{
		if (!player_pawn)
			return hitbox_data_t();

		auto game_scene = player_pawn->m_scene_node();
		if (!game_scene)
			return hitbox_data_t();

		auto skeleton = game_scene->get_skeleton_instance();
		if (!skeleton)
			return hitbox_data_t();

		auto& model_state = skeleton->m_model_state();
		auto& model = model_state.m_model();
		if (!model)
			return hitbox_data_t();

		auto hitbox_data = model->get_hitbox(hitbox_id);
		if (!hitbox_data)
			return hitbox_data_t();

		const auto bone_index = player_pawn->get_bone_index(hitbox_data->m_bone_name);
		if (bone_index < 0 || bone_index >= 128 || bone_index >= model_state.get_bone_count())
			return hitbox_data_t();

		hitbox_data_t hitbox;
		hitbox.m_num_hitbox = hitbox_id;
		hitbox.m_num_bone = bone_index;
		hitbox.m_hitbox_data = hitbox_data;
		hitbox.m_radius = hitbox_data->m_shape_radius;
		hitbox.m_num_hitgroup = hitbox_data->m_hitgroup;
		hitbox.m_mins = hitbox_data->m_vec_min;
		hitbox.m_maxs = hitbox_data->m_vec_maxs;
		hitbox.m_invalid_data = false;

		// Матрица из записи (бектрек/экстраполяция), если запись жива
		if (record && record->m_stored_bone_count > bone_index) {
			hitbox.m_matrix = g_math->transform_to_matrix(
				const_cast<c_bone_data&>(record->m_bone_data[bone_index]));
		}
		else {
			const auto bones = model_state.get_bone_data();
			if (!bones)
				return hitbox_data_t();
			hitbox.m_matrix = g_math->transform_to_matrix(bones[bone_index]);
		}

		return hitbox;
	}

	vec3_t get_removed_aim_punch_angle(c_cs_player_pawn* local_player)
	{
		using fnUpdateAnimPunch = void(__fastcall*)(void*, vec3_t*, bool);
		static fnUpdateAnimPunch fn = reinterpret_cast<fnUpdateAnimPunch>(
			g_opcodes->scan_absolute(g_modules->m_modules.client_dll.get_name(),
				andromeda::get_removed_aim_punch, 0x1));

		vec3_t aim_punch = {};
		if (fn)
			fn(local_player, &aim_punch, true);
		return aim_punch;
	}

	void store_hitboxes();
	lag_record_t* select_record(int handle, int min_damage);
	float quick_score_record(lag_record_t* record, vec3_t shoot_pos, bool is_taser);
	void find_targets();
	float calc_point_scale(const vec3_t& point, float hitbox_radius);
	vec3_t transform_point(matrix3x4_t matrix, vec3_t point);
	std::vector<vec3_t> calculate_sphere_points(float radius, int num_points);
	std::vector<vec3_t> calculate_capsule_points(float radius, matrix3x4_t matrix, hitbox_data_t hitbox, int rings, int per_ring);
	bool multi_points(lag_record_t* record, int hitbox, std::vector<aim_point_t>& points);
	aim_point_t select_points(lag_record_t* record, float& damage);
	void select_target();
	bool ray_intersects_capsule(const vec3_t& start, const vec3_t& dir_normalized, const vec3_t& cap_a, const vec3_t& cap_b, float radius, float max_dist);
	bool ray_hits_hitboxes(const vec3_t& start, const vec3_t& dir_normalized, lag_record_t* record, float max_dist);
	bool weapon_is_at_max_accuracy(c_cs_weapon_base_v_data* weapon_data, float inaccuracy);
	vec3_t calculate_spread_angles(vec3_t angle, int random_seed, float weapon_inaccuarcy, float weapon_spread);
	int calculate_hit_chance(c_cs_player_pawn* pawn, vec3_t angles, c_base_player_weapon* active_weapon, c_cs_weapon_base_v_data* weapon_data, bool no_spread);
	void process_backtrack(lag_record_t* record);
	bool can_shoot(c_cs_player_pawn* pawn, c_base_player_weapon* active_weapon);
	void process_attack(c_user_cmd* user_cmd, vec3_t angle);

	// ========================================================================
	// EXPLOITS: double tap / hide shots (CS2 subtick)
	// ========================================================================
	void process_exploit_attack(c_user_cmd* user_cmd, vec3_t angle, bool dt_active, bool hs_active, bool want_shoot);
	static void strip_attack(c_user_cmd* user_cmd);

	// Get weapon config based on current weapon
	c_config_system::weapon_config_t& get_current_weapon_config() {
		if (!g_ctx->m_local_pawn)
			return g_cfg->rage_bot.m_weapon_config[WEAPON_GROUP_GENERAL];

		auto* weapon = g_ctx->m_local_pawn->get_active_weapon();
		if (!weapon)
			return g_cfg->rage_bot.m_weapon_config[WEAPON_GROUP_GENERAL];

		auto* weapon_data = weapon->get_weapon_data();
		if (!weapon_data)
			return g_cfg->rage_bot.m_weapon_config[WEAPON_GROUP_GENERAL];

		int weapon_type = weapon_data->m_weapon_type();
		int item_def = weapon->get_item_def_index();

		weapon_group_t group = c_config_system::get_weapon_group(weapon_type, item_def);
		return g_cfg->rage_bot.get_weapon_config(static_cast<int>(group));
	}

public:

	void store_records();
	void on_create_move();
};

const auto g_rage_bot = std::make_unique<c_rage_bot>();
