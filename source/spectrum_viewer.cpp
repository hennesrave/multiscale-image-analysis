#include "spectrum_viewer.hpp"

#include "dataset.hpp"
#include "feature.hpp"
#include "segmentation.hpp"
#include "utility.hpp"

#include <qactiongroup.h>
#include <qevent.h>
#include <qfiledialog.h>
#include <qlabel.h>
#include <qmenu.h>
#include <qmessagebox.h>
#include <qpainter.h>
#include <qwidgetaction.h>

SpectrumViewer::SpectrumViewer( Database& database ) : _database { database }
{
    this->setMouseTracking( true );

    const auto dataset = _database.dataset();
    QObject::connect( dataset.get(), &Dataset::statistics_changed, this, &SpectrumViewer::update_hovered_feature );
    QObject::connect( dataset.get(), &Dataset::statistics_changed, this, [this]
    {
        const auto dataset = _database.dataset();
        const auto& statistics = dataset->statistics();

        auto xaxis = vec2<double> { dataset->channel_position( 0 ), dataset->channel_position( dataset->channel_count() - 1 ) };
        auto yaxis = vec2<double> { statistics.minimum, statistics.maximum };

        const auto xaxis_range = xaxis.y - xaxis.x;
        const auto yaxis_range = yaxis.y - yaxis.x;

        const auto xaxis_center = ( xaxis.x + xaxis.y ) / 2.0;
        const auto yaxis_center = ( yaxis.x + yaxis.y ) / 2.0;

        xaxis = vec2<double> {
            xaxis_center - 0.5 * 1.01 * xaxis_range,
            xaxis_center + 0.5 * 1.01 * xaxis_range
        };
        yaxis = vec2<double> {
            yaxis_center - 0.5 * 1.01 * yaxis_range,
            yaxis_center + 0.5 * 1.01 * yaxis_range
        };

        this->update_xaxis_bounds( xaxis );
        this->update_yaxis_bounds( yaxis );

        this->update_xaxis_domain( xaxis );
        this->update_yaxis_domain( yaxis );

        auto stepsize = std::numeric_limits<double>::max();
        for( uint32_t channel_index = 0; channel_index < dataset->channel_count() - 1; ++channel_index )
        {
            const auto distance = dataset->channel_position( channel_index + 1 ) - dataset->channel_position( channel_index );
            stepsize = std::min( stepsize, distance );
        }
        const auto precision = this->stepsize_to_precision( stepsize );

        auto ticks = std::vector<Tick>( dataset->channel_count() );
        for( uint32_t channel_index = 0; channel_index < dataset->channel_count(); ++channel_index )
        {
            ticks[channel_index] = Tick {
                .position = dataset->channel_position( channel_index ),
                .label = dataset->channel_identifier( channel_index )
            };
        }
        this->update_xaxis_ticks( std::move( ticks ) );
        this->update();
    } );
    QObject::connect( dataset.get(), &Dataset::segmentation_statistics_changed, this, qOverload<>( &QWidget::update ) );

    const auto features = _database.features();
    QObject::connect( features.get(), &CollectionObject::object_appended, this, [this] ( QSharedPointer<QObject> object )
    {
        if( auto feature = object.objectCast<DatasetChannelsFeature>() )
        {
            QObject::connect( feature.data(), &DatasetChannelsFeature::channel_range_changed, this, &SpectrumViewer::update_hovered_feature );
            QObject::connect( feature.data(), &DatasetChannelsFeature::channel_range_changed, this, qOverload<>( &QWidget::update ) );
            this->update_hovered_feature();
        }
    } );
    QObject::connect( features.get(), &CollectionObject::object_removed, this, &SpectrumViewer::update_hovered_feature );

    const auto segmentation = _database.segmentation();
    QObject::connect( segmentation.get(), &Segmentation::segment_color_changed, this, qOverload<>( &QWidget::update ) );

    QObject::connect( &_database, &Database::highlighted_element_index_changed, this, qOverload<>( &QWidget::update ) );
    QObject::connect( &_database, &Database::highlighted_channel_index_changed, this, &SpectrumViewer::update_hovered_feature );
    QObject::connect( this, &SpectrumViewer::hovered_feature_changed, this, qOverload<>( &QWidget::update ) );
}

