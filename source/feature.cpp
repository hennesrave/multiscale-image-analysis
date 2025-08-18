#include "feature.hpp"

#include "dataset.hpp"

// ----- Feature ----- //

Feature::Feature()
    : QObject {}
    , _identifier { "Feature", std::nullopt }
    , _values { std::bind( &Feature::compute_values, this ) }
    , _extremes { std::bind( &Feature::compute_extremes, this ) }
    , _moments { std::bind( &Feature::compute_moments, this ) }
    , _quantiles { std::bind( &Feature::compute_quantiles, this ) }
    , _sorted_indices { std::bind( &Feature::compute_sorted_indices, this ) }
{
    QObject::connect( &_values, &ComputedObject::changed, &_extremes, &ComputedObject::invalidate );
    QObject::connect( &_values, &ComputedObject::changed, &_moments, &ComputedObject::invalidate );
    QObject::connect( &_values, &ComputedObject::changed, &_quantiles, &ComputedObject::invalidate );
    QObject::connect( &_values, &ComputedObject::changed, &_sorted_indices, &ComputedObject::invalidate );

    QObject::connect( &_identifier, &Override<QString>::value_changed, this, [this] { emit identifier_changed( _identifier.value() ); } );
    QObject::connect( &_values, &ComputedObject::changed, this, &Feature::values_changed );
    QObject::connect( &_extremes, &ComputedObject::changed, this, &Feature::extremes_changed );
    QObject::connect( &_moments, &ComputedObject::changed, this, &Feature::moments_changed );
    QObject::connect( &_quantiles, &ComputedObject::changed, this, &Feature::quantiles_changed );
    QObject::connect( &_sorted_indices, &ComputedObject::changed, this, &Feature::sorted_indices_changed );
}

const QString& Feature::identifier() const noexcept
{
    return _identifier.value();
}
void Feature::update_identifier( const QString& identifier )
{
    _identifier.update_override_value( identifier );
}
Override<QString>& Feature::override_identifier() noexcept
{
    return _identifier;
}

const Array<double>& Feature::values() const noexcept
{
    return *_values;
}
const Feature::Extremes& Feature::extremes() const noexcept
{
    return *_extremes;
}
const Feature::Moments& Feature::moments() const noexcept
{
    return *_moments;
}
const Feature::Quantiles& Feature::quantiles() const noexcept
{
    return *_quantiles;
}
const Array<uint32_t>& Feature::sorted_indices() const noexcept
{
    return *_sorted_indices;
}

