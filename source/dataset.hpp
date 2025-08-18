#pragma once
#include "segmentation.hpp"
#include "utility.hpp"

#include <unordered_map>
#include <qobject.h>

// ----- Dataset ----- //

class Dataset : public QObject
{
    Q_OBJECT
public:
    enum class Basetype : uint32_t
    {
        eInt8, eInt16, eInt32,
        eUint8, eUint16, eUint32,
        eFloat, eDouble
    };

    struct SpatialMetadata
    {
        union
        {
            vec2<uint32_t> dimensions;
            struct
            {
                uint32_t x;
                uint32_t y;
            };
            struct
            {
                uint32_t width;
                uint32_t height;
            };
        };

        SpatialMetadata( uint32_t width = 0, uint32_t height = 0 );

        uint32_t element_index( vec2<uint32_t> coordinates ) const;
        vec2<uint32_t> coordinates( uint32_t element_index ) const;
    };

    struct Statistics
    {
        Array<double> channel_minimums;
        Array<double> channel_maximums;
        Array<double> channel_averages;

        double minimum = 0.0;
        double maximum = 0.0;
        double average = 0.0;
    };

    Dataset();

    void update_channel_identifiers( Array<QString> channel_identifiers );
    void update_spatial_metadata( std::unique_ptr<SpatialMetadata> spatial_metadata );

    virtual uint32_t element_count() const noexcept = 0;
    virtual uint32_t channel_count() const noexcept = 0;
    virtual Basetype basetype() const noexcept = 0;

    virtual double channel_position( uint32_t channel_index ) const = 0;
    const QString& channel_identifier( uint32_t channel_index ) const;
    const SpatialMetadata* spatial_metadata() const noexcept;

    virtual Array<double> element_intensities( uint32_t element_index ) const = 0;
    virtual void apply_baseline_correction_minimum() = 0;
    virtual void apply_baseline_correction_linear() = 0;

    const std::optional<Array<QString>>& override_channel_identifiers() const noexcept;

    void visit( auto&& callable ) const;
    const Statistics& statistics() const noexcept;
    const Array<Statistics>& segmentation_statistics( QSharedPointer<const Segmentation> segmentation ) const;

signals:
    void intensities_changed() const;
    void spatial_metadata_changed() const;
    void channel_identifiers_changed() const;

    void statistics_changed() const;
    void segmentation_statistics_changed( QSharedPointer<const Segmentation> segmentation ) const;

protected:
    virtual Array<QString> compute_channel_identifiers() const;
    virtual Statistics compute_statistics() const = 0;
    virtual Array<Statistics> compute_segmentation_statistics( QSharedPointer<const Segmentation> segmentation ) const = 0;

    Computed<Array<QString>> _computed_channel_identifiers;
    Override<int> _channel_identifier_precision { 2, std::nullopt };

    std::optional<Array<QString>> _override_channel_identifiers;
    std::unique_ptr<SpatialMetadata> _spatial_metadata;

    Computed<Statistics> _statistics;
    mutable std::unordered_map<const Segmentation*, std::unique_ptr<Computed<Array<Statistics>>>> _segmentation_statistics;
};

// ----- TensorDataset ----- //

