#include "dataset.hpp"

// ----- Dataset ----- //

Dataset::Dataset() : QObject {}, _statistics { std::bind( &Dataset::compute_statistics, this ) }
{
    QObject::connect( this, &Dataset::intensities_changed, &_statistics, &ComputedObject::invalidate );
    QObject::connect( &_channel_identifier_precision, &Override<int>::value_changed, this, &Dataset::channel_identifiers_changed );

    QObject::connect( &_statistics, &ComputedObject::changed, this, &Dataset::statistics_changed );
}

QString Dataset::channel_identifier( uint32_t channel_index ) const
{
    return QString::number( this->channel_position( channel_index ), 'f', _channel_identifier_precision.value() );
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

// ----- DatasetChannelsFeature ----- //

DatasetChannelsFeature::DatasetChannelsFeature( QSharedPointer<const Dataset> dataset, Range<uint32_t> channel_range, Reduction reduction, BaselineCorrection baseline_correction )
    : Feature {}, _dataset { dataset }, _channel_range { channel_range }, _reduction { reduction }, _baseline_correction { baseline_correction }
{
    QObject::connect( dataset.get(), &Dataset::intensities_changed, &_values, &ComputedObject::invalidate );
    QObject::connect( this, &DatasetChannelsFeature::channel_range_changed, &_values, &ComputedObject::invalidate );
    QObject::connect( this, &DatasetChannelsFeature::reduction_changed, &_values, &ComputedObject::invalidate );
    QObject::connect( this, &DatasetChannelsFeature::baseline_correction_changed, &_values, &ComputedObject::invalidate );

    QObject::connect( this, &DatasetChannelsFeature::channel_range_changed, this, &DatasetChannelsFeature::update_identifier );
    this->update_identifier();
}

uint32_t DatasetChannelsFeature::element_count() const noexcept
{
    return _dataset.lock()->element_count();
}

QSharedPointer<const Dataset> DatasetChannelsFeature::dataset() const
{
    return _dataset.lock();
}

Range<uint32_t> DatasetChannelsFeature::channel_range() const noexcept
{
    return _channel_range;
}
void DatasetChannelsFeature::update_channel_range( Range<uint32_t> channel_range )
{
    if( _channel_range != channel_range )
    {
        if( channel_range.lower > channel_range.upper )
        {
            std::swap( channel_range.lower, channel_range.upper );
        }
        _channel_range = channel_range;
        emit channel_range_changed( _channel_range );
    }
}

DatasetChannelsFeature::Reduction DatasetChannelsFeature::reduction() const noexcept
{
    return _reduction;
}
void DatasetChannelsFeature::update_reduction( Reduction reduction )
{
    if( _reduction != reduction )
    {
        _reduction = reduction;
        emit reduction_changed( _reduction );
    }
}

DatasetChannelsFeature::BaselineCorrection DatasetChannelsFeature::baseline_correction() const noexcept
{
    return _baseline_correction;
}
void DatasetChannelsFeature::update_baseline_correction( BaselineCorrection baseline_correction )
{
    if( _baseline_correction != baseline_correction )
    {
        _baseline_correction = baseline_correction;
        emit baseline_correction_changed( _baseline_correction );
    }
}

void DatasetChannelsFeature::update_identifier()
{
    if( const auto dataset = _dataset.lock() )
    {
        if( _channel_range.lower == _channel_range.upper )
        {
            _identifier.update_automatic_value( "Channel " + dataset->channel_identifier( _channel_range.lower ) );
        }
        else if( _channel_range.lower < _channel_range.upper )
        {
            _identifier.update_automatic_value( "Channel " + dataset->channel_identifier( _channel_range.lower ) + " to " + dataset->channel_identifier( _channel_range.upper ) );
        }
    }
    else
    {
        _identifier.update_automatic_value( "DatasetChannelsFeature" );
    }
}
Array<double> DatasetChannelsFeature::compute_values() const
{
    Console::info( "DatasetChannelsFeature::compute_values" );
    auto values = Array<double> { this->element_count(), 0.0 };

    if( const auto dataset = _dataset.lock() )
    {
        dataset->visit( [&] ( const auto& dataset )
        {
            const auto& intensities = dataset.intensities();
            const auto& channel_positions = dataset.channel_positions();

            const auto gather_value = [&] ( uint32_t element_index, uint32_t channel_index )
            {
                return static_cast<double>( intensities.value( { element_index, channel_index } ) );
            };

            if( _reduction == Reduction::eAccumulate )
            {
                if( _baseline_correction == BaselineCorrection::eNone )
                {
                    utility::iterate_parallel<uint32_t>( 0, dataset.element_count(), [&] ( uint32_t element_index )
                    {
                        auto& value = values[element_index] = 0.0;
                        for( uint32_t channel_index = _channel_range.lower; channel_index <= _channel_range.upper; ++channel_index )
                        {
                            value += gather_value( element_index, channel_index );
                        }
                    } );
                }
                else if( _baseline_correction == BaselineCorrection::eMinimum )
                {
                    utility::iterate_parallel<uint32_t>( 0, dataset.element_count(), [&] ( uint32_t element_index )
                    {
                        auto& value = values[element_index] = 0.0;
                        auto minimum_intensity = std::numeric_limits<double>::max();

                        for( uint32_t channel_index = _channel_range.lower; channel_index <= _channel_range.upper; ++channel_index )
                        {
                            const auto intensity = gather_value( element_index, channel_index );
                            value += intensity;
                            minimum_intensity = std::min( minimum_intensity, intensity );
                        }

                        value -= minimum_intensity * ( _channel_range.upper - _channel_range.lower + 1 );
                    } );
                }
                else if( _baseline_correction == BaselineCorrection::eLinear )
                {
                    utility::iterate_parallel<uint32_t>( 0, dataset.element_count(), [&] ( uint32_t element_index )
                    {
                        auto& value = values[element_index] = 0.0;

                        const auto first_channel = channel_positions[_channel_range.lower];
                        const auto first_intensity = gather_value( element_index, _channel_range.lower );

                        const auto last_channel = channel_positions[_channel_range.upper];
                        const auto last_intensity = gather_value( element_index, _channel_range.upper );

                        for( uint32_t channel_index = _channel_range.lower; channel_index <= _channel_range.upper; ++channel_index )
                        {
                            const auto intensity = gather_value( element_index, channel_index );
                            const auto t = ( channel_positions[channel_index] - first_channel ) / ( last_channel - first_channel );
                            const auto intensity_correction = first_intensity + t * ( last_intensity - first_intensity );
                            value += intensity - intensity_correction;
                        }
                    } );
                }
            }
            else if( _reduction == Reduction::eIntegrate )
            {
                if( _baseline_correction == BaselineCorrection::eNone )
                {
                    utility::iterate_parallel<uint32_t>( 0, dataset.element_count(), [&] ( uint32_t element_index )
                    {
                        auto& value = values[element_index] = 0.0;

                        auto previous_channel = channel_positions[_channel_range.lower];
                        auto previous_intensity = gather_value( element_index, _channel_range.lower );

                        for( uint32_t channel_index = _channel_range.lower + 1; channel_index <= _channel_range.upper; ++channel_index )
                        {
                            const auto channel = channel_positions[channel_index];
                            const auto intensity = gather_value( element_index, channel_index );
                            value += ( channel - previous_channel ) * ( previous_intensity + intensity ) / 2.0;
                            previous_channel = channel;
                            previous_intensity = intensity;
                        }
                    } );
                }
                else if( _baseline_correction == BaselineCorrection::eMinimum )
                {
                    utility::iterate_parallel<uint32_t>( 0, dataset.element_count(), [&] ( uint32_t element_index )
                    {
                        auto& value = values[element_index] = 0.0;

                        auto previous_channel = channel_positions[_channel_range.lower];
                        auto previous_intensity = gather_value( element_index, _channel_range.lower );

                        auto minimum_intensity = previous_intensity;

                        for( uint32_t channel_index = _channel_range.lower + 1; channel_index <= _channel_range.upper; ++channel_index )
                        {
                            const auto channel = channel_positions[channel_index];
                            const auto intensity = gather_value( element_index, channel_index );
                            value += ( channel - previous_channel ) * ( previous_intensity + intensity ) / 2.0;
                            previous_channel = channel;
                            previous_intensity = intensity;

                            minimum_intensity = std::min( minimum_intensity, intensity );
                        }

                        value -= minimum_intensity * ( channel_positions[_channel_range.upper] - channel_positions[_channel_range.lower] );
                    } );
                }
                else if( _baseline_correction == BaselineCorrection::eLinear )
                {
                    utility::iterate_parallel<uint32_t>( 0, dataset.element_count(), [&] ( uint32_t element_index )
                    {
                        auto& value = values[element_index] = 0.0;

                        auto previous_channel = channel_positions[_channel_range.lower];
                        auto previous_intensity = gather_value( element_index, _channel_range.lower );

                        const auto first_channel = previous_channel;
                        const auto first_intensity = previous_intensity;

                        for( uint32_t channel_index = _channel_range.lower + 1; channel_index <= _channel_range.upper; ++channel_index )
                        {
                            const auto channel = channel_positions[channel_index];
                            const auto intensity = gather_value( element_index, channel_index );
                            value += ( channel - previous_channel ) * ( previous_intensity + intensity ) / 2.0;
                            previous_channel = channel;
                            previous_intensity = intensity;
                        }

                        value -= ( previous_channel - first_channel ) * ( previous_intensity + first_intensity ) / 2.0;
                    } );
                }
            }
        } );
    }

    return values;
}