void SpectrumViewer::paintEvent( QPaintEvent* event )
{
    auto painter = QPainter { this };
    painter.setRenderHint( QPainter::Antialiasing, true );
    painter.setClipRect( this->content_rectangle() );

    const auto dataset = _database.dataset();
    const auto segmentation = _database.segmentation();
    const auto highlighted_element_index = _database.highlighted_element_index();
    const auto highlighted_channel_index = _database.highlighted_channel_index();
    const auto content_rectangle = this->content_rectangle();

    // Gather spectra
    auto polyline = QPolygonF {};
    for( uint32_t channel_index = 0; channel_index < dataset->channel_count(); ++channel_index )
    {
        const auto xscreen = this->world_to_screen_x( dataset->channel_position( channel_index ) );
        polyline.append( QPointF { xscreen, 0.0 } );
    }

    struct Spectrum
    {
        QPolygonF polyline;
        QColor color;
    };
    auto spectra = std::vector<Spectrum> {};

    const auto statistics_accessor =
        _statistics_mode == StatisticsMode::eAverage ? &Dataset::Statistics::channel_averages
        : _statistics_mode == StatisticsMode::eMinimum ? &Dataset::Statistics::channel_minimums
        : &Dataset::Statistics::channel_maximums;
    const auto& statistics = dataset->statistics();
    for( uint32_t channel_index = 0; channel_index < dataset->channel_count(); ++channel_index )
    {
        const auto yscreen = this->world_to_screen_y( ( statistics.*statistics_accessor )[channel_index] );
        polyline[channel_index].setY( yscreen );
    }
    spectra.push_back( Spectrum { polyline, QColor { config::palette[900] } } );


    const auto& segmentation_statistics = dataset->segmentation_statistics( segmentation );
    for( uint32_t segment_number = 1; segment_number < segmentation->segment_count(); ++segment_number )
    {
        const auto& segment = segmentation->segment( segment_number );
        if( segment->element_count() > 0 )
        {
            const auto& statistics = segmentation_statistics[segment_number];
            for( uint32_t channel_index = 0; channel_index < dataset->channel_count(); ++channel_index )
            {
                const auto yscreen = this->world_to_screen_y( ( statistics.*statistics_accessor )[channel_index] );
                polyline[channel_index].setY( yscreen );
            }
            spectra.push_back( Spectrum { polyline, segment->color().qcolor() } );
        }
    }

    if( highlighted_element_index.has_value() )
    {
        const auto element_intensities = dataset->element_intensities( *highlighted_element_index );
        for( uint32_t channel_index = 0; channel_index < dataset->channel_count(); ++channel_index )
        {
            const auto yscreen = this->world_to_screen_y( element_intensities[channel_index] );
            polyline[channel_index].setY( yscreen );
        }
        spectra.push_back( Spectrum { polyline, QColor { config::palette[500] } } );
    }

    // Render lollipop lines
    if( _visualization_mode == VisualizationMode::eLollipop )
    {
        const auto yscreen_baseline = this->world_to_screen_y( 0.0 );

        for( uint32_t channel_index = 0; channel_index < dataset->channel_count(); ++channel_index )
        {
            auto yminimum = static_cast<double>( yscreen_baseline );
            auto ymaximum = static_cast<double>( yscreen_baseline );

            for( const auto& spectrum : spectra )
            {
                const auto yscreen = spectrum.polyline[channel_index].y();
                yminimum = std::min( yminimum, yscreen );
                ymaximum = std::max( ymaximum, yscreen );
            }

            painter.setPen( QPen { QBrush { config::palette[300] }, 1.0 } );
            painter.drawLine( QPointF { polyline[channel_index].x(), yminimum }, QPointF { polyline[channel_index].x(), ymaximum } );
        }
    }

    // Render features
    for( const auto object : *_database.features() ) if( auto feature = object.objectCast<DatasetChannelsFeature>() )
    {
        const auto color = QColor { ( feature == _selected_feature.feature ) ? config::palette[900] : config::palette[400] };
        const auto channel_range = feature->channel_range();
        if( channel_range.x == channel_range.y )
        {
            const auto xscreen = this->world_to_screen_x( dataset->channel_position( channel_range.x ) );
            painter.setPen( QPen { QBrush { color }, 1.0 } );
            painter.drawLine( QPointF { xscreen, static_cast<qreal>( content_rectangle.bottom() ) }, QPointF { xscreen, static_cast<qreal>( content_rectangle.top() ) } );
        }
        else
        {
            const auto xscreen_lower = this->world_to_screen_x( dataset->channel_position( channel_range.x ) );
            const auto xscreen_upper = this->world_to_screen_x( dataset->channel_position( channel_range.y ) );

            const auto rectangle = QRectF { QPointF { xscreen_lower, static_cast<qreal>( content_rectangle.top() ) }, QPointF { xscreen_upper, static_cast<qreal>( content_rectangle.bottom() ) } };
            auto rectangle_color = color;
            rectangle_color.setAlpha( 20 );
            painter.fillRect( rectangle, rectangle_color );

            painter.setPen( QPen { QBrush { color }, 1.0 } );
            painter.drawLine( QPointF { xscreen_lower, static_cast<qreal>( content_rectangle.bottom() ) }, QPointF { xscreen_lower, static_cast<qreal>( content_rectangle.top() ) } );
            painter.drawLine( QPointF { xscreen_upper, static_cast<qreal>( content_rectangle.bottom() ) }, QPointF { xscreen_upper, static_cast<qreal>( content_rectangle.top() ) } );
        }
    }

    // Render highlighted channel
    if( highlighted_channel_index.has_value() )
    {
        const auto channel_index = *highlighted_channel_index;

        const auto xscreen = this->world_to_screen_x( dataset->channel_position( channel_index ) );
        painter.setPen( QPen { QBrush { config::palette[400] }, 1.0, Qt::DashLine } );
        painter.drawLine( QPointF { xscreen, static_cast<qreal>( content_rectangle.bottom() ) }, QPointF { xscreen, static_cast<qreal>( content_rectangle.top() ) } );
    }

    // Render spectra or lollipop handles
    if( _visualization_mode == VisualizationMode::eLine )
    {
        for( const auto& spectrum : spectra )
        {
            painter.setPen( QPen { QBrush { spectrum.color }, 2.0 } );
            painter.drawPolyline( spectrum.polyline );
        }
    }
    else if( _visualization_mode == VisualizationMode::eLollipop )
    {
        painter.setPen( Qt::NoPen );
        for( const auto& spectrum : spectra )
        {
            painter.setBrush( QBrush { spectrum.color } );
            for( uint32_t channel_index = 0; channel_index < dataset->channel_count(); ++channel_index )
            {
                painter.drawEllipse( spectrum.polyline[channel_index], 3.0, 3.0 );
            }
        }
    }

    // Render selected feature range
    if( auto feature = _selected_feature.feature.lock() )
    {
        const auto channel_range = feature->channel_range();
        if( channel_range.x != channel_range.y )
        {
            const auto position_lower = dataset->channel_position( channel_range.x );
            const auto position_upper = dataset->channel_position( channel_range.y );

            const auto xscreen_lower = this->world_to_screen_x( position_lower );
            const auto xscreen_upper = this->world_to_screen_x( position_upper );

            const auto string = QString { "\u0394 = " } + QString::number( position_upper - position_lower, 'f', this->xaxis().precision );

            auto rectangle = painter.fontMetrics().boundingRect( string ).toRectF().marginsAdded( QMarginsF { 5.0, 2.0, 5.0, 2.0 } );
            rectangle.moveCenter( QPointF { ( xscreen_lower + xscreen_upper ) / 2.0, 0.0 } );
            rectangle.moveTop( content_rectangle.top() + 10.0 );

            if( rectangle.left() < content_rectangle.left() + 5 ) rectangle.moveLeft( content_rectangle.left() + 5 );
            if( rectangle.right() > content_rectangle.right() - 5 ) rectangle.moveRight( content_rectangle.right() - 5 );

            painter.setPen( Qt::NoPen );
            painter.setBrush( QBrush { QColor { 255, 255, 255, 200 } } );
            painter.drawRoundedRect( rectangle, 2.0, 2.0 );

            painter.setPen( Qt::black );
            painter.drawText( rectangle, Qt::AlignCenter, string );
        }
    }

    // Render hovered value
    if( highlighted_channel_index.has_value() )
    {
        struct
        {
            double distance = std::numeric_limits<double>::max();
            QPointF screen;
            QColor color;
        } hovered_value;

        for( const auto& spectrum : spectra )
        {
            const auto yscreen = spectrum.polyline[*highlighted_channel_index].y();
            const auto distance = std::abs( yscreen - _cursor_position.y() );
            if( distance < hovered_value.distance )
            {
                hovered_value = { distance, spectrum.polyline[*highlighted_channel_index], spectrum.color };
            }
        }

        if( hovered_value.distance != std::numeric_limits<double>::max() )
        {
            painter.setPen( Qt::NoPen );
            painter.setBrush( hovered_value.color );
            painter.drawEllipse( hovered_value.screen, 5.0, 5.0 );

            const auto world = this->screen_to_world( hovered_value.screen );
            const auto string = QString {
                "x = " + QString::number( world.x(), 'f', this->xaxis().precision ) + ", " +
                "y = " + QString::number( world.y(), 'f', this->yaxis().precision + 2 )
            };

            auto rectangle = painter.fontMetrics().boundingRect( string ).toRectF().marginsAdded( QMarginsF { 5.0, 2.0, 5.0, 2.0 } );
            rectangle.moveTopLeft( hovered_value.screen + QPointF { 10.0, 10.0 } );
            if( rectangle.right() > content_rectangle.right() - 5 )
            {
                rectangle.moveRight( content_rectangle.right() - 5 );
            }
            if( rectangle.bottom() > content_rectangle.bottom() - 5 )
            {
                rectangle.moveBottom( content_rectangle.bottom() - 5 );
            }
            if( rectangle.top() < content_rectangle.top() + 5 )
            {
                rectangle.moveTop( content_rectangle.top() + 5 );
            }

            painter.setPen( Qt::NoPen );
            painter.setBrush( QBrush { QColor { 255, 255, 255, 200 } } );
            painter.drawRoundedRect( rectangle, 2.0, 2.0 );

            painter.setPen( Qt::black );
            painter.drawText( rectangle, Qt::AlignCenter, string );
        }
    }

    painter.setClipRect( this->rect() );
    PlottingWidget::paintEvent( event );
}

