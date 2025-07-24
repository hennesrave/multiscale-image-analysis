#include "image_viewer.hpp"

#include "colormap.hpp"
#include "database.hpp"
#include "dataset.hpp"
#include "colormap_viewer.hpp"
#include "feature.hpp"
#include "segment_selector.hpp"
#include "segmentation.hpp"
#include "segmentation_manager.hpp"

#include <qactiongroup.h>
#include <qapplication.h>
#include <qbuffer.h>
#include <qclipboard.h>
#include <qevent.h>
#include <qfiledialog.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qmenu.h>
#include <qmessagebox.h>
#include <qmimedata.h>
#include <qpainter.h>
#include <qtoolbutton.h>
#include <qwidgetaction.h>

ImageViewer::ImageViewer( Database& database ) : QWidget {}, _database { database }, _dataset { database.dataset() }, _segmentation { database.segmentation() }
{
    this->setFocusPolicy( Qt::WheelFocus );
    this->setMouseTracking( true );

    const auto segmentation = _database.segmentation();
    QObject::connect( segmentation.get(), &Segmentation::element_colors_changed, this, qOverload<>(&QWidget::update));
    QObject::connect( &_database, &Database::highlighted_element_index_changed, this, qOverload<>( &QWidget::update ) );
}

void ImageViewer::update_colormap( QSharedPointer<Colormap> colormap )
{
    if( _colormap != colormap )
    {
        if( auto colormap = _colormap.lock() )
        {
            QObject::disconnect( colormap.get(), &Colormap::colors_changed, this, qOverload<>( &QWidget::update ) );
        }
        if( _colormap = colormap )
        {
            QObject::connect( colormap.get(), &Colormap::colors_changed, this, qOverload<>( &QWidget::update ) );
        }
        this->update();
    }
}

