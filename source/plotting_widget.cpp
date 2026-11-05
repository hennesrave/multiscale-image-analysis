#include "plotting_widget.hpp"

#include "number_input.hpp"

#include <qapplication.h>
#include <qclipboard.h>
#include <qbuffer.h>
#include <qevent.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qmenu.h>
#include <qmimedata.h>
#include <qpainter.h>
#include <qwidgetaction.h>

// ----- PlottingWidget ----- //

PlottingWidget::PlottingWidget()
{
    this->setFocusPolicy( Qt::WheelFocus );
    this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );

    QObject::connect( this, &PlottingWidget::margins_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::linewidth_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::ticklength_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::labelspacing_major_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::labelspacing_minor_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::xaxis_domain_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::yaxis_domain_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::xaxis_type_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::yaxis_type_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::xaxis_symlog_threshold_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::yaxis_symlog_threshold_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::xaxis_symlog_scale_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::yaxis_symlog_scale_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::xaxis_visible_changed, this, &PlottingWidget::compute_content_rectangle );
    QObject::connect( this, &PlottingWidget::yaxis_visible_changed, this, &PlottingWidget::compute_content_rectangle );
}

QSize PlottingWidget::sizeHint() const
{
    const auto width = _content_rectangle.left() + _margins.right();
    const auto height = ( this->height() - _content_rectangle.bottom() ) + _margins.top();
    return QSize { width, height };
}

void PlottingWidget::resizeEvent( QResizeEvent* event )
{
    QWidget::resizeEvent( event );
    this->compute_content_rectangle();
}
void PlottingWidget::wheelEvent( QWheelEvent* event )
{
    QWidget::wheelEvent( event );

    const auto cursor = event->position();

    const auto to_axis = [] ( const Axis& axis, const vec2<double>& range )
    {
        return vec2<double> { to_axis_space( axis, range.x ), to_axis_space( axis, range.y ) };
    };
    const auto from_axis = [] ( const Axis& axis, const vec2<double>& range )
    {
        return vec2<double> { from_axis_space( axis, range.x ), from_axis_space( axis, range.y ) };
    };

    auto xdomain = to_axis( _xaxis, _xaxis.domain );
    auto ydomain = to_axis( _yaxis, _yaxis.domain );
    const auto xbounds = to_axis( _xaxis, _xaxis.bounds );
    const auto ybounds = to_axis( _yaxis, _yaxis.bounds );

    const auto x = std::clamp( to_axis_space( _xaxis, this->screen_to_world_x( cursor.x() ) ), xdomain.x, xdomain.y );
    const auto y = std::clamp( to_axis_space( _yaxis, this->screen_to_world_y( cursor.y() ) ), ydomain.x, ydomain.y );

    const auto scaling = ( event->angleDelta().y() > 0 ) ? 1.1 : ( 1.0 / 1.1 );

    const auto xchange = ( xdomain.y - xdomain.x ) * ( 1.0 - scaling );
    const auto ychange = ( ydomain.y - ydomain.x ) * ( 1.0 - scaling );

    auto xscaling = ( x - xdomain.x ) / ( xdomain.y - xdomain.x );
    auto yscaling = ( y - ydomain.x ) / ( ydomain.y - ydomain.x );

    if( xscaling < 0.05 ) xscaling = 0.0;
    else if( xscaling > 0.95 ) xscaling = 1.0;

    if( yscaling < 0.05 ) yscaling = 0.0;
    else if( yscaling > 0.95 ) yscaling = 1.0;

    xdomain = vec2<double> { xdomain.x - xscaling * xchange / 2.0, xdomain.y + ( 1.0 - xscaling ) * xchange / 2.0 };
    ydomain = vec2<double> { ydomain.x - yscaling * ychange / 2.0, ydomain.y + ( 1.0 - yscaling ) * ychange / 2.0 };

    xdomain.x = std::clamp( xdomain.x, xbounds.x, xbounds.y );
    xdomain.y = std::clamp( xdomain.y, xbounds.x, xbounds.y );

    ydomain.x = std::clamp( ydomain.x, ybounds.x, ybounds.y );
    ydomain.y = std::clamp( ydomain.y, ybounds.x, ybounds.y );

    if( _xaxis.zoomable && cursor.x() > _content_rectangle.left() && ( xbounds.y - xbounds.x ) / ( xdomain.y - xdomain.x ) <= 10000.0 )
        _xaxis.domain = from_axis( _xaxis, xdomain );

    if( _yaxis.zoomable && cursor.y() < _content_rectangle.bottom() && ( ybounds.y - ybounds.x ) / ( ydomain.y - ydomain.x ) <= 10000.0 )
        _yaxis.domain = from_axis( _yaxis, ydomain );

    this->compute_content_rectangle();
}
void PlottingWidget::paintEvent( QPaintEvent* event )
{
    QWidget::paintEvent( event );

    auto painter = QPainter { this };
    this->render( painter );
}
void PlottingWidget::render( QPainter& painter )
{
    if( _xaxis.visible )
    {
        painter.setPen( QPen { QBrush { config::palette[700] }, static_cast<qreal>( _linewidth ) } );
        painter.drawLine( _content_rectangle.bottomLeft(), _content_rectangle.bottomRight() );
        this->paint_xaxis_ticks( painter );
    }
    if( _yaxis.visible )
    {
        painter.setPen( QPen { QBrush { config::palette[700] }, static_cast<qreal>( _linewidth ) } );
        painter.drawLine( _content_rectangle.bottomLeft(), _content_rectangle.topLeft() );
        this->paint_yaxis_ticks( painter );
    }
}

