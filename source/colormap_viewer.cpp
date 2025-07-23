#include "colormap_viewer.hpp"

#include "colormap.hpp"
#include "feature.hpp"
#include "feature_selector.hpp"
#include "plotting_widget.hpp"
#include "utility.hpp"

#include <qevent.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qmenu.h>
#include <qpainter.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qtoolbutton.h>

// ----- Colormap1DPreview ----- //

Colormap1DPreview::Colormap1DPreview( Colormap1D& colormap ) : QWidget {}, _colormap { colormap }
{
    this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Minimum );
    QObject::connect( &_colormap, &Colormap1D::colors_changed, this, qOverload<>( &QWidget::update ) );
}
Colormap1D& Colormap1DPreview::colormap() const noexcept
{
    return _colormap;
}

QSize Colormap1DPreview::sizeHint() const
{
    return QSize { 100, _colormap_height };
}
void Colormap1DPreview::paintEvent( QPaintEvent* event )
{
    auto painter = QPainter { this };

    auto border_rectangle = this->rect();
    border_rectangle.setHeight( _colormap_height );
    border_rectangle.setRight( border_rectangle.right() - 1 );

    auto colormap_rectangle = border_rectangle.marginsRemoved( QMargins { 1, 0, 0, 0 } );
    auto colormap_image = QImage { colormap_rectangle.width(), 1, QImage::Format_RGBA8888 };
    for( int x = 0; x < colormap_image.width(); ++x )
    {
        const auto value = static_cast<double>( x ) / ( colormap_image.width() - 1 );
        const auto color = _colormap.colormap_template()->color( value ).qcolor();
        colormap_image.setPixelColor( x, 0, color );
    }

    painter.setRenderHint( QPainter::Antialiasing, false );
    painter.drawImage( colormap_rectangle, colormap_image );

    painter.setPen( QPen { QBrush { config::palette[700] }, 1.0 } );
    painter.setBrush( Qt::transparent );
    painter.drawRect( border_rectangle );
};

void Colormap1DPreview::mousePressEvent( QMouseEvent* event )
{
    if( event->button() == Qt::RightButton )
    {
        auto context_menu = QMenu {};
        auto template_menu = context_menu.addMenu( "Template" );
        for( const auto& [identifier, colormap_template] : ColormapTemplate::registry )
        {
            template_menu->addAction( identifier, [this, &colormap_template] { _colormap.update_colormap_template( colormap_template.clone() ); } );
        }
        context_menu.addAction( "Invert", [this] { _colormap.colormap_template()->invert(); } );

        context_menu.exec( event->globalPosition().toPoint() );
    }
}

// ----- Colormap1DViewer ----- //

Colormap1DViewer::Colormap1DViewer( Colormap1D& colormap, QSharedPointer<const Collection<Feature>> features )
    : QWidget {}
    , _colormap { colormap }
    , _feature_selector { new FeatureSelector { features } }
    , _colormap_range { new RangeInput { colormap.lower(), colormap.upper() } }
    , _colormap_preview { new Colormap1DPreview { colormap } }
    , _colormap_axis { new PlottingWidget }
{
    this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Minimum );

    _colormap_axis->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
    _colormap_axis->update_margins( QMargins { 0, 0, 0, 0 } );
    _colormap_axis->update_labelspacing_minor( 1 );
    _colormap_axis->update_xaxis_zoomable( false );
    _colormap_axis->update_yaxis_visible( false );
    _colormap_axis->update_ticklength( 2 );
    _colormap_axis->update_xaxis_bounds( vec2<double>{ _colormap.lower().value(), _colormap.upper().value() } );
    _colormap_axis->update_xaxis_domain( vec2<double>{ _colormap.lower().value(), _colormap.upper().value() } );

    auto header = new QHBoxLayout {};
    header->setContentsMargins( 5, 0, 0, 3 );
    header->setSpacing( 5 );

    header->addWidget( _feature_selector );
    header->addStretch( 1 );
    header->addWidget( _colormap_range );

    auto layout = new QVBoxLayout { this };
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( 0 );
    layout->addLayout( header );
    layout->addWidget( _colormap_preview );
    layout->addWidget( _colormap_axis );

    QObject::connect( _feature_selector, &FeatureSelector::selected_feature_changed, this, [this] ( QSharedPointer<Feature> feature )
    {
        _colormap.update_feature( feature );
    } );
    QObject::connect( &_colormap.lower(), &Override<double>::value_changed, this, [this]
    {
        _colormap_axis->update_xaxis_bounds( vec2<double>{ _colormap.lower().value(), _colormap.upper().value() } );
        _colormap_axis->update_xaxis_domain( vec2<double>{ _colormap.lower().value(), _colormap.upper().value() } );
    } );
    QObject::connect( &_colormap.upper(), &Override<double>::value_changed, this, [this]
    {
        _colormap_axis->update_xaxis_bounds( vec2<double>{ _colormap.lower().value(), _colormap.upper().value() } );
        _colormap_axis->update_xaxis_domain( vec2<double>{ _colormap.lower().value(), _colormap.upper().value() } );
    } );
}
Colormap1D& Colormap1DViewer::colormap() const noexcept
{
    return _colormap;
}

void Colormap1DViewer::colormap_feature_changed( const Feature* feature )
{
}

// ----- ColormapRGBViewer ----- //

ColormapRGBViewer::ColormapRGBViewer( ColormapRGB& colormap ) : QWidget {}, _colormap { colormap }
{
}

// ----- ColormapViewer ----- //

ColormapViewer::ColormapViewer( QSharedPointer<Collection<Colormap>> colormaps, QSharedPointer<const Collection<Feature>> features ) : QWidget {}, _colormaps { colormaps }, _features { features }
{
    auto layout = new QVBoxLayout { this };
    layout->setContentsMargins( 5, 5, 5, 5 );
    layout->addWidget( _colormap_widget = new QWidget );

    QObject::connect( _colormaps.data(), &CollectionObject::object_appended, this, &ColormapViewer::colormap_appended );
    QObject::connect( _colormaps.data(), &CollectionObject::object_removed, this, &ColormapViewer::colormap_removed );
}

QSharedPointer<Colormap> ColormapViewer::colormap() const noexcept
{
    return _colormap;
}
void ColormapViewer::update_colormap( QSharedPointer<Colormap> colormap )
{
    if( colormap != _colormap )
    {
        _colormap = colormap;

        if( auto colormap_1d = colormap.dynamicCast<Colormap1D>() )
        {
            auto colormap_widget = new Colormap1DViewer { *colormap_1d, _features.lock() };
            this->layout()->replaceWidget( _colormap_widget, colormap_widget );
            _colormap_widget = colormap_widget;
        }
        else if( auto colormap_rgb = colormap.dynamicCast<ColormapRGB>() )
        {
            auto colormap_widget = new ColormapRGBViewer { *colormap_rgb };
            this->layout()->replaceWidget( _colormap_widget, colormap_widget );
            _colormap_widget = colormap_widget;
        }

        emit colormap_changed( _colormap );
    }
}

void ColormapViewer::colormap_appended( QSharedPointer<QObject> object )
{
    if( auto colormap = object.objectCast<Colormap>() )
    {
        if( !_colormap )
        {
            this->update_colormap( colormap );
        }
    }
}
void ColormapViewer::colormap_removed( QSharedPointer<QObject> object )
{
    if( auto colormap = object.objectCast<Colormap>() )
    {
        if( colormap == _colormap )
        {
            this->update_colormap( nullptr );
        }
    }
}