#include "filestream.hpp"

#include "dataset.hpp"

#include <regex>

#include <qmessagebox.h>

// ----- BinaryStream ----- //

BinaryStream::BinaryStream( std::iostream& stream ) noexcept : _stream { stream } {}

BinaryStream& BinaryStream::write( const void* data, size_t size )
{
    _stream.write( static_cast<const char*>( data ), size );
    return *this;
}
BinaryStream& BinaryStream::read( void* data, size_t size )
{
    _stream.read( static_cast<char*>( data ), size );
    return *this;
}

template<> BinaryStream& BinaryStream::read( BinaryStream& stream, std::string& value )
{
    const auto size = stream.read<uint64_t>();
    value.resize( size );
    stream.read( value.data(), size );
    return stream;
}
template<> BinaryStream& BinaryStream::write( BinaryStream& stream, const std::string& value )
{
    stream.write( static_cast<uint64_t>( value.size() ) );
    stream.write( value.data(), value.size() );
    return stream;
}

template<> BinaryStream& BinaryStream::read( BinaryStream& stream, QSharedPointer<Dataset>& dataset )
{
    const auto identifier = stream.read<std::string>();
    const auto identifier_regex = std::regex { R"(^Dataset(?:\|(.+?))$)" };
    auto matches = std::smatch {};
    if( !std::regex_match( identifier, matches, identifier_regex ) )
    {
        QMessageBox::critical( nullptr, "", "Invalid dataset file.", QMessageBox::Ok );
        return stream;
    }

    auto attribute_spatial_metadata = false;
    auto attribute_channel_identifiers = false;

    for( size_t match_index = 1; match_index < matches.size(); ++match_index )
    {
        const auto attribute = matches[match_index].str();
        if( attribute == "ChannelIdentifiers" ) attribute_channel_identifiers = true;
        else if( attribute == "SpatialMetadata" ) attribute_spatial_metadata = true;
        else
        {
            QMessageBox::warning( nullptr, "", "Unknown dataset attribute: " + QString::fromStdString( attribute ), QMessageBox::Ok );
            return stream;
        }
    }

    const auto element_count = stream.read<uint32_t>();
    const auto channel_count = stream.read<uint32_t>();
    const auto basetype = stream.read<Dataset::Basetype>();

    auto channel_positions = Array<double>::allocate( channel_count );
    stream.read( channel_positions.data(), channel_positions.bytes() );

    if( basetype == Dataset::Basetype::eInt8 )
    {
        auto intensities = Matrix<int8_t>::allocate( { element_count, channel_count } );
        stream.read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<int8_t> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eInt16 )
    {
        auto intensities = Matrix<int16_t>::allocate( { element_count, channel_count } );
        stream.read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<int16_t> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eInt32 )
    {
        auto intensities = Matrix<int32_t>::allocate( { element_count, channel_count } );
        stream.read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<int32_t> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eUint8 )
    {
        auto intensities = Matrix<uint8_t>::allocate( { element_count, channel_count } );
        stream.read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<uint8_t> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eUint16 )
    {
        auto intensities = Matrix<uint16_t>::allocate( { element_count, channel_count } );
        stream.read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<uint16_t> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eUint32 )
    {
        auto intensities = Matrix<uint32_t>::allocate( { element_count, channel_count } );
        stream.read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<uint32_t> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eFloat )
    {
        auto intensities = Matrix<float>::allocate( { element_count, channel_count } );
        stream.read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<float> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eDouble )
    {
        auto intensities = Matrix<double>::allocate( { element_count, channel_count } );
        stream.read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<double> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else
    {
        QMessageBox::critical( nullptr, "", "Unsupported dataset value type.", QMessageBox::Ok );
        return stream;
    }

    if( attribute_channel_identifiers )
    {
        auto channel_identifiers = Array<QString> { channel_count, QString {} };
        for( auto& identifier : channel_identifiers )
        {
            identifier = QString::fromStdString( stream.read<std::string>() );
        }
        dataset->update_channel_identifiers( std::move( channel_identifiers ) );
    }

    if( attribute_spatial_metadata )
    {
        dataset->update_spatial_metadata( std::make_unique<Dataset::SpatialMetadata>( stream.read<Dataset::SpatialMetadata>() ) );
    }

    return stream;
}
template<> BinaryStream& BinaryStream::write( BinaryStream& stream, const QSharedPointer<Dataset>& dataset )
{
    auto identifier = std::string { "Dataset" };
    if( dataset->spatial_metadata() ) identifier += "|SpatialMetadata";
    if( dataset->override_channel_identifiers().has_value() ) identifier += "|ChannelIdentifiers";

    stream.write( identifier );
    stream.write( dataset->element_count() );
    stream.write( dataset->channel_count() );
    stream.write( dataset->basetype() );

    dataset->visit( [&stream] ( const auto& dataset )
    {
        const auto& channel_positions = dataset.channel_positions();
        stream.write( channel_positions.data(), channel_positions.bytes() );

        const auto& intensities = dataset.intensities();
        stream.write( intensities.data(), intensities.bytes() );
    } );

    if( const auto& identifiers = dataset->override_channel_identifiers(); identifiers.has_value() )
    {
        for( const auto& identifier : *identifiers )
        {
            stream.write( identifier.toStdString() );
        }
    }

    if( dataset->spatial_metadata() )
    {
        stream.write( *dataset->spatial_metadata() );
    }

    return stream;
}

// ----- MIAFileStream ----- //

MIAFileStream::MIAFileStream( const std::filesystem::path& filepath, std::ios::openmode openmode ) : BinaryStream { _filestream }
{
    openmode |= std::ios::binary;
    _filestream.open( filepath, openmode );
    if( !_filestream )
    {
        Console::error( "Failed to open file stream: " + filepath.string() );
        return;
    }

    if( openmode & std::ios::out )
    {
        _application_version = config::application_version;
        _filestream.write( reinterpret_cast<const char*>( MIAFileStream::magic_number ), sizeof( MIAFileStream::magic_number ) );
        _filestream.write( reinterpret_cast<const char*>( &_application_version ), sizeof( _application_version ) );
    }
    else if( openmode & std::ios::in )
    {
        uint8_t number[sizeof( MIAFileStream::magic_number )];
        _filestream.read( reinterpret_cast<char*>( number ), sizeof( number ) );

        if( _filestream.gcount() != sizeof( number ) || std::memcmp( number, MIAFileStream::magic_number, sizeof( number ) ) != 0 )
        {
            Console::warning( "Invalid magic number in file: " + filepath.string() );
            _filestream.close();
            return;
        }

        _filestream.read( reinterpret_cast<char*>( &_application_version ), sizeof( _application_version ) );
        if( _filestream.gcount() != sizeof( _application_version ) )
        {
            Console::warning( "Failed to read application version from file: " + filepath.string() );
            _filestream.close();
            return;
        }
    }
    else
    {
        Console::error( "Invalid open mode for file stream: " + filepath.string() );
        return;
    }
}

MIAFileStream::~MIAFileStream()
{
    if( _filestream.is_open() )
    {
        _filestream.close();
    }
}

MIAFileStream::operator bool() const noexcept
{
    return static_cast<bool>( _filestream );
}
const config::ApplicationVersion& MIAFileStream::application_version() const noexcept
{
    return _application_version;
}