#include "feature.hpp"

// ----- Feature ----- //

Feature::Feature() : QObject {},
_identifier { "Feature", std::nullopt },
_values { [this]( Array<double>& values ) { this->compute_values( values ); } },
_extremes { [this]( Extremes& extremes ) { this->compute_extremes( extremes ); } },
_moments { [this]( Moments& moments ) { this->compute_moments( moments ); } },
_quantiles { [this]( Quantiles& quantiles ) { this->compute_quantiles( quantiles ); } },
_sorted_indices { [this]( Array<uint32_t>& sorted_indices ) { this->compute_sorted_indices( sorted_indices ); } }
{
    QObject::connect( &_identifier, &Override<QString>::value_changed, this, [this] { emit identifier_changed( _identifier.value() ); } );
    QObject::connect( &_values, &PromiseObject::invalidated, &_extremes, &Promise<Extremes>::invalidate );
    QObject::connect( &_values, &PromiseObject::invalidated, &_moments, &Promise<Moments>::invalidate );
    QObject::connect( &_values, &PromiseObject::invalidated, &_quantiles, &Promise<Quantiles>::invalidate );
    QObject::connect( &_values, &PromiseObject::invalidated, &_sorted_indices, &Promise<Array<uint32_t>>::invalidate );
}

const QString& Feature::identifier() const noexcept
{
    return _identifier.value();
}
void Feature::update_identifier( const QString& identifier )
{
    _identifier.update_override_value( identifier );
}

const Promise<Array<double>>& Feature::values() const noexcept
{
    return _values;
}
const Promise<Feature::Extremes>& Feature::extremes() const noexcept
{
    return _extremes;
}
const Promise<Feature::Moments>& Feature::moments() const noexcept
{
    return _moments;
}
const Promise<Feature::Quantiles>& Feature::quantiles() const noexcept
{
    return _quantiles;
}
const Promise<Array<uint32_t>>& Feature::sorted_indices() const noexcept
{
    return _sorted_indices;
}

void Feature::compute_extremes( Extremes& extremes ) const
{
    if( this->element_count() == 0 )
    {
        extremes.minimum = 0.0;
        extremes.maximum = 0.0;
    }
    else
    {
        extremes.minimum = std::numeric_limits<double>::max();
        extremes.maximum = std::numeric_limits<double>::lowest();

        const auto [values, _] = _values.await_value();
        for( const auto& value : values )
        {
            extremes.minimum = std::min( extremes.minimum, static_cast<double>( value ) );
            extremes.maximum = std::max( extremes.maximum, static_cast<double>( value ) );
        }
    }
}
void Feature::compute_moments( Moments& moments ) const
{
    if( this->element_count() == 0 )
    {
        moments.average = 0.0;
        moments.standard_deviation = 0.0;
    }
    else
    {
        const auto [values, _] = _values.await_value();
        moments.average = 0.0;
        moments.standard_deviation = 0.0;
        auto counter = uint32_t { 0 };

        for( const auto& typed_value : values )
        {
            const auto value = static_cast<double>( typed_value );

            ++counter;
            const auto delta = value - moments.average;
            moments.average += delta / counter;

            const auto delta2 = value - moments.average;
            moments.standard_deviation += delta * delta2;
        }
        moments.standard_deviation = std::sqrt( moments.standard_deviation / counter );
    }
}
void Feature::compute_quantiles( Quantiles& quantiles ) const
{
    if( this->element_count() == 0 )
    {
        quantiles.lower_quartile = 0.0;
        quantiles.median = 0.0;
        quantiles.upper_quartile = 0.0;
    }
    else
    {
        _values.request_value();
        _sorted_indices.request_value();

        const auto [values, values_lock] = _values.await_value();
        const auto [sorted_indices, sorted_indices_lock] = _sorted_indices.await_value();

        const auto retrieve_value = [&values, &sorted_indices] ( uint32_t index ) -> double
        {
            return static_cast<double>( values[sorted_indices[index]] );
        };
        const auto compute_quantile = [&values, &sorted_indices, &retrieve_value] ( double quantile ) -> double
        {
            const auto position = ( values.size() - 1 ) * quantile;
            const auto lower_index = static_cast<uint32_t>( std::floor( position ) );
            const auto upper_index = static_cast<uint32_t>( std::ceil( position ) );
            const auto fraction = position - lower_index;

            if( lower_index == upper_index )
            {
                return retrieve_value( lower_index );
            }
            else
            {
                const auto lower_value = retrieve_value( lower_index );
                const auto upper_value = retrieve_value( upper_index );
                return lower_value + fraction * ( upper_value - lower_value );
            }
        };

        quantiles.lower_quartile = compute_quantile( 0.25 );
        quantiles.median = compute_quantile( 0.5 );
        quantiles.upper_quartile = compute_quantile( 0.75 );
    }
}
void Feature::compute_sorted_indices( Array<uint32_t>& sorted_indices ) const
{
    if( sorted_indices.size() != this->element_count() )
    {
        sorted_indices = Array<uint32_t>::allocate( this->element_count() );
    }
    std::iota( sorted_indices.begin(), sorted_indices.end(), 0 );

    const auto [values, _] = _values.await_value();
    std::sort( std::execution::par, sorted_indices.begin(), sorted_indices.end(), [&values] ( uint32_t a, uint32_t b )
    {
        return values[a] < values[b];
    } );
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

void ElementFilterFeature::compute_values( Array<double>& values ) const
{
    if( values.size() != this->element_count() )
    {
        values = Array<double> { this->element_count(), 0.0 };
    }

    if( const auto feature = _feature.lock() )
    {
        const auto [feature_values, _] = feature->values().await_value();
        utility::parallel_for( 0u, this->element_count(), [&] ( uint32_t element_index )
        {
            values[element_index] = feature_values[_element_indices[element_index]];
        } );
    }
}