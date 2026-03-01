#include "channel_glyphs_viewer.hpp"

#include "python.hpp"
#include "segment_selector.hpp"

#include <qactiongroup.h>
#include <qapplication.h>
#include <qcombobox.h>
#include <qdialog.h>
#include <qevent.h>
#include <qfiledialog.h>
#include <qformlayout.h>
#include <qlabel.h>
#include <qmenu.h>
#include <qmessagebox.h>
#include <qpainter.h>
#include <qpainterpath.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qtoolbutton.h>

#include <iostream>

namespace utility
{
    double compute_pearson_correlation(
        const Array<double>& vector_a, double vector_a_mean, double vector_a_std,
        const Array<double>& vector_b, double vector_b_mean, double vector_b_std
    )
    {
        if( vector_a_std == 0.0 || vector_b_std == 0.0 )
        {
            return 0.0;
        }

        auto correlation = 0.0;
        for( size_t i = 0; i < vector_a.size(); ++i )
        {
            correlation += ( ( vector_a[i] - vector_a_mean ) / vector_a_std ) * ( ( vector_b[i] - vector_b_mean ) / vector_b_std );
        }
        return correlation / static_cast<double>( vector_a.size() - 1 );
    }

    double compute_mutual_information(
        const Array<double>& vector_a, double vector_a_minimum, double vector_a_maximum,
        const Array<double>& vector_b, double vector_b_minimum, double vector_b_maximum
    )
    {
        constexpr auto bincount = size_t { 256 };

        const auto vector_a_range = vector_a_maximum - vector_a_minimum;
        const auto vector_b_range = vector_b_maximum - vector_b_minimum;

        if( vector_a_range == 0.0 || vector_b_range == 0.0 )
        {
            return 0.0;
        }

        auto combined_density   = Matrix<double> { { bincount, bincount }, 0.0 };
        auto density_a          = Array<double> { bincount, 0.0 };
        auto density_b          = Array<double> { bincount, 0.0 };

        for( size_t i = 0; i < vector_a.size(); ++i )
        {
            const auto normalized_a = ( vector_a[i] - vector_a_minimum ) / ( vector_a_maximum - vector_a_minimum );
            const auto normalized_b = ( vector_b[i] - vector_b_minimum ) / ( vector_b_maximum - vector_b_minimum );

            const auto index_a = std::clamp( static_cast<size_t>( normalized_a * bincount ), size_t { 0 }, bincount - 1 );
            const auto index_b = std::clamp( static_cast<size_t>( normalized_b * bincount ), size_t { 0 }, bincount - 1 );

            combined_density.value( { index_a, index_b } )  += 1.0;
            density_a.value( index_a )                      += 1.0;
            density_b.value( index_b )                      += 1.0;
        }

        const auto total_count = static_cast<double>( vector_a.size() );

        for( auto& value : combined_density )
        {
            value /= total_count;
        }
        for( auto& value : density_a )
        {
            value /= total_count;
        }
        for( auto& value : density_b )
        {
            value /= total_count;
        }

        auto mutual_information = 0.0;
        for( size_t index_a= 0; index_a < bincount; ++index_a )
        {
            for( size_t index_b = 0; index_b < bincount; ++index_b )
            {
                const auto probability_ab = combined_density.value( { index_a, index_b } );

                if( probability_ab > 0.0 )
                {
                    const auto probability_a = density_a.value( index_a );
                    const auto probability_b = density_b.value( index_b );

                    mutual_information += probability_ab * std::log( probability_ab / ( probability_a * probability_b ) );
                }
            }
        }

        return mutual_information;
    }

    double compute_euclidean_distance( const Array<double>& vector_a, const Array<double>& vector_b )
    {
        auto distance = 0.0;
        for( size_t i = 0; i < vector_a.size(); ++i )
        {
            const auto difference = vector_a[i] - vector_b[i];
            distance += difference * difference;
        }
        return std::sqrt( distance );
    }
    double compute_cosine_similarity( const Array<double>& vector_a, const Array<double>& vector_b )
    {
        auto dot_product = 0.0;
        auto magnitude_a = 0.0;
        auto magnitude_b = 0.0;
        for( size_t i = 0; i < vector_a.size(); ++i )
        {
            dot_product += vector_a[i] * vector_b[i];
            magnitude_a += vector_a[i] * vector_a[i];
            magnitude_b += vector_b[i] * vector_b[i];
        }

        if( magnitude_a == 0.0 || magnitude_b == 0.0 )
        {
            return 0.0;
        }

        return dot_product / ( std::sqrt( magnitude_a ) * std::sqrt( magnitude_b ) );
    }

    enum class Metric
    {
        ePearsonCorrelation,
        eMutualInformation,
        eEuclideanDistance,
        eCosineSimilarity
    };

    template<Metric metric, class value_type> void compute_distance_matrix(
        Matrix<double>& distance_matrix,
        const Matrix<value_type>& intensities
    )
    {
        const auto channel_count = distance_matrix.dimensions()[0];
        const auto element_count = intensities.dimensions()[0];

        auto vector_a = Array<double>::allocate( element_count );
        auto vector_b = Array<double>::allocate( element_count );

        auto channel_minimums = Array<double>::allocate( channel_count );
        auto channel_maximums = Array<double>::allocate( channel_count );
        auto channel_means    = Array<double>::allocate( channel_count );
        auto channel_stds     = Array<double>::allocate( channel_count );

        if constexpr( metric == Metric::ePearsonCorrelation )
        {
            Console::info( "Precomputing means and standard deviations for Pearson correlation..." );
            for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
            {
                auto mean = 0.0;
                for( uint32_t element_index = 0; element_index < element_count; ++element_index )
                {
                    const auto value = static_cast<double>( intensities.value( { element_index, channel_index } ) );
                    mean += value;

                    vector_a.value( element_index ) = value;
                }
                mean /= static_cast<double>( element_count );

                auto std = 0.0;
                for( uint32_t element_index = 0; element_index < element_count; ++element_index )
                {
                    const auto value        = vector_a.value( element_index );
                    const auto difference   = value - mean;
                    std += difference * difference;
                }
                std = std::sqrt( std / static_cast<double>( element_count - 1 ) );

                channel_means.value( channel_index )    = mean;
                channel_stds.value( channel_index )     = std;
            }
        }

        if constexpr( metric == Metric::eMutualInformation )
        {
            Console::info( "Precomputing minimums and maximums for mutual information..." );
            for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
            {
                auto minimum = std::numeric_limits<double>::max();
                auto maximum = std::numeric_limits<double>::lowest();

                for( uint32_t element_index = 0; element_index < element_count; ++element_index )
                {
                    const auto value = static_cast<double>( intensities.value( { element_index, channel_index } ) );
                    minimum = std::min( minimum, value );
                    maximum = std::max( maximum, value );
                }

                channel_minimums.value( channel_index ) = minimum;
                channel_maximums.value( channel_index ) = maximum;
            }
        }

        const auto computations_total   = ( ( channel_count * channel_count ) - channel_count ) / 2;
        auto computations_finished      = size_t { 0 };

        Console::info( "Computing distance matrix..." );
        for( uint32_t index_a = 0; index_a < channel_count; ++index_a )
        {
            distance_matrix.value( { index_a, index_a } ) = 0.0f;

            for( uint32_t element_index = 0; element_index < element_count; ++element_index )
            {
                vector_a.value( element_index ) = static_cast<double>( intensities.value( { element_index, index_a } ) );
            }

            for( uint32_t index_b = index_a + 1; index_b < channel_count; ++index_b )
            {
                const auto progress = static_cast<double>( computations_finished ) / static_cast<double>( computations_total );
                std::cout << "\r[Distance Matrix] Progress: " << std::fixed << std::setprecision( 2 ) << ( progress * 100.0 ) << "%... " << std::flush;

                for( uint32_t element_index = 0; element_index < element_count; ++element_index )
                {
                    vector_b.value( element_index ) = static_cast<double>( intensities.value( { element_index, index_b } ) );
                }

                auto distance = 0.0;
                if constexpr( metric == Metric::ePearsonCorrelation )
                {
                    distance = 1.0 - compute_pearson_correlation(
                        vector_a, channel_means.value( index_a ), channel_stds.value( index_a ),
                        vector_b, channel_means.value( index_b ), channel_stds.value( index_b )
                    );
                }
                else if constexpr( metric == Metric::eMutualInformation )
                {
                    distance = -compute_mutual_information(
                        vector_a, channel_minimums.value( index_a ), channel_maximums.value( index_a ),
                        vector_b, channel_minimums.value( index_b ), channel_maximums.value( index_b )
                    );
                }
                else if constexpr( metric == Metric::eEuclideanDistance )
                {
                    distance = compute_euclidean_distance( vector_a, vector_b );
                }
                else if constexpr( metric == Metric::eCosineSimilarity )
                {
                    distance = 1.0 - compute_cosine_similarity( vector_a, vector_b );
                }

                distance_matrix.value( { index_a, index_b } ) = distance;
                distance_matrix.value( { index_b, index_a } ) = distance;
                ++computations_finished;
            }
        }
        std::cout << "\r[Distance Matrix] Progress: 100.00%... " << std::endl;
    }

