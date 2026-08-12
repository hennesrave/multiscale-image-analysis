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

    QString identifier() const noexcept;
    void update_identifier( const QString& identifier );

    void update_channel_identifiers( Array<QString> channel_identifiers );
    void update_spatial_metadata( std::unique_ptr<SpatialMetadata> spatial_metadata );

    virtual uint32_t element_count() const noexcept = 0;
    virtual uint32_t channel_count() const noexcept = 0;
    virtual Basetype basetype() const noexcept = 0;

    virtual double channel_position( uint32_t channel_index ) const = 0;
    const QString& channel_identifier( uint32_t channel_index ) const;
    const SpatialMetadata* spatial_metadata() const noexcept;

    virtual Array<double> element_intensities( uint32_t element_index ) const = 0;

    struct MissingValues
    {
        enum class Mode : uint32_t
        {
            eConstant,          // every missing value becomes `constant`
            eChannelMean,       // mean of the measured values of the same channel
            eChannelMedian,     // median of the measured values of the same channel
            eLocalMedian        // median of the measured neighbours, requires spatial metadata
        };
        enum class Fallback : uint32_t
        {
            eNone,              // leave the cell missing
            eConstant,
            eChannelMedian
        };

        Mode mode = Mode::eChannelMedian;

        double constant = 0.0;                          // eConstant, and eLocalMedian with Fallback::eConstant

        uint32_t radius = 1;                            // eLocalMedian: neighbourhood extent in cells
        uint32_t passes = 0;                            // eLocalMedian: 0 == until no further progress
        Fallback fallback = Fallback::eChannelMedian;   // eLocalMedian: cells no neighbour ever reaches
    };

    virtual void replace_missing_values( const MissingValues& settings ) = 0;
    virtual void apply_baseline_correction_minimum() = 0;
    virtual void apply_baseline_correction_linear() = 0;
    virtual void apply_derivative( uint32_t degree ) = 0;

    const std::optional<Array<QString>>& override_channel_identifiers() const noexcept;

    void visit( auto&& callable ) const;
    const Statistics& statistics() const noexcept;
    const Array<Statistics>& segmentation_statistics( QSharedPointer<const Segmentation> segmentation ) const;

signals:
    void identifier_changed( const QString& ) const;
    void intensities_changed() const;
    void spatial_metadata_changed() const;
    void channel_identifiers_changed() const;

    void statistics_changed() const;
    void segmentation_statistics_changed( QSharedPointer<const Segmentation> segmentation ) const;

