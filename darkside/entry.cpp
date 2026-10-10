#include "darkside.hpp"
#include "directx/directx.hpp"
#include "entity_system/entity.hpp"

void destroy( HMODULE h_module ) {
    g_hooks->destroy( );
    logger::shutdown( );
    g_entity_system->level_shutdown( );
    FreeConsole( );

    FreeLibraryAndExitThread( h_module, 0 );
}

uintptr_t __stdcall start_address( const HMODULE h_module ) {
    logger::initialize( h_module );
	char module_path[MAX_PATH]{};
	if (!GetModuleFileNameA(h_module, module_path, MAX_PATH))
		strcpy_s(module_path, "<path unavailable>");
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(h_module);
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
		reinterpret_cast<std::uintptr_t>(h_module) + dos->e_lfanew);
	LOG_INFO("[Build] DLL=%s PE timestamp=0x%08X image=0x%X",
		module_path, nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage);
	LOG_INFO("[Pattern] Logging every signature scan: FOUND / NOT FOUND / MODULE MISSING");

    std::filesystem::create_directory( "c:\\airflow\\" );
    std::filesystem::create_directory( "c:\\airflow\\configs\\" );

    g_modules->m_modules.initialize( );
    if ( !g_interfaces->initialize( ) ) {
        LOG_ERROR( "Interface initialization failed" );
        return 0;
    }
    g_config_system->setup_values( );
    g_entity_system->initialize( );
    g_directx->initialize( );
    if ( !g_hooks->initialize( ) ) {
        LOG_ERROR( "Hook initialization failed" );
        g_directx->unitialize( );
        return 0;
    }
	LOG_INFO("[Pattern] Startup scans complete; deferred scans will be logged when used");

    LOG_SUCCESS( xorstr_( "[*] darkside successfully injected!\n" ) );

    while ( !GetAsyncKeyState( VK_END ) )
    {
        Sleep( 100 );
    }

    destroy( h_module );

    return 0;
}

BOOL APIENTRY DllMain( HMODULE h_module, DWORD  ul_reason_for_call, LPVOID lp_reserved ) {
    if ( ul_reason_for_call == DLL_PROCESS_ATTACH ) {
        auto current_process = WINCALL( GetCurrentProcess )( );
        auto priority_class = WINCALL( GetPriorityClass )( current_process );

        if ( priority_class != HIGH_PRIORITY_CLASS
            && priority_class != REALTIME_PRIORITY_CLASS )
            WINCALL( SetPriorityClass )( current_process, HIGH_PRIORITY_CLASS );

        WINCALL( CreateThread )( NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>( start_address ), h_module, NULL, NULL );

        return true;
    }

    return false;
}