    template<Metric metric, class value_type> void compute_distance_matrix(
        Matrix<double>& distance_matrix,
        const Matrix<value_type>& intensities,
        const std::vector<uint32_t>& indices
    )
    {
        const auto channel_count = distance_matrix.dimensions()[0];
        const auto element_count = indices.size();

        if( element_count == intensities.dimensions()[0] )
        {
            compute_distance_matrix<metric>( distance_matrix, intensities );
            return;
        }

        auto vector_a = Array<double>::allocate( element_count );
        auto vector_b = Array<double>::allocate( element_count );

        auto channel_minimums = Array<double>::allocate( channel_count );
        auto channel_maximums = Array<double>::allocate( channel_count );
        auto channel_means    = Array<double>::allocate( channel_count );
        auto channel_stds     = Array<double>::allocate( channel_count );

        if constexpr( metric == Metric::ePearsonCorrelation )
        {
            Console::info( "Precomputing means and standard deviations for Pearson correlation..." );
            for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
            {
                auto mean = 0.0;
                for( uint32_t element_index = 0; element_index < element_count; ++element_index )
                {
                    const auto value = static_cast<double>( intensities.value( { indices[element_index], channel_index } ) );
                    mean += value;

                    vector_a.value( element_index ) = value;
                }
                mean /= static_cast<double>( element_count );

                auto std = 0.0;
                for( uint32_t element_index = 0; element_index < element_count; ++element_index )
                {
                    const auto value        = vector_a.value( element_index );
                    const auto difference   = value - mean;
                    std += difference * difference;
                }
                std = std::sqrt( std / static_cast<double>( element_count - 1 ) );

                channel_means.value( channel_index )    = mean;
                channel_stds.value( channel_index )     = std;
            }
        }

        if constexpr( metric == Metric::eMutualInformation )
        {
            Console::info( "Precomputing minimums and maximums for mutual information..." );
            for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
            {
                auto minimum = std::numeric_limits<double>::max();
                auto maximum = std::numeric_limits<double>::lowest();

                for( uint32_t element_index = 0; element_index < element_count; ++element_index )
                {
                    const auto value = static_cast<double>( intensities.value( { indices[element_index], channel_index } ) );
                    minimum = std::min( minimum, value );
                    maximum = std::max( maximum, value );
                }

                channel_minimums.value( channel_index ) = minimum;
                channel_maximums.value( channel_index ) = maximum;
            }
        }

        const auto computations_total   = ( ( channel_count * channel_count ) - channel_count ) / 2;
        auto computations_finished      = size_t { 0 };

        Console::info( "Computing distance matrix..." );
        for( uint32_t index_a = 0; index_a < channel_count; ++index_a )
        {
            distance_matrix.value( { index_a, index_a } ) = 0.0f;

            for( uint32_t element_index = 0; element_index < element_count; ++element_index )
            {
                vector_a.value( element_index ) = static_cast<double>( intensities.value( { indices[element_index], index_a } ) );
            }

            for( uint32_t index_b = index_a + 1; index_b < channel_count; ++index_b )
            {
                const auto progress = static_cast<double>( computations_finished ) / static_cast<double>( computations_total );
                std::cout << "\r[Distance Matrix] Progress: " << std::fixed << std::setprecision( 2 ) << ( progress * 100.0 ) << "%... " << std::flush;

                for( uint32_t element_index = 0; element_index < element_count; ++element_index )
                {
                    vector_b.value( element_index ) = static_cast<double>( intensities.value( { indices[element_index], index_b } ) );
                }

                auto distance = 0.0;
                if constexpr( metric == Metric::ePearsonCorrelation )
                {
                    distance = 1.0 - compute_pearson_correlation(
                        vector_a, channel_means.value( index_a ), channel_stds.value( index_a ),
                        vector_b, channel_means.value( index_b ), channel_stds.value( index_b )
                    );
                }
                else if constexpr( metric == Metric::eMutualInformation )
                {
                    distance = -compute_mutual_information(
                        vector_a, channel_minimums.value( index_a ), channel_maximums.value( index_a ),
                        vector_b, channel_minimums.value( index_b ), channel_maximums.value( index_b )
                    );
                }
                else if constexpr( metric == Metric::eEuclideanDistance )
                {
                    distance = compute_euclidean_distance( vector_a, vector_b );
                }
                else if constexpr( metric == Metric::eCosineSimilarity )
                {
                    distance = 1.0 - compute_cosine_similarity( vector_a, vector_b );
                }

                distance_matrix.value( { index_a, index_b } ) = distance;
                distance_matrix.value( { index_b, index_a } ) = distance;
                ++computations_finished;
            }
        }
        std::cout << "\r[Distance Matrix] Progress: 100.00%... " << std::endl;
    }
}

// ----- ChannelSegmentAbundances ----- //

ChannelSegmentAbundances::ChannelSegmentAbundances( QSharedPointer<const Dataset> dataset, QSharedPointer<const Segmentation> segmentation )
    : QObject {}, _dataset { dataset }, _segmentation { segmentation }
{
    _abundances.initialize( std::bind( &ChannelSegmentAbundances::compute_abundances, this ) );

    QObject::connect( dataset.get(), &Dataset::intensities_changed, &_abundances, &ComputedObject::invalidate );

    QObject::connect( segmentation.get(), &Segmentation::segment_count_changed, &_abundances, &ComputedObject::invalidate );
    QObject::connect( segmentation.get(), &Segmentation::segment_numbers_changed, &_abundances, &ComputedObject::invalidate );

    QObject::connect( &_abundances, &ComputedObject::changed, this, &ChannelSegmentAbundances::abundances_changed );
}

QSharedPointer<const Dataset> ChannelSegmentAbundances::dataset() const
{
    return _dataset.lock();
}
QSharedPointer<const Segmentation> ChannelSegmentAbundances::segmentation() const
{
    return _segmentation.lock();
}

const Array<Array<double>>& ChannelSegmentAbundances::abundances() const
{
    return _abundances.value();
}