void PlottingWidget::paint_xaxis_ticks( QPainter& painter ) const
{
    const auto tickposition_top = _content_rectangle.bottom() + _linewidth;
    const auto tickposition_bottom = tickposition_top + _ticklength;

    auto previous_rectangle_right = _content_rectangle.left() - _labelspacing_major;
    for( const auto& tick : _xaxis.ticks )
    {
        if( tick.position >= _xaxis.domain.x && tick.position <= _xaxis.domain.y )
        {
            const auto xscreen = this->world_to_screen_x( tick.position );
            const auto tickposition = static_cast<int>( std::round( xscreen ) );

            auto rectangle = painter.fontMetrics().boundingRect( tick.label );
            rectangle.moveTop( tickposition_bottom + _labelspacing_minor );
            rectangle.moveLeft( static_cast<int>( std::round( xscreen - rectangle.width() / 2.0 ) ) );

            if( rectangle.left() < _content_rectangle.left() ) { rectangle.moveLeft( _content_rectangle.left() ); }
            if( rectangle.right() > _content_rectangle.right() - 1 ) { rectangle.moveRight( _content_rectangle.right() - 1 ); }

            if( rectangle.left() - previous_rectangle_right >= _labelspacing_major )
            {
                painter.setPen( config::palette[700] );
                painter.drawLine( QPoint { tickposition, tickposition_top }, QPoint { tickposition, tickposition_bottom } );

                painter.setPen( config::palette[900] );
                painter.drawText( rectangle, Qt::AlignHCenter | Qt::AlignTop | Qt::TextDontClip, tick.label );

                previous_rectangle_right = rectangle.right();
            }
        }
    }
}
void PlottingWidget::paint_yaxis_ticks( QPainter& painter ) const
{
    const auto tickposition_right = _content_rectangle.left() - _linewidth;
    const auto tickposition_left = tickposition_right - _ticklength;

    auto previous_rectangle_top = _content_rectangle.bottom() + _labelspacing_major;
    for( const auto& tick : _yaxis.ticks )
    {
        if( tick.position >= _yaxis.domain.x && tick.position <= _yaxis.domain.y )
        {
            const auto yscreen = this->world_to_screen_y( tick.position );
            const auto tickposition = static_cast<int>( std::round( yscreen ) );

            auto rectangle = painter.fontMetrics().boundingRect( tick.label );
            rectangle.moveRight( tickposition_left - _labelspacing_minor );
            rectangle.moveTop( static_cast<int>( std::round( yscreen - rectangle.height() / 2.0 ) ) );

            if( rectangle.bottom() > _content_rectangle.bottom() - 1 ) { rectangle.moveBottom( _content_rectangle.bottom() - 1 ); }
            if( rectangle.top() < _content_rectangle.top() ) { rectangle.moveTop( _content_rectangle.top() ); }

            if( previous_rectangle_top - rectangle.bottom() >= _labelspacing_major )
            {
                painter.setPen( config::palette[700] );
                painter.drawLine( QPoint { tickposition_left, tickposition }, QPoint { tickposition_right, tickposition } );

                painter.setPen( config::palette[900] );
                painter.drawText( rectangle, Qt::AlignHCenter | Qt::AlignTop | Qt::TextDontClip, tick.label );

                previous_rectangle_top = rectangle.top();
            }
        }
    }
}

const QMargins& PlottingWidget::margins() const noexcept
{
    return _margins;
}
void PlottingWidget::update_margins( const QMargins& margins ) noexcept
{
    if( _margins != margins )
    {
        emit margins_changed( _margins = margins );
    }
}

int PlottingWidget::linewidth() const noexcept
{
    return _linewidth;
}
void PlottingWidget::update_linewidth( int linewidth ) noexcept
{
    if( _linewidth != linewidth )
    {
        emit linewidth_changed( _linewidth = linewidth );
    }
}

int PlottingWidget::ticklength() const noexcept
{
    return _ticklength;
}
void PlottingWidget::update_ticklength( int ticklength )
{
    if( _ticklength != ticklength )
    {
        emit ticklength_changed( _ticklength = ticklength );
    }
}

int PlottingWidget::labelspacing_major() const noexcept
{
    return _labelspacing_major;
}
void PlottingWidget::update_labelspacing_major( int labelspacing_major ) noexcept
{
    if( _labelspacing_major != labelspacing_major )
    {
        emit labelspacing_major_changed( _labelspacing_major = labelspacing_major );
    }
}

int PlottingWidget::labelspacing_minor() const noexcept
{
    return _labelspacing_minor;
}
void PlottingWidget::update_labelspacing_minor( int labelspacing_minor ) noexcept
{
    if( _labelspacing_minor != labelspacing_minor )
    {
        emit labelspacing_minor_changed( _labelspacing_minor = labelspacing_minor );
    }
}

const PlottingWidget::Axis& PlottingWidget::xaxis() const noexcept
{
    return _xaxis;
}
const PlottingWidget::Axis& PlottingWidget::yaxis() const noexcept
{
    return _yaxis;
}

