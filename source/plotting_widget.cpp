#include "plotting_widget.hpp"

#include "number_input.hpp"

#include <qevent.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qmenu.h>
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

    auto xdomain = _xaxis.domain;
    auto ydomain = _yaxis.domain;

    const auto x = std::clamp( this->screen_to_world_x( cursor.x() ), xdomain.x, xdomain.y );
    const auto y = std::clamp( this->screen_to_world_y( cursor.y() ), ydomain.x, ydomain.y );

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

    xdomain.x = std::clamp( xdomain.x, _xaxis.bounds.x, _xaxis.bounds.y );
    xdomain.y = std::clamp( xdomain.y, _xaxis.bounds.x, _xaxis.bounds.y );

    ydomain.x = std::clamp( ydomain.x, _yaxis.bounds.x, _yaxis.bounds.y );
    ydomain.y = std::clamp( ydomain.y, _yaxis.bounds.x, _yaxis.bounds.y );

    if( _xaxis.zoomable && cursor.x() > _content_rectangle.left() && ( _xaxis.bounds.y - _xaxis.bounds.x ) / ( xdomain.y - xdomain.x ) <= 10000.0 )
        _xaxis.domain = xdomain;

    if( _yaxis.zoomable && cursor.y() < _content_rectangle.bottom() && ( _yaxis.bounds.y - _yaxis.bounds.x ) / ( ydomain.y - ydomain.x ) <= 10000.0 )
        _yaxis.domain = ydomain;

    this->compute_content_rectangle();
}
void PlottingWidget::paintEvent( QPaintEvent* event )
{
    QWidget::paintEvent( event );

    auto painter = QPainter { this };

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
    return _content_rectangle.left() + ( ( value - _xaxis.domain.x ) / ( _xaxis.domain.y - _xaxis.domain.x ) ) * ( _content_rectangle.width() - 1 );
}
double PlottingWidget::world_to_screen_y( double value ) const
{
    return _content_rectangle.bottom() - ( ( value - _yaxis.domain.x ) / ( _yaxis.domain.y - _yaxis.domain.x ) ) * ( _content_rectangle.height() - 1 );
}
QPointF PlottingWidget::world_to_screen( const QPointF& world ) const
{
    return QPointF { this->world_to_screen_x( world.x() ), this->world_to_screen_y( world.y() ) };
}

