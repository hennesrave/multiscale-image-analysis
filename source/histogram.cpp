#include "histogram.hpp"

#include "feature.hpp"
#include "segmentation.hpp"

// ----- Histogram ----- //

Histogram::Histogram( uint32_t bincount ) : QObject {}, _bincount { bincount }
{
    _edges.initialize( "Histogram::edges", [this] ( Array<double>& edges ) { compute_edges( edges ); } );
    _counts.initialize( "Histogram::counts", [this] ( Array<uint32_t>& counts ) { compute_counts( counts ); } );

    QObject::connect( this, &Histogram::feature_changed, &_edges, &Promise<Array<double>>::invalidate );
    QObject::connect( this, &Histogram::feature_changed, &_counts, &Promise<Array<uint32_t>>::invalidate );

    QObject::connect( this, &Histogram::bincount_changed, &_edges, &Promise<Array<double>>::invalidate );
    QObject::connect( this, &Histogram::bincount_changed, &_counts, &Promise<Array<uint32_t>>::invalidate );
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
            feature->values().unsubscribe( this );
        }

        if( _feature = feature )
        {
            feature->values().subscribe( this, [this]
            {
                _edges.invalidate();
                _counts.invalidate();
            } );
            QObject::connect( feature.get(), &QObject::destroyed, this, [this]
            {
                _edges.invalidate();
                _counts.invalidate();
            } );
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

const Promise<Array<double>>& Histogram::edges() const
{
    return _edges;
}
const Promise<Array<uint32_t>>& Histogram::counts() const
{
    return _counts;
}

void Histogram::compute_edges( Array<double>& edges ) const
{
    if( edges.size() != _bincount + 1 )
    {
        edges = Array<double>::allocate( _bincount + 1 );
    }

    auto minimum = 0.0;
    auto maximum = 1.0;

    if( auto feature = _feature.lock() )
    {
        const auto extremes = feature->extremes().value();
        minimum = extremes.minimum;
        maximum = extremes.maximum;
    }

    const auto binsize = ( maximum - minimum ) / _bincount;
    for( uint32_t i = 0; i <= _bincount; ++i )
    {
        const auto edge = std::clamp( minimum + i * binsize, minimum, maximum );
        edges[i] = edge;
    }
}
void Histogram::compute_counts( Array<uint32_t>& counts ) const
{
    counts = Array<uint32_t> { _bincount, 0 };

    if( auto feature = _feature.lock() )
    {
        _edges.request_value();
        feature->values().request_value();

        const auto [edges, edges_lock] = _edges.await_value();
        const auto [feature_values, feature_values_lock] = feature->values().await_value();

        for( const auto value : feature_values )
        {
            ++counts[std::clamp( static_cast<uint32_t>( ( value - edges[0] ) / ( edges[1] - edges[0] ) ), 0u, _bincount - 1 )];
        }
    }
}

// ----- StackedHistogram ----- //

StackedHistogram::StackedHistogram( uint32_t bincount ) : QObject {}, _bincount { bincount }
{
    _edges.initialize( "StackedHistogram::edges", [this] ( Array<double>& edges ) { compute_edges( edges ); } );
    _counts.initialize( "StackedHistogram::counts", [this] ( Array<Array<uint32_t>>& counts ) { compute_counts( counts ); } );

    QObject::connect( this, &StackedHistogram::segmentation_changed, &_counts, &Promise<Array<Array<uint32_t>>>::invalidate );

    QObject::connect( this, &StackedHistogram::feature_changed, &_edges, &Promise<Array<double>>::invalidate );
    QObject::connect( this, &StackedHistogram::feature_changed, &_counts, &Promise<Array<Array<uint32_t>>>::invalidate );

    QObject::connect( this, &StackedHistogram::bincount_changed, &_edges, &Promise<Array<double>>::invalidate );
    QObject::connect( this, &StackedHistogram::bincount_changed, &_counts, &Promise<Array<Array<uint32_t>>>::invalidate );
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
            QObject::connect( segmentation.get(), &Segmentation::values_changed, &_counts, &Promise<Array<Array<uint32_t>>>::invalidate );
            QObject::connect( segmentation.get(), &Segmentation::segment_count_changed, &_counts, &Promise<Array<Array<uint32_t>>>::invalidate );
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
            feature->values().unsubscribe( this );
        }

        if( _feature = feature )
        {
            feature->values().subscribe( this, [this]
            {
                _edges.invalidate();
                _counts.invalidate();
            } );
            QObject::connect( feature.get(), &QObject::destroyed, this, [this]
            {
                _edges.invalidate();
                _counts.invalidate();
            } );
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

const Promise<Array<double>>& StackedHistogram::edges() const
{
    return _edges;
}
const Promise<Array<Array<uint32_t>>>& StackedHistogram::counts() const
{
    return _counts;
}

void StackedHistogram::compute_edges( Array<double>& edges ) const
{
    if( edges.size() != _bincount + 1 )
    {
        edges = Array<double>::allocate( _bincount + 1 );
    }

    auto minimum = 0.0;
    auto maximum = 1.0;

    if( auto feature = _feature.lock() )
    {
        const auto extremes = feature->extremes().value();
        minimum = extremes.minimum;
        maximum = extremes.maximum;
    }

    const auto binsize = ( maximum - minimum ) / _bincount;
    for( uint32_t i = 0; i <= _bincount; ++i )
    {
        const auto edge = std::clamp( minimum + i * binsize, minimum, maximum );
        edges[i] = edge;
    }
}
void StackedHistogram::compute_counts( Array<Array<uint32_t>>& counts ) const
{
    if( const auto segmentation = _segmentation.lock() )
    {
        counts = Array<Array<uint32_t>> { segmentation->segment_count(), Array<uint32_t> { _bincount, 0 } };

        if( const auto feature = _feature.lock() )
        {
            _edges.request_value();
            feature->values().request_value();

            const auto [edges, edges_lock] = _edges.await_value();
            const auto [feature_values, feature_values_lock] = feature->values().await_value();

            for( uint32_t element_index = 0; element_index < feature->element_count(); ++element_index )
            {
                const auto segment_number = segmentation->segment_number( element_index );
                const auto value = feature_values[element_index];
                ++counts[segment_number][std::clamp( static_cast<uint32_t>( ( value - edges[0] ) / ( edges[1] - edges[0] ) ), 0u, _bincount - 1 )];

            }
        }
    }
    else
    {
        counts = Array<Array<uint32_t>> { 0, Array<uint32_t> { _bincount, 0 } };
    }
}