Array<Array<double>> ChannelSegmentAbundances::compute_abundances()
{
    const auto dataset      = this->dataset();
    const auto segmentation = this->segmentation();

    if( !dataset || !segmentation )
    {
        return Array<Array<double>> {};
    }

    const auto segment_count    = segmentation->segment_count();
    auto abundances             = Array<Array<double>> {
        dataset->channel_count(),
        Array<double> { segment_count, 0.0 }
    };

    Console::info( "Computing channel-segment abundances..." );
    dataset->visit( [&] ( const auto& dataset )
    {
        const auto& intensities = dataset.intensities();
        for( uint32_t element_index = 0; element_index < dataset.element_count(); ++element_index )
        {
            const auto segment_number = segmentation->segment_number( element_index );
            for( uint32_t channel_index = 0; channel_index < dataset.channel_count(); ++channel_index )
            {
                const auto intensity = intensities.value( { element_index, channel_index } );
                abundances[channel_index][segment_number] += static_cast<double>( intensity );
            }
        }
    } );

    for( uint32_t channel_index = 0; channel_index < dataset->channel_count(); ++channel_index )
    {
        auto& channel_abundances = abundances[channel_index];

        auto total = 0.0;
        for( uint32_t segment_number = 0; segment_number < segment_count; ++segment_number )
        {
            total += channel_abundances[segment_number];
        }

        if( total > 0.0 )
        {
            for( auto& abundance : channel_abundances )
            {
                abundance /= total;
            }
        }
    }

    return abundances;
}

// ----- ChannelGlyphsViewer ----- //

ChannelGlyphsViewer::ChannelGlyphsViewer( Database& database )
    : QWidget {}
    , _database { database }
    , _channel_segment_abundances { database.dataset(), database.segmentation() }
    , _colormap { &ColormapTemplate::turbo }
{
    this->setFocusPolicy( Qt::WheelFocus );
    this->setMouseTracking( true );

    QObject::connect( this, &ChannelGlyphsViewer::viewport_changed, this, &ChannelGlyphsViewer::update_glyph_values );
    QObject::connect( this, &ChannelGlyphsViewer::normalization_changed, this, &ChannelGlyphsViewer::update_glyph_values );
    QObject::connect( this, &ChannelGlyphsViewer::positioning_changed, this, &ChannelGlyphsViewer::update_glyph_layout );
    QObject::connect( this, &ChannelGlyphsViewer::abundances_changed, this, qOverload<>( &QWidget::update ) );

    const auto features = _database.features();
    QObject::connect( features.get(), &CollectionObject::object_appended, this, qOverload<>( &QWidget::update ) );
    QObject::connect( features.get(), &CollectionObject::object_removed, this, qOverload<>( &QWidget::update ) );

    QObject::connect( &_database, &Database::highlighted_channel_index_changed, this, qOverload<>( &QWidget::update ) );

    QObject::connect( &_channel_segment_abundances, &ChannelSegmentAbundances::abundances_changed, this, qOverload<>( &QWidget::update ) );

    const auto dataset          = _database.dataset();
    const auto spatial_metadata = dataset->spatial_metadata();
    this->update_viewport( Viewport { vec2<uint32_t> { 0, 0 }, spatial_metadata->dimensions } );
}

ChannelGlyphsViewer::Viewport ChannelGlyphsViewer::viewport() const noexcept
{
    return _viewport;
}
void ChannelGlyphsViewer::update_viewport( Viewport viewport )
{
    if( _viewport.offset == viewport.offset && _viewport.extent == viewport.extent )
    {
        return;
    }

    const auto spatial_metadata = _database.dataset()->spatial_metadata();
    if( viewport.offset.x + viewport.extent.x > spatial_metadata->dimensions.x ||
        viewport.offset.y + viewport.extent.y > spatial_metadata->dimensions.y )
    {
        Console::warning( "Invalid viewport: offset + extent exceeds dataset dimensions." );
        return;
    }

    _viewport = viewport;
    emit viewport_changed( _viewport );
}

ChannelGlyphsViewer::Normalization ChannelGlyphsViewer::normalization() const noexcept
{
    return _normalization;
}
void ChannelGlyphsViewer::update_normalization( Normalization normalization )
{
    if( _normalization != normalization )
    {
        _normalization = normalization;
        emit normalization_changed( _normalization );
    }
}

ChannelGlyphsViewer::Positioning ChannelGlyphsViewer::positioning() const noexcept
{
    return _positioning;
}
void ChannelGlyphsViewer::update_positioning( Positioning positioning )
{
    if( _positioning != positioning )
    {
        _positioning = positioning;
        emit positioning_changed( _positioning );
    }
}

ChannelGlyphsViewer::Abundances ChannelGlyphsViewer::abundances() const noexcept
{
    return _abundances;
}
void ChannelGlyphsViewer::update_abundances( Abundances abundances )
{
    if( _abundances != abundances )
    {
        _abundances = abundances;
        emit abundances_changed( _abundances );
    }
}

void ChannelGlyphsViewer::resizeEvent( QResizeEvent* event )
{
    QWidget::resizeEvent( event );
    this->reset_canvas_rectangle();
}

