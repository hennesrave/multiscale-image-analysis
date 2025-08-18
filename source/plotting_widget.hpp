#pragma once
#include "utility.hpp"

#include <qmenu.h>
#include <qwidget.h>

class PlottingWidget : public QWidget
{
    Q_OBJECT
public:
    struct Tick
    {
        double position;
        QString label;
    };

    struct Axis
    {
        vec2<double> bounds { 0.0, 1.0 };
        vec2<double> domain { 0.0, 1.0 };

        std::vector<Tick> ticks;
        bool visible = true;
        bool zoomable = true;
        bool automatic_ticks = true;
        int precision = 0;
    };

    PlottingWidget();

    QSize sizeHint() const override;

    virtual void resizeEvent( QResizeEvent* event ) override;
    virtual void wheelEvent( QWheelEvent* event ) override;
    virtual void paintEvent( QPaintEvent* event ) override;

    virtual void paint_xaxis_ticks( QPainter& painter ) const;
    virtual void paint_yaxis_ticks( QPainter& painter ) const;

    const QMargins& margins() const noexcept;
    void update_margins( const QMargins& margins ) noexcept;

    int linewidth() const noexcept;
    void update_linewidth( int linewidth ) noexcept;

    int ticklength() const noexcept;
    void update_ticklength( int ticklength );

    int labelspacing_major() const noexcept;
    void update_labelspacing_major( int labelspacing_major ) noexcept;

    int labelspacing_minor() const noexcept;
    void update_labelspacing_minor( int labelspacing_minor ) noexcept;

    const Axis& xaxis() const noexcept;
    const Axis& yaxis() const noexcept;

    void update_xaxis_bounds( vec2<double> bounds ) noexcept;
    void update_yaxis_bounds( vec2<double> bounds ) noexcept;

    void update_xaxis_domain( vec2<double> domain ) noexcept;
    void update_yaxis_domain( vec2<double> domain ) noexcept;

    void update_xaxis_visible( bool visible ) noexcept;
    void update_yaxis_visible( bool visible ) noexcept;

    void update_xaxis_zoomable( bool zoomable ) noexcept;
    void update_yaxis_zoomable( bool zoomable ) noexcept;

    void update_xaxis_ticks( std::vector<Tick> ticks );
    void update_yaxis_ticks( std::vector<Tick> ticks );

    const QRect& content_rectangle() const noexcept;

    virtual double world_to_screen_x( double value ) const;
    virtual double world_to_screen_y( double value ) const;
    QPointF world_to_screen( const QPointF& world ) const;

    virtual double screen_to_world_x( double value ) const;
    virtual double screen_to_world_y( double value ) const;
    QPointF screen_to_world( const QPointF& screen ) const;

    void reset_view();
    void populate_context_menu( QMenu& context_menu );

signals:
    void margins_changed( const QMargins& margins );
    void linewidth_changed( int linewidth );
    void ticklength_changed( int ticklength );
    void labelspacing_major_changed( int labelspacing_major );
    void labelspacing_minor_changed( int labelspacing_minor );
    void xaxis_bounds_changed( vec2<double> bounds );
    void yaxis_bounds_changed( vec2<double> bounds );
    void xaxis_domain_changed( vec2<double> domain );
    void yaxis_domain_changed( vec2<double> domain );
    void xaxis_visible_changed( bool visible );
    void yaxis_visible_changed( bool visible );
    void xaxis_zoomable_changed( bool zoomable );
    void yaxis_zoomable_changed( bool zoomable );
    void content_rectangle_changed( const QRect& content_rectangle );

protected:
    virtual int compute_xaxis_height() const noexcept;
    virtual int compute_yaxis_width( const std::vector<Tick>& ticks ) const noexcept;
    virtual void compute_xaxis_ticks( int xaxis_width );
    virtual void compute_yaxis_ticks( int yaxis_height );
    virtual int stepsize_to_precision( double stepsize ) const noexcept;

private:
    void compute_content_rectangle() noexcept;

    QMargins _margins { 5, 5, 5, 5 };
    int _linewidth = 1;
    int _ticklength = 5;
    int _labelspacing_major = 5;
    int _labelspacing_minor = 3;

    Axis _xaxis {};
    Axis _yaxis {};

    QRect _content_rectangle {};
};