void ImageViewer::resizeEvent( QResizeEvent* event )
{
    QWidget::resizeEvent( event );
    this->reset_image_rectangle();
}
void ImageViewer::paintEvent( QPaintEvent* event )
{
    auto timer = Timer {};

    auto painter = QPainter { this };
    painter.setRenderHint( QPainter::Antialiasing );
    painter.fillRect( _image_rectangle, Qt::black );

    // Render image
    if( auto colormap = _colormap.lock() )
    {
        const auto& colors = colormap->colors();
        const auto dimensions = _dataset->spatial_metadata()->dimensions;
        const auto image = QImage { reinterpret_cast<const uchar*>( colors.data() ), static_cast<int>( dimensions.x ), static_cast<int>( dimensions.y ), QImage::Format_RGBA32FPx4 };
        painter.setOpacity( _image_opacity );
        painter.drawImage( _image_rectangle, image );
        painter.setOpacity( 1.0 );
    }

    // Render segmentation colors
    const auto& segmentation_colors = _segmentation->element_colors();
    const auto dimensions = _dataset->spatial_metadata()->dimensions;
    const auto image = QImage { reinterpret_cast<const uchar*>( segmentation_colors.data() ), static_cast<int>( dimensions.x ), static_cast<int>( dimensions.y ), QImage::Format_RGBA32FPx4 };
    painter.setOpacity( _segmentation_opacity );
    painter.drawImage( _image_rectangle, image );
    painter.setOpacity( 1.0 );

    // Render highlighted element
    if( const auto element_index = _database.highlighted_element_index(); element_index.has_value() )
    {
        const auto spatial_metadata = _dataset->spatial_metadata();
        const auto dimensions = spatial_metadata->dimensions;
        const auto pixel = spatial_metadata->coordinates( *element_index );
        const auto screen = this->pixel_to_screen( pixel );

        const auto pixelwidth = _image_rectangle.width() / dimensions.x;
        const auto pixelheight = _image_rectangle.height() / dimensions.y;

        const QRectF pixel_rectangle {
            _image_rectangle.left() + pixel.x * pixelwidth,
            _image_rectangle.top() + pixel.y * pixelheight,
            std::max( 1.0, pixelwidth ),
            std::max( 1.0, pixelheight )
        };
        const QRectF vertical {
            QPointF { pixel_rectangle.left(), _image_rectangle.top() },
            QPointF { pixel_rectangle.right(), _image_rectangle.bottom() },
        };
        const QRectF horizontal {
            QPointF { _image_rectangle.left(), pixel_rectangle.top() },
            QPointF { _image_rectangle.right(), pixel_rectangle.bottom() },
        };

        painter.setBrush( QColor { 255, 255, 255, 50 } );
        painter.setPen( Qt::transparent );
        painter.setCompositionMode( QPainter::CompositionMode_Difference );
        painter.drawRect( vertical );
        painter.drawRect( horizontal );
        painter.setCompositionMode( QPainter::CompositionMode_SourceOver );
    }

    // Render border
    painter.setPen( Qt::black );
    painter.setBrush( Qt::NoBrush );
    painter.drawRect( _image_rectangle );

    // Render selection polygon
    if( !_selection_polygon.empty() )
    {
        const auto stroke_color = _selection_mode == SelectionMode::eGrowSegment ? _database.active_segment()->color().qcolor() : QColor { 200, 200, 200 };
        auto brush_color = stroke_color;
        brush_color.setAlpha( 150 );

        painter.setPen( QPen { stroke_color, 2.0 } );
        painter.setBrush( brush_color );
        painter.drawPolygon( _selection_polygon );
    }

    // Render highlighted element information
    if( const auto element_index = _database.highlighted_element_index(); element_index.has_value() )
    {
        const auto pixel = _dataset->spatial_metadata()->coordinates( *element_index );
        const auto position = QPointF { 0.0, 0.0 };

        auto labels_string = QString {
            "x: "
            "\ny:"
        };
        auto values_string = QString {
            QString::number( pixel.x, 'f', 0 ) +
            '\n' + QString::number( pixel.y, 'f', 0 )
        };

        if( auto colormap = _colormap.lock() )
        {
            if( auto colormap_1d = colormap.dynamicCast<Colormap1D>() )
            {
                if( auto feature = colormap_1d->feature() )
                {
                    const auto& feature_values = feature->values();
                    const auto value = feature_values[*element_index];
                    labels_string += QString { "\nvalue: " };
                    values_string += '\n' + QString::number( value, 'f', std::min( 5, utility::compute_precision( value ) ) );
                }
            }
        }

        auto labels_rectangle = painter.fontMetrics().boundingRect( QRect {}, Qt::TextWordWrap, labels_string ).toRectF();
        auto values_rectangle = painter.fontMetrics().boundingRect( QRect {}, Qt::TextWordWrap, values_string ).toRectF();

        labels_rectangle.moveTopLeft( position + QPointF { 10.0, 10.0 } );
        values_rectangle.moveLeft( labels_rectangle.right() + 10.0 );
        values_rectangle.moveTop( labels_rectangle.top() );

        auto content_rectangle = labels_rectangle.united( values_rectangle ).marginsAdded( QMarginsF { 5.0, 5.0, 5.0, 5.0 } );

        painter.setPen( Qt::NoPen );
        painter.setBrush( QBrush { QColor { 255, 255, 255, 200 } } );
        painter.drawRoundedRect( content_rectangle, 5.0, 5.0 );

        painter.setPen( Qt::black );
        painter.drawText( labels_rectangle, Qt::AlignLeft | Qt::AlignTop, labels_string );
        painter.drawText( values_rectangle, Qt::AlignRight | Qt::AlignTop, values_string );
    }
}
void ImageViewer::wheelEvent( QWheelEvent* event )
{
    const auto cursor = event->position();
    const auto x { ( cursor.x() - _image_rectangle.left() ) / _image_rectangle.width() };
    const auto y { ( cursor.y() - _image_rectangle.top() ) / _image_rectangle.height() };

    const double scaling = ( event->angleDelta().y() > 0 ) ? 1.1 : ( 1.0 / 1.1 );
    _image_rectangle.setSize( _image_rectangle.size() * scaling );

    const auto newx = std::lerp( _image_rectangle.left(), _image_rectangle.right(), x );
    const auto newy = std::lerp( _image_rectangle.top(), _image_rectangle.bottom(), y );

    _image_rectangle.translate( cursor.x() - newx, cursor.y() - newy );

    this->update();
}

