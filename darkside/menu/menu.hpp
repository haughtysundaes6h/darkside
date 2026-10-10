#pragma once

#include <memory>
#include <atomic>

class c_menu {
	int m_selected_tab{ 1 };
public:
	std::atomic_bool m_opened{false};
	std::atomic_bool m_block_attack_until_release{false};
	bool blocks_attacks() const { return m_opened.load() || m_block_attack_until_release.load(); }

	void draw( );
	bool on_create_move( );
};

inline const auto g_menu = std::make_unique<c_menu>( );
