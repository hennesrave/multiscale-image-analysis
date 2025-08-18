#include "dataset.hpp"

#include <regex>

#include <qmessagebox.h>

// ----- Dataset ----- //

Dataset::Dataset()
    : QObject {}
    , _computed_channel_identifiers { std::bind( &Dataset::compute_channel_identifiers, this ) }
    , _override_channel_identifiers { std::nullopt }
    , _statistics { std::bind( &Dataset::compute_statistics, this ) }
{
    QObject::connect( this, &Dataset::intensities_changed, &_statistics, &ComputedObject::invalidate );
    QObject::connect( &_channel_identifier_precision, &OverrideObject::value_changed, &_computed_channel_identifiers, &ComputedObject::invalidate );

    QObject::connect( &_computed_channel_identifiers, &ComputedObject::changed, this, &Dataset::channel_identifiers_changed );
    QObject::connect( &_statistics, &ComputedObject::changed, this, &Dataset::statistics_changed );

    emit _computed_channel_identifiers.changed();
}

void Dataset::update_channel_identifiers( Array<QString> channel_identifiers )
{
    _override_channel_identifiers = std::move( channel_identifiers );
    _computed_channel_identifiers.invalidate();
}
void Dataset::update_spatial_metadata( std::unique_ptr<SpatialMetadata> spatial_metadata )
{
    if( _spatial_metadata != spatial_metadata )
    {
        _spatial_metadata = std::move( spatial_metadata );
        emit spatial_metadata_changed();
    }
}

const QString& Dataset::channel_identifier( uint32_t channel_index ) const
{
    return _computed_channel_identifiers.value()[channel_index];
}
const Dataset::SpatialMetadata* Dataset::spatial_metadata() const noexcept
{
    return _spatial_metadata.get();
}

const std::optional<Array<QString>>& Dataset::override_channel_identifiers() const noexcept
{
    return _override_channel_identifiers;
}

const Dataset::Statistics& Dataset::statistics() const noexcept
{
    return *_statistics;
}
const Array<Dataset::Statistics>& Dataset::segmentation_statistics( QSharedPointer<const Segmentation> segmentation ) const
{
    const auto segmentation_pointer = segmentation.get();
    if( !_segmentation_statistics.contains( segmentation_pointer ) )
    {
        auto promise = new Computed<Array<Statistics>> { [this, pointer = QWeakPointer { segmentation }]
        {
            if( auto segmentation = pointer.lock() )
            {
                return this->compute_segmentation_statistics( segmentation );
            }
            else
            {
                return Array<Statistics> {};
            }
        } };
        _segmentation_statistics[segmentation_pointer] = std::unique_ptr<Computed<Array<Statistics>>>( promise );

        QObject::connect( this, &Dataset::intensities_changed, promise, &ComputedObject::invalidate );
        QObject::connect( segmentation_pointer, &Segmentation::segment_numbers_changed, promise, &ComputedObject::invalidate );
        QObject::connect( segmentation_pointer, &Segmentation::destroyed, this, [this, segmentation_pointer] { _segmentation_statistics.erase( segmentation_pointer ); } );
        QObject::connect( promise, &ComputedObject::changed, [this, pointer = QWeakPointer { segmentation }]
        {
            emit segmentation_statistics_changed( pointer.lock() );
        } );
    }
    return **_segmentation_statistics[segmentation_pointer];
}

Array<QString> Dataset::compute_channel_identifiers() const
{
    if( _override_channel_identifiers.has_value() )
    {
        return *_override_channel_identifiers;
    }
    else
    {
        auto channel_identifiers = Array<QString> { this->channel_count(), QString {} };
        utility::iterate_parallel<uint32_t>( 0, this->channel_count(), [&] ( uint32_t channel_index )
        {
            channel_identifiers[channel_index] = QString::number( this->channel_position( channel_index ), 'f', _channel_identifier_precision.value() );
        } );
        return channel_identifiers;
    }
}

// ----- Dataset::SpatialMetadata ----- //

Dataset::SpatialMetadata::SpatialMetadata( uint32_t width, uint32_t height ) : dimensions { width, height }
{
}

uint32_t Dataset::SpatialMetadata::element_index( vec2<uint32_t> coordinates ) const
{
    return coordinates.y * width + coordinates.x;
}
vec2<uint32_t> Dataset::SpatialMetadata::coordinates( uint32_t element_index ) const
{
    return vec2<uint32_t> { element_index% width, element_index / width };
}