void ChannelGlyphsViewer::paintEvent( QPaintEvent* event )
{
    auto painter = QPainter { this };
    painter.setRenderHint( QPainter::Antialiasing, true );

    painter.drawImage( _canvas_rectangle, _canvas );

    if( _abundances == Abundances::eEnabled || _abundances == Abundances::eSegmentsOnly )
    {
        const auto segmentation = _database.segmentation();
        const auto& abundances  = _channel_segment_abundances.abundances();

        auto segment_colors = Array<QColor> { segmentation->segment_count(), QColor {} };
        segment_colors[0] = QColor { 230, 230, 230, 255 };
        for( uint32_t segment_number = 1; segment_number < segmentation->segment_count(); ++segment_number )
        {
            segment_colors[segment_number] = segmentation->segment( segment_number )->color().qcolor();
        }

        auto outer_rectangle    = this->glyph_rectangle( 0 );
        const auto extent       = std::min( outer_rectangle.width(), outer_rectangle.height() );
        outer_rectangle.setWidth( extent );
        outer_rectangle.setHeight( extent );
        outer_rectangle.adjust( 3.0, 3.0, -3.0, -3.0 );

        auto inner_rectangle = outer_rectangle;
        inner_rectangle.setWidth( extent * 0.3 );
        inner_rectangle.setHeight( extent * 0.3 );

        for( uint32_t channel_index = 0; channel_index < abundances.size(); ++channel_index )
        {
            auto channel_abundances = abundances[channel_index];

            if( _abundances == Abundances::eSegmentsOnly )
            {
                channel_abundances[0] = 0.0;

                auto total = 0.0;
                for( uint32_t segment_number = 1; segment_number < segmentation->segment_count(); ++segment_number )
                {
                    total += channel_abundances[segment_number];
                }

                if( total > 0.0 )
                {
                    for( uint32_t segment_number = 1; segment_number < segmentation->segment_count(); ++segment_number )
                    {
                        channel_abundances[segment_number] /= total;
                    }
                }
            }

            const auto glyph_rectangle = this->glyph_rectangle( channel_index );
            outer_rectangle.moveCenter( glyph_rectangle.center() );
            inner_rectangle.moveCenter( glyph_rectangle.center() );

            painter.setPen( Qt::NoPen );

            auto start_angle = 0.0;
            for( uint32_t segment_number = 0; segment_number < channel_abundances.size(); ++segment_number )
            {
                const auto abundance = channel_abundances[segment_number];
                if( abundance > 0.0 )
                {
                    const auto span_angle = abundance * 360.0;

                    auto painter_path = QPainterPath {};
                    painter_path.arcMoveTo( outer_rectangle, start_angle );
                    painter_path.arcTo( outer_rectangle, start_angle, span_angle );

                    painter_path.arcTo( inner_rectangle, start_angle + span_angle, -span_angle );
                    painter_path.closeSubpath();

                    painter.setBrush( segment_colors[segment_number] );
                    painter.drawPath( painter_path );

                    start_angle += span_angle;
                }
            }

            painter.setPen( Qt::black );
            painter.setBrush( Qt::NoBrush );
            painter.drawEllipse( outer_rectangle );
            painter.drawEllipse( inner_rectangle );
        }
    }

    // Render features
    for( const auto object : *_database.features() ) if( auto feature = object.objectCast<DatasetChannelsFeature>() )
    {
        const auto channel_range = feature->channel_range();
        if( channel_range.x != channel_range.y )
        {
            continue;
        }

        painter.setBrush( Qt::NoBrush );
        painter.setPen( QPen { QColor { 255, 255, 255, 255 }, 1.0 } );
        painter.setCompositionMode( QPainter::CompositionMode_Difference );

        const auto rectangle = this->glyph_rectangle( channel_range.x );
        painter.drawRect( rectangle );

        painter.setCompositionMode( QPainter::CompositionMode_SourceOver );
    }

    // Render highlighted channel
    const auto highlighted_channel_index = _database.highlighted_channel_index();
    if( highlighted_channel_index.has_value() )
    {
        const auto channel_index = highlighted_channel_index.value();

        painter.setBrush( Qt::NoBrush );
        painter.setPen( QPen { QColor { 255, 255, 255, 100 }, 1.0, Qt::DashLine } );
        painter.setCompositionMode( QPainter::CompositionMode_Difference );

        const auto glyph_rectangle = this->glyph_rectangle( channel_index );
        painter.drawRect( glyph_rectangle );

        painter.setCompositionMode( QPainter::CompositionMode_SourceOver );
    }

    // Render border
    painter.setPen( Qt::black );
    painter.setBrush( Qt::NoBrush );
    painter.drawRect( _canvas_rectangle );

    // Render channel information
    if( highlighted_channel_index.has_value() )
    {
        const auto dataset              = _database.dataset();
        const auto channel_index        = highlighted_channel_index.value();
        const auto channel_identifier   = dataset->channel_identifier( channel_index );
        const auto string               = "Channel " + channel_identifier;

        auto string_rectangle = painter.fontMetrics().boundingRect( QRect {}, Qt::TextSingleLine, string ).toRectF();
        string_rectangle.moveTopLeft( QPointF { 10.0, 10.0 } );

        auto content_rectangle = string_rectangle.marginsAdded( QMarginsF { 5.0, 5.0, 5.0, 5.0 } );

        painter.setPen( Qt::NoPen );
        painter.setBrush( QBrush { QColor { 255, 255, 255, 200 } } );
        painter.drawRoundedRect( content_rectangle, 5.0, 5.0 );

        painter.setPen( Qt::black );
        painter.drawText( string_rectangle, Qt::AlignCenter, string );
    }
}
void ChannelGlyphsViewer::mouseMoveEvent( QMouseEvent* event )
{
    if( event->buttons() == Qt::NoButton )
    {
        const auto cursor = event->position();

        struct
        {
            std::optional<uint32_t> channel_index;
            double distance = std::numeric_limits<double>::max();
        } closest;

        for( uint32_t channel_index = 0; channel_index < _glyphs.size(); ++channel_index )
        {
            const auto glyph_rectangle = this->glyph_rectangle( channel_index );
            if( glyph_rectangle.contains( cursor ) )
            {
                const auto center = glyph_rectangle.center();
                const auto distance = QLineF { cursor, center }.length();
                if( distance < closest.distance )
                {
                    closest.channel_index = channel_index;
                    closest.distance = distance;
                }
            }
        }

        _database.update_highlighted_channel_index( closest.channel_index );
    }
}
void ChannelGlyphsViewer::mousePressEvent( QMouseEvent* event )
{
    if( event->button() == Qt::LeftButton )
    {
        const auto highlighted_channel_index = _database.highlighted_channel_index();
        if( highlighted_channel_index.has_value() )
        {
            const auto channel_index    = highlighted_channel_index.value();
            const auto feature          = QSharedPointer<DatasetChannelsFeature> { new DatasetChannelsFeature {
                _database.dataset(),
                Range<uint32_t> { channel_index, channel_index },
                DatasetChannelsFeature::Reduction::eAccumulate,
                DatasetChannelsFeature::BaselineCorrection::eNone
            } };
            _database.features()->append( feature );
        }
    }
    else if( event->button() == Qt::RightButton )
    {
        auto context_menu = QMenu {};

        // Viewport
        auto viewport_menu = context_menu.addMenu( "Viewport" );
        viewport_menu->addAction( "Entire Dataset", [this]
        {
            const auto spatial_metadata = this->_database.dataset()->spatial_metadata();
            this->update_viewport( Viewport { vec2<uint32_t> { 0, 0 }, spatial_metadata->dimensions } );
        } );

        const auto segmentation         = _database.segmentation();
        const auto& element_indices     = segmentation->element_indices();
        const auto spatial_metadata     = _database.dataset()->spatial_metadata();

        for( uint32_t segment_number = 1; segment_number < segmentation->segment_count(); ++segment_number )
        {
            const auto segment = segmentation->segment( segment_number );

            auto pixmap = QPixmap { 16, 16 };
            pixmap.fill( segment->color().qcolor() );

            auto action = viewport_menu->addAction( QIcon { pixmap }, segment->identifier(), [this, segment_number, spatial_metadata, &element_indices]
            {
                const auto& indices = element_indices.value( segment_number );
                if( indices.empty() )
                {
                    QMessageBox::warning( this, "", "The selected segment is empty." );
                    return;
                }

                auto minx = std::numeric_limits<uint32_t>::max();
                auto miny = std::numeric_limits<uint32_t>::max();
                auto maxx = std::numeric_limits<uint32_t>::min();
                auto maxy = std::numeric_limits<uint32_t>::min();

                for( const auto index : indices )
                {
                    const auto coordinates = spatial_metadata->coordinates( index );
                    minx = std::min( minx, coordinates.x );
                    miny = std::min( miny, coordinates.y );
                    maxx = std::max( maxx, coordinates.x );
                    maxy = std::max( maxy, coordinates.y );
                }

                this->update_viewport( Viewport {
                    vec2<uint32_t> { minx, miny },
                    vec2<uint32_t> { maxx - minx + 1, maxy - miny + 1 }
                } );
            } );
        }

        // Normalization
        auto normalization_menu = context_menu.addMenu( "Normalization" );
        auto normalization_action_group = new QActionGroup { normalization_menu };
        normalization_action_group->setExclusive( true );

        const auto normalization_options = std::vector<std::pair<const char*, Normalization>> {
            { "Global", Normalization::eGlobal },
            { "Glyph", Normalization::eGlyph },
        };

        for( const auto& [label, normalization] : normalization_options )
        {
            const auto action = normalization_menu->addAction( label, [this, normalization]
            {
                this->update_normalization( normalization );
            } );

            action->setCheckable( true );
            action->setChecked( _normalization == normalization );
            normalization_action_group->addAction( action );
        }

        // Colormap
        auto colormap_menu = context_menu.addMenu( "Colormap" );

        for( const auto& [identifier, colormap_template] : ColormapTemplate::registry )
        {
            colormap_menu->addAction( identifier, [this, &colormap_template]
            {
                _colormap = &colormap_template;
                this->update_glyph_images();
            } );
        }

        // Positioning
        auto positioning_menu = context_menu.addMenu( "Positioning" );
        auto positioning_action_group = new QActionGroup { positioning_menu };
        positioning_action_group->setExclusive( true );

        const auto positioning_options = std::vector<std::pair<const char*, Positioning>> {
            { "Linear", Positioning::eLinear },
            { "Embedding", Positioning::eEmbedding },
            { "Gridified", Positioning::eGridified }
        };

        for( const auto& [label, positioning] : positioning_options )
        {
            const auto action = positioning_menu->addAction( label, [this, positioning]
            {
                this->update_positioning( positioning );
            } );
            action->setCheckable( true );
            action->setChecked( _positioning == positioning );

            positioning_action_group->addAction( action );
        }

        positioning_menu->addSeparator();

        auto distance_matrix_menu = positioning_menu->addMenu( "Distance Matrix" );
        distance_matrix_menu->addAction( "Create", this, &ChannelGlyphsViewer::on_create_distance_matrix );
        distance_matrix_menu->addAction( "Export", [this] { this->on_export_distance_matrix(); } );
        distance_matrix_menu->addAction( "Import", this, &ChannelGlyphsViewer::on_import_distance_matrix );

        auto embedding_menu = positioning_menu->addMenu( "Embedding" );
        embedding_menu->addAction( "Create", this, &ChannelGlyphsViewer::on_create_embedding );
        embedding_menu->addAction( "Export", [this] { this->on_export_embedding(); } );
        embedding_menu->addAction( "Import", this, &ChannelGlyphsViewer::on_import_embedding );

        // Abundances
        auto abundances_menu = context_menu.addMenu( "Abundances" );
        auto abundances_action_group = new QActionGroup { abundances_menu };
        abundances_action_group->setExclusive( true );

        const auto abundances_options = std::vector<std::pair<const char*, Abundances>> {
            { "Disabled", Abundances::eDisabled },
            { "Enabled", Abundances::eEnabled },
            { "Segments Only", Abundances::eSegmentsOnly }
        };

        for( const auto& [label, abundances] : abundances_options )
        {
            const auto action = abundances_menu->addAction( label, [this, abundances]
            {
                this->update_abundances( abundances );
            } );
            action->setCheckable( true );
            action->setChecked( _abundances == abundances );

            abundances_action_group->addAction( action );
        }

        context_menu.addAction( "Reset View", this, &ChannelGlyphsViewer::reset_canvas_rectangle );

        context_menu.exec( event->globalPosition().toPoint() );
    }
}

