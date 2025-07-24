#pragma once
#include <source_location>
#include <string>

class Console
{
public:
    static void initialize();

    static void info( const std::string& message, const std::source_location location = std::source_location::current() );
    static void warning( const std::string& message, const std::source_location location = std::source_location::current() );
    static void error( const std::string& message, const std::source_location location = std::source_location::current() );
    static void critical( const std::string& message, const std::source_location location = std::source_location::current() );

    static void info( const std::wstring& message, const std::source_location location = std::source_location::current() );
    static void warning( const std::wstring& message, const std::source_location location = std::source_location::current() );
    static void error( const std::wstring& message, const std::source_location location = std::source_location::current() );
    static void critical( const std::wstring& message, const std::source_location location = std::source_location::current() );

private:
    Console() = delete;
};