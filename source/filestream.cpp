#include "filestream.hpp"

#include "dataset.hpp"

#include <regex>

#include <qmessagebox.h>

// ----- MIAFileStream ----- //

template<> void MIAFileStream::read( QSharedPointer<Dataset>& dataset )
{
    const auto identifier = this->read<std::string>();
    const auto identifier_regex = std::regex { R"(^Dataset(?:\|(.+?))$)" };
    auto matches = std::smatch {};
    if( !std::regex_match( identifier, matches, identifier_regex ) )
    {
        QMessageBox::critical( nullptr, "", "Invalid dataset file.", QMessageBox::Ok );
        return;
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
            return;
        }
    }

    const auto element_count = this->read<uint32_t>();
    const auto channel_count = this->read<uint32_t>();
    const auto basetype = this->read<Dataset::Basetype>();

    auto channel_positions = Array<double>::allocate( channel_count );
    this->read( channel_positions.data(), channel_positions.bytes() );

    if( basetype == Dataset::Basetype::eInt8 )
    {
        auto intensities = Matrix<int8_t>::allocate( { element_count, channel_count } );
        this->read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<int8_t> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eInt16 )
    {
        auto intensities = Matrix<int16_t>::allocate( { element_count, channel_count } );
        this->read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<int16_t> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eInt32 )
    {
        auto intensities = Matrix<int32_t>::allocate( { element_count, channel_count } );
        this->read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<int32_t> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eUint8 )
    {
        auto intensities = Matrix<uint8_t>::allocate( { element_count, channel_count } );
        this->read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<uint8_t> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eUint16 )
    {
        auto intensities = Matrix<uint16_t>::allocate( { element_count, channel_count } );
        this->read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<uint16_t> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eUint32 )
    {
        auto intensities = Matrix<uint32_t>::allocate( { element_count, channel_count } );
        this->read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<uint32_t> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eFloat )
    {
        auto intensities = Matrix<float>::allocate( { element_count, channel_count } );
        this->read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<float> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else if( basetype == Dataset::Basetype::eDouble )
    {
        auto intensities = Matrix<double>::allocate( { element_count, channel_count } );
        this->read( intensities.data(), intensities.bytes() );
        dataset.reset( new TensorDataset<double> { std::move( intensities ), std::move( channel_positions ) } );
    }
    else
    {
        QMessageBox::critical( nullptr, "", "Unsupported dataset value type.", QMessageBox::Ok );
        return;
    }

    if( attribute_channel_identifiers )
    {
        auto channel_identifiers = Array<QString> { channel_count, QString {} };
        for( auto& identifier : channel_identifiers )
        {
            identifier = QString::fromStdString( this->read<std::string>() );
        }
        dataset->update_channel_identifiers( std::move( channel_identifiers ) );
    }

    if( attribute_spatial_metadata )
    {
        dataset->update_spatial_metadata( std::make_unique<Dataset::SpatialMetadata>( this->read<Dataset::SpatialMetadata>() ) );
    }
}
template<> void MIAFileStream::write( const QSharedPointer<Dataset>& dataset )
{
    auto identifier = std::string { "Dataset" };
    if( dataset->spatial_metadata() ) identifier += "|SpatialMetadata";
    if( dataset->override_channel_identifiers().has_value() ) identifier += "|ChannelIdentifiers";

    this->write( identifier );
    this->write( dataset->element_count() );
    this->write( dataset->channel_count() );
    this->write( dataset->basetype() );

    dataset->visit( [this] ( const auto& dataset )
    {
        const auto& channel_positions = dataset.channel_positions();
        this->write( channel_positions.data(), channel_positions.bytes() );

        const auto& intensities = dataset.intensities();
        this->write( intensities.data(), intensities.bytes() );
    } );

    if( const auto& identifiers = dataset->override_channel_identifiers(); identifiers.has_value() )
    {
        for( const auto& identifier : *identifiers )
        {
            this->write( identifier.toStdString() );
        }
    }

    if( dataset->spatial_metadata() )
    {
        this->write( *dataset->spatial_metadata() );
    }
}