void PlottingWidget::update_xaxis_bounds( vec2<double> bounds ) noexcept
{
    if( _xaxis.bounds != bounds )
    {
        auto xaxis_domain = _xaxis.domain;
        xaxis_domain.x = std::clamp( xaxis_domain.x, bounds.x, bounds.y );
        xaxis_domain.y = std::clamp( xaxis_domain.y, bounds.x, bounds.y );
        this->update_xaxis_domain( xaxis_domain );
        emit xaxis_bounds_changed( _xaxis.bounds = bounds );
    }
}
void PlottingWidget::update_yaxis_bounds( vec2<double> bounds ) noexcept
{
    if( _yaxis.bounds != bounds )
    {
        auto yaxis_domain = _yaxis.domain;
        yaxis_domain.x = std::clamp( yaxis_domain.x, bounds.x, bounds.y );
        yaxis_domain.y = std::clamp( yaxis_domain.y, bounds.x, bounds.y );
        this->update_yaxis_domain( yaxis_domain );
        emit yaxis_bounds_changed( _yaxis.bounds = bounds );
    }
}

void PlottingWidget::update_xaxis_domain( vec2<double> domain ) noexcept
{
    domain.x = std::clamp( domain.x, _xaxis.bounds.x, _xaxis.bounds.y );
    domain.y = std::clamp( domain.y, _xaxis.bounds.x, _xaxis.bounds.y );
    if( _xaxis.domain != domain )
    {
        emit xaxis_domain_changed( _xaxis.domain = domain );
    }
}
void PlottingWidget::update_yaxis_domain( vec2<double> domain ) noexcept
{
    domain.x = std::clamp( domain.x, _yaxis.bounds.x, _yaxis.bounds.y );
    domain.y = std::clamp( domain.y, _yaxis.bounds.x, _yaxis.bounds.y );
    if( _yaxis.domain != domain )
    {
        emit yaxis_domain_changed( _yaxis.domain = domain );
    }
}

void PlottingWidget::update_xaxis_type( Axis::Type type ) noexcept
{
    if( _xaxis.type != type )
    {
        emit xaxis_type_changed( _xaxis.type = type );
    }
}
void PlottingWidget::update_yaxis_type( Axis::Type type ) noexcept
{
    if( _yaxis.type != type )
    {
        emit yaxis_type_changed( _yaxis.type = type );
    }
}

void PlottingWidget::update_xaxis_symlog_threshold( double threshold ) noexcept
{
    if( _xaxis.symlog_threshold != threshold )
    {
        emit xaxis_symlog_threshold_changed( _xaxis.symlog_threshold = threshold );
    }
}
void PlottingWidget::update_yaxis_symlog_threshold( double threshold ) noexcept
{
    if( _yaxis.symlog_threshold != threshold )
    {
        emit yaxis_symlog_threshold_changed( _yaxis.symlog_threshold = threshold );
    }
}

void PlottingWidget::update_xaxis_symlog_scale( double scale ) noexcept
{
    scale = std::clamp( scale, 0.0, 1.0 );
    if( _xaxis.symlog_scale != scale )
    {
        emit xaxis_symlog_scale_changed( _xaxis.symlog_scale = scale );
    }
}
void PlottingWidget::update_yaxis_symlog_scale( double scale ) noexcept
{
    scale = std::clamp( scale, 0.0, 1.0 );
    if( _yaxis.symlog_scale != scale )
    {
        emit yaxis_symlog_scale_changed( _yaxis.symlog_scale = scale );
    }
}

void PlottingWidget::update_xaxis_visible( bool visible ) noexcept
{
    if( _xaxis.visible != visible )
    {
        emit xaxis_visible_changed( _xaxis.visible = visible );
        this->compute_content_rectangle();
    }
}
void PlottingWidget::update_yaxis_visible( bool visible ) noexcept
{
    if( _yaxis.visible != visible )
    {
        emit yaxis_visible_changed( _yaxis.visible = visible );
        this->compute_content_rectangle();
    }
}

void PlottingWidget::update_xaxis_zoomable( bool zoomable ) noexcept
{
    if( _xaxis.zoomable != zoomable )
    {
        emit xaxis_zoomable_changed( _xaxis.zoomable = zoomable );
    }
}
void PlottingWidget::update_yaxis_zoomable( bool zoomable ) noexcept
{
    if( _yaxis.zoomable != zoomable )
    {
        emit yaxis_zoomable_changed( _yaxis.zoomable = zoomable );
    }
}

void PlottingWidget::update_xaxis_ticks( std::vector<Tick> ticks )
{
    _xaxis.ticks = std::move( ticks );
    _xaxis.automatic_ticks = _xaxis.ticks.empty();
    this->compute_content_rectangle();
}
void PlottingWidget::update_yaxis_ticks( std::vector<Tick> ticks )
{
    _yaxis.ticks = std::move( ticks );
    _yaxis.automatic_ticks = _yaxis.ticks.empty();
    this->compute_content_rectangle();
}

const QRect& PlottingWidget::content_rectangle() const noexcept
{
    return _content_rectangle;
}

