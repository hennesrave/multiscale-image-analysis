#pragma once
#include <filesystem>
#include <source_location>

#include <qstring.h>
#include <qtextstream.h>

inline QTextStream& operator<<( QTextStream& stream, const std::filesystem::path& filepath )
{
    return stream << QString::fromStdWString( filepath.wstring() );
}

class Logger
{
public:
    static QTextStream& stream();
    static QTextStream& terminate( QTextStream& stream );

    static std::string timestamp();

    class info
    {
    public:
        info( const std::source_location& location = std::source_location::current() );

        info( const info& ) = delete;
        info( info&& ) = delete;

        info& operator=( const info& ) = delete;
        info& operator=( info&& ) = delete;

        ~info();

        info& operator<<( const auto& value )
        {
            Logger::stream() << value;
            return *this;
        }
    };

    class warning
    {
    public:
        warning( const std::source_location& location = std::source_location::current() );

        warning( const warning& ) = delete;
        warning( warning&& ) = delete;

        warning& operator=( const warning& ) = delete;
        warning& operator=( warning&& ) = delete;

        ~warning();

        warning& operator<<( const auto& value )
        {
            Logger::stream() << value;
            return *this;
        }
    };

    class error
    {
    public:
        error( const std::source_location& location = std::source_location::current() );

        error( const error& ) = delete;
        error( error&& ) = delete;

        error& operator=( const error& ) = delete;
        error& operator=( error&& ) = delete;

        ~error();

        error& operator<<( const auto& value )
        {
            Logger::stream() << value;
            return *this;
        }
    };

private:
    Logger() = delete;
};