void ChannelGlyphsViewer::leaveEvent( QEvent* event )
{
    _database.update_highlighted_channel_index( std::nullopt );
}
void ChannelGlyphsViewer::wheelEvent( QWheelEvent* event )
{
    const auto cursor = event->position();
    const auto x { ( cursor.x() - _canvas_rectangle.left() ) / _canvas_rectangle.width() };
    const auto y { ( cursor.y() - _canvas_rectangle.top() ) / _canvas_rectangle.height() };

    const double scaling = ( event->angleDelta().y() > 0 ) ? 1.1 : ( 1.0 / 1.1 );
    _canvas_rectangle.setSize( _canvas_rectangle.size() * scaling );

    const auto newx = std::lerp( _canvas_rectangle.left(), _canvas_rectangle.right(), x );
    const auto newy = std::lerp( _canvas_rectangle.top(), _canvas_rectangle.bottom(), y );

    _canvas_rectangle.translate( cursor.x() - newx, cursor.y() - newy );

    this->update();
}

void ChannelGlyphsViewer::keyPressEvent( QKeyEvent* event )
{
    const auto dataset = _database.dataset();

    if( event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace )
    {
        const auto highlighted_channel_index = _database.highlighted_channel_index();
        if( highlighted_channel_index.has_value() )
        {
            const auto channel_index = highlighted_channel_index.value();

            for( const auto object : *_database.features() ) if( auto feature = object.objectCast<DatasetChannelsFeature>() )
            {
                const auto channel_range = feature->channel_range();
                if( channel_range.x == channel_index && channel_range.y == channel_index )
                {
                    _database.features()->remove( feature );
                    break;
                }
            }
        }
    }
}

void ChannelGlyphsViewer::on_create_distance_matrix()
{
    const auto segmentation = _database.segmentation();
    const auto dataset      = _database.dataset();

    auto segment_selector = new SegmentSelector { segmentation };

    auto metric_combobox = new QComboBox {};
    metric_combobox->addItem( "Pearson Correlation" );
    metric_combobox->addItem( "Mutual Information" );
    // metric_combobox->addItem( "Structural Similarity" );
    metric_combobox->addItem( "Euclidean" );
    metric_combobox->addItem( "Cosine" );

    auto filepath_label = new QLabel {};
    filepath_label->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    filepath_label->setTextInteractionFlags( Qt::TextSelectableByMouse );
    filepath_label->setStyleSheet( "QLabel { background: #fafafa; border-radius: 3px; }" );
    filepath_label->setMaximumWidth( 300 );
    filepath_label->setContentsMargins( 5, 3, 5, 3 );

    auto filepath_button = new QToolButton {};
    filepath_button->setIcon( QIcon { ":/edit.svg" } );
    filepath_button->setCursor( Qt::ArrowCursor );

    auto filepath_layout = new QHBoxLayout {};
    filepath_layout->setContentsMargins( 0, 0, 0, 0 );
    filepath_layout->setSpacing( 10 );
    filepath_layout->addWidget( filepath_label, 1 );
    filepath_layout->addWidget( filepath_button );

    auto create_button = new QPushButton { "Create" };
    create_button->setStyleSheet( "QPushButton { padding: 2px 5px 2px 5px; }" );

    auto layout = new QFormLayout {};
    layout->addRow( "Filter", segment_selector );
    layout->addRow( "Metric", metric_combobox );
    layout->addRow( "Filepath", filepath_layout );
    layout->addRow( create_button );

    auto dialog = QDialog {};
    dialog.setWindowTitle( "Create Distance Matrix..." );
    dialog.setLayout( layout );

    QObject::connect( filepath_button, &QToolButton::clicked, [filepath_label]
    {
        const auto filepath = QFileDialog::getSaveFileName( nullptr, "Choose Filepath...", "", "*.mia" );
        if( !filepath.isEmpty() )
        {
            filepath_label->setText( filepath );
        }
    } );
    QObject::connect( create_button, &QPushButton::clicked, this, [&]
    {
        const auto channel_count    = dataset->channel_count();
        const auto dimensions       = std::array<size_t, 2> { channel_count, channel_count };

        auto element_indices = std::vector<uint32_t> {};
        if( const auto segment = segment_selector->selected_segment() )
        {
            element_indices = segmentation->element_indices()[segment->number()];
        }
        else
        {
            element_indices.resize( dataset->element_count() );
            std::iota( element_indices.begin(), element_indices.end(), 0 );
        }

        if( element_indices.empty() )
        {
            QMessageBox::warning( nullptr, "Create Distance Matrix...", "The selected segment is empty." );
            return;
        }

        dataset->visit( [&] ( const auto& dataset )
        {
            const auto& intensities = dataset.intensities();
            auto distance_matrix = Matrix<double>::allocate( dimensions );

            if( metric_combobox->currentText() == "Pearson Correlation" )
            {
                utility::compute_distance_matrix<utility::Metric::ePearsonCorrelation>( distance_matrix, intensities, element_indices );
            }
            else if( metric_combobox->currentText() == "Mutual Information" )
            {
                utility::compute_distance_matrix<utility::Metric::eMutualInformation>( distance_matrix, intensities, element_indices );
            }
            else if( metric_combobox->currentText() == "Euclidean" )
            {
                utility::compute_distance_matrix<utility::Metric::eEuclideanDistance>( distance_matrix, intensities, element_indices );
            }
            else if( metric_combobox->currentText() == "Cosine" )
            {
                utility::compute_distance_matrix<utility::Metric::eCosineSimilarity>( distance_matrix, intensities, element_indices );
            }
            else
            {
                QMessageBox::warning( nullptr, "Create Distance Matrix...", "The selected metric is not supported." );
                return;
            }

            _distance_matrix = std::move( distance_matrix );
            dialog.accept();
        } );
    } );

    dialog.exec();

    if( !filepath_label->text().isEmpty() )
    {
        this->on_export_distance_matrix( filepath_label->text() );
    }
}
void ChannelGlyphsViewer::on_export_distance_matrix( QString filepath )
{
    if( _distance_matrix.empty() )
    {
        QMessageBox::warning( nullptr, "Export Distance Matrix...", "There is no distance matrix to export. Please create a distance matrix before exporting." );
        return;
    }

    if( filepath.isEmpty() )
    {
        filepath = QFileDialog::getSaveFileName( nullptr, "Export Distance Matrix...", "", "*.mia" );
        if( filepath.isEmpty() )
        {
            return;
        }
    }

    auto filestream = MIAFileStream {
        std::filesystem::path { filepath.toStdWString() },
        std::ios::out
    };
    filestream.write( std::string { "distance_matrix" } );

    filestream.write( _distance_matrix.dimensions() );
    filestream.write( _distance_matrix.data(), _distance_matrix.bytes() );

    if( !filestream )
    {
        QMessageBox::warning( nullptr, "Export Distance Matrix...", "An error occurred while writing the distance matrix to the file." );
        return;
    }

    QMessageBox::information( nullptr, "Export Distance Matrix...", "Distance matrix exported successfully." );
}
void ChannelGlyphsViewer::on_import_distance_matrix()
{
    const auto filepath = QFileDialog::getOpenFileName( nullptr, "Import Distance Matrix...", "", "*.mia" );
    if( filepath.isEmpty() )
    {
        return;
    }

    auto filestream = MIAFileStream {
        std::filesystem::path { filepath.toStdWString() },
        std::ios::in
    };

    const auto identifier = filestream.read<std::string>();
    if( identifier != "distance_matrix" )
    {
        QMessageBox::warning( nullptr, "Import Distance Matrix...", "The selected file does not contain a valid distance matrix." );
        return;
    }

    const auto dimensions = filestream.read<std::array<size_t, 2>>();
    auto distance_matrix = Matrix<double>::allocate( dimensions );
    filestream.read( distance_matrix.data(), distance_matrix.bytes() );

    if( !filestream )
    {
        QMessageBox::warning( nullptr, "Import Distance Matrix...", "An error occurred while reading the distance matrix from the file." );
        return;
    }

    const auto dataset = _database.dataset();
    if( dimensions[0] != dataset->channel_count() || dimensions[1] != dataset->channel_count() )
    {
        QMessageBox::warning( nullptr, "Import Distance Matrix...", "The dimensions of the imported distance matrix do not match the number of channels in the dataset." );
        return;
    }

    _distance_matrix = std::move( distance_matrix );

    QMessageBox::information( nullptr, "Import Distance Matrix...", "Distance matrix imported successfully." );
}