double PlottingWidget::world_to_screen_x( double value ) const
{
    const auto lower = to_axis_space( _xaxis, _xaxis.domain.x );
    const auto upper = to_axis_space( _xaxis, _xaxis.domain.y );
    return _content_rectangle.left() + ( ( to_axis_space( _xaxis, value ) - lower ) / ( upper - lower ) ) * ( _content_rectangle.width() - 1 );
}
double PlottingWidget::world_to_screen_y( double value ) const
{
    const auto lower = to_axis_space( _yaxis, _yaxis.domain.x );
    const auto upper = to_axis_space( _yaxis, _yaxis.domain.y );
    return _content_rectangle.bottom() - ( ( to_axis_space( _yaxis, value ) - lower ) / ( upper - lower ) ) * ( _content_rectangle.height() - 1 );
}
QPointF PlottingWidget::world_to_screen( const QPointF& world ) const
{
    return QPointF { this->world_to_screen_x( world.x() ), this->world_to_screen_y( world.y() ) };
}

double PlottingWidget::screen_to_world_x( double value ) const
{
    const auto lower = to_axis_space( _xaxis, _xaxis.domain.x );
    const auto upper = to_axis_space( _xaxis, _xaxis.domain.y );
    return from_axis_space( _xaxis, lower + ( ( value - _content_rectangle.left() ) / ( _content_rectangle.width() - 1 ) ) * ( upper - lower ) );
}
double PlottingWidget::screen_to_world_y( double value ) const
{
    const auto lower = to_axis_space( _yaxis, _yaxis.domain.x );
    const auto upper = to_axis_space( _yaxis, _yaxis.domain.y );
    return from_axis_space( _yaxis, lower - ( ( value - _content_rectangle.bottom() ) / ( _content_rectangle.height() - 1 ) ) * ( upper - lower ) );
}
QPointF PlottingWidget::screen_to_world( const QPointF& screen ) const
{
    return QPointF { this->screen_to_world_x( screen.x() ), this->screen_to_world_y( screen.y() ) };
}