void ImageViewer::mousePressEvent( QMouseEvent* event )
{
    if( event->button() == Qt::LeftButton )
    {
        _selection_polygon.append( event->position() );
        _selection_mode = SelectionMode::eGrowSegment;
    }
    else if( event->button() == Qt::RightButton )
    {
        _selection_polygon.append( event->position() );
        _selection_mode = SelectionMode::eShrinkSegment;
    }
}
void ImageViewer::mouseReleaseEvent( QMouseEvent* event )
{
    if( event->button() == Qt::LeftButton || event->button() == Qt::RightButton )
    {
        if( _selection_polygon.size() == 1 )
        {
            if( event->button() == Qt::RightButton )
            {
                auto context_menu = QMenu {};

                _database.populate_segmentation_menu( context_menu );
                context_menu.addSeparator();

                context_menu.addAction( "Reset View", [this] { this->reset_image_rectangle(); } );

                auto image_opacity_menu = context_menu.addMenu( "Image Opacity" );
                auto image_opacity_action_group = new QActionGroup { image_opacity_menu };
                image_opacity_action_group->setExclusive( true );

                for( int opacity = 0; opacity <= 100; opacity += 10 )
                {
                    const double opacity_value = static_cast<double>( opacity ) / 100.0;
                    const auto action = image_opacity_menu->addAction( QString::number( opacity ) + " %", [this, opacity_value]
                    {
                        _image_opacity = opacity_value;
                        this->update();
                    } );
                    action->setCheckable( true );
                    action->setChecked( _image_opacity == opacity_value );
                    image_opacity_action_group->addAction( action );
                }

                auto segmentation_opacity_menu = context_menu.addMenu( "Segmentation Opacity" );
                auto segmentation_opacity_action_group = new QActionGroup { segmentation_opacity_menu };
                segmentation_opacity_action_group->setExclusive( true );

                for( int opacity = 0; opacity <= 100; opacity += 10 )
                {
                    const double opacity_value = static_cast<double>( opacity ) / 100.0;
                    const auto action = segmentation_opacity_menu->addAction( QString::number( opacity ) + " %", [this, opacity_value]
                    {
                        _segmentation_opacity = opacity_value;
                        this->update();
                    } );
                    action->setCheckable( true );
                    action->setChecked( _segmentation_opacity == opacity_value );
                    segmentation_opacity_action_group->addAction( action );
                }

                auto screenshot_menu = context_menu.addMenu( "Screenshot" );
                screenshot_menu->addAction( "1x Resolution", [this] { this->create_screenshot( 1 ); } );
                screenshot_menu->addAction( "2x Resolution", [this] { this->create_screenshot( 2 ); } );
                screenshot_menu->addAction( "4x Resolution", [this] { this->create_screenshot( 4 ); } );
                screenshot_menu->addAction( "Clipboard", [this] { this->create_screenshot( 0 ); } );

                auto export_menu = context_menu.addMenu( "Export" );
                export_menu->addAction( "Columns", [this] { this->export_columns(); } );
                export_menu->addAction( "Matrix", [this] { this->export_matrix(); } );

                context_menu.exec( event->globalPosition().toPoint() );
            }
        }
        else if( _selection_mode == SelectionMode::eGrowSegment || _selection_mode == SelectionMode::eShrinkSegment )
        {
            const auto rectangle = _selection_polygon.boundingRect().intersected( _image_rectangle );
            if( !rectangle.isEmpty() )
            {
                auto begin = this->screen_to_pixel( rectangle.topLeft() );
                auto end = this->screen_to_pixel( rectangle.bottomRight() );
                if( begin.x > end.x ) std::swap( begin.x, end.x );
                if( begin.y > end.y ) std::swap( begin.y, end.y );

                const auto spatial_metadata = _dataset->spatial_metadata();
                const auto active_segment = _database.active_segment();
                const auto segment_number = _selection_mode == SelectionMode::eGrowSegment ? _database.active_segment()->number() : 0;
                auto segmentation_editor = _segmentation->editor();
                for( auto x = begin.x; x <= end.x; ++x )
                {
                    for( auto y = begin.y; y <= end.y; ++y )
                    {
                        const auto coordinates = vec2<uint32_t> { x, y };
                        const auto screen = this->pixel_to_screen( coordinates );
                        if( _selection_polygon.containsPoint( screen, Qt::OddEvenFill ) )
                        {
                            segmentation_editor.update_value( spatial_metadata->element_index( coordinates ), segment_number );
                        }
                    }
                }
            }
        }

        _selection_polygon.clear();
        _selection_mode = SelectionMode::eNone;
    }

    this->update();
}

