#pragma once

#include <Windows.h>
#include <memory>
#include <mutex>
#include "key_state.hpp"

class c_key_handler {
	std::mutex m_mutex;
	key_toggle_state m_state;
public:
	void update() {
		std::lock_guard lock(m_mutex);
		for (int key=1;key<=0xA5;++key) {
			const bool down=(GetAsyncKeyState(key)&0x8000)!=0;
			m_state.sample(key,down);
		}
	}
	bool is_pressed( int key, int key_style ) {
        if (key_style == 2)
            return true;
        if (key <= 0 || key > 0xA5)
            return false;
        switch ( key_style ) {
        case 0:
            return (GetAsyncKeyState(key)&0x8000)!=0;
        case 1: {
			std::lock_guard lock(m_mutex);
			return m_state.toggled(key);
		}
        default: return false;
        }
	}
};

inline const auto g_key_handler = std::make_unique<c_key_handler>( );
