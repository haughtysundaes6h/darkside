#pragma once

#include "../darkside.hpp"

#include "../render/render.hpp"

class c_directx {
	HMODULE m_present_module = nullptr;
	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_device_context = nullptr;
	ID3D11RenderTargetView* m_render_target = nullptr;
	bool m_initialized = false;
public:
	void initialize( );
	void unitialize( );
	bool start_frame( IDXGISwapChain* swap_chain );
	void new_frame( );
	void end_frame( );

	void* m_present_address = nullptr;
	WNDPROC m_window_proc_original = nullptr;
	HWND m_window = nullptr;
};

inline const auto g_directx = std::make_unique<c_directx>( );