void SpectrumViewer::mousePressEvent( QMouseEvent* event )
{
    const auto dataset = _database.dataset();
    const auto channel_index = this->screen_to_channel_index( event->position().x() );

    if( event->button() == Qt::LeftButton )
    {
        if( _hovered_feature.feature )
        {
            _selected_feature = _hovered_feature;
            if( auto feature = _selected_feature.feature.lock() )
            {
                auto channel_range = feature->channel_range();
                if( _selected_feature.both )
                {
                    if( channel_range.x == _selected_feature.channel_index ) channel_range.x = channel_index;
                    if( channel_range.y == _selected_feature.channel_index ) channel_range.y = channel_index;
                }
                else
                {
                    if( channel_range.x == _selected_feature.channel_index ) channel_range.x = channel_index;
                    else if( channel_range.y == _selected_feature.channel_index ) channel_range.y = channel_index;
                }
                _selected_feature.channel_index = channel_index;
                feature->update_channel_range( channel_range );
            }
        }
        else
        {
            auto feature = QSharedPointer<DatasetChannelsFeature> { new DatasetChannelsFeature {
                dataset,
                Range<uint32_t> { channel_index, channel_index },
                DatasetChannelsFeature::Reduction::eAccumulate,
                DatasetChannelsFeature::BaselineCorrection::eNone
            } };
            _database.features()->append( feature );
            _selected_feature = FeatureHandle { feature, channel_index, false };
        }
    }
    else if( event->button() == Qt::RightButton )
    {
        auto menu = QMenu {};

        if( auto feature = _hovered_feature.feature.lock() )
        {
            auto reduction_menu = menu.addMenu( "Reduction" );

            auto reduction_mode_accumulate = reduction_menu->addAction( "Accumulate", [feature] { feature->update_reduction( DatasetChannelsFeature::Reduction::eAccumulate ); } );
            reduction_mode_accumulate->setCheckable( true );
            reduction_mode_accumulate->setChecked( feature->reduction() == DatasetChannelsFeature::Reduction::eAccumulate );

            auto recution_mode_integrate = reduction_menu->addAction( "Integrate", [feature] { feature->update_reduction( DatasetChannelsFeature::Reduction::eIntegrate ); } );
            recution_mode_integrate->setCheckable( true );
            recution_mode_integrate->setChecked( feature->reduction() == DatasetChannelsFeature::Reduction::eIntegrate );

            auto reduction_action_group = new QActionGroup { reduction_menu };
            reduction_action_group->setExclusive( true );
            reduction_action_group->addAction( reduction_mode_accumulate );
            reduction_action_group->addAction( recution_mode_integrate );

            auto baseline_correction_menu = menu.addMenu( "Baseline Correction" );
            auto baseline_correction_none = baseline_correction_menu->addAction( "None", [feature] { feature->update_baseline_correction( DatasetChannelsFeature::BaselineCorrection::eNone ); } );
            baseline_correction_none->setCheckable( true );
            baseline_correction_none->setChecked( feature->baseline_correction() == DatasetChannelsFeature::BaselineCorrection::eNone );

            auto baseline_correction_minimum = baseline_correction_menu->addAction( "Minimum", [feature] { feature->update_baseline_correction( DatasetChannelsFeature::BaselineCorrection::eMinimum ); } );
            baseline_correction_minimum->setCheckable( true );
            baseline_correction_minimum->setChecked( feature->baseline_correction() == DatasetChannelsFeature::BaselineCorrection::eMinimum );

            auto baseline_correction_linear = baseline_correction_menu->addAction( "Linear", [feature] { feature->update_baseline_correction( DatasetChannelsFeature::BaselineCorrection::eLinear ); } );
            baseline_correction_linear->setCheckable( true );
            baseline_correction_linear->setChecked( feature->baseline_correction() == DatasetChannelsFeature::BaselineCorrection::eLinear );

            auto baseline_correction_action_group = new QActionGroup { baseline_correction_menu };
            baseline_correction_action_group->setExclusive( true );
            baseline_correction_action_group->addAction( baseline_correction_none );
            baseline_correction_action_group->addAction( baseline_correction_minimum );
            baseline_correction_action_group->addAction( baseline_correction_linear );

            auto action = menu.addAction( "Remove Feature", [this, feature] { _database.features()->remove( feature ); } );
            action->setEnabled( true );

            menu.addSeparator();
        }

        {
            auto visualization_menu = menu.addMenu( "Visualization" );
            auto statistics_mode_average = visualization_menu->addAction( "Average" );
            statistics_mode_average->setCheckable( true );
            statistics_mode_average->setChecked( _statistics_mode == StatisticsMode::eAverage );
            QObject::connect( statistics_mode_average, &QAction::triggered, this, [this] { _statistics_mode = StatisticsMode::eAverage; } );

            auto statistics_mode_minimum = visualization_menu->addAction( "Minimum" );
            statistics_mode_minimum->setCheckable( true );
            statistics_mode_minimum->setChecked( _statistics_mode == StatisticsMode::eMinimum );
            QObject::connect( statistics_mode_minimum, &QAction::triggered, this, [this] { _statistics_mode = StatisticsMode::eMinimum; } );

            auto statistics_mode_maximum = visualization_menu->addAction( "Maximum" );
            statistics_mode_maximum->setCheckable( true );
            statistics_mode_maximum->setChecked( _statistics_mode == StatisticsMode::eMaximum );
            QObject::connect( statistics_mode_maximum, &QAction::triggered, this, [this] { _statistics_mode = StatisticsMode::eMaximum; } );

            auto actions_statistics_mode = new QActionGroup { this };
            actions_statistics_mode->setExclusive( true );
            actions_statistics_mode->addAction( statistics_mode_average );
            actions_statistics_mode->addAction( statistics_mode_minimum );
            actions_statistics_mode->addAction( statistics_mode_maximum );

            visualization_menu->addSeparator();
            auto visualization_mode_spectrum = visualization_menu->addAction( "Line" );
            visualization_mode_spectrum->setCheckable( true );
            visualization_mode_spectrum->setChecked( _visualization_mode == VisualizationMode::eLine );
            QObject::connect( visualization_mode_spectrum, &QAction::triggered, this, [this] { _visualization_mode = VisualizationMode::eLine; } );

            auto visualization_mode_lollipop = visualization_menu->addAction( "Lollipop" );
            visualization_mode_lollipop->setCheckable( true );
            visualization_mode_lollipop->setChecked( _visualization_mode == VisualizationMode::eLollipop );
            QObject::connect( visualization_mode_lollipop, &QAction::triggered, this, [this] { _visualization_mode = VisualizationMode::eLollipop; } );

            auto actions_visualization_mode = new QActionGroup { this };
            actions_visualization_mode->setExclusive( true );
            actions_visualization_mode->addAction( visualization_mode_spectrum );
            actions_visualization_mode->addAction( visualization_mode_lollipop );
        }

        menu.addAction( "Reset View", [this] { this->reset_view(); } );
        menu.addSeparator();

        auto baseline_correction_menu = menu.addMenu( "Baseline Correction" );
        baseline_correction_menu->addAction( "Minimum", [this]
        {
            const auto answer = QMessageBox::question( nullptr, "Baseline Correction", "This will apply a minimum baseline correction to the whole dataset.\nDo you want to continue?", QMessageBox::Yes | QMessageBox::No );
            if( answer == QMessageBox::Yes )
            {
                _database.dataset()->apply_baseline_correction_minimum();
            }
        } );
        baseline_correction_menu->addAction( "Linear", [this]
        {
            const auto answer = QMessageBox::question( nullptr, "Baseline Correction", "This will apply a linear baseline correction to the whole dataset.\nDo you want to continue?", QMessageBox::Yes | QMessageBox::No );
            if( answer == QMessageBox::Yes )
            {
                _database.dataset()->apply_baseline_correction_linear();
            }
        } );

        auto export_menu = menu.addMenu( "Export" );
        export_menu->addAction( "Spectra", [this] { this->export_spectra(); } );
        export_menu->addAction( "Dataset", [this] { this->export_dataset(); } );

        menu.exec( event->globalPosition().toPoint() );

        const auto cursor_position = this->mapFromGlobal( QCursor::pos() );
        if( this->rect().contains( cursor_position ) )
        {
            const auto channel_index = this->screen_to_channel_index( cursor_position.x() );
            _database.update_highlighted_channel_index( channel_index );
        }
    }

    this->update();
}
void SpectrumViewer::mouseReleaseEvent( QMouseEvent* event )
{
    if( event->button() == Qt::LeftButton )
    {
        if( auto feature = _selected_feature.feature.lock() )
        {
            if( const auto channel_range = feature->channel_range(); channel_range.x == channel_range.y )
            {
                _selected_feature.both = true;
            }
        }
    }

    this->update();
}

