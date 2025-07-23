#include "boxplot_viewer.hpp"

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

BoxplotViewer::BoxplotViewer( Database& database ) : _database { database }
{
    this->setMouseTracking( true );

    _boxplot.update_segmentation( _database.segmentation() );
    _boxplot.statistics().subscribe( this, [this] { this->update(); } );

    const auto update_xaxis = [this]
    {
        const auto segmentation = _database.segmentation();
        const auto segment_count = segmentation->segment_count();
        this->update_xaxis_bounds( { 0.0, static_cast<double>( segment_count ) } );
        this->update_xaxis_domain( { 0.0, static_cast<double>( segment_count ) } );

        //auto ticks = std::vector<Tick> {
        //    Tick { 0.5, "Dataset" }
        //};
        //for( uint32_t segment_number = 1; segment_number < segment_count; ++segment_number )
        //{
        //    const auto& segment = segmentation->segment( segment_number );
        //    ticks.push_back( Tick { segment_number + 0.5, segment->identifier() } );
        //}
        //this->update_xaxis_ticks( std::move( ticks ) );
    };
    update_xaxis();

    const auto segmentation = _database.segmentation();
    QObject::connect( segmentation.get(), &Segmentation::segment_count_changed, this, update_xaxis );
    QObject::connect( segmentation.get(), &Segmentation::segment_identifier_changed, this, update_xaxis );
    QObject::connect( segmentation.get(), &Segmentation::segment_color_changed, this, qOverload<>( &QWidget::update ) );
}

QSharedPointer<Feature> BoxplotViewer::feature() const noexcept
{
    return _feature.lock();
}
void BoxplotViewer::update_feature( QSharedPointer<Feature> feature )
{
    if( _feature != feature )
    {
        if( auto feature = _feature.lock() )
        {
            feature->extremes().unsubscribe( this );
        }

        if( _feature = feature )
        {
            feature->extremes().subscribe( this, [this]
            {
                if( auto feature = _feature.lock() )
                {
                    const auto [minimum, maximum] = feature->extremes().value();
                    const auto range = maximum - minimum;

                    this->update_yaxis_bounds( { minimum - 0.01 * range, maximum + 0.01 * range } );
                    this->update_yaxis_domain( { minimum - 0.01 * range, maximum + 0.01 * range } );
                }
                else
                {
                    this->update_yaxis_bounds( { 0.0, 1.0 } );
                    this->update_yaxis_domain( { 0.0, 1.0 } );
                }
            } );
        }

        _boxplot.update_feature( feature );
    }
}

void BoxplotViewer::paintEvent( QPaintEvent* event )
{
    auto painter = QPainter { this };
    painter.setRenderHint( QPainter::Antialiasing, true );
    painter.setClipRect( this->content_rectangle() );


    const auto render_boxplot = [&](
        double position, const QColor& color,
        double minimum, double maximum,
        double average, double standard_deviation,
        double lower_quartile, double upper_quartile, double median )
    {
        painter.setPen( QPen { QBrush { config::palette[600] }, 1.0 } );
        painter.setBrush( QBrush { color } );

        const auto xscreen = this->world_to_screen_x( position );
        const auto xleft = this->world_to_screen_x( position - 0.25 );
        const auto xright = this->world_to_screen_x( position + 0.25 );

        const auto yminimum = this->world_to_screen_y( minimum );
        const auto ymaximum = this->world_to_screen_y( maximum );

        const auto yaverage = this->world_to_screen_y( average );
        const auto ystandard_deviation_upper = this->world_to_screen_y( average + standard_deviation );
        const auto ystandard_deviation_lower = this->world_to_screen_y( average - standard_deviation );

        const auto ylower_quartile = this->world_to_screen_y( lower_quartile );
        const auto yupper_quartile = this->world_to_screen_y( upper_quartile );
        const auto ymedian = this->world_to_screen_y( median );

        auto brush_color = color;
        brush_color.setAlpha( 50 );

        painter.setPen( QPen { color, 1.0 } );
        painter.setBrush( brush_color );
        painter.drawRect( QRectF { QPointF { xleft, yupper_quartile }, QPointF { xright, ylower_quartile } } );
        painter.drawLine( QPointF { xleft, yminimum }, QPointF { xright, yminimum } );
        painter.drawLine( QPointF { xleft, ymaximum }, QPointF { xright, ymaximum } );
        painter.drawLine( QPointF { xscreen, ylower_quartile }, QPointF { xscreen, yminimum } );
        painter.drawLine( QPointF { xscreen, yupper_quartile }, QPointF { xscreen, ymaximum } );

        painter.setPen( QPen { color, 2.0 } );
        painter.drawLine( QPointF { xleft, ymedian }, QPointF { xright, ymedian } );

        painter.setPen( QPen { color, 1.0, Qt::DashLine } );
        painter.drawLine( QPointF { xleft, yaverage }, QPointF { xscreen, ystandard_deviation_lower } );
        painter.drawLine( QPointF { xleft, yaverage }, QPointF { xscreen, ystandard_deviation_upper } );
        painter.drawLine( QPointF { xright, yaverage }, QPointF { xscreen, ystandard_deviation_lower } );
        painter.drawLine( QPointF { xright, yaverage }, QPointF { xscreen, ystandard_deviation_upper } );
        painter.drawLine( QPointF { xleft, yaverage }, QPointF { xright, yaverage } );

        painter.setPen( Qt::NoPen );
        painter.setBrush( color );
        painter.drawEllipse( QPointF { xscreen, yaverage }, 5.0, 5.0 );
        painter.drawEllipse( QPointF { xscreen, ystandard_deviation_lower }, 2.5, 2.5 );
        painter.drawEllipse( QPointF { xscreen, ystandard_deviation_upper }, 2.5, 2.5 );
    };

    if( auto feature = _feature.lock() )
    {
        const auto [minimum, maximum] = feature->extremes().value();
        const auto [average, standard_deviation] = feature->moments().value();
        const auto [lower_quartile, median, upper_quartile] = feature->quantiles().value();

        render_boxplot( 0.5, config::palette[600], minimum, maximum, average, standard_deviation, lower_quartile, upper_quartile, median );
    }

    painter.setClipRect( this->rect() );
    PlottingWidget::paintEvent( event );
}

void BoxplotViewer::mousePressEvent( QMouseEvent* event )
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

        context_menu.addAction( "Export", [this] { this->export_boxplots(); } );

        context_menu.exec( event->globalPosition().toPoint() );
    }

    this->update();
}
void BoxplotViewer::mouseMoveEvent( QMouseEvent* event )
{
    _cursor_position = event->position();
    this->update();
}
void BoxplotViewer::leaveEvent( QEvent* event )
{
    _cursor_position = QPointF { -1.0, -1.0 };
    this->update();
}

void BoxplotViewer::export_boxplots() const
{
    const auto filepath = QFileDialog::getSaveFileName( nullptr, "Export Boxplots...", "", "*.csv" );
    if( !filepath.isEmpty() )
    {

    }
}