void ChannelGlyphsViewer::on_create_embedding()
{
    if( _distance_matrix.empty() )
    {
        QMessageBox::warning( nullptr, "Create Channels Embedding...", "Please create or import a distance matrix before creating an embedding." );
        return;
    }

    const auto channel_count = _distance_matrix.dimensions()[0];

    auto neighbors = new QSpinBox {};
    neighbors->setRange( 2, static_cast<int>( channel_count - 1 ) );
    neighbors->setValue( 15 );

    auto minimum_distance = new QDoubleSpinBox {};
    minimum_distance->setRange( 0.0, 1.0 );
    minimum_distance->setSingleStep( 0.01 );
    minimum_distance->setValue( 0.1 );

    auto filepath_label = new QLabel {};
    filepath_label->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    filepath_label->setTextInteractionFlags( Qt::TextSelectableByMouse );
    filepath_label->setStyleSheet( "QLabel { background: #fafafa; border-radius: 3px; }" );
    filepath_label->setMaximumWidth( 300 );
    filepath_label->setContentsMargins( 5, 3, 5, 3 );

    auto filepath_button = new QToolButton {};
    filepath_button->setIcon( QIcon { ":/edit.svg" } );
    filepath_button->setCursor( Qt::ArrowCursor );

    auto filepath_layout = new QHBoxLayout {};
    filepath_layout->setContentsMargins( 0, 0, 0, 0 );
    filepath_layout->setSpacing( 10 );
    filepath_layout->addWidget( filepath_label, 1 );
    filepath_layout->addWidget( filepath_button );

    auto create_button = new QPushButton { "Create" };
    create_button->setStyleSheet( "QPushButton { padding: 2px 5px 2px 5px; }" );

    auto layout = new QFormLayout {};
    layout->addRow( "Neighbors", neighbors );
    layout->addRow( "Minimum Distance", minimum_distance );
    layout->addRow( "Filepath", filepath_layout );
    layout->addRow( create_button );

    auto dialog = QDialog {};
    dialog.setWindowTitle( "Create Channels Embedding..." );
    dialog.setLayout( layout );

    QObject::connect( create_button, &QPushButton::clicked, this, [&]
    {
        auto intepreter = py::interpreter {};

        const auto distance_matrix_memoryview = py::memoryview::from_buffer(
            _distance_matrix.data(),
            { _distance_matrix.dimensions()[0], _distance_matrix.dimensions()[1] },
            { sizeof( double ) * _distance_matrix.dimensions()[1], sizeof( double ) }
        );

        using namespace py::literals;
        auto locals = py::dict {
            "distance_matrix"_a = distance_matrix_memoryview,
            "n_neighbors"_a = neighbors->value(),
            "min_dist"_a = minimum_distance->value(),
            "error"_a = std::string {}
        };

        Console::info( "Computing channels embedding with UMAP..." );

        try
        {
            py::exec( R"(
try:
    import numpy as np
    import umap

    distance_matrix = np.asarray( distance_matrix )
    print( f"[Embedding] Distance matrix shape: {distance_matrix.shape}" )

    model = umap.UMAP(
        n_components	= 2,
        n_neighbors		= n_neighbors,
        min_dist		= min_dist,
        metric			= "precomputed",                
        random_state	= 42,
        n_jobs			= 1,
        verbose			= True
    )
    embedding = model.fit_transform( distance_matrix ).astype( np.float64 )
    print( f"[Embedding] Embedding shape: {embedding.shape}" )

    # Normalize embedding to [-1, 1]x[-1, 1]
    embedding_minimum   = np.min( embedding, axis=0 )
    embedding_maximum   = np.max( embedding, axis=0 )
    embedding_center    = ( embedding_minimum + embedding_maximum ) / 2.0
    embedding_scale     = np.max( embedding_maximum - embedding_minimum ) / 2.0
    embedding           = ( embedding - embedding_center ) / embedding_scale

except Exception as exception:
    error = str( exception ))", py::globals(), locals );
        }
        catch( const py::error_already_set& error )
        {
            locals["error"] = error.what();
        }

        if( const auto error = locals["error"].cast<std::string>(); !error.empty() )
        {
            Console::error( std::format( "Python error during channels embedding computation: {}", error ) );
            QMessageBox::critical( nullptr, "", "Failed to compute channels embedding.", QMessageBox::Ok );
            return;
        }

        Console::info( "Channels embedding computed successfully." );

        const auto embedding = locals["embedding"].cast<py::array_t<double>>();
        _embedding = Array<vec2<double>>::allocate( channel_count );
        for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
        {
            _embedding[channel_index] = vec2<double> {
                embedding.at( channel_index, 0 ),
                embedding.at( channel_index, 1 )
            };
        }

        dialog.accept();
    } );

    dialog.exec();

    if( !filepath_label->text().isEmpty() )
    {
        this->on_export_embedding( filepath_label->text() );
    }
}
void ChannelGlyphsViewer::on_export_embedding( QString filepath )
{
    if( _embedding.empty() )
    {
        QMessageBox::warning( nullptr, "Export Channels Embedding...", "There is no channels embedding to export. Please create a channels embedding before exporting." );
        return;
    }

    if( filepath.isEmpty() )
    {
        filepath = QFileDialog::getSaveFileName( nullptr, "Export Channels Embedding...", "", "*.mia" );
        if( filepath.isEmpty() )
        {
            return;
        }
    }

    auto filestream = MIAFileStream {
        std::filesystem::path { filepath.toStdWString() },
        std::ios::out
    };
    filestream.write( std::string { "channels_embedding" } );

    filestream.write( _embedding.size() );
    filestream.write( _embedding.data(), _embedding.bytes() );

    if( !filestream )
    {
        QMessageBox::warning( nullptr, "Export Channels Embedding...", "An error occurred while writing the distance matrix to the file." );
        return;
    }

    QMessageBox::information( nullptr, "Export Channels Embedding...", "Channels embedding exported successfully." );
}
void ChannelGlyphsViewer::on_import_embedding()
{
    const auto filepath = QFileDialog::getOpenFileName( nullptr, "Import Channels Embedding...", "", "*.mia" );
    if( filepath.isEmpty() )
    {
        return;
    }

    auto filestream = MIAFileStream {
        std::filesystem::path { filepath.toStdWString() },
        std::ios::in
    };

    const auto identifier = filestream.read<std::string>();
    if( identifier != "channels_embedding" )
    {
        QMessageBox::warning( nullptr, "Import Channels Embedding...", "The selected file does not contain a valid channels embedding." );
        return;
    }

    const auto size = filestream.read<size_t>();
    auto embedding = Array<vec2<double>>::allocate( size );
    filestream.read( embedding.data(), embedding.bytes() );

    if( !filestream )
    {
        QMessageBox::warning( nullptr, "Import Channels Embedding...", "An error occurred while reading the channels embedding from the file." );
        return;
    }

    const auto dataset = _database.dataset();
    if( size != dataset->channel_count() )
    {
        QMessageBox::warning( nullptr, "Import Channels Embedding...", "The size of the imported channels embedding do not match the number of channels in the dataset." );
        return;
    }

    _embedding = std::move( embedding );

    QMessageBox::information( nullptr, "Import Channels Embedding...", "Channels embedding imported successfully." );
}

