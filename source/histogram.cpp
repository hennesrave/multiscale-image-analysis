#include "histogram.hpp"

#include "feature.hpp"
#include "segmentation.hpp"

// ----- Histogram ----- //

Histogram::Histogram( uint32_t bincount ) : QObject {}, _bincount { bincount }
{
    _edges.initialize( std::bind( &Histogram::compute_edges, this ) );
    _counts.initialize( std::bind( &Histogram::compute_counts, this ) );

    QObject::connect( this, &Histogram::feature_changed, &_edges, &ComputedObject::invalidate );
    QObject::connect( this, &Histogram::feature_changed, &_counts, &ComputedObject::invalidate );

    QObject::connect( this, &Histogram::bincount_changed, &_edges, &ComputedObject::invalidate );
    QObject::connect( this, &Histogram::bincount_changed, &_counts, &ComputedObject::invalidate );

    QObject::connect( &_edges, &ComputedObject::changed, this, &Histogram::edges_changed );
    QObject::connect( &_counts, &ComputedObject::changed, this, &Histogram::counts_changed );
}

QSharedPointer<const Feature> Histogram::feature() const
{
    return _feature.lock();
}
void Histogram::update_feature( QSharedPointer<const Feature> feature )
{
    if( _feature != feature )
    {
        if( auto feature = _feature.lock() )
        {
            QObject::disconnect( feature.get(), nullptr, this, nullptr );
        }

        if( _feature = feature )
        {
            QObject::connect( feature.get(), &Feature::values_changed, &_edges, &ComputedObject::invalidate );
            QObject::connect( feature.get(), &Feature::values_changed, &_counts, &ComputedObject::invalidate );
            QObject::connect( feature.get(), &QObject::destroyed, this, [this] { emit feature_changed( nullptr ); } );
        }

        emit feature_changed( feature );
    }
}

uint32_t Histogram::bincount() const noexcept
{
    return _bincount;
}
void Histogram::update_bincount( uint32_t bincount )
{
    if( _bincount != bincount )
    {
        _bincount = bincount;
        emit bincount_changed( bincount );
    }
}

const Array<double>& Histogram::edges() const
{
    return *_edges;
}
const Array<uint32_t>& Histogram::counts() const
{
    return *_counts;
}

Array<double> Histogram::compute_edges() const
{
    auto edges = Array<double> { _bincount + 1, 0.0 };

    auto minimum = 0.0;
    auto maximum = 1.0;
    if( auto feature = _feature.lock() )
    {
        const auto& extremes = feature->extremes();
        minimum = extremes.minimum;
        maximum = extremes.maximum;
    }

    const auto binsize = ( maximum - minimum ) / _bincount;
    for( uint32_t i = 0; i <= _bincount; ++i )
    {
        const auto edge = std::clamp( minimum + i * binsize, minimum, maximum );
        edges[i] = edge;
    }

    return edges;
}
Array<uint32_t> Histogram::compute_counts() const
{
    auto counts = Array<uint32_t> { _bincount, 0 };

    if( auto feature = _feature.lock() )
    {
        const auto& edges = this->edges();
        const auto& values = feature->values();

        const auto minimum = edges[0];
        const auto binsize = edges[1] - edges[0];

        for( const auto value : values )
        {
            ++counts[std::clamp( static_cast<uint32_t>( ( value - minimum ) / binsize ), 0u, _bincount - 1 )];
        }
    }

    return counts;
}

// ----- StackedHistogram ----- //

StackedHistogram::StackedHistogram( uint32_t bincount ) : QObject {}, _bincount { bincount }
{
    _edges.initialize( std::bind( &StackedHistogram::compute_edges, this ) );
    _counts.initialize( std::bind( &StackedHistogram::compute_counts, this ) );

    QObject::connect( this, &StackedHistogram::feature_changed, &_edges, &ComputedObject::invalidate );
    QObject::connect( this, &StackedHistogram::feature_changed, &_counts, &ComputedObject::invalidate );

    QObject::connect( this, &StackedHistogram::bincount_changed, &_edges, &ComputedObject::invalidate );
    QObject::connect( this, &StackedHistogram::bincount_changed, &_counts, &ComputedObject::invalidate );

    QObject::connect( this, &StackedHistogram::segmentation_changed, &_counts, &ComputedObject::invalidate );

    QObject::connect( &_edges, &ComputedObject::changed, this, &StackedHistogram::edges_changed );
    QObject::connect( &_counts, &ComputedObject::changed, this, &StackedHistogram::counts_changed );
}

