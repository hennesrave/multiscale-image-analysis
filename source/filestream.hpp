#pragma once
#include "configuration.hpp"

#include <filesystem>
#include <fstream>

class Dataset;

// ----- MIAFileStream ----- //

class MIAFileStream
{
public:
    static constexpr uint8_t magic_number[8] = { 'M', 'I', 'A', '_', 'F', 'I', 'L', 'E' };

    MIAFileStream() noexcept = default;
    MIAFileStream( const MIAFileStream& ) = delete;
    MIAFileStream( MIAFileStream&& ) = delete;

    MIAFileStream& operator=( const MIAFileStream& ) = delete;
    MIAFileStream& operator=( MIAFileStream&& ) = delete;

    ~MIAFileStream();

    operator bool() const noexcept;

    bool open( const std::filesystem::path& filepath, std::ios::openmode openmode );
    const config::ApplicationVersion& application_version() const noexcept
    {
        return _application_version;
    }

    void read( void* data, size_t size );
    void write( const void* data, size_t size );

    template<class Type> void read( Type& value )
    {
        static_assert( std::is_trivially_copyable_v<Type>, "Type must be trivially copyable." );
        this->read( std::addressof( value ), sizeof( Type ) );
    }
    template<class Type> void write( const Type& value )
    {
        static_assert( std::is_trivially_copyable_v<Type>, "Type must be trivially copyable." );
        this->write( std::addressof( value ), sizeof( Type ) );
    }

    template<class Type> Type read()
    {
        auto value = Type {};
        this->read<Type>( value );
        return value;
    }

private:
    std::fstream _stream;
    config::ApplicationVersion _application_version;
};

template<> void MIAFileStream::read( std::string& string );
template<> void MIAFileStream::write( const std::string& string );

template<> void MIAFileStream::read( QSharedPointer<Dataset>& dataset );
template<> void MIAFileStream::write( const QSharedPointer<Dataset>& dataset );