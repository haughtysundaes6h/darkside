#pragma once

#include "../../darkside.hpp"

struct bbox_t {
	bool m_found = false;

	float x;
	float y;
	float width;
	float height;
};

class c_visuals {
	struct player_info_t {
		bool m_valid = false;

		int m_handle;
		int m_health;
		int m_ammo;
		int m_max_ammo;

		vec3_t m_bottom{}, m_top{};

		std::string m_name;
		std::string m_weapon_name;
		std::vector<std::string> m_flags;
	};

	bbox_t calculate_bbox( vec3_t bottom, vec3_t top );

	std::mutex m_player_mutex;
	std::chrono::steady_clock::time_point m_snapshot_time{};
	bool m_local_scoped = false;
	std::unordered_map<int, player_info_t> m_player_map;

	void handle_players( );
public:
	void store_players( );
	void clear();
	bool local_scoped();
	void on_present( );
};

inline const auto g_visuals = std::make_unique<c_visuals>( );
