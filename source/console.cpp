#include "console.hpp"

#include <spdlog/spdlog.h>    
#include <spdlog/sinks/stdout_color_sinks.h>
#include <windows.h>

void Console::initialize()
{
    SetConsoleOutputCP( CP_UTF8 );
    spdlog::set_pattern( "[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%l] %v" );
}

void Console::info( const std::string& message, const std::source_location location )
{
    //spdlog::info( "[{}, line = {}] {}", location.function_name(), location.line(), message );
    spdlog::info( "{}", message );
}
void Console::warning( const std::string& message, const std::source_location location )
{
    //spdlog::warn( "[{}, line = {}] {}", location.function_name(), location.line(), message );
    spdlog::warn( "{}", message );
}
void Console::error( const std::string& message, const std::source_location location )
{
    //spdlog::error( "[{}, line = {}] {}", location.function_name(), location.line(), message );
    spdlog::error( "{}", message );
}
void Console::critical( const std::string& message, const std::source_location location )
{
    //spdlog::critical( "[{}, line = {}] {}", location.function_name(), location.line(), message );
    spdlog::critical( "{}", message );
    std::exit( 1 );
}

void Console::info( const std::wstring& message, const std::source_location location )
{
    //spdlog::stdout_color_mt<wchar_t>( "wlogger" )->info( "[{} | {}, line = {}] {}", Console::timestamp_wstring(), location.file_name(), location.line(), message );
}
void Console::warning( const std::wstring& message, const std::source_location location )
{
    //spdlog::stdout_color_mt<wchar_t>( "wlogger" )->warn( "[{} | {}, line = {}] {}", Console::timestamp_wstring(), location.file_name(), location.line(), message );
}
void Console::error( const std::wstring& message, const std::source_location location )
{
    //spdlog::stdout_color_mt<wchar_t>( "wlogger" )->error( "[{} | {}, line = {}] {}", Console::timestamp_wstring(), location.file_name(), location.line(), message );
}
void Console::critical( const std::wstring& message, const std::source_location location )
{
    //spdlog::stdout_color_mt<wchar_t>( "wlogger" )->critical( "[{} | {}, line = {}] {}", Console::timestamp_wstring(), location.file_name(), location.line(), message );
    //std::exit( 1 );
}