template<class T> class TensorDataset : public Dataset
{
public:
    using value_type = T;

    TensorDataset( Matrix<value_type> intensities, Array<double> channel_positions ) : Dataset {}, _intensities { std::move( intensities ) }, _channel_positions { std::move( channel_positions ) }
    {
        auto stepsize = std::numeric_limits<double>::max();
        for( uint32_t channel_index = 0; channel_index < _channel_positions.size() - 1; ++channel_index )
        {
            const auto distance = _channel_positions[channel_index + 1] - _channel_positions[channel_index];
            stepsize = std::min( stepsize, distance );
        }
        _channel_identifier_precision.update_automatic_value( utility::stepsize_to_precision( stepsize ) + 1 );
    }

    const Matrix<value_type>& intensities() const noexcept
    {
        return _intensities;
    }
    const Array<double>& channel_positions() const noexcept
    {
        return _channel_positions;
    }

    // Dataset interface
    uint32_t element_count() const noexcept override
    {
        return static_cast<uint32_t>( _intensities.dimensions()[0] );
    }
    uint32_t channel_count() const noexcept override
    {
        return static_cast<uint32_t>( _channel_positions.size() );
    }
    Basetype basetype() const noexcept override
    {
        if constexpr( std::is_same_v<value_type, int8_t> )              return Basetype::eInt8;
        else if constexpr( std::is_same_v<value_type, int16_t> )        return Basetype::eInt16;
        else if constexpr( std::is_same_v<value_type, int32_t> )        return Basetype::eInt32;
        else if constexpr( std::is_same_v<value_type, uint8_t> )        return Basetype::eUint8;
        else if constexpr( std::is_same_v<value_type, uint16_t> )       return Basetype::eUint16;
        else if constexpr( std::is_same_v<value_type, uint32_t> )       return Basetype::eUint32;
        else if constexpr( std::is_same_v<value_type, float> )          return Basetype::eFloat;
        else if constexpr( std::is_same_v<value_type, double> )         return Basetype::eDouble;
        else static_assert( false, "Unsupported value type" );
    }

    Array<double> element_intensities( uint32_t element_index ) const override
    {
        auto element_intensities = Array<double>::allocate( this->channel_count() );
        for( uint32_t channel_index = 0; channel_index < this->channel_count(); ++channel_index )
        {
            element_intensities[channel_index] = static_cast<double>( _intensities.value( { element_index, channel_index } ) );
        }
        return element_intensities;
    }
    double channel_position( uint32_t channel_index ) const override
    {
        return _channel_positions[channel_index];
    }

    void apply_baseline_correction_minimum() override
    {
        utility::iterate_parallel( this->element_count(), [this] ( uint32_t element_index )
        {
            auto* element_intensities = _intensities.data() + element_index * this->channel_count();
            const auto minimum = *std::min_element( element_intensities, element_intensities + this->channel_count() );
            for( uint32_t channel_index = 0; channel_index < this->channel_count(); ++channel_index )
            {
                element_intensities[channel_index] -= minimum;
            }
        } );
        emit intensities_changed();
    }
    void apply_baseline_correction_linear() override
    {
        utility::iterate_parallel( this->element_count(), [this] ( uint32_t element_index )
        {
            auto element_intensities = _intensities.data() + element_index * this->channel_count();

            const auto first_channel = _channel_positions[0];
            const auto first_intensity = element_intensities[0];

            const auto last_channel = _channel_positions[this->channel_count() - 1];
            const auto last_intensity = element_intensities[this->channel_count() - 1];

            for( uint32_t channel_index = 0; channel_index < this->channel_count(); ++channel_index )
            {
                const auto t = ( _channel_positions[channel_index] - first_channel ) / ( last_channel - first_channel );
                const auto intensity_correction = first_intensity + t * ( last_intensity - first_intensity );
                element_intensities[channel_index] -= intensity_correction;
            }
        } );
        emit intensities_changed();
    }

private:
    Statistics compute_statistics() const override
    {
        auto statistics = Statistics {};

        statistics.channel_minimums = Array<double> { this->channel_count(), std::numeric_limits<double>::max() };
        statistics.channel_maximums = Array<double> { this->channel_count(), std::numeric_limits<double>::lowest() };
        statistics.channel_averages = Array<double> { this->channel_count(), 0.0 };

        const auto* value_pointer = _intensities.data();
        for( uint32_t element_index = 0; element_index < this->element_count(); ++element_index )
        {
            for( uint32_t channel_index = 0; channel_index < this->channel_count(); ++channel_index, ++value_pointer )
            {
                const auto value = static_cast<double>( *value_pointer );
                statistics.channel_minimums[channel_index] = std::min( statistics.channel_minimums[channel_index], value );
                statistics.channel_maximums[channel_index] = std::max( statistics.channel_maximums[channel_index], value );
                statistics.channel_averages[channel_index] += value;
            }
        }
        for( uint32_t channel_index = 0; channel_index < this->channel_count(); ++channel_index )
        {
            statistics.channel_averages[channel_index] /= this->element_count();
        }

        statistics.minimum = std::numeric_limits<double>::max();
        statistics.maximum = std::numeric_limits<double>::lowest();
        statistics.average = 0.0;

        for( uint32_t channel_index = 0; channel_index < this->channel_count(); ++channel_index )
        {
            statistics.minimum = std::min( statistics.minimum, statistics.channel_minimums[channel_index] );
            statistics.maximum = std::max( statistics.maximum, statistics.channel_maximums[channel_index] );
            statistics.average += statistics.channel_averages[channel_index];
        }
        statistics.average /= this->channel_count();

        return statistics;
    }
    Array<Statistics> compute_segmentation_statistics( QSharedPointer<const Segmentation> segmentation ) const override
    {
        auto segmentation_statistics = Array<Statistics> { segmentation->segment_count(), Statistics {} };
        for( auto& statistics : segmentation_statistics )
        {
            statistics.channel_minimums = Array<double> { this->channel_count(), std::numeric_limits<double>::max() };
            statistics.channel_maximums = Array<double> { this->channel_count(), std::numeric_limits<double>::lowest() };
            statistics.channel_averages = Array<double> { this->channel_count(), 0.0 };
        }

        const auto* value_pointer = _intensities.data();
        for( uint32_t element_index = 0; element_index < this->element_count(); ++element_index )
        {
            for( uint32_t channel_index = 0; channel_index < this->channel_count(); ++channel_index, ++value_pointer )
            {
                const auto value = static_cast<double>( *value_pointer );
                auto& statistics = segmentation_statistics[segmentation->segment_number( element_index )];
                statistics.channel_minimums[channel_index] = std::min( statistics.channel_minimums[channel_index], value );
                statistics.channel_maximums[channel_index] = std::max( statistics.channel_maximums[channel_index], value );
                statistics.channel_averages[channel_index] += value;
            }
        }

        for( uint32_t segment_index = 0; segment_index < segmentation->segment_count(); ++segment_index )
        {
            const auto segment = segmentation->segment( segment_index );
            auto& statistics = segmentation_statistics[segment_index];

            for( uint32_t channel_index = 0; channel_index < this->channel_count(); ++channel_index )
            {
                statistics.channel_averages[channel_index] /= segment->element_count();
            }
        }

        for( auto& statistics : segmentation_statistics )
        {
            statistics.minimum = std::numeric_limits<double>::max();
            statistics.maximum = std::numeric_limits<double>::lowest();
            statistics.average = 0.0;

            for( uint32_t channel_index = 0; channel_index < this->channel_count(); ++channel_index )
            {
                statistics.minimum = std::min( statistics.minimum, statistics.channel_minimums[channel_index] );
                statistics.maximum = std::max( statistics.maximum, statistics.channel_maximums[channel_index] );
                statistics.average += statistics.channel_averages[channel_index];
            }

            statistics.average /= this->channel_count();
        }

        return segmentation_statistics;
    }

    Matrix<value_type> _intensities;
    Array<double> _channel_positions;
};

void Dataset::visit( auto&& callable ) const
{
    if( auto dataset = dynamic_cast<const TensorDataset<int8_t>*>( this ) ) callable( *dataset );
    else if( auto dataset = dynamic_cast<const TensorDataset<int16_t>*>( this ) ) callable( *dataset );
    else if( auto dataset = dynamic_cast<const TensorDataset<int32_t>*>( this ) ) callable( *dataset );
    else if( auto dataset = dynamic_cast<const TensorDataset<uint8_t>*>( this ) ) callable( *dataset );
    else if( auto dataset = dynamic_cast<const TensorDataset<uint16_t>*>( this ) ) callable( *dataset );
    else if( auto dataset = dynamic_cast<const TensorDataset<uint32_t>*>( this ) ) callable( *dataset );
    else if( auto dataset = dynamic_cast<const TensorDataset<float>*>( this ) ) callable( *dataset );
    else if( auto dataset = dynamic_cast<const TensorDataset<double>*>( this ) ) callable( *dataset );
}
