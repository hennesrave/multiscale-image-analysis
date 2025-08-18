#include "boxplot.hpp"

#include "feature.hpp"
#include "segmentation.hpp"

// ----- GroupedBoxplot ----- //

GroupedBoxplot::GroupedBoxplot() : QObject {}
{
    _statistics.initialize( std::bind( &GroupedBoxplot::compute_statistics, this ) );
    QObject::connect( this, &GroupedBoxplot::segmentation_changed, &_statistics, &ComputedObject::invalidate );
    QObject::connect( this, &GroupedBoxplot::feature_changed, &_statistics, &ComputedObject::invalidate );

    QObject::connect( &_statistics, &ComputedObject::changed, this, &GroupedBoxplot::statistics_changed );
}

QSharedPointer<const Segmentation> GroupedBoxplot::segmentation() const
{
    return _segmentation.lock();
}
void GroupedBoxplot::update_segmentation( QSharedPointer<const Segmentation> segmentation )
{
    if( _segmentation != segmentation )
    {
        if( auto segmentation = _segmentation.lock() )
        {
            QObject::disconnect( segmentation.get(), nullptr, this, nullptr );
        }
        if( _segmentation = segmentation )
        {
            QObject::connect( segmentation.get(), &Segmentation::segment_count_changed, &_statistics, &ComputedObject::invalidate );
            QObject::connect( segmentation.get(), &Segmentation::element_indices_changed, &_statistics, &ComputedObject::invalidate );
        }
        emit segmentation_changed( segmentation );
    }
}

QSharedPointer<const Feature> GroupedBoxplot::feature() const
{
    return _feature.lock();
}
void GroupedBoxplot::update_feature( QSharedPointer<const Feature> feature )
{
    if( _feature != feature )
    {
        if( auto feature = _feature.lock() )
        {
            QObject::disconnect( feature.get(), nullptr, this, nullptr );
        }

        if( _feature = feature )
        {
            QObject::connect( feature.get(), &Feature::values_changed, &_statistics, &ComputedObject::invalidate );
            QObject::connect( feature.get(), &QObject::destroyed, &_statistics, &ComputedObject::invalidate );
        }

        emit feature_changed( feature );
    }
}

const Array<GroupedBoxplot::Statistics>& GroupedBoxplot::statistics() const
{
    return *_statistics;
}

Array<GroupedBoxplot::Statistics> GroupedBoxplot::compute_statistics() const
{
    Console::info( "GroupedBoxplot::compute_statistics" );

    auto statistics = Array<Statistics> {};

    const auto segmentation = _segmentation.lock();
    const auto feature = _feature.lock();

    if( segmentation && feature )
    {
        statistics = Array<Statistics> { segmentation->segment_count(), Statistics {} };

        const auto& element_indices = segmentation->element_indices();
        for( uint32_t segment_number = 0; segment_number < segmentation->segment_count(); ++segment_number )
        {
            auto element_filter_feature = ElementFilterFeature { feature, element_indices[segment_number] };
            const auto& extremes = element_filter_feature.extremes();
            const auto& moments = element_filter_feature.moments();
            const auto& quantiles = element_filter_feature.quantiles();

            statistics[segment_number] = Statistics {
                extremes.minimum,
                extremes.maximum,
                moments.average,
                moments.standard_deviation,
                quantiles.lower_quartile,
                quantiles.upper_quartile,
                quantiles.median
            };
        }
    }

    return statistics;
}