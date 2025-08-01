#include "histogram_viewer.hpp"

#include "configuration.hpp"
#include "console.hpp"
#include "dataset.hpp"
#include "feature.hpp"
#include "feature_manager.hpp"
#include "segmentation.hpp"

#include <qactiongroup.h>
#include <qevent.h>
#include <qfiledialog.h>
#include <qlabel.h>
#include <qmenu.h>
#include <qmessagebox.h>
#include <qpainter.h>
#include <qwidgetaction.h>

HistogramViewer::HistogramViewer( Database& database ) : _database { database }
{
    this->setMouseTracking( true );

    _segmentation_histogram.update_segmentation( _database.segmentation() );

    QObject::connect( &_histogram, &Histogram::edges_changed, this, [this]
    {
        const auto& edges = _histogram.edges();
        const auto precision = utility::stepsize_to_precision( edges[1] - edges[0] ) + 1;

        auto ticks = std::vector<Tick> {};
        for( const auto edge : edges )
        {
            ticks.push_back( Tick { edge, QString::number( edge, 'f', precision ) } );
        }
        this->update_xaxis_ticks( std::move( ticks ) );

        const auto range = edges.last() - edges.first();
        this->update_xaxis_bounds( { edges.first() - 0.01 * range, edges.last() + 0.01 * range } );
        this->update_xaxis_domain( { edges.first() - 0.01 * range, edges.last() + 0.01 * range } );
        this->update();
    } );
    QObject::connect( &_histogram, &Histogram::counts_changed, this, [this]
    {
        const auto& counts = _histogram.counts();
        const auto maximum = std::max( 1.0, static_cast<double>( *std::max_element( counts.begin(), counts.end() ) ) );
        this->update_yaxis_bounds( { 0.0, maximum + 0.01 * maximum } );
        this->update_yaxis_domain( { 0.0, maximum + 0.01 * maximum } );
        this->update();
    } );
    QObject::connect( &_segmentation_histogram, &StackedHistogram::edges_changed, this, qOverload<>( &QWidget::update ) );
    QObject::connect( &_segmentation_histogram, &StackedHistogram::counts_changed, this, qOverload<>( &QWidget::update ) );

    const auto segmentation = _database.segmentation();
    QObject::connect( segmentation.get(), &Segmentation::segment_color_changed, this, qOverload<>( &QWidget::update ) );
    QObject::connect( &_database, &Database::highlighted_element_index_changed, this, qOverload<>( &QWidget::update ) );
}

QSharedPointer<const Feature> HistogramViewer::feature() const
{
    return _histogram.feature();
}
void HistogramViewer::update_feature( QSharedPointer<const Feature> feature )
{
    _histogram.update_feature( feature );
    _segmentation_histogram.update_feature( feature );
}

uint32_t HistogramViewer::bincount() const noexcept
{
    return _histogram.bincount();
}
void HistogramViewer::update_bincount( uint32_t bincount )
{
    _histogram.update_bincount( bincount );
    _segmentation_histogram.update_bincount( bincount );
}