void PlottingWidget::reset_view()
{
    _xaxis.domain = _xaxis.bounds;
    _yaxis.domain = _yaxis.bounds;

    emit xaxis_domain_changed( _xaxis.domain );
    emit yaxis_domain_changed( _yaxis.domain );
}
void PlottingWidget::populate_context_menu( QMenu& context_menu )
{
    auto axes_menu = context_menu.addMenu( "Axis Settings" );

    auto minimum_label_width = 0;
    for( const auto string : { "Bounds: ", "Threshold: ", "Scale: " } )
    {
        const auto width = QFontMetrics { axes_menu->font() }.boundingRect( string ).width();
        minimum_label_width = std::max( minimum_label_width, width );
    }

    for( auto& axis_reference : { std::ref( _xaxis ), std::ref( _yaxis ) } )
    {
        auto& axis = axis_reference.get();
        const auto is_xaxis = ( &axis == &_xaxis );
        const auto is_yaxis = ( &axis == &_yaxis );

        // Add a label to the axes menu
        auto label = new QLabel { is_xaxis? "<b>X-Axis</b>" : "<b>Y-Axis</b>" };
        label->setAlignment( Qt::AlignCenter );

        auto label_action = new QWidgetAction { axes_menu };
        label_action->setDefaultWidget( label );
        axes_menu->addAction( label_action );

        // Add a bounds input to the axes menu
        auto bounds_label = new QLabel { "Bounds: " };
        bounds_label->setFixedWidth( minimum_label_width );

        auto axis_lower = new Override<double> { axis.bounds.x, axis.domain.x == axis.bounds.x ? std::nullopt : std::optional<double> { axis.domain.x } };
        auto axis_upper = new Override<double> { axis.bounds.y, axis.domain.y == axis.bounds.y ? std::nullopt : std::optional<double> { axis.domain.y } };

        auto axis_lower_input = new NumberInput { *axis_lower, [=] ( double value ) { return value >= axis.bounds.x && value < axis_upper->value(); } };
        auto axis_upper_input = new NumberInput { *axis_upper, [=] ( double value ) { return value > axis_lower->value() && value <= axis.bounds.y; } };

        axis_lower->setParent( axis_lower_input );
        axis_upper->setParent( axis_upper_input );

        auto bounds_container_widget = new QWidget { axes_menu };
        auto bounds_container_layout = new QHBoxLayout { bounds_container_widget };
        bounds_container_layout->setContentsMargins( 20, 2, 20, 2 );
        bounds_container_layout->setSpacing( 3 );
        bounds_container_layout->addWidget( bounds_label );
        bounds_container_layout->addWidget( axis_lower_input );
        bounds_container_layout->addWidget( new QLabel { " \u2014 " } );
        bounds_container_layout->addWidget( axis_upper_input );

        auto bounds_widget_action = new QWidgetAction { axes_menu };
        bounds_widget_action->setDefaultWidget( bounds_container_widget );
        axes_menu->addAction( bounds_widget_action );

        // Add type selection to the axes menu
        auto type_linear_action = new QAction { "Linear", axes_menu };
        auto type_symlog_action = new QAction { "Symlog", axes_menu };

        type_linear_action->setCheckable( true );
        type_symlog_action->setCheckable( true );

        type_linear_action->setChecked( axis.type == Axis::Type::eLinear );
        type_symlog_action->setChecked( axis.type == Axis::Type::eSymmetricLogarithmic );

        axes_menu->addAction( type_linear_action );
        axes_menu->addAction( type_symlog_action );

        // Setup connections
        if( is_xaxis )
        {
            QObject::connect( axis_lower, &Override<double>::value_changed, this, [=]
            {
                this->update_xaxis_domain( vec2<double> { axis_lower->value(), _xaxis.domain.y } );
            } );
            QObject::connect( axis_upper, &Override<double>::value_changed, this, [=]
            {
                this->update_xaxis_domain( vec2<double> { _xaxis.domain.x, axis_upper->value() } );
            } );
            QObject::connect( type_linear_action, &QAction::triggered, this, [=]
            {
                this->update_xaxis_type( Axis::Type::eLinear );
            } );
            QObject::connect( type_symlog_action, &QAction::triggered, this, [=]
            {
                this->update_xaxis_type( Axis::Type::eSymmetricLogarithmic );
            } );
        }
        else if( is_yaxis )
        {
            QObject::connect( axis_lower, &Override<double>::value_changed, this, [=]
            {
                this->update_yaxis_domain( vec2<double> { axis_lower->value(), _yaxis.domain.y } );
            } );
            QObject::connect( axis_upper, &Override<double>::value_changed, this, [=]
            {
                this->update_yaxis_domain( vec2<double> { _yaxis.domain.x, axis_upper->value() } );
            } );
            QObject::connect( type_linear_action, &QAction::triggered, this, [=]
            {
                this->update_yaxis_type( Axis::Type::eLinear );
            } );
            QObject::connect( type_symlog_action, &QAction::triggered, this, [=]
            {
                this->update_yaxis_type( Axis::Type::eSymmetricLogarithmic );
            } );
        }

        if( axis.type ==Axis::Type::eSymmetricLogarithmic )
        {
            // Add symlog threshold input to the axes menu
            auto threshold_label = new QLabel { "Threshold: " };
            threshold_label->setFixedWidth( minimum_label_width );

            auto threshold_override = new Override<double> { 1.0, axis.symlog_threshold == 1.0? std::nullopt : std::optional<double> { axis.symlog_threshold } };
            auto threshold_input = new NumberInput { *threshold_override, [=] ( double value ) { return value > 0.0; } };

            auto threshold_container_widget = new QWidget { axes_menu };
            auto threshold_container_layout = new QHBoxLayout { threshold_container_widget };
            threshold_container_layout->setContentsMargins( 20, 2, 20, 2 );
            threshold_container_layout->setSpacing( 3 );
            threshold_container_layout->addWidget( threshold_label );
            threshold_container_layout->addWidget( threshold_input );

            auto threshold_widget_action = new QWidgetAction { axes_menu };
            threshold_widget_action->setDefaultWidget( threshold_container_widget );
            axes_menu->addAction( threshold_widget_action );

            // Add symlog scale input to the axes menu
            auto scale_label = new QLabel { "Scale: " };
            scale_label->setFixedWidth( minimum_label_width );

            auto scale_override = new Override<double> { 0.05, axis.symlog_scale == 0.05? std::nullopt : std::optional<double> { axis.symlog_scale } };
            auto scale_input = new NumberInput { *scale_override, [=] ( double value ) { return value >= 0.0 && value < 1.0; } };

            auto scale_container_widget = new QWidget { axes_menu };
            auto scale_container_layout = new QHBoxLayout { scale_container_widget };
            scale_container_layout->setContentsMargins( 20, 2, 20, 2 );
            scale_container_layout->setSpacing( 3 );
            scale_container_layout->addWidget( scale_label );
            scale_container_layout->addWidget( scale_input );

            auto scale_widget_action = new QWidgetAction { axes_menu };
            scale_widget_action->setDefaultWidget( scale_container_widget );
            axes_menu->addAction( scale_widget_action );

            if( is_xaxis )
            {
                QObject::connect( threshold_override, &Override<double>::value_changed, this, [=]
                {
                    this->update_xaxis_symlog_threshold( threshold_override->value() );
                } );
                QObject::connect( scale_override, &Override<double>::value_changed, this, [=]
                {
                    this->update_xaxis_symlog_scale( scale_override->value() );
                } );
            }
            else if( is_yaxis )
            {
                QObject::connect( threshold_override, &Override<double>::value_changed, this, [=]
                {
                    this->update_yaxis_symlog_threshold( threshold_override->value() );
                } );
                QObject::connect( scale_override, &Override<double>::value_changed, this, [=]
                {
                    this->update_yaxis_symlog_scale( scale_override->value() );
                } );
            }
        }

        axes_menu->addSeparator();
    }

    //auto screenshot = context_menu.addAction( "Screenshot", [this]
    //{
    //    const auto original_font = this->font();

    //    auto font = original_font;
    //    font.setPixelSize( static_cast<int>( std::ceil( 12.0 / 72.0 * 300.0 ) ) );
    //    this->setFont( font );

    //    auto viewport = QRect { 0, 0, 1024, 1024 };
    //    this->update_content_rectangle( viewport );

    //    auto pixmap = QPixmap { viewport.size() };
    //    pixmap.fill( Qt::transparent );

    //    auto painter = QPainter { &pixmap };
    //    painter.setFont( font );

    //    this->render( painter );

    //    // Workaround for copying and image with transparency into the clipboard
    //    // https://stackoverflow.com/questions/1260253/how-do-i-put-an-qimage-with-transparency-onto-the-clipboard-for-another-applicat
    //    auto mime_data = new QMimeData();
    //    auto image_data = QByteArray {};
    //    auto image_buffer = QBuffer { &image_data };

    //    image_buffer.open( QIODevice::WriteOnly );
    //    pixmap.save( &image_buffer, "PNG" );
    //    image_buffer.close();
    //    mime_data->setData( "PNG", image_data );

    //    QApplication::clipboard()->setMimeData( mime_data );

    //    this->setFont( original_font );
    //    this->compute_content_rectangle();
    //} );
}