void SpectrumViewer::enterEvent( QEnterEvent* event )
{
    QWidget::enterEvent( event );
    this->setFocus();
}
void SpectrumViewer::mouseMoveEvent( QMouseEvent* event )
{
    const auto channel_index = this->screen_to_channel_index( event->position().x() );
    if( event->buttons() == Qt::LeftButton )
    {
        if( auto feature = _selected_feature.feature.lock() )
        {
            auto channel_range = feature->channel_range();
            if( _selected_feature.both )
            {
                if( channel_range.x == _selected_feature.channel_index ) channel_range.x = channel_index;
                if( channel_range.y == _selected_feature.channel_index ) channel_range.y = channel_index;
            }
            else
            {
                if( channel_range.x == _selected_feature.channel_index ) channel_range.x = channel_index;
                else if( channel_range.y == _selected_feature.channel_index ) channel_range.y = channel_index;
            }
            _selected_feature.channel_index = channel_index;
            feature->update_channel_range( channel_range );
        }
    }

    _cursor_position = event->position();
    _database.update_highlighted_channel_index( channel_index );
    this->update();
}
void SpectrumViewer::leaveEvent( QEvent* event )
{
    _cursor_position = QPointF { -1.0, -1.0 };
    _database.update_highlighted_channel_index( std::nullopt );
}