double PlottingWidget::screen_to_world_x( double value ) const
{
    return _xaxis.domain.x + ( ( value - _content_rectangle.left() ) / ( _content_rectangle.width() - 1 ) ) * ( _xaxis.domain.y - _xaxis.domain.x );
}
double PlottingWidget::screen_to_world_y( double value ) const
{
    return _yaxis.domain.x - ( ( value - _content_rectangle.bottom() ) / ( _content_rectangle.height() - 1 ) ) * ( _yaxis.domain.y - _yaxis.domain.x );
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
    auto xaxis_label = new QLabel { "X-Axis: " };
    auto yaxis_label = new QLabel { "Y-Axis: " };

    auto xaxis_lower = new Override<double> { _xaxis.bounds.x, _xaxis.domain.x == _xaxis.bounds.x ? std::nullopt : std::optional<double> { _xaxis.domain.x } };
    auto xaxis_upper = new Override<double> { _xaxis.bounds.y, _xaxis.domain.y == _xaxis.bounds.y ? std::nullopt : std::optional<double> { _xaxis.domain.y } };

    auto yaxis_lower = new Override<double> { _yaxis.bounds.x, _yaxis.domain.x == _yaxis.bounds.x ? std::nullopt : std::optional<double> { _yaxis.domain.x } };
    auto yaxis_upper = new Override<double> { _yaxis.bounds.y, _yaxis.domain.y == _yaxis.bounds.y ? std::nullopt : std::optional<double> { _yaxis.domain.y } };

    auto xaxis_lower_input = new NumberInput { *xaxis_lower, [=] ( double value ) { return value >= _xaxis.bounds.x && value < xaxis_upper->value(); } };
    auto xaxis_upper_input = new NumberInput { *xaxis_upper, [=] ( double value ) { return value > xaxis_lower->value() && value <= _xaxis.bounds.y; } };

    auto yaxis_lower_input = new NumberInput { *yaxis_lower, [=] ( double value ) { return value >= _yaxis.bounds.x && value < yaxis_upper->value(); } };
    auto yaxis_upper_input = new NumberInput { *yaxis_upper, [=] ( double value ) { return value > yaxis_lower->value() && value <= _yaxis.bounds.y; } };

    xaxis_lower->setParent( xaxis_lower_input );
    xaxis_upper->setParent( xaxis_upper_input );
    yaxis_lower->setParent( yaxis_lower_input );
    yaxis_upper->setParent( yaxis_upper_input );

    auto xaxis_container_widget = new QWidget { &context_menu };
    auto xaxis_container_layout = new QHBoxLayout { xaxis_container_widget };
    xaxis_container_layout->setContentsMargins( 20, 2, 20, 2 );
    xaxis_container_layout->setSpacing( 3 );
    xaxis_container_layout->addWidget( xaxis_label );
    xaxis_container_layout->addWidget( xaxis_lower_input );
    xaxis_container_layout->addWidget( new QLabel { " \u2014 " } );
    xaxis_container_layout->addWidget( xaxis_upper_input );

    auto yaxis_container_widget = new QWidget { &context_menu };
    auto yaxis_container_layout = new QHBoxLayout { yaxis_container_widget };
    yaxis_container_layout->setContentsMargins( 20, 2, 20, 2 );
    yaxis_container_layout->setSpacing( 3 );
    yaxis_container_layout->addWidget( yaxis_label );
    yaxis_container_layout->addWidget( yaxis_lower_input );
    yaxis_container_layout->addWidget( new QLabel { " \u2014 " } );
    yaxis_container_layout->addWidget( yaxis_upper_input );

    auto xaxis_widget_action = new QWidgetAction { &context_menu };
    xaxis_widget_action->setDefaultWidget( xaxis_container_widget );

    auto yaxis_widget_action = new QWidgetAction { &context_menu };
    yaxis_widget_action->setDefaultWidget( yaxis_container_widget );

    QObject::connect( xaxis_lower, &Override<double>::value_changed, this, [=]
    {
        this->update_xaxis_domain( vec2<double> { xaxis_lower->value(), _xaxis.domain.y } );
    } );
    QObject::connect( xaxis_upper, &Override<double>::value_changed, this, [=]
    {
        this->update_xaxis_domain( vec2<double> { _xaxis.domain.x, xaxis_upper->value() } );
    } );
    QObject::connect( yaxis_lower, &Override<double>::value_changed, this, [=]
    {
        this->update_yaxis_domain( vec2<double> { yaxis_lower->value(), _yaxis.domain.y } );
    } );
    QObject::connect( yaxis_upper, &Override<double>::value_changed, this, [=]
    {
        this->update_yaxis_domain( vec2<double> { _yaxis.domain.x, yaxis_upper->value() } );
    } );

    auto menu_axes = context_menu.addMenu( "Axis Settings" );
    menu_axes->addAction( xaxis_widget_action );
    menu_axes->addAction( yaxis_widget_action );
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
void PlottingWidget::compute_yaxis_ticks( int yaxis_height )
{
    const auto labelheight = this->fontMetrics().height();
    _yaxis.ticks = std::vector<Tick> {};

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
int PlottingWidget::stepsize_to_precision( double stepsize ) const noexcept
{
    return std::max( 0, static_cast<int>( std::ceil( -std::log10( stepsize ) ) ) );
}

void PlottingWidget::compute_content_rectangle() noexcept
{
    _content_rectangle = this->rect().marginsRemoved( _margins );

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