int PlottingWidget::compute_xaxis_height() const noexcept
{
    const auto fontheight = this->fontMetrics().height();
    return _xaxis.visible ? _linewidth + _ticklength + _labelspacing_minor + fontheight : 0;
}
int PlottingWidget::compute_yaxis_width( const std::vector<Tick>& ticks ) const noexcept
{
    if( !_yaxis.visible )
    {
        return 0;
    }

    const auto& fontmetrics = this->fontMetrics();

    auto labelwidth = 0;
    for( const auto& tick : ticks )
    {
        labelwidth = std::max( labelwidth, fontmetrics.horizontalAdvance( tick.label ) );
    }
    return labelwidth + _labelspacing_minor + _ticklength + _linewidth;
}

void PlottingWidget::compute_xaxis_ticks( int xaxis_width )
{
    _xaxis.ticks = std::vector<Tick> {};

    if( _xaxis.type == Axis::Type::eLinear )
    {
        for( auto exponent = static_cast<int>( std::floor( std::log10( _xaxis.domain.y - _xaxis.domain.x ) + 1.0 ) ); true; exponent-- )
        {
            for( const auto mantissa : { 1.0, 0.5, 0.2 } )
            {
                const auto stepsize = mantissa * std::pow( 10.0, exponent );
                const auto precision = this->stepsize_to_precision( stepsize );

                auto previous_rectangle = QRect { -10000, 0, 0, 0 };
                auto current_ticks = std::vector<Tick> {};

                const auto start = std::min( std::ceil( _xaxis.domain.x / stepsize ) * stepsize, _xaxis.domain.y );
                const auto tickcount = static_cast<uint64_t>( std::round( ( _xaxis.domain.y - start ) / stepsize ) );

                for( uint64_t i = 0; i <= tickcount; ++i )
                {
                    const auto position = std::clamp( start + i * stepsize, _xaxis.domain.x, _xaxis.domain.y );

                    auto rectangle = previous_rectangle;
                    const auto label = QString::number( position, 'f', precision );
                    const auto labelwidth = this->fontMetrics().boundingRect( label ).width() + 2; // TODO: why is +2 necessary?

                    rectangle.setWidth( labelwidth );
                    rectangle.moveLeft( static_cast<int>( std::round( this->world_to_screen_x( position ) - ( labelwidth / 2.0 ) ) ) );
                    if( rectangle.left() < _content_rectangle.left() ) rectangle.moveLeft( _content_rectangle.left() );
                    if( rectangle.right() > _content_rectangle.right() - 1 ) rectangle.moveRight( _content_rectangle.right() - 1 );

                    if( rectangle.left() - previous_rectangle.right() < _labelspacing_major )
                    {
                        return;
                    }

                    current_ticks.push_back( Tick { .position = position, .label = label } );
                    previous_rectangle = rectangle;
                }

                _xaxis.ticks = std::move( current_ticks );
                _xaxis.precision = precision;
            }
        }
    }
    else if( _xaxis.type == Axis::Type::eSymmetricLogarithmic )
    {
        this->compute_symlog_ticks(
            _xaxis, _content_rectangle.left(), _content_rectangle.right() - 1,
            [this] ( double position ) { return this->world_to_screen_x( position ); },
            [this] ( const QString& label ) { return this->fontMetrics().boundingRect( label ).width() + 2; } );
    }
}
void PlottingWidget::compute_yaxis_ticks( int yaxis_height )
{
    const auto labelheight = this->fontMetrics().height();
    _yaxis.ticks = std::vector<Tick> {};

    if( _yaxis.type == Axis::Type::eLinear )
    {
        for( auto exponent = static_cast<int>( std::floor( std::log10( _yaxis.domain.y - _yaxis.domain.x ) + 1.0 ) ); true; exponent-- )
        {
            for( const auto mantissa : { 1.0, 0.5, 0.2 } )
            {
                const auto stepsize = mantissa * std::pow( 10.0, exponent );
                const auto precision = this->stepsize_to_precision( stepsize );

                auto previous_rectangle = QRect { 0, _content_rectangle.bottom() + 10000, 0, labelheight };
                auto current_ticks = std::vector<Tick> {};

                const auto start = std::min( std::ceil( _yaxis.domain.x / stepsize ) * stepsize, _yaxis.domain.y );
                const auto tickcount = static_cast<uint64_t>( std::round( ( _yaxis.domain.y - start ) / stepsize ) );

                for( uint64_t i = 0; i <= tickcount; ++i )
                {
                    const auto position = std::clamp( start + i * stepsize, _yaxis.domain.x, _yaxis.domain.y );

                    auto rectangle = previous_rectangle;
                    rectangle.moveTop( static_cast<int>( std::round( this->world_to_screen_y( position ) - ( labelheight / 2.0 ) ) ) );
                    if( rectangle.bottom() > _content_rectangle.bottom() - 1 ) rectangle.moveBottom( _content_rectangle.bottom() - 1 );
                    if( rectangle.top() < _content_rectangle.top() ) rectangle.moveTop( _content_rectangle.top() );

                    if( previous_rectangle.top() - rectangle.bottom() < _labelspacing_major )
                    {
                        return;
                    }

                    current_ticks.push_back( Tick { .position = position, .label = QString::number( position, 'f', precision ) } );
                    previous_rectangle = rectangle;
                }

                _yaxis.ticks = std::move( current_ticks );
                _yaxis.precision = precision;
            }
        }
    }
    else if( _yaxis.type == Axis::Type::eSymmetricLogarithmic )
    {
        this->compute_symlog_ticks(
            _yaxis, _content_rectangle.top(), _content_rectangle.bottom() - 1,
            [this] ( double position ) { return this->world_to_screen_y( position ); },
            [labelheight] ( const QString& ) { return labelheight; } );
    }
}
int PlottingWidget::stepsize_to_precision( double stepsize ) const noexcept
{
    return std::max( 0, static_cast<int>( std::ceil( -std::log10( stepsize ) ) ) );
}