void SpectrumViewer::keyPressEvent( QKeyEvent* event )
{
    const auto dataset = _database.dataset();

    if( event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace )
    {
        if( auto feature = _hovered_feature.feature.lock() )
        {
            _database.features()->remove( feature );
        }
    }
    else if( event->key() == Qt::Key_Left || event->key() == Qt::Key_Right )
    {
        if( auto feature = _selected_feature.feature.lock() )
        {
            auto channel_index = _selected_feature.channel_index;
            if( event->key() == Qt::Key_Left && channel_index > 0 ) --channel_index;
            else if( event->key() == Qt::Key_Right && channel_index < dataset->channel_count() - 1 ) ++channel_index;

            auto channel_range = feature->channel_range();
            if( _selected_feature.both )
            {
                if( channel_range.x == _selected_feature.channel_index ) channel_range.x = channel_index;
                if( channel_range.y == _selected_feature.channel_index ) channel_range.y = channel_index;
            }
            else
            {
                if( channel_range.x == _selected_feature.channel_index ) channel_range.x = channel_index;
                else if( channel_range.y == _selected_feature.channel_index ) channel_range.y = channel_index;
            }

            _selected_feature.channel_index = channel_index;
            feature->update_channel_range( channel_range );
        }
    }
}

uint32_t SpectrumViewer::screen_to_channel_index( double x ) const
{
    const auto dataset = _database.dataset();

    const auto xworld = this->screen_to_world_x( x );
    if( xworld <= dataset->channel_position( 0 ) )
    {
        return 0;
    }
    else if( xworld >= dataset->channel_position( dataset->channel_count() - 1 ) )
    {
        return dataset->channel_count() - 1;
    }
    else
    {
        for( uint32_t channel_index = 0; channel_index < dataset->channel_count() - 1; ++channel_index )
        {
            const auto current_position = dataset->channel_position( channel_index );
            const auto next_position = dataset->channel_position( channel_index + 1 );
            if( xworld >= current_position && xworld < next_position )
            {
                const auto fraction = ( xworld - current_position ) / ( next_position - current_position );
                return fraction < 0.5 ? channel_index : channel_index + 1;
            }
        }
    }

    return std::numeric_limits<size_t>::max();
}