void ChannelGlyphsViewer::update_glyph_values()
{
    Console::info( std::format( "Generating glyphs for viewport (offset: {}, {}, extent: {}, {})...", _viewport.offset.x, _viewport.offset.y, _viewport.extent.x, _viewport.extent.y ) );

    const auto screen_rectangle = QApplication::primaryScreen()->availableGeometry();
    const auto maximum_width    = std::min( screen_rectangle.width(), static_cast<int>( _viewport.extent.x ) );
    const auto maximum_height   = std::min( screen_rectangle.height(), static_cast<int>( _viewport.extent.y ) );

    const auto aspect_ratio     = static_cast<double>( _viewport.extent.x ) / _viewport.extent.y;
    auto canvas_width           = std::min( maximum_width, static_cast<int>( maximum_height * aspect_ratio ) );
    auto canvas_height          = std::min( maximum_height, static_cast<int>( maximum_width / aspect_ratio ) );

    const auto dataset          = _database.dataset();
    const auto channel_count    = dataset->channel_count();
    const auto gridsize         = static_cast<int>( std::ceil( std::sqrt( channel_count ) ) );
    const auto glyph_width      = static_cast<int>( std::round( static_cast<double>( canvas_width ) / gridsize ) );
    const auto glyph_height     = static_cast<int>( std::round( static_cast<double>( canvas_height ) / gridsize ) );
    Console::info( std::format( "Optimal glyph dimensions: {}x{}, gridsize: {}", glyph_width, glyph_height, gridsize ) );

    canvas_width                = glyph_width * gridsize;
    canvas_height               = glyph_height * gridsize;
    _canvas                     = QImage { canvas_width, canvas_height, QImage::Format_RGB32 };
    Console::info( std::format( "Optimal canvas dimensions: {}x{}", canvas_width, canvas_height ) );

    Console::info( "Initializing glyphs..." );
    _glyphs.resize( channel_count );
    for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
    {
        auto& glyph     = _glyphs[channel_index];
        glyph.values    = Matrix<double> {
            { static_cast<size_t>( glyph_width ), static_cast<size_t>( glyph_height ) },
            0.0
        };
        glyph.image     = QImage { glyph_width, glyph_height, QImage::Format_RGB32 };
    }

    Console::info( "Accumulating glyph intensities..." );
    dataset->visit( [&] ( const auto& dataset )
    {
        const auto spatial_metadata = dataset.spatial_metadata();
        const auto& intensities     = dataset.intensities();

        const auto glyph_x_maximum  = static_cast<size_t>( glyph_width - 1 );
        const auto glyph_y_maximum  = static_cast<size_t>( glyph_height - 1 );

        for( uint32_t x = _viewport.offset.x; x < _viewport.offset.x + _viewport.extent.x; ++x )
        {
            for( uint32_t y = _viewport.offset.y; y < _viewport.offset.y + _viewport.extent.y; ++y )
            {
                const auto element_index = spatial_metadata->element_index( { x, y } );

                const auto normalized_x = ( 0.5 + x - _viewport.offset.x ) / _viewport.extent.x;
                const auto normalized_y = ( 0.5 + y - _viewport.offset.y ) / _viewport.extent.y;

                auto glyph_x = static_cast<size_t>( std::round( normalized_x * glyph_width ) );
                auto glyph_y = static_cast<size_t>( std::round( normalized_y * glyph_height ) );

                glyph_x = std::min( glyph_x, glyph_x_maximum );
                glyph_y = std::min( glyph_y, glyph_y_maximum );

                for( uint32_t channel_index = 0; channel_index < dataset.channel_count(); ++channel_index )
                {
                    const auto value = static_cast<double>( intensities.value( { element_index, channel_index } ) );
                    _glyphs[channel_index].values.value( { glyph_x, glyph_y } ) += value;
                }
            }
        }
    } );

    Console::info( "Normalizing glyph values..." );
    if( _normalization == Normalization::eGlobal )
    {
        auto value_minimum = std::numeric_limits<double>::max();
        auto value_maximum = std::numeric_limits<double>::lowest();

        for( const auto& glyph : _glyphs )
        {
            for( const auto& value : glyph.values )
            {
                value_minimum = std::min( value_minimum, value );
                value_maximum = std::max( value_maximum, value );
            }
        }
        Console::info( std::format( "Global value range: [{}, {}]", value_minimum, value_maximum ) );

        const auto value_range = value_maximum - value_minimum;
        if( value_range == 0.0 )
        {
            for( auto& glyph : _glyphs )
            {
                for( auto& value : glyph.values )
                {
                    value = 0.5;
                }
            }
        }
        else
        {
            for( auto& glyph : _glyphs )
            {
                for( auto& value : glyph.values )
                {
                    value = ( value - value_minimum ) / value_range;
                }
            }
        }
    }
    else if( _normalization == Normalization::eGlyph )
    {
        for( auto& glyph : _glyphs )
        {
            auto value_minimum = std::numeric_limits<double>::max();
            auto value_maximum = std::numeric_limits<double>::lowest();

            for( const auto& value : glyph.values )
            {
                value_minimum = std::min( value_minimum, value );
                value_maximum = std::max( value_maximum, value );
            }

            const auto value_range = value_maximum - value_minimum;
            if( value_range == 0.0 )
            {
                for( auto& value : glyph.values )
                {
                    value = 0.5;
                }
            }
            else
            {
                for( auto& value : glyph.values )
                {
                    value = ( value - value_minimum ) / value_range;
                }
            }
        }
    }
    else
    {
        Console::warning( "Unknown normalization method." );
    }

    this->reset_canvas_rectangle();
    this->update_glyph_images();
    this->update_glyph_layout();
}
void ChannelGlyphsViewer::update_glyph_images()
{
    Console::info( "Mapping glyph values to colors..." );

    if( _glyphs.empty() )
    {
        return;
    }

    const auto glyph_width  = _glyphs.front().image.width();
    const auto glyph_height = _glyphs.front().image.height();

    for( auto& glyph : _glyphs )
    {
        for( int x = 0; x < glyph_width; ++x )
        {
            for( int y = 0; y < glyph_height; ++y )
            {
                const auto value = glyph.values.value( { static_cast<size_t>( x ), static_cast<size_t>( y ) } );
                const auto color = _colormap->color( value ).qcolor();
                glyph.image.setPixelColor( x, y, color );
            }
        }
    }

    this->update_canvas();
}
void ChannelGlyphsViewer::update_glyph_layout()
{
    Console::info( "Updating glyph layout..." );

    if( _glyphs.empty() )
    {
        return;
    }

    if( _positioning == Positioning::eEmbedding && _embedding.empty() )
    {
        QMessageBox::warning( nullptr, "Channels Embedding", "Please create or import a channels embedding." );
        _positioning = Positioning::eLinear;
    }

    if( _positioning == Positioning::eGridified && _embedding.empty() )
    {
        QMessageBox::warning( nullptr, "Channels Embedding", "Please create or import a channels embedding." );
        _positioning = Positioning::eLinear;
    }

    const auto dataset          = _database.dataset();
    const auto channel_count    = dataset->channel_count();
    const auto gridsize         = static_cast<int>( std::ceil( std::sqrt( channel_count ) ) );
    const auto glyph_width      = _glyphs.front().image.width();
    const auto glyph_height     = _glyphs.front().image.height();

    if( _positioning == Positioning::eLinear )
    {
        for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
        {
            const auto col                  = channel_index % static_cast<uint32_t>( gridsize );
            const auto row                  = channel_index / static_cast<uint32_t>( gridsize );
            _glyphs[channel_index].offset   = vec2<uint32_t> {
                ( row & 0x1? gridsize - 1 - col : col ) * glyph_width,
                row * glyph_height
            };
        }
    }
    else if( _positioning == Positioning::eEmbedding || _positioning == Positioning::eGridified )
    {
        const auto canvas_rectangle     = _canvas.rect().toRectF();
        const auto canvas_center        = canvas_rectangle.center();
        const auto canvas_radius        = std::min(
            canvas_rectangle.width() - glyph_width,
            canvas_rectangle.height() - glyph_height
        ) / 2.0;

        auto glyph_centers = Array<vec2<double>>::allocate( channel_count );
        for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
        {
            const auto embedding_position   = _embedding.value( channel_index );
            glyph_centers[channel_index]    = vec2<double> {
                canvas_center.x() + embedding_position.x * canvas_radius,
                canvas_center.y() + embedding_position.y * canvas_radius
            };
        }

        const auto glyph_width_half     = glyph_width / 2.0;
        const auto glyph_height_half    = glyph_height / 2.0;

        if( _positioning == Positioning::eEmbedding )
        {
            for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
            {
                const auto glyph_center         = glyph_centers[channel_index];
                _glyphs[channel_index].offset   = vec2<uint32_t> {
                    static_cast<uint32_t>( std::round( glyph_center.x - glyph_width_half ) ),
                    static_cast<uint32_t>( std::round( glyph_center.y - glyph_height_half ) )
                };
            }
        }
        else
        {
            auto candidate_positions = Array<vec2<double>>::allocate( gridsize * gridsize );
            for( int col = 0; col < gridsize; ++col )
            {
                for( int row = 0; row < gridsize; ++row )
                {
                    candidate_positions.value( gridsize * col + row ) = vec2<double> {
                        ( col + 0.5 ) * glyph_width,
                        ( row + 0.5 ) * glyph_height
                    };
                }
            }

            auto intepreter = py::interpreter {};

            const auto sources_memoryview = py::memoryview::from_buffer(
                reinterpret_cast<const double*>( glyph_centers.data() ),
                { static_cast<size_t>( channel_count ), size_t { 2 } },
                { sizeof( vec2<double> ), sizeof( double ) }
            );

            const auto targets_memoryview = py::memoryview::from_buffer(
                reinterpret_cast<const double*>( candidate_positions.data() ),
                { static_cast<size_t>( gridsize * gridsize ), size_t { 2 } },
                { sizeof( vec2<double> ), sizeof( double ) }
            );

            using namespace py::literals;
            auto locals = py::dict {
                "sources"_a = sources_memoryview,
                "targets"_a = targets_memoryview,
                "error"_a = std::string {}
            };

            Console::info( "Optimizing glyph layout with linear sum assignment..." );

            try
            {
                py::exec( R"(
try:
    import numpy as np
    from scipy.optimize import linear_sum_assignment

    sources = np.asarray( sources, copy=True )
    targets = np.asarray( targets )

    cost_matrix = np.linalg.norm( sources[:, None, :] - targets[None, :, :], axis=-1 )
    row_indices, col_indices = linear_sum_assignment( cost_matrix )

    for source_index, target_index in zip( row_indices, col_indices ):
        sources[source_index] = targets[target_index]
    sources = sources.astype( np.float64 )

except Exception as exception:
    error = str( exception ))", py::globals(), locals );
            }
            catch( const py::error_already_set& error )
            {
                locals["error"] = error.what();
            }

            if( const auto error = locals["error"].cast<std::string>(); !error.empty() )
            {
                Console::error( std::format( "Python error during gridification: {}", error ) );
                QMessageBox::critical( nullptr, "", "Failed to compute gridification.", QMessageBox::Ok );

                _positioning = Positioning::eEmbedding;
                this->update_glyph_layout();
                return;
            }

            const auto optimized_glyph_centers = locals["sources"].cast<py::array_t<float>>();

            for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
            {
                const auto x                    = optimized_glyph_centers.at( channel_index, 0 );
                const auto y                    = optimized_glyph_centers.at( channel_index, 1 );
                _glyphs[channel_index].offset   = vec2<uint32_t> {
                    static_cast<uint32_t>( std::round( x - glyph_width_half ) ),
                    static_cast<uint32_t>( std::round( y - glyph_height_half ) )
                };
            }
        }
    }
    this->update_canvas();
}
void ChannelGlyphsViewer::update_canvas()
{
    Console::info( "Compositing glyphs onto canvas..." );

    auto painter = QPainter { &_canvas };
    _canvas.fill( QColor { 255, 255, 255 } );

    for( const auto& glyph : _glyphs )
    {
        painter.drawImage( glyph.offset.x, glyph.offset.y, glyph.image );
    }

    this->update();
}

