#pragma once

#include "../../darkside.hpp"
#include "../../valve/andromeda_patterns.hpp"
#include "../auto_wall/auto_wall.hpp"
#include "../eng_pred/eng_pred.hpp"

#include <mutex>

struct lag_record_t {
	vec3_t m_origin{};
	c_cs_player_pawn* m_pawn{};
	c_bone_data m_bone_data[128];
	c_bone_data m_bone_data_backup[128];
	int m_stored_bone_count{};
	float m_simulation_time{};
	vec3_t m_mins{};
	vec3_t m_maxs{};
	vec3_t m_velocity{};
	vec3_t m_eye_angles{};
	int m_flags{};
	float m_duck_amount{};
	bool m_throwing{};

	// Filled when this copy is a movement prediction, not a networked record.
	bool m_extrapolated{};
	int m_extrapolate_ticks{};
	float m_confidence{ 1.f };

	lag_record_t() = default;

	void store(c_cs_player_pawn* pawn);
	void apply(c_cs_player_pawn* pawn);
	void reset(c_cs_player_pawn* pawn);
	void shift(const vec3_t& delta);
	bool is_valid() const;
};

struct hitbox_data_t {
	int m_num_hitbox{};
	int m_num_bone{};
	int m_num_hitgroup{};
	float m_radius{};
	matrix3x4_t m_matrix{};
	vec3_t m_mins{};
	vec3_t m_maxs{};
	bool m_invalid_data{ true };
};

struct aim_point_t {
	aim_point_t() = default;
	aim_point_t(vec3_t point, int hitbox, bool center = false)
		: m_point(point), m_hitbox(hitbox), m_center(center) {}

	vec3_t m_point{};
	int m_hitbox{ -1 };
	bool m_center{};
	float m_damage{};
	bool m_penetrated{};
};

struct rage_capsule_t {
	int m_hitbox{ -1 };
	int m_hitgroup{};
	vec3_t m_a{};
	vec3_t m_b{};
	float m_radius{};
	bool m_valid{};
};

struct aim_target_t {
	lag_record_t* m_lag_record{};
	std::unique_ptr<aim_point_t> m_best_point{};
	c_cs_player_pawn* m_pawn{};

	aim_target_t() = default;
	explicit aim_target_t(lag_record_t* record) { store(record); }

	void store(lag_record_t* record) {
		if (!record || !record->m_pawn)
			return;
		m_lag_record = record;
		m_pawn = record->m_pawn;
	}
};

class c_rage_bot {
	struct resolver_state_t {
		int m_side{ 1 };
		int m_misses{};
		int m_last_shot_tick{};
		int m_health_at_shot{ -1 };
		float m_last_eye_yaw{};
		bool m_has_eye{};
	};

	struct shot_memory_t {
		int m_handle{};
		int m_health{};
		int m_tick{};
		int m_side{ 1 };
		bool m_pending{};
	};

	struct peek_state_t {
		bool m_active{};
		bool m_retreating{};
		bool m_retreat_after_shot{};
		vec3_t m_anchor{};
		int m_health_at_start{};
	};

	struct peek_draw_t {
		bool m_active{};
		bool m_retreating{};
		vec3_t m_anchor{};
		vec3_t m_player{};
		float m_max_distance{};
		int m_style{};
	};

	std::vector<int> m_hitboxes;
	std::unordered_map<int, std::deque<lag_record_t>> m_lag_records{};
	std::unordered_map<int, aim_target_t> m_aim_targets{};
	std::unordered_map<int, resolver_state_t> m_resolver{};
	aim_target_t* m_best_target{};
	lag_record_t m_active_record{};
	int m_active_handle{ -1 };
	int m_active_side{};
	shot_memory_t m_last_shot{};
	peek_state_t m_peek{};
	peek_draw_t m_peek_draw{};
	std::mutex m_peek_mutex;
	int m_last_fire_tick{ -1 };
	int m_last_scope_tick{ -1 };
	bool m_dt_fired{};
	float m_move_yaw{};