Feature::Extremes Feature::compute_extremes() const
{
    Console::info( "Feature::compute_extremes" );
    auto extremes = Feature::Extremes {
        .minimum = 0.0,
        .maximum = 0.0
    };

    if( this->element_count() > 0 )
    {
        extremes.minimum = std::numeric_limits<double>::max();
        extremes.maximum = std::numeric_limits<double>::lowest();

        for( const auto value : this->values() )
        {
            extremes.minimum = std::min( extremes.minimum, static_cast<double>( value ) );
            extremes.maximum = std::max( extremes.maximum, static_cast<double>( value ) );
        }
    }

    return extremes;
}
Feature::Moments Feature::compute_moments() const
{
    Console::info( "Feature::compute_moments" );
    auto moments = Feature::Moments {
        .average = 0.0,
        .standard_deviation = 0.0
    };

    if( this->element_count() > 0 )
    {
        auto counter = uint32_t { 0 };
        for( const auto value : this->values() )
        {
            ++counter;
            const auto delta = value - moments.average;
            moments.average += delta / counter;

            const auto delta2 = value - moments.average;
            moments.standard_deviation += delta * delta2;
        }
        moments.standard_deviation = std::sqrt( moments.standard_deviation / counter );
    }

    return moments;
}
Feature::Quantiles Feature::compute_quantiles() const
{
    Console::info( "Feature::compute_quantiles" );
    auto quantiles = Feature::Quantiles {
        .lower_quartile = 0.0,
        .median = 0.0,
        .upper_quartile = 0.0
    };

    if( this->element_count() > 0 )
    {
        const auto& values = this->values();
        const auto& sorted_indices = this->sorted_indices();

        const auto compute_quantile = [&values, &sorted_indices] ( double quantile ) -> double
        {
            const auto position = ( values.size() - 1 ) * quantile;
            const auto lower_index = static_cast<uint32_t>( std::floor( position ) );
            const auto upper_index = static_cast<uint32_t>( std::ceil( position ) );
            const auto fraction = position - lower_index;

            if( lower_index == upper_index )
            {
                return values[sorted_indices[lower_index]];
            }
            else
            {
                const auto lower_value = values[sorted_indices[lower_index]];
                const auto upper_value = values[sorted_indices[upper_index]];
                return lower_value + fraction * ( upper_value - lower_value );
            }
        };

        quantiles.lower_quartile = compute_quantile( 0.25 );
        quantiles.median = compute_quantile( 0.5 );
        quantiles.upper_quartile = compute_quantile( 0.75 );
    }

    return quantiles;
}
Array<uint32_t> Feature::compute_sorted_indices() const
{
    Console::info( "Feature::compute_sorted_indices" );
    auto sorted_indices = Array<uint32_t>::allocate( this->element_count() );
    std::iota( sorted_indices.begin(), sorted_indices.end(), 0 );

    const auto& values = this->values();
    std::sort( std::execution::par, sorted_indices.begin(), sorted_indices.end(), [&] ( uint32_t a, uint32_t b )
    {
        return values[a] < values[b];
    } );

    return sorted_indices;
}

// ----- ElementFilterFeature ----- //

ElementFilterFeature::ElementFilterFeature( QSharedPointer<const Feature> feature, std::vector<uint32_t> element_indices )
    : Feature {}, _feature { feature }, _element_indices { std::move( element_indices ) }
{
}

uint32_t ElementFilterFeature::element_count() const noexcept
{
    return static_cast<uint32_t>( _element_indices.size() );
}