void PlottingWidget::compute_content_rectangle() noexcept
{
    this->update_content_rectangle( this->rect() );
}
void PlottingWidget::update_content_rectangle( const QRect& viewport ) noexcept
{
    _content_rectangle = viewport.marginsRemoved( _margins );

    if( _xaxis.visible )
    {
        const auto xaxis_height = this->compute_xaxis_height();
        _content_rectangle.setBottom( _content_rectangle.bottom() - xaxis_height );
    }

    if( _yaxis.visible )
    {
        if( _yaxis.automatic_ticks )
        {
            this->compute_yaxis_ticks( _content_rectangle.height() );
        }

        const auto yaxis_width = this->compute_yaxis_width( _yaxis.ticks );
        _content_rectangle.setLeft( _content_rectangle.left() + yaxis_width );
    }

    if( _xaxis.visible && _xaxis.automatic_ticks )
    {
        this->compute_xaxis_ticks( _content_rectangle.width() );
    }

    this->update();
}

double PlottingWidget::symlog_slope( const Axis& axis )
{
    const auto threshold = axis.symlog_threshold;
    const auto scale = axis.symlog_scale;

    const auto linear_width = std::min( axis.bounds.y, threshold ) - std::max( axis.bounds.x, -threshold );
    const auto decades = std::log10( std::max( -axis.bounds.x, threshold ) / threshold )
        + std::log10( std::max( axis.bounds.y, threshold ) / threshold );

    if( linear_width <= 0.0 || decades <= 0.0 || scale <= 0.0 || scale >= 1.0 ) return 1.0;
    return scale * decades * threshold / ( ( 1.0 - scale ) * linear_width );
}
double PlottingWidget::to_axis_space( const Axis& axis, double value )
{
    if( axis.type == Axis::Type::eLinear ) return value;

    const auto threshold = axis.symlog_threshold;
    const auto slope = symlog_slope( axis );
    if( std::abs( value ) <= threshold ) return slope * value / threshold;
    return std::copysign( slope + std::log10( std::abs( value ) / threshold ), value );
}
double PlottingWidget::from_axis_space( const Axis& axis, double value )
{
    if( axis.type == Axis::Type::eLinear ) return value;

    const auto threshold = axis.symlog_threshold;
    const auto slope = symlog_slope( axis );
    if( std::abs( value ) <= slope ) return value * threshold / slope;
    return std::copysign( threshold * std::pow( 10.0, std::abs( value ) - slope ), value );
}
void PlottingWidget::compute_symlog_ticks( Axis& axis, int screen_min, int screen_max, const std::function<double( double )>& world_to_screen, const std::function<int( const QString& )>& label_extent )
{
    struct Candidate
    {
        double position;
        int precision;
    };
    enum Region { eNegative, eLinear, ePositive };

    constexpr auto epsilon = 1e-9;
    constexpr auto max_level = 100; // safety net for degenerate domains
    constexpr auto nice_strides = std::array { 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000 };

    const auto threshold = axis.symlog_threshold; // must be > 0
    const auto lower = axis.domain.x;
    const auto upper = axis.domain.y;

    const auto linear_lower = std::max( lower, -threshold );
    const auto linear_upper = std::min( upper, threshold );
    const auto linear_exists = linear_lower < linear_upper;

    // Linear part: same 1/0.5/0.2 * 10^n scheme as for linear axes
    const auto generate_linear = [&] ( int level )
    {
        const auto exponent = static_cast<int>( std::floor( std::log10( linear_upper - linear_lower ) + 1.0 ) ) - level / 3;
        const auto stepsize = std::array { 1.0, 0.5, 0.2 }[level % 3] * std::pow( 10.0, exponent );
        const auto precision = this->stepsize_to_precision( stepsize );

        auto candidates = std::vector<Candidate> {};
        const auto first = static_cast<int64_t>( std::ceil( linear_lower / stepsize - epsilon ) );
        const auto last = static_cast<int64_t>( std::floor( linear_upper / stepsize + epsilon ) );
        for( auto i = first; i <= last; ++i )
        {
            candidates.push_back( Candidate { .position = std::clamp( i * stepsize, linear_lower, linear_upper ), .precision = precision } );
        }
        return candidates;
    };

    // One logarithmic part, as ascending magnitudes within [magnitude_lower, magnitude_upper]
    const auto generate_logarithmic = [&] ( double magnitude_lower, double magnitude_upper, int level )
    {
        auto candidates = std::vector<Candidate> {};
        const auto add = [&] ( double magnitude, int precision )
        {
            if( linear_exists && magnitude <= threshold * ( 1.0 + epsilon ) ) return; // belongs to the linear part
            if( magnitude < magnitude_lower * ( 1.0 - epsilon ) || magnitude > magnitude_upper * ( 1.0 + epsilon ) ) return;
            candidates.push_back( Candidate { .position = std::clamp( magnitude, magnitude_lower, magnitude_upper ), .precision = precision } );
        };

        const auto first_decade = static_cast<int>( std::floor( std::log10( magnitude_lower ) ) );
        const auto last_decade = static_cast<int>( std::floor( std::log10( magnitude_upper ) ) );
        const auto decadecount = last_decade - first_decade + 1;

        auto coarsest = 0;
        while( coarsest + 1 < static_cast<int>( nice_strides.size() ) && nice_strides[coarsest] < decadecount ) ++coarsest;

        if( level <= coarsest )
        {
            // Stage 1: powers of ten, only every n-th decade
            const auto stride = nice_strides[coarsest - level];
            for( auto exponent = first_decade; exponent <= last_decade; ++exponent )
            {
                if( exponent % stride != 0 ) continue;
                const auto decade = std::pow( 10.0, exponent );
                add( decade, this->stepsize_to_precision( decade ) );
            }
        }
        else if( level == coarsest + 1 )
        {
            // Stage 2: 1, 2 and 5 in every decade
            for( auto exponent = first_decade; exponent <= last_decade; ++exponent )
            {
                const auto decade = std::pow( 10.0, exponent );
                for( const auto mantissa : { 1.0, 2.0, 5.0 } ) add( mantissa * decade, this->stepsize_to_precision( decade ) );
            }
        }
        else
        {
            // Stage 3: equidistant steps inside every decade (1, 0.5, 0.2, 0.1, ... times its power of ten)
            const auto refinement = level - coarsest - 2;
            const auto factor = std::array { 1.0, 0.5, 0.2 }[refinement % 3] * std::pow( 10.0, -( refinement / 3 ) );
            for( auto exponent = first_decade; exponent <= last_decade; ++exponent )
            {
                const auto decade = std::pow( 10.0, exponent );
                const auto stepsize = factor * decade;
                const auto precision = this->stepsize_to_precision( stepsize );
                const auto first = static_cast<int64_t>( std::ceil( std::max( decade, magnitude_lower ) / stepsize - epsilon ) );
                const auto last = static_cast<int64_t>( std::floor( std::min( 10.0 * decade, magnitude_upper ) / stepsize + epsilon ) );
                for( auto i = first; i <= last; ++i )
                {
                    const auto magnitude = i * stepsize;
                    if( magnitude < 10.0 * decade * ( 1.0 - epsilon ) ) add( magnitude, precision ); // next power of ten belongs to the next decade
                }
            }
        }
        return candidates;
    };

    const auto generate = [&] ( int region, int level ) -> std::vector<Candidate>
    {
        if( region == eLinear ) return generate_linear( level );
        if( region == ePositive ) return generate_logarithmic( std::max( lower, threshold ), upper, level );

        auto candidates = generate_logarithmic( std::max( -upper, threshold ), -lower, level );
        std::ranges::reverse( candidates );
        for( auto& candidate : candidates ) candidate.position = -candidate.position;
        return candidates;
    };

    // Same overlap rule as for linear axes, applied to all regions together
    const auto fits = [&] ( const std::array<std::vector<Candidate>, 3>& regions )
    {
        auto previous = std::optional<std::pair<int, int>> {};
        for( const auto& region : regions )
        {
            for( const auto& candidate : region )
            {
                const auto extent = label_extent( QString::number( candidate.position, 'f', candidate.precision ) );
                auto first = static_cast<int>( std::round( world_to_screen( candidate.position ) - extent / 2.0 ) );
                if( first < screen_min ) first = screen_min;
                if( first + extent - 1 > screen_max ) first = screen_max - extent + 1;
                const auto last = first + extent - 1;

                if( previous && std::max( first - previous->second, previous->first - last ) < _labelspacing_major )
                {
                    return false;
                }
                previous = std::pair { first, last };
            }
        }
        return true;
    };

    // Refine each region independently (round-robin) until its next level would overlap
    auto accepted = std::array<std::vector<Candidate>, 3> {};
    auto levels = std::array { 0, 0, 0 };
    auto active = std::array { lower < -threshold, linear_exists, upper > threshold };

    while( std::ranges::any_of( active, std::identity {} ) )
    {
        for( const auto region : { eNegative, eLinear, ePositive } )
        {
            if( !active[region] ) continue;

            auto refined = accepted;
            refined[region] = generate( region, levels[region] );
            if( fits( refined ) )
            {
                accepted = std::move( refined );
                ++levels[region];
            }
            else
            {
                active[region] = false;
            }
            if( levels[region] > max_level ) active[region] = false;
        }
    }

    axis.ticks.clear();
    axis.precision = 0;
    for( const auto& region : accepted )
    {
        for( const auto& candidate : region )
        {
            axis.ticks.push_back( Tick { .position = candidate.position, .label = QString::number( candidate.position, 'f', candidate.precision ) } );
            axis.precision = std::max( axis.precision, candidate.precision );
        }
    }
}