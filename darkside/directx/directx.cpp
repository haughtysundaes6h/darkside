#include "directx.hpp"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

LRESULT hk_wnd_proc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
	ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

	if ( uMsg == WM_KEYUP && wParam == VK_INSERT ) {
		g_menu->m_opened = !g_menu->m_opened.load();
		if (g_menu->m_opened)
			g_menu->m_block_attack_until_release = true;
	}

	static auto original = hooks::enable_cursor::m_enable_cursor.get_original<decltype( &hooks::enable_cursor::hk_enable_cursor )>( );
	original( g_interfaces->m_input_system, g_menu->m_opened ? false : hooks::enable_cursor::m_enable_cursor_input );
	if ((uMsg == WM_KEYUP || uMsg == WM_KEYDOWN) && wParam == VK_INSERT)
		return 0;
	if (g_menu->m_opened && uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
		return uMsg == WM_XBUTTONDOWN || uMsg == WM_XBUTTONUP || uMsg == WM_XBUTTONDBLCLK ? TRUE : 0;

	return CallWindowProc( g_directx->m_window_proc_original, hWnd, uMsg, wParam, lParam );
}
namespace {
void* find_present_address(HMODULE& present_module)
{
	const HWND window = CreateWindowExW(0, L"STATIC", L"darkside D3D11 probe",
		WS_OVERLAPPEDWINDOW, 0, 0, 1, 1, nullptr, nullptr,
		GetModuleHandleW(nullptr), nullptr);
	if (!window) {
		LOG_ERROR("Present probe window creation failed: Win32 error=%lu", GetLastError());
		return nullptr;
	}

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.Width = 1;
	sd.BufferDesc.Height = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = window;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = 0;

	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;
	IDXGISwapChain* swapChain = nullptr;

	HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
		nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd,
		&swapChain, &device, nullptr, &context);
	if (FAILED(hr)) {
		LOG_WARNING("Present probe hardware device failed: HRESULT=0x%08X", static_cast<unsigned>(hr));
		if (context) { context->Release(); context = nullptr; }
		if (device) { device->Release(); device = nullptr; }
		if (swapChain) { swapChain->Release(); swapChain = nullptr; }
		hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP,
			nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd,
			&swapChain, &device, nullptr, &context);
	}

	void* address = swapChain ? vmt::get_v_method(swapChain, 8) : nullptr;
	MEMORY_BASIC_INFORMATION memory{};
	const auto queried = address ? VirtualQuery(address, &memory, sizeof(memory)) : 0;
	const DWORD protection = queried ? (memory.Protect & 0xFF) : 0;
	const bool executable = protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
		protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
	if (FAILED(hr) || !address || !executable) {
		LOG_ERROR("Present probe failed: HRESULT=0x%08X target=%p protection=0x%lX",
			static_cast<unsigned>(hr), address, protection);
		address = nullptr;
	}
	if (address && !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
		reinterpret_cast<LPCWSTR>(address), &present_module)) {
		LOG_ERROR("Present probe could not retain target module: Win32 error=%lu", GetLastError());
		address = nullptr;
	}

	if (context) context->Release();
	if (swapChain) swapChain->Release();
	if (device) device->Release();
	DestroyWindow(window);

	return address;
}
}
void c_directx::initialize( ) {
	m_present_address = find_present_address(m_present_module);
	if (m_present_address)
		LOG_INFO("Present probe target=%p", m_present_address);
}

void c_directx::unitialize( ) {
	if (m_window_proc_original)
		SetWindowLongPtr(m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_window_proc_original));
	if (m_present_module) {
		FreeLibrary(m_present_module);
		m_present_module = nullptr;
	}
}

std::once_flag init_flag;

void c_directx::start_frame( IDXGISwapChain* swap_chain ) {
	ID3D11Texture2D* back_buffer;
	DXGI_SWAP_CHAIN_DESC desc;
	swap_chain->GetDesc( &desc );

	m_window = desc.OutputWindow;

	std::call_once( init_flag, [&]( )
	{
		swap_chain->GetDevice( __uuidof( ID3D11Device ), reinterpret_cast<void**>( &m_device ) );
		m_device->GetImmediateContext( &m_device_context );
		swap_chain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<void**>( &back_buffer ) );

		m_device->CreateRenderTargetView( back_buffer, nullptr, &m_render_target );

		if ( back_buffer )
			back_buffer->Release( );

		m_window_proc_original = reinterpret_cast<WNDPROC>( SetWindowLongPtr( m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( hk_wnd_proc ) ) );

		ImGui::CreateContext( );
		ImGui_ImplWin32_Init( m_window );
		ImGui_ImplDX11_Init( m_device, m_device_context );

		g_render->initialize( );
	} );
}

void c_directx::new_frame( ) {
	ImGuiIO io = ImGui::GetIO( );

	g_render->update_screen_size( io );

	ImGui_ImplDX11_NewFrame( );
	ImGui_ImplWin32_NewFrame( );
	ImGui::NewFrame( );
}

void c_directx::end_frame( ) {
	ImGui::Render( );

	m_device_context->OMSetRenderTargets( 1, &m_render_target, nullptr );

	ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData( ) );
}
