#include "boxplot_viewer.hpp"

#include "configuration.hpp"
#include "console.hpp"
#include "dataset.hpp"
#include "feature.hpp"
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
    QObject::connect( &_boxplot, &GroupedBoxplot::statistics_changed, this, qOverload<>( &QWidget::update ) );

    const auto update_xaxis = [this]
    {
        const auto segmentation = _database.segmentation();
        const auto segment_count = segmentation->segment_count();
        this->update_xaxis_bounds( { 0.0, static_cast<double>( segment_count ) } );
        this->update_xaxis_domain( { 0.0, static_cast<double>( segment_count ) } );
        this->update_xaxis_visible( false );
    };
    update_xaxis();

    const auto segmentation = _database.segmentation();
    QObject::connect( segmentation.get(), &Segmentation::segment_count_changed, this, update_xaxis );
    QObject::connect( segmentation.get(), &Segmentation::segment_identifier_changed, this, update_xaxis );
    QObject::connect( segmentation.get(), &Segmentation::segment_color_changed, this, qOverload<>( &QWidget::update ) );
    QObject::connect( &_database, &Database::highlighted_element_index_changed, this, qOverload<>( &QWidget::update ) );
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
            QObject::disconnect( feature.get(), &Feature::values_changed, this, nullptr );
        }

        if( _feature = feature )
        {
            QObject::connect( feature.get(), &Feature::values_changed, this, [this]
            {
                if( auto feature = _feature.lock() )
                {
                    const auto& extremes = feature->extremes();
                    const auto range = extremes.maximum - extremes.minimum;
                    this->update_yaxis_bounds( { extremes.minimum - 0.01 * range, extremes.maximum + 0.01 * range } );
                    this->update_yaxis_domain( { extremes.minimum - 0.01 * range, extremes.maximum + 0.01 * range } );
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

    const auto content_rectangle = this->content_rectangle();
    const auto segmentation = _database.segmentation();

    auto hovered_statistics = std::optional<GroupedBoxplot::Statistics> {};

    const auto render_boxplot = [&](
        double position, const QColor& color,
        double minimum, double maximum,
        double average, double standard_deviation,
        double lower_quartile, double upper_quartile, double median )
    {
        const auto xscreen = this->world_to_screen_x( position );
        const auto xleft = this->world_to_screen_x( position - 0.25 );
        const auto xright = this->world_to_screen_x( position + 0.25 );

        const auto hovered = _cursor_position.x() >= xleft && _cursor_position.x() <= xright;
        const auto thickness = hovered ? 2.0 : 1.0;
        if( hovered )
        {
            hovered_statistics = GroupedBoxplot::Statistics {
                .minimum = minimum,
                .maximum = maximum,
                .average = average,
                .standard_deviation = standard_deviation,
                .lower_quartile = lower_quartile,
                .upper_quartile = upper_quartile,
                .median = median
            };
        }

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

        painter.setPen( QPen { color, thickness } );
        painter.setBrush( brush_color );
        painter.drawRect( QRectF { QPointF { xleft, yupper_quartile }, QPointF { xright, ylower_quartile } } );
        painter.drawLine( QPointF { xleft, yminimum }, QPointF { xright, yminimum } );
        painter.drawLine( QPointF { xleft, ymaximum }, QPointF { xright, ymaximum } );
        painter.drawLine( QPointF { xscreen, ylower_quartile }, QPointF { xscreen, yminimum } );
        painter.drawLine( QPointF { xscreen, yupper_quartile }, QPointF { xscreen, ymaximum } );

        painter.setPen( QPen { color, thickness + 1.0 } );
        painter.drawLine( QPointF { xleft, ymedian }, QPointF { xright, ymedian } );

        painter.setPen( QPen { color, thickness, Qt::DashLine } );
        painter.drawLine( QPointF { xleft, yaverage }, QPointF { xscreen, ystandard_deviation_lower } );
        painter.drawLine( QPointF { xleft, yaverage }, QPointF { xscreen, ystandard_deviation_upper } );
        painter.drawLine( QPointF { xright, yaverage }, QPointF { xscreen, ystandard_deviation_lower } );
        painter.drawLine( QPointF { xright, yaverage }, QPointF { xscreen, ystandard_deviation_upper } );
        painter.drawLine( QPointF { xleft, yaverage }, QPointF { xright, yaverage } );

        painter.setPen( Qt::NoPen );
        painter.setBrush( color );
        painter.drawEllipse( QPointF { xscreen, yaverage }, 5.0, 5.0 );
    };

    if( auto feature = _feature.lock() )
    {
        const auto& extremes = feature->extremes();
        const auto& moments = feature->moments();
        const auto& quantiles = feature->quantiles();

        render_boxplot(
            0.5, config::palette[600],
            extremes.minimum, extremes.maximum,
            moments.average, moments.standard_deviation,
            quantiles.lower_quartile, quantiles.upper_quartile, quantiles.median
        );
    }

    const auto& segmentation_statistics = _boxplot.statistics();
    for( uint32_t segment_number = 1; segment_number < segmentation_statistics.size(); ++segment_number )
    {
        const auto segment = segmentation->segment( segment_number );
        if( segment->element_count() > 0 )
        {
            const auto& statistics = segmentation_statistics[segment_number];
            render_boxplot(
                segment_number + 0.5, segment->color().qcolor(),
                statistics.minimum, statistics.maximum,
                statistics.average, statistics.standard_deviation,
                statistics.lower_quartile, statistics.upper_quartile, statistics.median
            );
        }
    }

    if( const auto element_index = _database.highlighted_element_index(); element_index.has_value() )
    {
        if( const auto feature = _feature.lock() )
        {
            const auto& feature_values = feature->values();
            const auto yscreen = this->world_to_screen_y( feature_values[*element_index] );
            painter.setPen( QPen { QColor { config::palette[500] }, 1.0, Qt::DashLine } );
            painter.drawLine(
                QPointF { static_cast<double>( content_rectangle.left() ), yscreen },
                QPointF { static_cast<double>( content_rectangle.right() ), yscreen }
            );
        }
    }

    if( hovered_statistics.has_value() )
    {
        const auto range = hovered_statistics->maximum - hovered_statistics->minimum;
        const auto precision = utility::stepsize_to_precision( range ) + 3;

        auto labels_string = QString {
            "average:"
            "\nstdev:"
            "\nmaximum:"
            "\nupper:"
            "\nmedian:"
            "\nlower:"
            "\nminimum:"
        };
        auto values_string = QString {
            QString::number( hovered_statistics->average, 'f', precision ) +
            '\n' + QString::number( hovered_statistics->standard_deviation, 'f', precision ) +
            '\n' + QString::number( hovered_statistics->maximum, 'f', precision ) +
            '\n' + QString::number( hovered_statistics->upper_quartile, 'f', precision ) +
            '\n' + QString::number( hovered_statistics->median, 'f', precision ) +
            '\n' + QString::number( hovered_statistics->lower_quartile, 'f', precision ) +
            '\n' + QString::number( hovered_statistics->minimum, 'f', precision )
        };

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

        context_menu.addAction( "Reset View", [this] { this->reset_view(); } );
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
        auto file = QFile { filepath };
        if( !file.open( QFile::WriteOnly | QFile::Text ) )
        {
            QMessageBox::critical( nullptr, "", "Failed to open file" );
            return;
        }

        auto stream = QTextStream { &file };
        stream << "identifier,element_count,minimum,maximum,average,standard_deviation,lower_quartile,upper_quartile,median\n";

        const auto write_boxplot = [&stream]( const QString& identifier, uint32_t element_count,
            double minimum, double maximum,
            double average, double standard_deviation,
            double lower_quartile, double upper_quartile, double median )
        {
            stream << identifier << "," << element_count
                << ',' << minimum << ',' << maximum
                << ',' << average << ',' << standard_deviation
                << ',' << lower_quartile << ',' << upper_quartile << ',' << median << '\n';
        };

        if( auto feature = _feature.lock() )
        {
            const auto& extremes = feature->extremes();
            const auto& moments = feature->moments();
            const auto& quantiles = feature->quantiles();

            write_boxplot(
                "Dataset", feature->element_count(),
                extremes.minimum, extremes.maximum,
                moments.average, moments.standard_deviation,
                quantiles.lower_quartile, quantiles.upper_quartile, quantiles.median
            );
        }

        const auto segmentation = _database.segmentation();
        const auto& segmentation_statistics = _boxplot.statistics();
        for( uint32_t segment_number = 0; segment_number < segmentation->segment_count(); ++segment_number )
        {
            const auto segment = segmentation->segment( segment_number );
            const auto& statistics = segmentation_statistics[segment_number];
            write_boxplot(
                segment->identifier(), segment->element_count(),
                statistics.minimum, statistics.maximum,
                statistics.average, statistics.standard_deviation,
                statistics.lower_quartile, statistics.upper_quartile, statistics.median
            );
        }
    }
}
