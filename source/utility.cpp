#include "utility.hpp"

#include <numbers>
#include <limits>

namespace utility
{
    double degrees_to_radians( double degrees ) noexcept
    {
        return degrees * ( std::numbers::pi / 180.0 );
    }
    double radians_to_degrees( double radians ) noexcept
    {
        return radians * ( 180.0 / std::numbers::pi );
    }

    int stepsize_to_precision( double stepsize )
    {
        return std::max( 0, static_cast<int>( std::ceil( -std::log10( stepsize ) ) ) );
    }
    int compute_precision( double value )
    {
        constexpr auto tolerance = 100.0 * std::numeric_limits<double>::epsilon();
        value = std::abs( value );

        auto precision = 0;
        auto fractional = value - std::floor( value );
        while( fractional > tolerance && precision < 17 )
        {
            value *= 10.0;
            fractional = value - std::floor( value );
            ++precision;
        }
        return precision;
    }
}

// ----- Timer ----- //

double Timer::milliseconds() const
{
    const auto current = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>( current - _start ).count() / 1000.0;
}
double Timer::microseconds() const
{
    const auto current = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>( current - _start ).count() / 1000.0;
}
void Timer::reset()
{
    _start = std::chrono::high_resolution_clock::now();
}

// ----- MIAFileStream ----- //

MIAFileStream::~MIAFileStream()
{
    if( _stream.is_open() )
    {
        _stream.close();
    }
}

MIAFileStream::operator bool() const noexcept
{
    return static_cast<bool>( _stream );
}

bool MIAFileStream::open( const std::filesystem::path& filepath, std::ios::openmode openmode )
{
    if( _stream.is_open() )
    {
        Console::error( "[MIAFileStream::open] Cannot open file stream, already open." );
        return false;
    }

    openmode |= std::ios::binary;
    _stream.open( filepath, openmode );
    if( !_stream )
    {
        Console::error( "[MIAFileStream::open] Failed to open file stream: " + filepath.string() );
        return false;
    }

    if( openmode & std::ios::out )
    {
        _application_version = config::application_version;
        _stream.write( reinterpret_cast<const char*>( MIAFileStream::magic_number ), sizeof( MIAFileStream::magic_number ) );
        _stream.write( reinterpret_cast<const char*>( &_application_version ), sizeof( _application_version ) );
    }
    else if( openmode & std::ios::in )
    {
        uint8_t number[sizeof( MIAFileStream::magic_number )];
        _stream.read( reinterpret_cast<char*>( number ), sizeof( number ) );

        if( _stream.gcount() != sizeof( number ) || std::memcmp( number, MIAFileStream::magic_number, sizeof( number ) ) != 0 )
        {
            Console::warning( "[MIAFileStream::open] Invalid magic number in file: " + filepath.string() );
            _stream.close();
            return false;
        }

        _stream.read( reinterpret_cast<char*>( &_application_version ), sizeof( _application_version ) );
        if( _stream.gcount() != sizeof( _application_version ) )
        {
            Console::warning( "[MIAFileStream::open] Failed to read application version from file: " + filepath.string() );
            _stream.close();
            return false;
        }
    }
    else
    {
        Console::error( "[MIAFileStream::open] Invalid open mode for file stream: " + filepath.string() );
        return false;
    }

    return static_cast<bool>( _stream );
}

void MIAFileStream::read( void* data, size_t size )
{
    _stream.read( reinterpret_cast<char*>( data ), size );
}
void MIAFileStream::write( const void* data, size_t size )
{
    _stream.write( reinterpret_cast<const char*>( data ), size );
}

template<> void MIAFileStream::read( std::string& value )
{
    const auto size = this->read<uint64_t>();
    value.resize( size );
    _stream.read( value.data(), size );
}
template<> void MIAFileStream::write( const std::string& value )
{
    this->write( static_cast<uint64_t>( value.size() ) );
    _stream.write( value.data(), value.size() );
}
