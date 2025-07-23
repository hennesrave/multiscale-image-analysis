#include "boxplot.hpp"

#include "feature.hpp"
#include "segmentation.hpp"

// ----- GroupedBoxplot ----- //

GroupedBoxplot::GroupedBoxplot() : QObject {}
{
	_statistics.initialize( [this] ( Array<Statistics>& statistics ) { compute_statistics( statistics ); } );
	QObject::connect( this, &GroupedBoxplot::segmentation_changed, &_statistics, &Promise<Array<Statistics>>::invalidate );
	QObject::connect( this, &GroupedBoxplot::feature_changed, &_statistics, &Promise<Array<Statistics>>::invalidate );

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
			segmentation->element_indices().unsubscribe( this );
		}
		if( _segmentation = segmentation )
		{
			QObject::connect( segmentation.get(), &Segmentation::segment_count_changed, &_statistics, &Promise<Array<Statistics>>::invalidate );
			segmentation->element_indices().subscribe( this, [this] { _statistics.invalidate(); } );
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
			feature->values().unsubscribe( this );
		}

		if( _feature = feature )
		{
			feature->values().subscribe( this, [this]
			{
				_statistics.invalidate();
			} );
		}

		emit feature_changed( feature );
	}
}

const Promise<Array<GroupedBoxplot::Statistics>>& GroupedBoxplot::statistics() const
{
	return _statistics;
}

void GroupedBoxplot::compute_statistics( Array<Statistics>& statistics ) const
{
	if( const auto segmentation = _segmentation.lock() )
	{
		if( const auto feature = _feature.lock() )
		{
			statistics = Array<Statistics>::allocate( segmentation->segment_count() );

			feature->values().request_value();
			feature->sorted_indices().request_value();
			segmentation->element_indices().request_value();

			const auto [element_indices, element_indices_lock] = segmentation->element_indices().await_value();

			for( uint32_t segment_number = 0; segment_number < segmentation->segment_count(); ++segment_number )
			{
				auto element_filter_feature = ElementFilterFeature { feature, element_indices[segment_number] };

				const auto extremes = element_filter_feature.extremes().value();
				const auto moments = element_filter_feature.moments().value();
				const auto quantiles = element_filter_feature.quantiles().value();

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
		else
		{
			statistics = Array<Statistics> { 0, Statistics {} };
		}
	}
	else
	{
		statistics = Array<Statistics> { 0, Statistics {} };
	}
}