#include "image_viewer.hpp"

#include "colormap.hpp"
#include "database.hpp"
#include "dataset.hpp"
#include "feature.hpp"
#include "python.hpp"
#include "segmentation.hpp"

#include <qactiongroup.h>
#include <qapplication.h>
#include <qbuffer.h>
#include <qclipboard.h>
#include <qevent.h>
#include <qfiledialog.h>
#include <qlineedit.h>
#include <qmenu.h>
#include <qmessagebox.h>
#include <qmimedata.h>
#include <qpainter.h>

// ----- ImageViewer ----- //

ImageViewer::ImageViewer( Database& database ) : QWidget {}, _database { database }
{
    this->setFocusPolicy( Qt::WheelFocus );
    this->setMouseTracking( true );

    const auto segmentation = _database.segmentation();
    QObject::connect( segmentation.get(), &Segmentation::element_colors_changed, this, qOverload<>( &QWidget::update ) );
    QObject::connect( &_database, &Database::highlighted_element_index_changed, this, qOverload<>( &QWidget::update ) );

    const auto colormap_embedding = _database.colormap_embedding();
    QObject::connect( colormap_embedding.get(), &ColormapEmbedding::colors_changed, this, qOverload<>( &QWidget::update ) );
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
    QWidget::paintEvent( event );

    auto painter = QPainter { this };
    painter.setRenderHint( QPainter::Antialiasing );
    painter.fillRect( _image_rectangle, Qt::black );

    const auto dataset = _database.dataset();
    const auto segmentation = _database.segmentation();

    // Render image
    if( const auto colormap = _colormap.lock() )
    {
        if( const auto& colors = colormap->colors(); colors.size() )
        {
            const auto dimensions = dataset->spatial_metadata()->dimensions;
            const auto image = QImage { reinterpret_cast<const uchar*>( colors.data() ), static_cast<int>( dimensions.x ), static_cast<int>( dimensions.y ), QImage::Format_RGBA32FPx4 };
            painter.setOpacity( _image_opacity );
            painter.drawImage( _image_rectangle, image );
            painter.setOpacity( 1.0 );
        }
    }

    if( _coloring == ColoringMode::eSegmentation )
    {
        // Render segmentation colors
        const auto& segmentation_colors = segmentation->element_colors();
        const auto dimensions = dataset->spatial_metadata()->dimensions;
        const auto image = QImage { reinterpret_cast<const uchar*>( segmentation_colors.data() ), static_cast<int>( dimensions.x ), static_cast<int>( dimensions.y ), QImage::Format_RGBA32FPx4 };
        painter.setOpacity( _segmentation_opacity );
        painter.drawImage( _image_rectangle, image );
        painter.setOpacity( 1.0 );
    }
    else if( _coloring == ColoringMode::eFalseColoring )
    {
        if( const auto colormap = _database.colormap_embedding() )
        {
            const auto& colors = colormap->colors();
            const auto dimensions = dataset->spatial_metadata()->dimensions;
            const auto image = QImage { reinterpret_cast<const uchar*>( colors.data() ), static_cast<int>( dimensions.x ), static_cast<int>( dimensions.y ), QImage::Format_RGBA32FPx4 };
            painter.setOpacity( _segmentation_opacity );
            painter.drawImage( _image_rectangle, image );
            painter.setOpacity( 1.0 );
        }
    }

    // Render overlay image
    if( _overlay_image.size() )
    {
        const auto image = QImage {
            reinterpret_cast<const uchar*>( _overlay_image.data() ),
            static_cast<int>( _overlay_image.dimensions()[0] ),
            static_cast<int>( _overlay_image.dimensions()[1] ),
            QImage::Format_RGB888
        };
        painter.setOpacity( _segmentation_opacity );
        painter.drawImage( _image_rectangle, image );
        painter.setOpacity( 1.0 );
    }

    // Render highlighted element
    if( const auto element_index = _database.highlighted_element_index(); element_index.has_value() )
    {
        const auto spatial_metadata = dataset->spatial_metadata();
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
        const auto stroke_color = _selection_mode == InteractionMode::eGrowSegment ? _database.active_segment()->color().qcolor() : QColor { 200, 200, 200 };
        auto brush_color = stroke_color;
        brush_color.setAlpha( 150 );

        painter.setPen( QPen { stroke_color, 2.0 } );
        painter.setBrush( brush_color );
        painter.drawPolygon( _selection_polygon );
    }

    // Render highlighted element information
    if( const auto element_index = _database.highlighted_element_index(); element_index.has_value() )
    {
        const auto pixel = dataset->spatial_metadata()->coordinates( *element_index );
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
                    if( feature_values.size() > *element_index )
                    {
                        const auto value = feature_values[*element_index];
                        labels_string += QString { "\nvalue: " };
                        values_string += '\n' + QString::number( value, 'f', std::min( 5, utility::compute_precision( value ) ) );
                    }
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

    // Render sidebar
    {
        const auto sidebar_width = 20.0;
        const auto sidebar_height = 150.0;

        _sidebar_rectangle = QRectF { 5.0, 0.0, sidebar_width, sidebar_height };
        _sidebar_rectangle.moveTop( this->rect().center().y() - sidebar_height / 2.0 );

        painter.setPen( Qt::NoPen );
        painter.setBrush( QBrush { QColor { 255, 255, 255, 200 } } );
        painter.drawRoundedRect( _sidebar_rectangle, 5.0, 5.0 );

        auto handle_color = QColor { 0, 0, 0, 255 };
        if( _coloring == ColoringMode::eSegmentation )
        {
            handle_color = _database.active_segment()->color().qcolor();
        }

        auto groove_rectangle = _sidebar_rectangle.marginsRemoved( QMarginsF { 8.0, 8.0, 8.0, 8.0 } );
        const auto handle_position = groove_rectangle.bottom() - _segmentation_opacity * groove_rectangle.height();

        painter.setBrush( QColor { 220, 220, 220 } );
        painter.drawRoundedRect( groove_rectangle, 2.0, 2.0 );

        groove_rectangle.setTop( handle_position );
        painter.setBrush( handle_color );
        painter.drawRoundedRect( groove_rectangle, 2.0, 2.0 );

        auto handle_rectangle = QRectF { groove_rectangle.center().x() - 5.0, handle_position - 5.0, 10.0, 10.0 };
        const auto hovered = handle_rectangle.contains( _cursor_position );

        painter.setPen( hovered ? QPen { handle_color, 2.0 } : Qt::NoPen );
        painter.setBrush( QBrush { handle_color } );
        painter.drawRoundedRect( handle_rectangle, 5.0, 5.0 );
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
    if( _sidebar_rectangle.contains( event->position() ) )
    {
        _selection_mode = InteractionMode::eOpacitySlider;
        return;
    }

    if( event->button() == Qt::LeftButton )
    {
        _selection_polygon.append( event->position() );
        _selection_mode = InteractionMode::eGrowSegment;
    }
    else if( event->button() == Qt::RightButton )
    {
        _selection_polygon.append( event->position() );
        _selection_mode = InteractionMode::eShrinkSegment;
    }
}
void ImageViewer::mouseReleaseEvent( QMouseEvent* event )
{
    if( _selection_mode == InteractionMode::eOpacitySlider )
    {
        _selection_mode = InteractionMode::eNone;
        this->update();
        return;
    }

    if( event->button() == Qt::LeftButton || event->button() == Qt::RightButton )
    {
        if( _selection_polygon.size() == 1 )
        {
            if( event->button() == Qt::RightButton )
            {
                auto context_menu = QMenu {};

                _database.populate_segmentation_menu( context_menu );
                context_menu.addSeparator();

                auto coloring_menu = context_menu.addMenu( "Coloring" );
                auto coloring_action_group = new QActionGroup { coloring_menu };
                coloring_action_group->setExclusive( true );

                const auto coloring_options = std::vector<std::pair<const char*, ColoringMode>> {
                    { "Segmentation", ColoringMode::eSegmentation },
                    { "False-coloring", ColoringMode::eFalseColoring },
                };

                for( const auto [label, coloring] : coloring_options )
                {
                    const auto action = coloring_menu->addAction( label, [this, coloring]
                    {
                        _coloring = coloring;
                        this->update();
                    } );

                    action->setCheckable( true );
                    action->setChecked( _coloring == coloring );
                    coloring_action_group->addAction( action );
                }

                //auto overlay_menu = context_menu.addMenu( "Overlay" );
                //overlay_menu->addAction( "Import", [this] { this->import_overlay(); } );
                //overlay_menu->addAction( "Remove", [this]
                //{
                //    _overlay_image.clear();
                //    this->update();
                //} );

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

                context_menu.addAction( "Reset View", [this] { this->reset_image_rectangle(); } );

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
        else if( _selection_mode == InteractionMode::eGrowSegment || _selection_mode == InteractionMode::eShrinkSegment )
        {
            const auto rectangle = _selection_polygon.boundingRect().intersected( _image_rectangle );
            if( !rectangle.isEmpty() )
            {
                auto begin = this->screen_to_pixel( rectangle.topLeft() );
                auto end = this->screen_to_pixel( rectangle.bottomRight() );
                if( begin.x > end.x ) std::swap( begin.x, end.x );
                if( begin.y > end.y ) std::swap( begin.y, end.y );

                const auto dataset          = _database.dataset();
                const auto segmentation     = _database.segmentation();
                const auto active_segment   = _database.active_segment();

                const auto spatial_metadata = dataset->spatial_metadata();
                const auto segment_number = _selection_mode == InteractionMode::eGrowSegment ? _database.active_segment()->number() : 0;

                auto segmentation_editor = segmentation->editor();
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
        _selection_mode = InteractionMode::eNone;
    }

    this->update();
}

void ImageViewer::mouseMoveEvent( QMouseEvent* event )
{
    if( _selection_mode == InteractionMode::eOpacitySlider )
    {
        const auto slider_height = _sidebar_rectangle.height();
        const auto position = std::clamp( event->position().y() - _sidebar_rectangle.top(), 0.0, slider_height );
        _segmentation_opacity = 1.0 - position / slider_height;
        this->update();
        return;
    }

    if( event->buttons() & ( Qt::LeftButton | Qt::RightButton ) )
    {
        if( _selection_mode == InteractionMode::eGrowSegment || _selection_mode == InteractionMode::eShrinkSegment )
        {
            _selection_polygon.append( event->position() );
            this->update();
        }
    }

    if( _image_rectangle.contains( event->position() ) )
    {
        const auto spatial_metadata = _database.dataset()->spatial_metadata();
        const auto pixel = this->screen_to_pixel( event->position() );
        _database.update_highlighted_element_index( spatial_metadata->element_index( pixel ) );
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
    const auto dimensions = _database.dataset()->spatial_metadata()->dimensions;
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

    const auto dimensions = _database.dataset()->spatial_metadata()->dimensions;
    const vec2<uint32_t> pixel {
        static_cast<uint32_t>( std::clamp( x * dimensions.x, 0.0, static_cast<double>( dimensions.x - 1 ) ) ),
        static_cast<uint32_t>( std::clamp( y * dimensions.y, 0.0, static_cast<double>( dimensions.y - 1 ) ) )
    };
    return pixel;
}

void ImageViewer::reset_image_rectangle()
{
    const auto dataset      = _database.dataset();
    const auto dimensions   = dataset->spatial_metadata()->dimensions;
    const auto scaling      = std::min(
        ( this->width() - 20.0 ) / dimensions.x,
        ( this->height() - 20.0 ) / dimensions.y
    );
    _image_rectangle = QRectF { 0.0, 0.0, scaling * dimensions.x, scaling * dimensions.y };
    _image_rectangle.moveCenter( this->rect().center().toPointF() );
    this->update();
}
void ImageViewer::import_overlay()
{
    const auto filepath = QFileDialog::getOpenFileName( nullptr, "Import Overlay...", "", "*.png;*.jpg;*.tif", nullptr );
    if( !filepath.isEmpty() )
    {
        if( const auto colormap = _colormap.lock() )
        {
            auto interpreter = py::interpreter {};

            const auto dataset      = _database.dataset();
            const auto dimensions   = dataset->spatial_metadata()->dimensions;

            auto dataset_memoryview = std::optional<py::memoryview> {};
            dataset->visit( [&dataset_memoryview, dimensions] ( const auto& dataset )
            {
                using value_type = std::remove_cvref_t<decltype( dataset )>::value_type;
                dataset_memoryview = py::memoryview::from_buffer(
                    dataset.intensities().data(),
                    { dimensions.y, dimensions.x, dataset.channel_count() },
                    { dimensions.x * dataset.channel_count() * sizeof( value_type ), dataset.channel_count() * sizeof( value_type ), sizeof( value_type ) }
                );
            } );
            if( !dataset_memoryview.has_value() )
            {
                QMessageBox::critical( nullptr, "Import Overlay...", "Cannot import overlay for this dataset" );
                return;
            }

            const auto colors_memoryview = py::memoryview::from_buffer(
                reinterpret_cast<const float*>( colormap->colors().data() ),
                { dimensions.y, dimensions.x, 4u },
                { dimensions.x * 4 * sizeof( float ), 4 * sizeof( float ), sizeof( float ) }
            );

            using namespace py::literals;
            auto locals = py::dict {
                "filepath"_a = filepath.toStdWString(),
                "dataset"_a = dataset_memoryview,
                "colors"_a = colors_memoryview
            };

            try
            {
                py::exec( R"(
try:
    import cv2
    import numpy as np

    # --- 1. Preprocessing ---
    print( "Computing reference image..." )
    dataset = np.asarray( dataset, copy=False )
    dataset = np.sum( dataset, axis=2 )
    dataset = dataset - np.min( dataset )
    dataset = dataset / np.max( dataset )

    # img_ref = np.asarray( colors, copy=True )
    # gray_ref = cv2.cvtColor( img_ref, cv2.COLOR_RGBA2GRAY )
    # gray_ref = gray_ref - np.min( gray_ref )
    # gray_ref = gray_ref / np.max( gray_ref )
    gray_ref = dataset
    gray_ref = ( gray_ref * 255 ).astype( np.uint8 )
    print( f"Overlay: {gray_ref.shape}, {gray_ref.dtype}, {np.min( gray_ref )} - {np.max( gray_ref )}" )

    img_ovl = cv2.imread( filepath, cv2.IMREAD_UNCHANGED )
    gray_ovl = cv2.cvtColor( img_ovl, cv2.COLOR_BGR2GRAY ).astype( np.float32 )
    gray_ovl = gray_ovl - np.min( gray_ovl )
    gray_ovl = gray_ovl / np.max( gray_ovl )
    gray_ovl = ( gray_ovl * 255 ).astype( np.uint8 )
    print( f"Overlay: {gray_ovl.shape}, {gray_ovl.dtype}, {np.min( gray_ovl )} - {np.max( gray_ovl )}" )

    hist_ref = cv2.calcHist( [gray_ref], [0], None, [256], [0,256] )
    hist_ovl = cv2.calcHist( [gray_ovl], [0], None, [256], [0,256] )
    
    score_original = cv2.compareHist( hist_ref, hist_ovl, cv2.HISTCMP_CORREL )
    score_inverted = cv2.compareHist( hist_ref, hist_ovl[::-1], cv2.HISTCMP_CORREL )
    print( f"Histogram matching scores: original={score_original}, inverted={score_inverted}" )

    if score_inverted > score_original:
        print( "Inverting overlay image for better matching..." )
        gray_ovl = cv2.bitwise_not( gray_ovl )

    # --- 2. SIFT Feature Detection ---
    print( "Detecting SIFT features..." )
    sift = cv2.SIFT_create()
    
    # Detect keypoints and compute descriptors
    kp_ref, des_ref = sift.detectAndCompute( gray_ref, None )
    kp_ovl, des_ovl = sift.detectAndCompute( gray_ovl, None )
    
    if des_ref is None or des_ovl is None:
        raise ValueError( "Could not find features in one of the images." )

    # --- 3. Feature Matching (FLANN) ---
    print( "Matching features..." )
    # FLANN parameters for SIFT
    FLANN_INDEX_KDTREE = 1
    index_params = dict( algorithm=FLANN_INDEX_KDTREE, trees=5 )
    search_params = dict( checks=50 )
    
    flann = cv2.FlannBasedMatcher( index_params, search_params )
    
    # k=2 for Ratio Test
    matches = flann.knnMatch( des_ovl, des_ref, k=2 )

    # --- 4. Lowe's Ratio Test ---
    # Keep only matches where the best match is significantly better than the second best
    good_matches = []
    ratio_thresh = 0.7  # Typically 0.7 or 0.75
    for m, n in matches:
        if m.distance < ratio_thresh * n.distance:
            good_matches.append( m )
            
    print( f"Good matches found: {len( good_matches )}" )

    if len( good_matches ) < 4:
        raise ValueError("Not enough good matches to compute Homography.")

    # --- 5. Compute Homography (RANSAC) ---
    # Extract coordinates from the keypoints
    src_pts = np.float32( [kp_ovl[m.queryIdx].pt for m in good_matches] ).reshape( -1, 1, 2 )
    dst_pts = np.float32( [kp_ref[m.trainIdx].pt for m in good_matches] ).reshape( -1, 1, 2 )

    # Calculate Homography
    M, mask = cv2.findHomography( src_pts, dst_pts, cv2.RANSAC, 5.0 )
    # M, mask = cv2.estimateAffine2D( src_pts, dst_pts, method=cv2.RANSAC )

    if M is None:
        raise ValueError( "Homography calculation failed." )

    # --- 6. Apply Warping ---
    aligned = cv2.warpPerspective( img_ovl, M, ( gray_ref.shape[1], gray_ref.shape[0] ) )
    # aligned = cv2.warpAffine( img_ovl, M, ( gray_ref.shape[1], gray_ref.shape[0] ) )
    print( f"Aligned: {aligned.shape}, {aligned.dtype}, {np.min( aligned )} - {np.max( aligned )}" )

except Exception as exception:
    error = str( exception ))", py::globals(), locals );
            }
            catch( py::error_already_set& error )
            {
                locals["error"] = error.what();
            }

            if( locals.contains( "error" ) )
            {
                const auto error = locals["error"].cast<std::string>();
                Console::error( std::format( "Python error during overlay import: {}", error ) );
                QMessageBox::critical( nullptr, "Import Overlay...", "Failed to import overlay", QMessageBox::Ok );
            }
            else
            {
                const auto aligned = locals["aligned"].cast<py::array>();
                _overlay_image = Tensor::with_rank<3>::with_type<uint8_t> { { dimensions.x, dimensions.y, 3 }, 0 };
                std::memcpy( _overlay_image.data(), aligned.data(), _overlay_image.bytes() );
                this->update();
            }
        }
    }
}
void ImageViewer::create_screenshot( uint32_t scaling ) const
{
    const auto filepath = scaling == 0 ? QString { "clipboard" } : QFileDialog::getSaveFileName( nullptr, "Export Image...", "", "*.png", nullptr );

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
            const auto dimensions = _database.dataset()->spatial_metadata()->dimensions;
            const auto image = QImage { reinterpret_cast<const uchar*>( colors.data() ), static_cast<int>( dimensions.x ), static_cast<int>( dimensions.y ), QImage::Format_RGBA32FPx4 };
            painter.setOpacity( _image_opacity );
            painter.drawImage( image.rect(), image );
        }

        // Render segmentation colors or false-coloring
        if( _coloring == ColoringMode::eSegmentation )
        {
            // Render segmentation colors
            const auto segmentation = _database.segmentation();
            const auto& segmentation_colors = segmentation->element_colors();
            const auto image = QImage { reinterpret_cast<const uchar*>( segmentation_colors.data() ), static_cast<int>( dimensions.x ), static_cast<int>( dimensions.y ), QImage::Format_RGBA32FPx4 };
            painter.setOpacity( _segmentation_opacity );
            painter.drawImage( image.rect(), image );
        }
        else if( _coloring == ColoringMode::eFalseColoring )
        {
            if( const auto colormap = _database.colormap_embedding() )
            {
                const auto& colors = colormap->colors();
                const auto image = QImage { reinterpret_cast<const uchar*>( colors.data() ), static_cast<int>( dimensions.x ), static_cast<int>( dimensions.y ), QImage::Format_RGBA32FPx4 };
                painter.setOpacity( _segmentation_opacity );
                painter.drawImage( image.rect(), image );
            }
        }

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
            const auto result = image.scaledToHeight( scaling * image.height(), Qt::FastTransformation ).save( filepath );
        }
    }
}
void ImageViewer::export_columns() const
{
    const auto filepath = QFileDialog::getSaveFileName( nullptr, "Export Columns...", "", "*.csv", nullptr );
    if( !filepath.isEmpty() )
    {
        auto file = QFile { filepath };
        if( !file.open( QFile::WriteOnly | QFile::Text ) )
        {
            QMessageBox::critical( nullptr, "", "Failed to open file" );
            return;
        }

        auto stream = QTextStream { &file };
        stream << "index,segment_number,x,y,value\n";

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
            QMessageBox::warning( nullptr, "", "The current colormap does not support exporting as columns" );
        }
    }
}
void ImageViewer::export_matrix() const
{
    const auto filepath = QFileDialog::getSaveFileName( nullptr, "Export Matrix...", "", "*.csv", nullptr );
    if( !filepath.isEmpty() )
    {
        auto file = QFile { filepath };
        if( !file.open( QFile::WriteOnly | QFile::Text ) )
        {
            QMessageBox::critical( nullptr, "", "Failed to open file" );
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
            QMessageBox::warning( nullptr, "", "The current colormap does not support exporting as matrix" );
        }
    }
}