QSharedPointer<const Segmentation> StackedHistogram::segmentation() const
{
    return _segmentation.lock();
}
void StackedHistogram::update_segmentation( QSharedPointer<const Segmentation> segmentation )
{
    if( _segmentation != segmentation )
    {
        if( auto segmentation = _segmentation.lock() )
        {
            QObject::disconnect( segmentation.get(), nullptr, this, nullptr );
        }
        if( _segmentation = segmentation )
        {
            QObject::connect( segmentation.get(), &Segmentation::segment_numbers_changed, &_counts, &ComputedObject::invalidate );
            QObject::connect( segmentation.get(), &Segmentation::segment_count_changed, &_counts, &ComputedObject::invalidate );
        }
        emit segmentation_changed( segmentation );
    }
}

QSharedPointer<const Feature> StackedHistogram::feature() const
{
    return _feature.lock();
}
void StackedHistogram::update_feature( QSharedPointer<const Feature> feature )
{
    if( _feature != feature )
    {
        if( auto feature = _feature.lock() )
        {
            QObject::disconnect( feature.get(), nullptr, this, nullptr );
        }

        if( _feature = feature )
        {
            QObject::connect( feature.get(), &Feature::values_changed, &_edges, &ComputedObject::invalidate );
            QObject::connect( feature.get(), &Feature::values_changed, &_counts, &ComputedObject::invalidate );
            QObject::connect( feature.get(), &QObject::destroyed, this, [this] { emit feature_changed( nullptr ); } );
        }

        emit feature_changed( feature );
    }
}

uint32_t StackedHistogram::bincount() const noexcept
{
    return _bincount;
}
void StackedHistogram::update_bincount( uint32_t bincount )
{
    if( _bincount != bincount )
    {
        _bincount = bincount;
        emit bincount_changed( bincount );
    }
}

const Array<double>& StackedHistogram::edges() const
{
    return *_edges;
}
const Array<Array<uint32_t>>& StackedHistogram::counts() const
{
    return *_counts;
}

Array<double> StackedHistogram::compute_edges() const
{
    auto edges = Array<double> { _bincount + 1, 0.0 };

    auto minimum = 0.0;
    auto maximum = 1.0;
    if( auto feature = _feature.lock() )
    {
        const auto& extremes = feature->extremes();
        minimum = extremes.minimum;
        maximum = extremes.maximum;
    }

    const auto binsize = ( maximum - minimum ) / _bincount;
    for( uint32_t i = 0; i <= _bincount; ++i )
    {
        const auto edge = std::clamp( minimum + i * binsize, minimum, maximum );
        edges[i] = edge;
    }

    return edges;
}
Array<Array<uint32_t>> StackedHistogram::compute_counts() const
{
    auto counts = Array<Array<uint32_t>> {};

    if( const auto segmentation = _segmentation.lock() )
    {
        counts = Array<Array<uint32_t>> { segmentation->segment_count(), Array<uint32_t> { _bincount, 0 } };

        if( const auto feature = _feature.lock() )
        {
            const auto& edges = this->edges();
            const auto& values = feature->values();

            const auto minimum = edges[0];
            const auto binsize = edges[1] - edges[0];

            for( uint32_t element_index = 0; element_index < feature->element_count(); ++element_index )
            {
                const auto segment_number = segmentation->segment_number( element_index );
                const auto value = values[element_index];
                ++counts[segment_number][std::clamp( static_cast<uint32_t>( ( value - minimum ) / binsize ), 0u, _bincount - 1 )];

            }
        }
    }

    return counts;
}