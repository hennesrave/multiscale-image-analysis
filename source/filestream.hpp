#pragma once
#include "configuration.hpp"

#include <filesystem>
#include <fstream>

class Dataset;

// ----- BinaryStream ----- //

class BinaryStream
{
public:
    BinaryStream( std::iostream& stream ) noexcept;

    BinaryStream& write( const void* data, size_t size );
    BinaryStream& read( void* data, size_t size );

    template<class Type> static BinaryStream& write( BinaryStream& stream, const Type& value )
    {
        static_assert( std::is_trivially_copyable_v<Type>, "Type must be trivially copyable." );
        stream.write( std::addressof( value ), sizeof( Type ) );
        return stream;
    }
    template<class Type> static BinaryStream& read( BinaryStream& stream, Type& value )
    {
        static_assert( std::is_trivially_copyable_v<Type>, "Type must be trivially copyable." );
        stream.read( std::addressof( value ), sizeof( Type ) );
        return stream;
    }

    template<class Type> BinaryStream& write( const Type& value )
    {
        return BinaryStream::write<Type>( *this, value );
    }
    template<class Type> BinaryStream& read( Type& value )
    {
        return BinaryStream::read<Type>( *this, value );
    }
    template<class Type> Type read()
    {
        auto value = Type {};
        this->read<Type>( value );
        return value;
    }

private:
    std::iostream& _stream;
};

template<> BinaryStream& BinaryStream::write( BinaryStream& stream, const std::string& string );
template<> BinaryStream& BinaryStream::read( BinaryStream& stream, std::string& string );

template<> BinaryStream& BinaryStream::write( BinaryStream& stream, const QSharedPointer<Dataset>& dataset );
template<> BinaryStream& BinaryStream::read( BinaryStream& stream, QSharedPointer<Dataset>& dataset );

// ----- MIAFileStream ----- //

class MIAFileStream : public BinaryStream
{
public:
    static constexpr uint8_t magic_number[8] = { 'M', 'I', 'A', '_', 'F', 'I', 'L', 'E' };

    MIAFileStream( const std::filesystem::path& filepath, std::ios::openmode openmode );
    ~MIAFileStream();

    operator bool() const noexcept;
    const config::ApplicationVersion& application_version() const noexcept;

private:
    std::fstream _filestream;
    config::ApplicationVersion _application_version;
};