void ImageViewer::mouseMoveEvent( QMouseEvent* event )
{
    if( event->buttons() & ( Qt::LeftButton | Qt::RightButton ) )
    {
        if( _selection_mode == SelectionMode::eGrowSegment || _selection_mode == SelectionMode::eShrinkSegment )
        {
            _selection_polygon.append( event->position() );
            this->update();
        }
    }

    if( _image_rectangle.contains( event->position() ) )
    {
        const auto pixel = this->screen_to_pixel( event->position() );
        _database.update_highlighted_element_index( _dataset->spatial_metadata()->element_index( pixel ) );
    }
    else
    {
        _database.update_highlighted_element_index( std::nullopt );
    }

    _cursor_position = event->position();
    this->update();
}
void ImageViewer::leaveEvent( QEvent* event )
{
    _database.update_highlighted_element_index( std::nullopt );
    _cursor_position = QPointF { -1.0, -1.0 };
}

QPointF ImageViewer::pixel_to_screen( vec2<uint32_t> pixel ) const
{
    const auto dimensions = _dataset->spatial_metadata()->dimensions;
    const auto x = ( pixel.x + 0.5 ) / dimensions.x;
    const auto y = ( pixel.y + 0.5 ) / dimensions.y;

    const QPointF screen {
        _image_rectangle.left() + x * _image_rectangle.width(),
        _image_rectangle.top() + y * _image_rectangle.height(),
    };
    return screen;
}
vec2<uint32_t> ImageViewer::screen_to_pixel( QPointF screen ) const
{
    const auto x = ( screen.x() - _image_rectangle.left() ) / _image_rectangle.width();
    const auto y = ( screen.y() - _image_rectangle.top() ) / _image_rectangle.height();

    const auto dimensions = _dataset->spatial_metadata()->dimensions;
    const vec2<uint32_t> pixel {
        static_cast<uint32_t>( std::clamp( x * dimensions.x, 0.0, static_cast<double>( dimensions.x - 1 ) ) ),
        static_cast<uint32_t>( std::clamp( y * dimensions.y, 0.0, static_cast<double>( dimensions.y - 1 ) ) )
    };
    return pixel;
}

