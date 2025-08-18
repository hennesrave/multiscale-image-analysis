#include "console.hpp"

#include <filesystem>

#include <spdlog/spdlog.h>    
#include <spdlog/sinks/stdout_color_sinks.h>
#include <windows.h>

namespace
{
    std::shared_ptr<spdlog::logger> logger;
}

void Console::initialize()
{
    SetConsoleOutputCP( CP_UTF8 );
    logger = spdlog::stdout_color_st( "console" );
    logger->set_pattern( "[%Y-%m-%d %H:%M:%S] [%^%l%$] %v" );
}

void Console::info( const std::string& message, const std::source_location location )
{
    const auto filename = std::filesystem::path { location.file_name() }.filename().string();
    logger->info( "[{}, line {}] {}", filename, location.line(), message );
}
void Console::warning( const std::string& message, const std::source_location location )
{
    const auto filename = std::filesystem::path { location.file_name() }.filename().string();
    logger->warn( "[{}, line {}] {}", filename, location.line(), message );
}
void Console::error( const std::string& message, const std::source_location location )
{
    const auto filename = std::filesystem::path { location.file_name() }.filename().string();
    logger->error( "[{}, line {}] {}", filename, location.line(), message );
}
void Console::critical( const std::string& message, const std::source_location location )
{
    const auto filename = std::filesystem::path { location.file_name() }.filename().string();
    logger->critical( "[{}, line {}] {}", filename, location.line(), message );
    std::terminate();
}