Array<double> ElementFilterFeature::compute_values() const
{
    Console::info( "ElementFilterFeature::compute_values" );
    auto values = Array<double> { this->element_count(), 0.0 };

    if( const auto feature = _feature.lock(); feature && feature->element_count() > _element_indices.back() )
    {
        const auto& feature_values = feature->values();
        utility::iterate_parallel( this->element_count(), [&] ( uint32_t element_index )
        {
            values[element_index] = feature_values[_element_indices[element_index]];
        } );
    }

    return values;
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

// ----- CombinationFeature ----- //

CombinationFeature::CombinationFeature( QSharedPointer<const Feature> first_feature, QSharedPointer<const Feature> second_feature, Operation operation ) : Feature {}
{
    this->update_first_feature( first_feature );
    this->update_second_feature( second_feature );
    this->update_operation( operation );

    QObject::connect( this, &CombinationFeature::first_feature_changed, &_values, &ComputedObject::invalidate );
    QObject::connect( this, &CombinationFeature::second_feature_changed, &_values, &ComputedObject::invalidate );
    QObject::connect( this, &CombinationFeature::operation_changed, &_values, &ComputedObject::invalidate );

    QObject::connect( this, &CombinationFeature::first_feature_changed, this, &CombinationFeature::update_identifier );
    QObject::connect( this, &CombinationFeature::second_feature_changed, this, &CombinationFeature::update_identifier );
    QObject::connect( this, &CombinationFeature::operation_changed, this, &CombinationFeature::update_identifier );
    this->update_identifier();
}

uint32_t CombinationFeature::element_count() const noexcept
{
    const auto first = _first_feature.lock();
    const auto second = _second_feature.lock();
    return first && second ? std::min( first->element_count(), second->element_count() ) : 0;
}

QSharedPointer<const Feature> CombinationFeature::first_feature() const
{
    return _first_feature.lock();
}
void CombinationFeature::update_first_feature( QSharedPointer<const Feature> feature )
{
    if( _first_feature != feature )
    {
        if( auto feature = _first_feature.lock() )
        {
            QObject::disconnect( feature.get(), nullptr, this, nullptr );
        }

        if( _first_feature = feature )
        {
            QObject::connect( feature.get(), &Feature::values_changed, &_values, &ComputedObject::invalidate );
            QObject::connect( feature.get(), &Feature::identifier_changed, this, &CombinationFeature::update_identifier );
            QObject::connect( feature.get(), &Feature::destroyed, [this] { emit first_feature_changed( nullptr ); } );
        }
        emit first_feature_changed( feature );
    }
}

QSharedPointer<const Feature> CombinationFeature::second_feature() const
{
    return _second_feature.lock();
}
void CombinationFeature::update_second_feature( QSharedPointer<const Feature> feature )
{
    if( _second_feature != feature )
    {
        if( auto feature = _second_feature.lock() )
        {
            QObject::disconnect( feature.get(), nullptr, this, nullptr );
        }

        if( _second_feature = feature )
        {
            QObject::connect( feature.get(), &Feature::values_changed, &_values, &ComputedObject::invalidate );
            QObject::connect( feature.get(), &Feature::identifier_changed, this, &CombinationFeature::update_identifier );
            QObject::connect( feature.get(), &Feature::destroyed, [this] { emit second_feature_changed( nullptr ); } );
        }
        emit second_feature_changed( feature );
    }
}

CombinationFeature::Operation CombinationFeature::operation() const noexcept
{
    return _operation;
}
void CombinationFeature::update_operation( Operation operation )
{
    if( _operation != operation )
    {
        _operation = operation;
        emit operation_changed( operation );
    }
}

void CombinationFeature::update_identifier()
{
    auto identifier = QString {};

    identifier += '(';
    if( auto feature = _first_feature.lock() ) identifier += feature->identifier();
    identifier += ')';

    if( _operation == Operation::eAddition ) identifier += " + ";
    else if( _operation == Operation::eSubtraction ) identifier += " - ";
    else if( _operation == Operation::eMultiplication ) identifier += " \u00d7 ";
    else if( _operation == Operation::eDivision ) identifier += " \u00f7 ";

    identifier += '(';
    if( auto feature = _second_feature.lock() ) identifier += feature->identifier();
    identifier += ')';

    _identifier.update_automatic_value( identifier );
}
Array<double> CombinationFeature::compute_values() const
{
    Console::info( "CombinationFeature::compute_values" );
    auto values = Array<double> { this->element_count(), 0.0 };

    const auto first = _first_feature.lock();
    const auto second = _second_feature.lock();

    if( first && second )
    {
        const auto& first_values = first->values();
        const auto& second_values = second->values();

        if( _operation == Operation::eAddition )
        {
            utility::iterate_parallel( this->element_count(), [&] ( uint32_t element_index )
            {
                values[element_index] = first_values[element_index] + second_values[element_index];
            } );
        }
        else if( _operation == Operation::eSubtraction )
        {
            utility::iterate_parallel( this->element_count(), [&] ( uint32_t element_index )
            {
                values[element_index] = first_values[element_index] - second_values[element_index];
            } );
        }
        else if( _operation == Operation::eMultiplication )
        {
            utility::iterate_parallel( this->element_count(), [&] ( uint32_t element_index )
            {
                values[element_index] = first_values[element_index] * second_values[element_index];
            } );
        }
        else if( _operation == Operation::eDivision )
        {
            auto contains_nan = false;

            utility::iterate_parallel( this->element_count(), [&] ( uint32_t element_index )
            {
                values[element_index] = first_values[element_index] / second_values[element_index];
                if( std::isnan( values[element_index] ) )
                {
                    contains_nan = true;
                }
            } );

            if( contains_nan )
            {
                Console::warning( "CombinationFeature::compute_values: Division by zero" );
            }
        }
        else
        {
            Console::error( "CombinationFeature::compute_values: Unsupported operation" );
        }
    }

    return values;
}