void ImageViewer::reset_image_rectangle()
{
    const auto dimensions = _dataset->spatial_metadata()->dimensions;
    const auto scaling = std::min(
        ( this->width() - 20.0 ) / dimensions.x,
        ( this->height() - 20.0 ) / dimensions.y
    );
    _image_rectangle = QRectF { 0.0, 0.0, scaling * dimensions.x, scaling * dimensions.y };
    _image_rectangle.moveCenter( this->rect().center().toPointF() );
    this->update();
}
void ImageViewer::create_screenshot( uint32_t scaling ) const
{
    const auto filepath = scaling == 0 ? QString { "clipboard" } : QFileDialog::getSaveFileName( nullptr, "Export Image...", "", "*.png" );

    if( !filepath.isEmpty() )
    {
        const auto dimensions = _database.dataset()->spatial_metadata()->dimensions;
        auto image = QImage {
            static_cast<int32_t>( dimensions.x ),
            static_cast<int32_t>( dimensions.y ),
            QImage::Format_RGBA8888
        };
        image.fill( QColor { 0, 0, 0, 0 } );

        auto painter  = QPainter { &image };

        // Render image
        if( auto colormap = _colormap.lock() )
        {
            const auto& colors = colormap->colors();
            const auto dimensions = _dataset->spatial_metadata()->dimensions;
            const auto image = QImage { reinterpret_cast<const uchar*>( colors.data() ), static_cast<int>( dimensions.x ), static_cast<int>( dimensions.y ), QImage::Format_RGBA32FPx4 };
            painter.setOpacity( _image_opacity );
            painter.drawImage( image.rect(), image );
        }

        // Render segmentation colors
        const auto& segmentation_colors = _database.segmentation()->element_colors();
        const auto segmentation_image = QImage { reinterpret_cast<const uchar*>( segmentation_colors.data() ), static_cast<int>( dimensions.x ), static_cast<int>( dimensions.y ), QImage::Format_RGBA32FPx4 };
        painter.setOpacity( _segmentation_opacity );
        painter.drawImage( image.rect(), segmentation_image );

        if( filepath == "clipboard" )
        {
            // Workaround for copying and image with transparency into the clipboard
            // https://stackoverflow.com/questions/1260253/how-do-i-put-an-qimage-with-transparency-onto-the-clipboard-for-another-applicat
            auto mime_data = new QMimeData();
            auto image_data = QByteArray {};
            auto image_buffer = QBuffer { &image_data };

            image_buffer.open( QIODevice::WriteOnly );
            image.save( &image_buffer, "PNG" );
            image_buffer.close();
            mime_data->setData( "PNG", image_data );

            QApplication::clipboard()->setMimeData( mime_data );
        }
        else
        {
            image.scaledToHeight( scaling * image.height(), Qt::FastTransformation ).save( filepath );
        }
    }
}
void ImageViewer::export_columns() const
{
    const auto filepath = QFileDialog::getSaveFileName( nullptr, "Export Columns...", "", "*.csv" );
    if( !filepath.isEmpty() )
    {
        auto file = QFile { filepath };
        if( !file.open( QFile::WriteOnly | QFile::Text ) )
        {
            QMessageBox::critical( nullptr, "Error", "Could not open file for writing." );
            return;
        }

        auto stream = QTextStream { &file };
        stream << "element_index,segment_number,x,y,value\n";

        const auto dataset = _database.dataset();
        const auto spatial_metadata = dataset->spatial_metadata();
        const auto segmentation = _database.segmentation();
        const auto colormap = _colormap.lock();

        if( auto colormap_1d = colormap.dynamicCast<Colormap1D>() )
        {
            if( auto feature = colormap_1d->feature() )
            {
                const auto& feature_values = feature->values();
                for( uint32_t element_index = 0; element_index < dataset->element_count(); ++element_index )
                {
                    const auto coordinates = spatial_metadata->coordinates( element_index );
                    const auto segment_number = segmentation->segment_number( element_index );
                    stream << element_index << ',' << segment_number << ',' << coordinates.x << ',' << coordinates.y << ',' << feature_values[element_index] << '\n';
                }
            }
        }
        else
        {
            QMessageBox::warning( nullptr, "", "The current colormap does not support exporting as columns." );
        }
    }
}
void ImageViewer::export_matrix() const
{
    const auto filepath = QFileDialog::getSaveFileName( nullptr, "Export Matrix...", "", "*.csv" );
    if( !filepath.isEmpty() )
    {
        auto file = QFile { filepath };
        if( !file.open( QFile::WriteOnly | QFile::Text ) )
        {
            QMessageBox::critical( nullptr, "Error", "Could not open file for writing." );
            return;
        }

        auto stream = QTextStream { &file };
        stream << "y\\x";

        const auto dataset = _database.dataset();
        const auto spatial_metadata = dataset->spatial_metadata();
        const auto colormap = _colormap.lock();

        for( uint32_t x = 0; x < spatial_metadata->dimensions.x; ++x )
        {
            stream << ',' << x;
        }
        stream << '\n';

        if( auto colormap_1d = colormap.dynamicCast<Colormap1D>() )
        {
            if( auto feature = colormap_1d->feature() )
            {
                const auto& feature_values = feature->values();
                for( uint32_t y = 0; y < spatial_metadata->dimensions.y; ++y )
                {
                    stream << y;
                    for( uint32_t x = 0; x < spatial_metadata->dimensions.x; ++x )
                    {
                        const auto element_index = spatial_metadata->element_index( vec2<uint32_t> { x, y } );
                        stream << ',' << feature_values[element_index];
                    }
                    stream << '\n';
                }
            }
        }
        else
        {
            QMessageBox::warning( nullptr, "", "The current colormap does not support exporting as matrix." );
        }
    }
}