void SpectrumViewer::update_hovered_feature()
{
    const auto dataset = _database.dataset();
    const auto highlighted_channel_index = _database.highlighted_channel_index();

    auto hovered_feature = FeatureHandle {};
    auto minimum_distance = 10.0;

    if( highlighted_channel_index.has_value() )
    {
        const auto highlighted_channel_screen = this->world_to_screen_x( dataset->channel_position( *highlighted_channel_index ) );

        for( const auto object : *_database.features() ) if( auto feature = object.objectCast<DatasetChannelsFeature>() )
        {
            const auto channel_range = feature->channel_range();

            if( channel_range.x == *highlighted_channel_index || channel_range.y == *highlighted_channel_index )
            {
                hovered_feature = FeatureHandle { feature, *highlighted_channel_index, channel_range.x == channel_range.y };
                break;
            }

            const auto channel_lower_screen = this->world_to_screen_x( dataset->channel_position( channel_range.x ) );
            const auto distance_lower = std::abs( channel_lower_screen - highlighted_channel_screen );
            if( distance_lower < minimum_distance )
            {
                minimum_distance = distance_lower;
                hovered_feature = FeatureHandle { feature, channel_range.x, channel_range.x == channel_range.y };
            }

            const auto channel_upper_screen = this->world_to_screen_x( dataset->channel_position( channel_range.y ) );
            const auto distance_upper = std::abs( channel_upper_screen - highlighted_channel_screen );
            if( distance_upper < minimum_distance )
            {
                minimum_distance = distance_upper;
                hovered_feature = FeatureHandle { feature, channel_range.y, channel_range.x == channel_range.y };
            }
        }
    }

    if( _hovered_feature.feature != hovered_feature.feature || _hovered_feature.channel_index != hovered_feature.channel_index )
    {
        emit hovered_feature_changed( _hovered_feature = hovered_feature );
        this->setCursor( _hovered_feature.feature ? Qt::SizeHorCursor : Qt::ArrowCursor );
    }

    this->update();
}
void SpectrumViewer::baseline_correction() const
{

}
void SpectrumViewer::export_spectra() const
{
    const auto filepath = QFileDialog::getSaveFileName( nullptr, "Export Spectra...", "", "*.csv" );
    if( !filepath.isEmpty() )
    {
        auto file = QFile { filepath };
        if( !file.open( QFile::WriteOnly | QFile::Text ) )
        {
            QMessageBox::critical( nullptr, "", "Failed to open file" );
            return;
        }

        auto stream = QTextStream { &file };
        stream << "identifier,count,statistic";

        const auto dataset = _database.dataset();
        for( uint32_t channel_index = 0; channel_index < dataset->channel_count(); ++channel_index )
        {
            stream << ',' << dataset->channel_identifier( channel_index );
        }
        stream << '\n';

        const auto write_statistics = [&stream] ( const QString& identifier, uint32_t element_count, const Dataset::Statistics& statistics )
        {
            stream << identifier << "," << element_count << ",average";
            for( const auto value : statistics.channel_averages ) stream << ',' << value;
            stream << '\n';

            stream << identifier << "," << element_count << ",minimum";
            for( const auto value : statistics.channel_minimums ) stream << ',' << value;
            stream << '\n';

            stream << identifier << "," << element_count << ",maximum";
            for( const auto value : statistics.channel_maximums ) stream << ',' << value;
            stream << '\n';
        };

        const auto& statistics = dataset->statistics();
        write_statistics( "Dataset", dataset->element_count(), statistics );

        const auto& segmentation_statistics = dataset->segmentation_statistics( _database.segmentation() );
        for( uint32_t segment_number = 1; segment_number < _database.segmentation()->segment_count(); ++segment_number )
        {
            const auto& segment = _database.segmentation()->segment( segment_number );
            write_statistics( segment->identifier(), segment->element_count(), segmentation_statistics[segment_number] );
        }
    }
}
void SpectrumViewer::export_dataset() const
{
    const auto filepath = QFileDialog::getSaveFileName( nullptr, "Export Dataset...", "", "*.mia" );
    if( !filepath.isEmpty() )
    {
        auto stream = MIAFileStream {};
        if( !stream.open( filepath.toUtf8().constData(), std::ios::out ) )
        {
            QMessageBox::critical( nullptr, "", "Failed to open file" );
            return;
        }

        stream.write( std::string { "Dataset[SpatialMetadata]" } );

        const auto dataset = _database.dataset();
        stream.write( dataset->element_count() );
        stream.write( dataset->channel_count() );
        stream.write( *dataset->spatial_metadata() );
        stream.write( dataset->basetype() );

        dataset->visit( [&stream] ( const auto& dataset )
        {
            const auto& channel_positions = dataset.channel_positions();
            stream.write( channel_positions.data(), channel_positions.bytes() );

            const auto& intensities = dataset.intensities();
            stream.write( intensities.data(), intensities.bytes() );
        } );
    }
}