void ChannelGlyphsViewer::reset_canvas_rectangle()
{
    const auto canvas_scaling = std::min(
        ( this->width() - 20.0 ) / _canvas.width(),
        ( this->height() - 20.0 ) / _canvas.height()
    );
    _canvas_rectangle = QRectF {
        0,
        0,
        _canvas.width() * canvas_scaling,
        _canvas.height() * canvas_scaling
    };
    _canvas_rectangle.moveCenter( this->rect().center() );
    this->update();
}
QRectF ChannelGlyphsViewer::glyph_rectangle( uint32_t channel_index ) const
{
    if( channel_index >= _glyphs.size() )
    {
        return QRectF {};
    }

    const auto& glyph   = _glyphs[channel_index];
    const auto x        = static_cast<double>( glyph.offset.x ) / _canvas.width();
    const auto y        = static_cast<double>( glyph.offset.y ) / _canvas.height();

    const auto canvas_scaling   = _canvas_rectangle.width() / _canvas.width();

    const auto width    = canvas_scaling * glyph.image.width();
    const auto height   = canvas_scaling * glyph.image.height();

    return QRectF {
        _canvas_rectangle.left() + x * _canvas_rectangle.width(),
        _canvas_rectangle.top() + y * _canvas_rectangle.height(),
        width,
        height
    };
}