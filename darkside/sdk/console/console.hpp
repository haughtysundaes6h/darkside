#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <mutex>
#include <cstring>

namespace logger
{
    inline std::mutex log_mutex;
    inline FILE* log_file = nullptr;

    inline void initialize( HMODULE owner = nullptr )
    {
        char module_path[MAX_PATH]{};
        const DWORD length = owner ? GetModuleFileNameA(owner, module_path, MAX_PATH) : 0;
        if ( length && length < MAX_PATH ) {
            if ( auto* slash = strrchr(module_path, '\\') ) {
                slash[1] = '\0';
                if ( strlen(module_path) + strlen("darkside-runtime.log") < sizeof(module_path) ) {
                    strcat_s(module_path, "darkside-runtime.log");
                    fopen_s(&log_file, module_path, "w");
                }
            }
        }
        // allocate console
        AllocConsole( );

        // set console title
        SetConsoleTitleA( xorstr_( "darkside" ) );

        // set output handle
        freopen_s( reinterpret_cast<FILE**>( stdout ), "CONOUT$", "w", stdout );
        freopen_s( reinterpret_cast<FILE**>( stderr ), "CONOUT$", "w", stderr );
        freopen_s( reinterpret_cast<FILE**>( stdin ), "CONIN$", "r", stdin );
    }

    inline void shutdown( )
    {
        const std::lock_guard<std::mutex> lock(log_mutex);
        if ( log_file ) {
            fclose(log_file);
            log_file = nullptr;
        }
        // close console window
        PostMessageA( GetConsoleWindow( ), WM_CLOSE, 0, 0 );

        // free console
        FreeConsole( );
    }

    inline void log( const char* file, int line, WORD color, const char* fmt, ... )
    {
        const std::lock_guard<std::mutex> lock(log_mutex);
        // Get current time
        SYSTEMTIME time;
        GetLocalTime( &time );

        // Save the current console attributes
        HANDLE hConsole = GetStdHandle( STD_OUTPUT_HANDLE );
        CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
        GetConsoleScreenBufferInfo( hConsole, &consoleInfo );
        WORD originalAttrs = consoleInfo.wAttributes;

        // Print the time
        printf( xorstr_("[%02d:%02d:%02d] "), time.wHour, time.wMinute, time.wSecond );

        // remove path from file
        const char* file_name = strrchr( file, '\\' );
        if ( !file_name ) file_name = strrchr( file, '/' );
        file_name = file_name ? file_name + 1 : file;

        // Set the desired text color for file:line
        SetConsoleTextAttribute( hConsole, color );
        printf( xorstr_( "%s:%d:" ), file_name, line );

        // Reset to original console attributes
        SetConsoleTextAttribute( hConsole, originalAttrs );
        printf( " " );

        // Print the rest of the log message
        va_list args;
        va_start( args, fmt );
        char message[4096]{};
        vsnprintf_s(message, sizeof(message), _TRUNCATE, fmt, args);
        va_end( args );
        printf("%s", message);

        // Print a newline
        printf( "\n" );
        if ( log_file ) {
            fprintf(log_file, "[%02d:%02d:%02d] %s:%d: %s\n", time.wHour, time.wMinute,
                time.wSecond, file_name, line, message);
            fflush(log_file);
        }
    }
}

#define LOG(fmt, ...) logger::log(__FILE__, __LINE__, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, fmt, __VA_ARGS__)
#define LOG_ERROR(fmt, ...) logger::log(__FILE__, __LINE__, FOREGROUND_INTENSITY | FOREGROUND_RED, fmt, __VA_ARGS__)
#define LOG_WARNING(fmt, ...) logger::log(__FILE__, __LINE__, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN, fmt, __VA_ARGS__)
#define LOG_SUCCESS(fmt, ...) logger::log(__FILE__, __LINE__, FOREGROUND_INTENSITY | FOREGROUND_GREEN, fmt, __VA_ARGS__)
#define LOG_INFO(fmt, ...) logger::log(__FILE__, __LINE__, FOREGROUND_INTENSITY | FOREGROUND_BLUE, fmt, __VA_ARGS__)
