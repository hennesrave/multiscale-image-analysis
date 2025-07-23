#include "histogram_viewer.hpp"

#include "configuration.hpp"
#include "dataset.hpp"
#include "feature.hpp"
#include "logger.hpp"
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

    _histogram.edges().subscribe( this, [this]
    {
        const auto [edges, _] = _histogram.edges().await_value();
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
    } );
    _histogram.counts().subscribe( this, [this]
    {
        const auto [counts, _] = _histogram.counts().await_value();
        const auto maximum = std::max( 1.0, static_cast<double>( *std::max_element( counts.begin(), counts.end() ) ) );
        this->update_yaxis_bounds( { 0.0, maximum + 0.01 * maximum } );
        this->update_yaxis_domain( { 0.0, maximum + 0.01 * maximum } );
    } );

    _segmentation_histogram.update_segmentation( _database.segmentation() );
    _segmentation_histogram.counts().subscribe( this, [this] { this->update(); } );

    const auto segmentation = _database.segmentation();
    QObject::connect( segmentation.get(), &Segmentation::segment_color_changed, this, qOverload<>( &QWidget::update ) );
}

QSharedPointer<Feature> HistogramViewer::feature() const noexcept
{
    return _feature.lock();
}
void HistogramViewer::update_feature( QSharedPointer<Feature> feature )
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

    const auto [edges, edges_lock] = _histogram.edges().request_value();
    const auto [counts, counts_lock] = _histogram.counts().request_value();

    if( edges && counts )
    {
        auto edges_screen = Array<double>::allocate( edges->size() );
        for( uint32_t index = 0; index < edges->size(); ++index )
        {
            edges_screen[index] = this->world_to_screen_x( edges->value( index ) );
        }

        const auto xaxis_screen = this->world_to_screen_y( 0.0 );
        auto bins_count = std::vector<uint32_t>( this->bincount(), 0 );
        auto bins_bottom = std::vector<double>( this->bincount(), xaxis_screen );

        const auto render_histogram = [&] ( const Array<uint32_t>& counts, const QColor& color )
        {
            painter.setPen( QPen { QBrush { config::palette[600] }, 1.0 } );
            painter.setBrush( QBrush { color } );

            for( uint32_t bin = 0; bin < this->bincount(); ++bin ) if( counts[bin] > 0 )
            {
                const auto left = edges_screen[bin];
                const auto right = edges_screen[bin + 1];
                const auto top = this->world_to_screen_y( bins_count[bin] + counts[bin] );
                const auto bottom = bins_bottom[bin];
                painter.drawRect( QRectF { left, top, right - left, bottom - top } );

                bins_count[bin] += counts[bin];
                bins_bottom[bin] = top;
            }
        };
        render_histogram( *counts, config::palette[400] );

        if( const auto [segmentation_counts, _] = _segmentation_histogram.counts().request_value(); segmentation_counts )
        {
            const auto segmentation = _database.segmentation();

            std::fill( bins_count.begin(), bins_count.end(), 0 );
            std::fill( bins_bottom.begin(), bins_bottom.end(), xaxis_screen );

            for( uint32_t segment_number = 1; segment_number < segmentation->segment_count(); ++segment_number )
            {
                const auto& counts = segmentation_counts->value( segment_number );
                render_histogram( counts, segmentation->segment( segment_number )->color().qcolor() );
            }
        }
    }

    painter.setClipRect( this->rect() );
    PlottingWidget::paintEvent( event );
}

void HistogramViewer::mousePressEvent( QMouseEvent* event )
{
    if( event->button() == Qt::RightButton )
    {
        auto context_menu = QMenu { this };
        auto feature_menu = context_menu.addMenu( "Feature" );

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
            action->setChecked( feature == _feature );
        }

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
    const auto filepath = QFileDialog::getSaveFileName( nullptr, "Export Histograms...", "", "*.csv" );
    if( !filepath.isEmpty() )
    {
        auto file = QFile { filepath };
        if( !file.open( QFile::WriteOnly | QFile::Text ) )
        {
            QMessageBox::critical( nullptr, "", "Failed to open file" );
            return;
        }

        auto stream = QTextStream { &file };
        stream << "identifier,total_element_count";

        const auto [edges, edges_lock] = _histogram.edges().await_value();
        const auto precision = utility::stepsize_to_precision( edges[1] - edges[0] ) + 1;
        for( uint32_t bin = 0; bin < this->bincount(); ++bin )
        {
            stream << "," << QString::number( edges[bin], 'f', precision ) << " \u2014 " << QString::number( edges[bin + 1], 'f', precision );
        }
        stream << '\n';

        const auto write_histogram = [&stream] ( const QString& identifier, uint32_t total_element_count, const Array<uint32_t>& counts )
        {
            stream << identifier << "," << total_element_count;
            for( const auto value : counts ) stream << ',' << value;
            stream << '\n';
        };

        const auto segmentation = _database.segmentation();
        const auto [counts, counts_lock] = _histogram.counts().await_value();
        write_histogram( "Dataset", segmentation->element_count(), counts );

        const auto [segmentation_counts, segmentation_counts_lock] = _segmentation_histogram.counts().await_value();

        for( uint32_t segment_number = 0; segment_number < segmentation->segment_count(); ++segment_number )
        {
            const auto segment = _database.segmentation()->segment( segment_number );
            write_histogram( segment->identifier(), segment->element_count(), segmentation_counts[segment_number] );
        }
    }
}