void HistogramViewer::paintEvent( QPaintEvent* event )
{
    auto painter = QPainter { this };
    painter.setRenderHint( QPainter::Antialiasing, true );
    painter.setClipRect( this->content_rectangle() );

    const auto content_rectangle = this->content_rectangle();
    const auto& edges = _histogram.edges();
    const auto& counts = _histogram.counts();

    auto edges_screen = Array<double>::allocate( edges.size() );
    for( uint32_t index = 0; index < edges.size(); ++index )
    {
        edges_screen[index] = this->world_to_screen_x( edges.value( index ) );
    }

    const auto xaxis_screen = this->world_to_screen_y( 0.0 );
    auto bins_count = std::vector<uint32_t>( this->bincount(), 0 );
    auto bins_bottom = std::vector<double>( this->bincount(), xaxis_screen );

    struct HoveredObject
    {
        QRectF rectangle {};
        QColor color {};
        uint32_t bin = 0;
        uint32_t segment_number = 0;
    } hovered_object;

    const auto render_histogram = [&] ( const Array<uint32_t>& counts, const QColor& color, uint32_t segment_number )
    {
        painter.setPen( QPen { QBrush { config::palette[600] }, 1.0 } );
        painter.setBrush( QBrush { color } );

        for( uint32_t bin = 0; bin < this->bincount(); ++bin ) if( counts[bin] > 0 )
        {
            const auto left = edges_screen[bin];
            const auto right = edges_screen[bin + 1];
            const auto top = this->world_to_screen_y( bins_count[bin] + counts[bin] );
            const auto bottom = bins_bottom[bin];

            const auto rectangle = QRectF { left, top, right - left, bottom - top };
            if( rectangle.contains( _cursor_position ) )
            {
                hovered_object = { rectangle, color, bin, segment_number };
            }

            painter.drawRect( rectangle );

            bins_count[bin] += counts[bin];
            bins_bottom[bin] = top;
        }
    };
    render_histogram( counts, config::palette[200], 0 );
    const auto hovered_object_global = hovered_object;
    hovered_object = {};

    const auto segmentation = _database.segmentation();
    const auto& segmentation_counts = _segmentation_histogram.counts();

    std::fill( bins_count.begin(), bins_count.end(), 0 );
    std::fill( bins_bottom.begin(), bins_bottom.end(), xaxis_screen );

    for( uint32_t segment_number = 1; segment_number < segmentation->segment_count(); ++segment_number )
    {
        const auto& counts = segmentation_counts[segment_number];
        render_histogram( counts, segmentation->segment( segment_number )->color().qcolor(), segment_number );
    }
    const auto hovered_object_segmentation = hovered_object;

    // Highlight hovered rectangle
    painter.setBrush( Qt::NoBrush );
    if( hovered_object_segmentation.rectangle.width() )
    {
        painter.setPen( QPen { QBrush { config::palette[600] }, 2.0 } );
        painter.drawRect( hovered_object_segmentation.rectangle );
    }
    else if( hovered_object_global.rectangle.width() )
    {
        painter.setPen( QPen { QBrush { config::palette[600] }, 2.0 } );
        painter.drawRect( hovered_object_global.rectangle );
    }

    // Render hovered rectangle information
    auto labels_string = QString {};
    auto values_string = QString {};

    if( hovered_object_global.rectangle.width() )
    {
        const auto percentage = counts[hovered_object_global.bin] / static_cast<double>( _database.dataset()->element_count() );
        labels_string += "Bin:";
        values_string += QString::number( 100.0 * percentage, 'f', 1 ) + " % of dataset";
    }

    if( hovered_object_segmentation.rectangle.width() )
    {
        const auto segment = _database.segmentation()->segment( hovered_object_segmentation.segment_number );
        const auto segment_count = segmentation_counts[hovered_object_segmentation.segment_number][hovered_object_segmentation.bin];
        const auto percentage_segment = segment_count / static_cast<double>( segment->element_count() );
        const auto percentage_bin = segment_count / static_cast<double>( counts[hovered_object_segmentation.bin] );

        labels_string += "\nBin:";
        values_string += '\n' + QString::number( 100.0 * percentage_segment, 'f', 1 ) + " % of segment";

        labels_string += "\nSegment:";
        values_string += '\n' + QString::number( 100.0 * percentage_bin, 'f', 1 ) + " % of bin";
    }

    auto labels_rectangle = painter.fontMetrics().boundingRect( QRect { 0, 0, 10000, 10000 }, Qt::TextWordWrap, labels_string ).toRectF();
    auto values_rectangle = painter.fontMetrics().boundingRect( QRect { 0, 0, 10000, 10000 }, Qt::TextWordWrap, values_string ).toRectF();

    values_rectangle.moveTopRight( content_rectangle.topRight() + QPointF { -10.0, 10.0 } );
    labels_rectangle.moveRight( values_rectangle.left() - 10.0 );
    labels_rectangle.moveTop( values_rectangle.top() );

    auto background_rectangle = labels_rectangle.united( values_rectangle ).marginsAdded( QMarginsF { 5.0, 5.0, 5.0, 5.0 } );

    painter.setPen( Qt::NoPen );
    painter.setBrush( QBrush { QColor { 255, 255, 255, 200 } } );
    painter.drawRoundedRect( background_rectangle, 5.0, 5.0 );

    painter.setPen( Qt::black );
    painter.drawText( labels_rectangle, Qt::AlignLeft | Qt::AlignTop, labels_string );
    painter.drawText( values_rectangle, Qt::AlignRight | Qt::AlignTop, values_string );

    if( const auto element_index = _database.highlighted_element_index(); element_index.has_value() )
    {
        if( const auto feature = _histogram.feature() )
        {
            const auto& feature_values = feature->values();
            const auto xscreen = this->world_to_screen_x( feature_values[*element_index] );
            painter.setPen( QPen { QColor { config::palette[500] }, 1.0, Qt::DashLine } );
            painter.drawLine(
                QPointF { xscreen, static_cast<double>( content_rectangle.bottom() ) },
                QPointF { xscreen, static_cast<double>( content_rectangle.top() ) }
            );
        }
    }

    painter.setClipRect( this->rect() );

    // Render current feature
    if( const auto feature = _histogram.feature() )
    {
        const auto& string = feature->identifier();

        painter.save();
        auto font = painter.font();
        font.setBold( true );
        painter.setFont( font );

        auto rectangle = painter.fontMetrics().boundingRect( string ).toRectF().marginsAdded( QMarginsF { 5.0, 2.0, 5.0, 2.0 } );
        rectangle.moveCenter( content_rectangle.center() );
        rectangle.moveTop( content_rectangle.top() );

        painter.setPen( Qt::NoPen );
        painter.setBrush( QBrush { QColor { 255, 255, 255, 200 } } );
        painter.drawRoundedRect( rectangle, 2.0, 2.0 );

        painter.setPen( Qt::black );
        painter.drawText( rectangle, Qt::AlignCenter, string );

        painter.restore();
    }

    PlottingWidget::paintEvent( event );
}

