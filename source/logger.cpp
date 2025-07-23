#include "logger.hpp"

#include "configuration.hpp"

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>

QTextStream& Logger::stream()
{
    if constexpr( config::logger_console_enabled )
    {
        static auto stream = QTextStream { stdout };
        return stream;
    }
    else
    {
        struct LoggerFile
        {
            QFile file;
            QTextStream stream;

            LoggerFile() : file { config::executable_directory.filePath( QString { config::application_identifier } + ".log" ) }, stream { &file }
            {
                file.open( QIODevice::WriteOnly | QIODevice::Text );
            }
        };

        static auto logger_file = LoggerFile {};
        return logger_file.stream;
    }
}
QTextStream& Logger::terminate( QTextStream& stream )
{
    stream.flush();

    if constexpr( !config::logger_console_enabled )
    {
        if( auto file = qobject_cast<QFile*>( stream.device() ) )
        {
            file->flush();
            file->close();

            const auto timestamp = std::format( "{:%Y-%m-%d_%H-%M-%S}", std::chrono::floor<std::chrono::seconds>( std::chrono::utc_clock::now() ) );
            file->copy( config::executable_directory.filePath( QString { config::application_identifier } + "_" + timestamp.data() + "_error.log" ) );
        }
    }
    std::exit( 1 );
}

std::string Logger::timestamp()
{
    return std::format( "{:%Y-%m-%d %H:%M:%S}", std::chrono::utc_clock::now() );
}

// ----- Logger::info ----- //

Logger::info::info( const std::source_location& location )
{
    Logger::stream() << "[" << Logger::timestamp().data() << " | " << location.function_name() << ", line " << location.line() << "] ";
}

Logger::info::~info()
{
    Logger::stream() << Qt::endl;
}

// ----- Logger::warning ----- //

Logger::warning::warning( const std::source_location& location )
{
    Logger::stream() << "[WARNING] [" << Logger::timestamp().data() << " | " << location.function_name() << ", line " << location.line() << "] ";
}

Logger::warning::~warning()
{
    Logger::stream() << Qt::endl;
}

// ----- Logger::error ----- //

Logger::error::error( const std::source_location& location )
{
    const auto timestamp = std::format( "{:%Y-%m-%d %H:%M:%S}", std::chrono::utc_clock::now() );
    Logger::stream() << "[ERROR] [" << Logger::timestamp().data() << " | " << location.function_name() << ", line " << location.line() << "] ";
}

Logger::error::~error()
{
    Logger::stream() << Qt::endl;
}