protected:
    virtual Array<QString> compute_channel_identifiers() const;
    virtual Statistics compute_statistics() const = 0;
    virtual Array<Statistics> compute_segmentation_statistics( QSharedPointer<const Segmentation> segmentation ) const = 0;

    QString _identifier { "Dataset" };

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

    void replace_missing_values( const MissingValues& settings ) override
    {
        if constexpr( !std::is_floating_point_v<value_type> )
        {
            Console::info( "No missing values to replace: the dataset does not hold floating point values" );
        }
        else
        {
            const auto element_count = this->element_count();
            const auto channel_count = this->channel_count();
            const auto* const spatial_metadata = this->spatial_metadata();

            if( settings.mode == MissingValues::Mode::eLocalMedian )
            {
                if( !spatial_metadata )
                {
                    Console::error( "The local median requires a dataset with spatial metadata" );
                    return;
                }
                if( static_cast<uint64_t>( spatial_metadata->width ) * spatial_metadata->height != element_count )
                {
                    Console::error( "The spatial metadata does not match the number of elements" );
                    return;
                }
            }

            // Channels are independent, so every mode is computed one channel at
            // a time. Threads write to distinct addresses of the same column.
            auto replaced_count = std::atomic<uint64_t> { 0 };
            auto remaining_count = std::atomic<uint64_t> { 0 };

            utility::iterate_parallel( channel_count, [&] ( uint32_t channel_index )
            {
                auto* const channel_begin = _intensities.data() + channel_index;
                const auto stride = static_cast<size_t>( channel_count );

                // Gather the channel into a contiguous buffer: the neighbourhood
                // pass needs random access across elements of a single channel.
                auto values = std::vector<double>( element_count );
                auto missing = uint64_t { 0 };
                for( uint32_t element_index = 0; element_index < element_count; ++element_index )
                {
                    values[element_index] = static_cast<double>( channel_begin[element_index * stride] );
                    if( std::isnan( values[element_index] ) )
                    {
                        ++missing;
                    }
                }
                if( missing == 0 )
                {
                    return;
                }

                // Computed before anything is filled, so that replacements can
                // never be derived from other replacements.
                const auto scalar = TensorDataset::scalar_replacement( values, settings );

                if( settings.mode == MissingValues::Mode::eLocalMedian )
                {
                    TensorDataset::fill_local_median( values, *spatial_metadata, settings.radius, settings.passes );
                }

                auto replaced = uint64_t { 0 };
                auto remaining = uint64_t { 0 };
                for( uint32_t element_index = 0; element_index < element_count; ++element_index )
                {
                    if( !std::isnan( channel_begin[element_index * stride] ) )
                    {
                        continue;
                    }

                    auto value = values[element_index];
                    if( std::isnan( value ) )
                    {
                        // Not reached by the neighbourhood pass, or the whole
                        // channel is missing and no statistic exists.
                        if( !scalar.has_value() )
                        {
                            ++remaining;
                            continue;
                        }
                        value = *scalar;
                    }

                    channel_begin[element_index * stride] = static_cast<value_type>( value );
                    ++replaced;
                }

                replaced_count += replaced;
                remaining_count += remaining;
            } );

            Console::info( std::format( "Replaced {} missing value(s), {} left unchanged",
                           replaced_count.load(), remaining_count.load() ) );

            emit intensities_changed();
        }
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
    void apply_derivative( uint32_t degree ) override
    {
        for( uint32_t index = 0; index < degree; ++index )
        {
            Console::info( "Computing derivative..." );
            utility::iterate_parallel( this->element_count(), [this] ( uint32_t element_index )
            {
                auto element_intensities = _intensities.data() + element_index * this->channel_count();

                auto previous_value = element_intensities[0];
                const auto maximum_channel_index = this->channel_count() - 1;

                for( uint32_t channel_index = 0; channel_index < this->channel_count(); ++channel_index )
                {
                    const auto previous_channel_index   = ( channel_index == 0 )? 0 : channel_index - 1;
                    const auto next_channel_index       = ( channel_index == maximum_channel_index )? maximum_channel_index : channel_index + 1;

                    const auto previous_channel     = _channel_positions[previous_channel_index];
                    const auto next_channel         = _channel_positions[next_channel_index];

                    const auto next_value           = element_intensities[next_channel_index];
                    const auto current_value        = element_intensities[channel_index];

                    const auto value_difference     = next_value - previous_value;
                    const auto channel_difference   = next_channel - previous_channel;

                    const auto derivative = ( channel_difference < 1e-8 )? 0.0 : ( value_difference / channel_difference );
                    element_intensities[channel_index] = static_cast<value_type>( derivative );

                    previous_value = current_value;
                }
            } );
        }
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

    // Reorders the values it is given.
    static double median( std::vector<double>& values )
    {
        const auto middle = values.size() / 2;
        std::nth_element( values.begin(), values.begin() + middle, values.end() );
        const auto upper = values[middle];

        if( values.size() % 2 == 1 )
        {
            return upper;
        }
        const auto lower = *std::max_element( values.begin(), values.begin() + middle );
        return 0.5 * ( lower + upper );
    }

    // The value used for cells that the mode cannot resolve individually.
    // std::nullopt means "leave the cell missing".
    static std::optional<double> scalar_replacement( const std::vector<double>& values, const MissingValues& settings )
    {
        const auto mode = settings.mode;
        if( mode == MissingValues::Mode::eConstant )
        {
            return settings.constant;
        }
        if( mode == MissingValues::Mode::eLocalMedian )
        {
            if( settings.fallback == MissingValues::Fallback::eNone ) return std::nullopt;
            if( settings.fallback == MissingValues::Fallback::eConstant ) return settings.constant;
        }

        if( mode == MissingValues::Mode::eChannelMean )
        {
            auto sum = 0.0;
            auto count = uint64_t { 0 };
            for( const auto value : values ) if( !std::isnan( value ) )
            {
                sum += value;
                ++count;
            }
            return ( count > 0 ) ? std::optional<double> { sum / static_cast<double>( count ) } : std::nullopt;
        }

        auto measured = std::vector<double> {};
        measured.reserve( values.size() );
        for( const auto value : values ) if( !std::isnan( value ) )
        {
            measured.push_back( value );
        }
        return measured.empty() ? std::nullopt : std::optional<double> { TensorDataset::median( measured ) };
    }

    // Replaces every missing cell by the median of the measured cells in its
    // (2 * radius + 1) square neighbourhood, repeatedly, so that gaps wider
    // than the neighbourhood are closed from their edges inwards. Cells filled
    // during a pass only become available as neighbours in the next one, which
    // keeps the result independent of the traversal order. passes == 0 runs
    // until no further progress is made.
    static void fill_local_median( std::vector<double>& values, const SpatialMetadata& spatial_metadata, uint32_t radius, uint32_t passes )
    {
        const auto width = spatial_metadata.width;
        const auto height = spatial_metadata.height;
        if( width == 0 || height == 0 || radius == 0 )
        {
            return;
        }

        const auto maximum_passes = ( passes > 0 ) ? passes : ( width + height );
        const auto neighborhood_size = ( 2ull * radius + 1ull ) * ( 2ull * radius + 1ull );

        auto source = values;
        auto neighborhood = std::vector<double> {};
        neighborhood.reserve( static_cast<size_t>( neighborhood_size ) );

        auto previous_remaining = std::numeric_limits<uint64_t>::max();
        for( uint32_t pass = 0; pass < maximum_passes; ++pass )
        {
            auto remaining = uint64_t { 0 };
            for( uint32_t y = 0; y < height; ++y ) for( uint32_t x = 0; x < width; ++x )
            {
                const auto element_index = spatial_metadata.element_index( vec2<uint32_t> { x, y } );
                if( !std::isnan( source[element_index] ) )
                {
                    continue;
                }

                const auto minimum_x = ( x > radius ) ? x - radius : 0;
                const auto minimum_y = ( y > radius ) ? y - radius : 0;
                const auto maximum_x = std::min( x + radius, width - 1 );
                const auto maximum_y = std::min( y + radius, height - 1 );

                neighborhood.clear();
                for( auto neighbor_y = minimum_y; neighbor_y <= maximum_y; ++neighbor_y )
                {
                    for( auto neighbor_x = minimum_x; neighbor_x <= maximum_x; ++neighbor_x )
                    {
                        const auto neighbor_index = spatial_metadata.element_index( vec2<uint32_t> { neighbor_x, neighbor_y } );
                        if( !std::isnan( source[neighbor_index] ) )
                        {
                            neighborhood.push_back( source[neighbor_index] );
                        }
                    }
                }

                if( neighborhood.empty() )
                {
                    ++remaining;
                }
                else
                {
                    values[element_index] = TensorDataset::median( neighborhood );
                }
            }

            if( remaining == 0 || remaining == previous_remaining )
            {
                break;
            }
            previous_remaining = remaining;
            source = values;
        }
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