	int get_hitbox_from_menu(int hitbox) const;
	hitbox_data_t get_hitbox_data(c_cs_player_pawn* pawn, int hitbox, const lag_record_t* record) const;
	vec3_t get_removed_aim_punch_angle(c_cs_player_pawn* local_player) const;
	c_config_system::weapon_config_t& weapon_config() const;

	void store_hitboxes();
	lag_record_t* select_record(int handle);
	void find_targets();
	bool build_prediction(int handle, lag_record_t& out) const;
	float point_scale(const vec3_t& point, float radius, float inaccuracy, float spread) const;
	bool multi_points(lag_record_t* record, int hitbox, std::vector<aim_point_t>& points, float inaccuracy, float spread, int resolver_side);
	int minimum_for(c_cs_player_pawn* target, c_cs_weapon_base_v_data* data) const;
	int hitgroup_of(const hitbox_data_t& data, int hitbox) const;
	float unscaled_damage(c_cs_weapon_base_v_data* data, float distance) const;
	float scaled_damage(c_cs_weapon_base_v_data* data, c_cs_player_pawn* pawn, int hitgroup, float distance, float pen_ratio) const;
	bool hitbox_can_meet(c_cs_weapon_base_v_data* data, c_cs_player_pawn* pawn, int hitbox, float distance, float minimum, bool taser) const;
	void build_capsules(lag_record_t* record, rage_capsule_t* caps, int count) const;
	int capsule_hit(const rage_capsule_t* caps, int count, const vec3_t& eye, const vec3_t& point, float& entry, int prefer = -1) const;
	float score_centers(lag_record_t* record, const vec3_t& eye, c_cs_weapon_base_v_data* data, bool allow_pen, bool taser, int minimum) const;
	aim_point_t select_points(lag_record_t* record, float& damage, float inaccuracy, float spread, int resolver_side, int minimum);
	void select_target(float inaccuracy, float spread);
	void press_scope(c_user_cmd* cmd);

	bool ray_capsule(const vec3_t& start, const vec3_t& dir, const vec3_t& a, const vec3_t& b, float radius, float max_dist) const;
	bool game_spread(c_base_player_weapon* weapon, int seed, float inaccuracy, float spread, float& out_x, float& out_y) const;
	int hitchance(const vec3_t& eye, const vec3_t& angles, lag_record_t* record, int hitbox, c_base_player_weapon* weapon, float inaccuracy, float spread, int samples, int required) const;
	bool at_max_accuracy(c_cs_weapon_base_v_data* data, c_base_player_weapon* weapon, float inaccuracy) const;
	float predict_inaccuracy(c_base_player_weapon* weapon, c_cs_weapon_base_v_data* data, float speed, float vertical_speed, bool on_ground, bool ducking) const;
	float required_hitchance(int configured, float inaccuracy, float spread, float distance, float radius, int health, float damage, bool taser) const;

	int resolver_side(int handle, c_cs_player_pawn* pawn, const lag_record_t* record);
	void note_shot(int handle, c_cs_player_pawn* pawn, int side);
	void resolve_misses();

	void process_backtrack(const lag_record_t* record);
	bool can_shoot(c_base_player_weapon* weapon) const;
	void process_attack(c_user_cmd* cmd, const vec3_t& angle, bool backtrack);
	void process_exploit(c_user_cmd* cmd, const vec3_t& angle, bool dt, bool hide);
	void apply_autostop(c_user_cmd* cmd, c_cs_player_pawn* local, c_base_player_weapon* weapon, bool no_spread, bool early, bool full, bool between, bool air);
	void update_peek(c_user_cmd* cmd, c_cs_player_pawn* local, bool fired, bool have_target);

public:
	void store_records();
	void on_create_move();
	void on_present();
};

inline const auto g_rage_bot = std::make_unique<c_rage_bot>();