void HistogramViewer::mousePressEvent( QMouseEvent* event )
{
    if( event->button() == Qt::RightButton )
    {
        auto context_menu = QMenu { this };
        auto feature_menu = context_menu.addMenu( "Change Feature" );

        const auto features = _database.features();
        auto feature_action_group = new QActionGroup { feature_menu };
        feature_action_group->setExclusive( true );

        for( qsizetype feature_index = 0; feature_index < features->object_count(); ++feature_index )
        {
            const auto feature = features->object( feature_index );
            auto action = feature_menu->addAction( feature->identifier(), [this, feature]
            {
                this->update_feature( feature );
            } );
            action->setCheckable( true );
            action->setChecked( feature == _histogram.feature() );
        }

        context_menu.addAction( "Feature Manager", [this] { FeatureManager::execute( _database ); } );
        context_menu.addAction( "Reset View", [this] { this->reset_view(); } );
        context_menu.addAction( "Export", [this] { this->export_histograms(); } );

        context_menu.exec( event->globalPosition().toPoint() );
    }

    this->update();
}
void HistogramViewer::mouseMoveEvent( QMouseEvent* event )
{
    _cursor_position = event->position();
    this->update();
}
void HistogramViewer::leaveEvent( QEvent* event )
{
    _cursor_position = QPointF { -1.0, -1.0 };
    this->update();
}

void HistogramViewer::export_histograms() const
{
    const auto filepath = QFileDialog::getSaveFileName( nullptr, "Export Histograms...", "", "*.csv", nullptr );
    if( !filepath.isEmpty() )
    {
        auto file = QFile { filepath };
        if( !file.open( QFile::WriteOnly | QFile::Text ) )
        {
            QMessageBox::critical( nullptr, "", "Failed to open file" );
            return;
        }

        auto stream = QTextStream { &file };
        stream << "identifier,total_count";

        const auto& edges = _histogram.edges();
        const auto precision = utility::stepsize_to_precision( edges[1] - edges[0] ) + 1;
        for( uint32_t bin = 0; bin < this->bincount(); ++bin )
        {
            stream << "," << QString::number( edges[bin], 'f', precision ) << " to " << QString::number( edges[bin + 1], 'f', precision );
        }
        stream << '\n';

        const auto write_histogram = [&stream] ( const QString& identifier, uint32_t total_element_count, const Array<uint32_t>& counts )
        {
            stream << identifier << "," << total_element_count;
            for( const auto value : counts ) stream << ',' << value;
            stream << '\n';
        };

        const auto segmentation = _database.segmentation();
        const auto counts = _histogram.counts();
        write_histogram( "Dataset", segmentation->element_count(), counts );

        const auto& segmentation_counts = _segmentation_histogram.counts();

        for( uint32_t segment_number = 1; segment_number < segmentation->segment_count(); ++segment_number )
        {
            const auto segment = _database.segmentation()->segment( segment_number );
            write_histogram( segment->identifier(), segment->element_count(), segmentation_counts[segment_number] );
        }
    }
}
