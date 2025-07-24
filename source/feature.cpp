#include "feature.hpp"

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
    QObject::connect( &_identifier, &Override<QString>::value_changed, this, [this] { emit identifier_changed( _identifier.value() ); } );
    QObject::connect( &_values, &ComputedObject::changed, this, &Feature::values_changed );
    QObject::connect( &_values, &ComputedObject::changed, &_extremes, &ComputedObject::invalidate );
    QObject::connect( &_values, &ComputedObject::changed, &_moments, &ComputedObject::invalidate );
    QObject::connect( &_values, &ComputedObject::changed, &_quantiles, &ComputedObject::invalidate );
    QObject::connect( &_values, &ComputedObject::changed, &_sorted_indices, &ComputedObject::invalidate );

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
    auto values = Array<double> { this->element_count(), 0.0 };

    if( const auto feature = _feature.lock() )
    {
        const auto& feature_values = feature->values();
        utility::iterate_parallel( 0u, this->element_count(), [&] ( uint32_t element_index )
        {
            values[element_index] = feature_values[_element_indices[element_index]];
        } );
    }

    return values;
}