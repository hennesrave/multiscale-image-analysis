#include "dataset_alignment_dialog.hpp"

#include "colormap.hpp"
#include "dataset.hpp"
#include "feature.hpp"
#include "number_input.hpp"

#include <qactiongroup.h>
#include <qevent.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qmenu.h>
#include <qpainter.h>
#include <qwidgetaction.h>

#include <iostream>

namespace
{
    std::optional<QTransform> createAffineTransform(
        const std::array<QPointF, 3>& src,
        const std::array<QPointF, 3>& tgt )
    {
        // Vectors relative to the first point
        qreal v_xx = src[1].x() - src[0].x();
        qreal v_xy = src[1].y() - src[0].y();
        qreal v_yx = src[2].x() - src[0].x();
        qreal v_yy = src[2].y() - src[0].y();

        qreal w_xx = tgt[1].x() - tgt[0].x();
        qreal w_xy = tgt[1].y() - tgt[0].y();
        qreal w_yx = tgt[2].x() - tgt[0].x();
        qreal w_yy = tgt[2].y() - tgt[0].y();

        // Calculate the determinant
        qreal det = v_xx * v_yy - v_xy * v_yx;

        // If determinant is 0, the source points are collinear
        if( std::abs( det ) < 1e-10 )
        {
            return std::nullopt;
        }

        qreal inv_det = 1.0 / det;

        // Solve for the 2x2 matrix elements
        qreal m11 = ( v_yy * w_xx - v_xy * w_yx ) * inv_det;
        qreal m12 = ( v_yy * w_xy - v_xy * w_yy ) * inv_det;
        qreal m21 = ( -v_yx * w_xx + v_xx * w_yx ) * inv_det;
        qreal m22 = ( -v_yx * w_xy + v_xx * w_yy ) * inv_det;

        // Solve for the translation
        qreal dx = tgt[0].x() - ( src[0].x() * m11 + src[0].y() * m21 );
        qreal dy = tgt[0].y() - ( src[0].x() * m12 + src[0].y() * m22 );

        // QTransform(m11, m12, m21, m22, dx, dy)
        return QTransform( m11, m12, m21, m22, dx, dy );
    }
}

// ----- DatasetAlignmentDialog ----- //

DatasetAlignmentDialog::DatasetAlignmentDialog( QSharedPointer<Dataset> source, QSharedPointer<Dataset> target )
    : QDialog {}, _source { source }, _target { target }
{
    this->setWindowTitle( "Dataset Alignment..." );
    this->setFocusPolicy( Qt::WheelFocus );
    this->setFocus();

    const auto source_spatial_metadata = _source->spatial_metadata();
    const auto target_spatial_metadata = _target->spatial_metadata();

    _source_feature = QSharedPointer<DatasetChannelsFeature>::create( source, Range<uint32_t> { 0, source->channel_count() - 1 }, DatasetChannelsFeature::Reduction::eAccumulate, DatasetChannelsFeature::BaselineCorrection::eNone );
    _target_feature = QSharedPointer<DatasetChannelsFeature>::create( target, Range<uint32_t> { 0, target->channel_count() - 1 }, DatasetChannelsFeature::Reduction::eAccumulate, DatasetChannelsFeature::BaselineCorrection::eNone );

    _source_colormap = QSharedPointer<Colormap1D>::create( ColormapTemplate::turbo.clone() );
    _target_colormap = QSharedPointer<Colormap1D>::create( ColormapTemplate::turbo.clone() );

    _source_colormap->update_feature( _source_feature );
    _target_colormap->update_feature( _target_feature );

    QObject::connect( _source_colormap.get(), &Colormap1D::colors_changed, this, qOverload<>( &QWidget::update ) );
    QObject::connect( _target_colormap.get(), &Colormap1D::colors_changed, this, qOverload<>( &QWidget::update ) );
}

QSharedPointer<Dataset> DatasetAlignmentDialog::source() const noexcept
{
    return _source;
}
QSharedPointer<Dataset> DatasetAlignmentDialog::target() const noexcept
{
    return _target;
}
QSharedPointer<Dataset> DatasetAlignmentDialog::aligned() const noexcept
{
    return _aligned;
}

void DatasetAlignmentDialog::paintEvent( QPaintEvent* event )
{
    const auto source_dimensions = _source->spatial_metadata()->dimensions;
    const auto target_dimensions = _target->spatial_metadata()->dimensions;

    const auto& source_colors = _source_colormap->colors();
    const auto& target_colors = _target_colormap->colors();

    const auto source_image = QImage {
        reinterpret_cast<const uchar*>( source_colors.data() ),
        static_cast<int>( source_dimensions.x ),
        static_cast<int>( source_dimensions.y ),
        QImage::Format_RGBA32FPx4
    };
    const auto target_image = QImage {
        reinterpret_cast<const uchar*>( target_colors.data() ),
        static_cast<int>( target_dimensions.x ),
        static_cast<int>( target_dimensions.y ),
        QImage::Format_RGBA32FPx4
    };

    const auto source_rectangle = this->source_rectangle();
    const auto target_rectangle = this->target_rectangle();

    auto painter = QPainter { this };
    painter.setRenderHint( QPainter::Antialiasing );

    painter.drawImage( target_rectangle, target_image );

    const auto source_polygon = QPolygonF {
        source_rectangle.topLeft() + QPointF { _source_points[0].x() * source_rectangle.width(), _source_points[0].y() * source_rectangle.height() },
        source_rectangle.topLeft() + QPointF { _source_points[1].x() * source_rectangle.width(), _source_points[1].y() * source_rectangle.height() },
        source_rectangle.topLeft() + QPointF { _source_points[2].x() * source_rectangle.width(), _source_points[2].y() * source_rectangle.height() }
    };
    const auto target_polygon = QPolygonF {
        target_rectangle.topLeft() + QPointF { _target_points[0].x() * target_rectangle.width(), _target_points[0].y() * target_rectangle.height() },
        target_rectangle.topLeft() + QPointF { _target_points[1].x() * target_rectangle.width(), _target_points[1].y() * target_rectangle.height() },
        target_rectangle.topLeft() + QPointF { _target_points[2].x() * target_rectangle.width(), _target_points[2].y() * target_rectangle.height() }
    };

    const auto current_transform = createAffineTransform( { source_polygon[0], source_polygon[1], source_polygon[2] }, { target_polygon[0], target_polygon[1], target_polygon[2] } );
    if( current_transform.has_value() )
    {
        _transform = current_transform.value();
    }

    painter.setOpacity( _source_opacity );
    painter.setTransform( _transform, false );
    painter.drawImage( source_rectangle, source_image );
    painter.resetTransform();
    painter.setOpacity( 1.0 );

    painter.setPen( Qt::black );
    painter.setBrush( Qt::lightGray );
    for( const auto& point : target_polygon )
    {
        painter.drawEllipse( point, 5.0, 5.0 );
    }
}

void DatasetAlignmentDialog::mousePressEvent( QMouseEvent* event )
{
    if( event->button() == Qt::LeftButton || event->button() == Qt::RightButton )
    {
        const auto target_rectangle = this->target_rectangle();
        const auto cursor = event->position();

        struct
        {
            std::optional<size_t> index = std::nullopt;
            qreal distance = 20.0;
        } closest;

        for( size_t i = 0; i < _target_points.size(); ++i )
        {
            const auto& point = _target_points[i];
            const auto screen = QPointF {
                target_rectangle.left() + point.x() * target_rectangle.width(),
                target_rectangle.top() + point.y() * target_rectangle.height()
            };

            const auto distance = QLineF { cursor, screen }.length();
            if( distance < closest.distance )
            {
                closest.index = i;
                closest.distance = distance;
            }
        }

        _active_point_index = closest.index;
    }
}
void DatasetAlignmentDialog::mouseMoveEvent( QMouseEvent* event )
{
    if( event->buttons() & Qt::LeftButton )
    {
        if( _active_point_index.has_value() )
        {
            _interaction_mode = InteractionMode::eMovePoint;

            const auto target_rectangle = this->target_rectangle();
            const auto cursor = event->position();
            const auto index = _active_point_index.value();

            _target_points[index] = QPointF {
                ( cursor.x() - target_rectangle.left() ) / target_rectangle.width(),
                ( cursor.y() - target_rectangle.top() ) / target_rectangle.height()
            };

            this->update();
        }
    }
    else if( event->buttons() & Qt::RightButton )
    {
        if( _active_point_index.has_value() )
        {
            _interaction_mode = InteractionMode::eMovePoint;

            const auto source_rectangle = this->source_rectangle();
            const auto target_rectangle = this->target_rectangle();
            const auto cursor = event->position();
            const auto index = _active_point_index.value();

            const auto inverse = _transform.inverted().map( cursor );
            _source_points[index] = QPointF {
                ( inverse.x() - source_rectangle.left() ) / source_rectangle.width(),
                ( inverse.y() - source_rectangle.top() ) / source_rectangle.height()
            };

            _target_points[index] = QPointF {
                ( cursor.x() - target_rectangle.left() ) / target_rectangle.width(),
                ( cursor.y() - target_rectangle.top() ) / target_rectangle.height()
            };

            this->update();

        }
    }
}
void DatasetAlignmentDialog::mouseReleaseEvent( QMouseEvent* event )
{
    if( event->button() == Qt::RightButton && _interaction_mode == InteractionMode::eNone )
    {
        auto context_menu = QMenu {};

        {
            struct Axis
            {
                vec2<double> bounds;
                vec2<double> domain;
            } _xaxis, _yaxis;

            _xaxis.bounds.x = _source_colormap->lower().automatic_value();
            _xaxis.bounds.y = _source_colormap->upper().automatic_value();
            _xaxis.domain.x = _source_colormap->lower().override_value().value_or( _xaxis.bounds.x );
            _xaxis.domain.y = _source_colormap->upper().override_value().value_or( _xaxis.bounds.y );

            _yaxis.bounds.x = _target_colormap->lower().automatic_value();
            _yaxis.bounds.y = _target_colormap->upper().automatic_value();
            _yaxis.domain.x = _target_colormap->lower().override_value().value_or( _yaxis.bounds.x );
            _yaxis.domain.y = _target_colormap->upper().override_value().value_or( _yaxis.bounds.y );

            auto xaxis_label = new QLabel { "  Source" };
            auto xaxis_label_action = new QWidgetAction { &context_menu };
            xaxis_label_action->setDefaultWidget( xaxis_label );

            auto yaxis_label = new QLabel { "  Target" };
            auto yaxis_label_action = new QWidgetAction { &context_menu };
            yaxis_label_action->setDefaultWidget( yaxis_label );

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
            xaxis_container_layout->addWidget( xaxis_lower_input );
            xaxis_container_layout->addWidget( new QLabel { " \u2014 " } );
            xaxis_container_layout->addWidget( xaxis_upper_input );

            auto yaxis_container_widget = new QWidget { &context_menu };
            auto yaxis_container_layout = new QHBoxLayout { yaxis_container_widget };
            yaxis_container_layout->setContentsMargins( 20, 2, 20, 2 );
            yaxis_container_layout->setSpacing( 3 );
            yaxis_container_layout->addWidget( yaxis_lower_input );
            yaxis_container_layout->addWidget( new QLabel { " \u2014 " } );
            yaxis_container_layout->addWidget( yaxis_upper_input );

            auto xaxis_widget_action = new QWidgetAction { &context_menu };
            xaxis_widget_action->setDefaultWidget( xaxis_container_widget );

            auto yaxis_widget_action = new QWidgetAction { &context_menu };
            yaxis_widget_action->setDefaultWidget( yaxis_container_widget );

            QObject::connect( xaxis_lower, &Override<double>::value_changed, this, [=]
            {
                _source_colormap->lower().update_override_value( xaxis_lower->value() );
            } );
            QObject::connect( xaxis_upper, &Override<double>::value_changed, this, [=]
            {
                _source_colormap->upper().update_override_value( xaxis_upper->value() );
            } );
            QObject::connect( yaxis_lower, &Override<double>::value_changed, this, [=]
            {
                _target_colormap->lower().update_override_value( yaxis_lower->value() );
            } );
            QObject::connect( yaxis_upper, &Override<double>::value_changed, this, [=]
            {
                _target_colormap->upper().update_override_value( yaxis_upper->value() );
            } );

            auto menu_axes = context_menu.addMenu( "Colormaps" );
            menu_axes->addAction( xaxis_label_action );
            menu_axes->addAction( xaxis_widget_action );
            menu_axes->addSeparator();
            menu_axes->addAction( yaxis_label_action );
            menu_axes->addAction( yaxis_widget_action );
        }

        auto interpolation_menu = context_menu.addMenu( "Interpolation Mode" );

        auto action_nearest_neighbor = interpolation_menu->addAction( "Nearest Neighbor", [this]
        {
            _interpolation_mode = InterpolationMode::eNearestNeighbor;
        } );
        action_nearest_neighbor->setCheckable( true );
        action_nearest_neighbor->setChecked( _interpolation_mode == InterpolationMode::eNearestNeighbor );

        auto action_bilinear = interpolation_menu->addAction( "Bilinear", [this]
        {
            _interpolation_mode = InterpolationMode::eBilinear;
        } );
        action_bilinear->setCheckable( true );
        action_bilinear->setChecked( _interpolation_mode == InterpolationMode::eBilinear );

        auto interpolation_action_group = new QActionGroup { interpolation_menu };
        interpolation_action_group->setExclusive( true );
        interpolation_action_group->addAction( action_nearest_neighbor );
        interpolation_action_group->addAction( action_bilinear );

        auto edge_menu = context_menu.addMenu( "Edge Mode" );
        auto action_zero = edge_menu->addAction( "Zero", [this]
        {
            _edge_mode = EdgeMode::eZero;
        } );
        action_zero->setCheckable( true );
        action_zero->setChecked( _edge_mode == EdgeMode::eZero );

        auto action_clamp = edge_menu->addAction( "Clamp", [this]
        {
            _edge_mode = EdgeMode::eClamp;
        } );
        action_clamp->setCheckable( true );
        action_clamp->setChecked( _edge_mode == EdgeMode::eClamp );

        auto edge_action_group = new QActionGroup { edge_menu };
        edge_action_group->setExclusive( true );
        edge_action_group->addAction( action_zero );
        edge_action_group->addAction( action_clamp );

        context_menu.addAction( "Reset Alignment", [this]
        {
            _source_points = { QPointF { 0.0, 0.0 }, QPointF { 1.0, 0.0 }, QPointF { 0.0, 1.0 } };
            _target_points = { QPointF { 0.0, 0.0 }, QPointF { 1.0, 0.0 }, QPointF { 0.0, 1.0 } };
            this->update();
        } );
        context_menu.addAction( "Confirm Alignment", [this]
        {
            Console::info( "Computing aligned dataset..." );

            _source->visit( [this] ( const auto& source_typed )
            {
                using value_type = std::remove_cvref_t<decltype( source_typed )>::value_type;

                const auto& source_intensities = source_typed.intensities();

                const auto source_spatial_metadata = _source->spatial_metadata();
                const auto target_spatial_metadata = _target->spatial_metadata();

                const auto source_rectangle = this->source_rectangle();
                const auto target_rectangle = this->target_rectangle();

                const auto element_count = _target->element_count();
                const auto channel_count = _source->channel_count();

                auto intensities = Matrix<value_type> { { element_count, channel_count }, value_type {} };

                for( uint32_t element_index = 0; element_index < element_count; ++element_index )
                {
                    if( element_index % 1000 == 0 )
                    {
                        const auto progress = static_cast<double>( element_index ) / element_count * 100.0;
                        std::cout << "\r[Dataset Alignment] Progress: " << std::fixed << std::setprecision( 2 ) << progress << "%";
                    }

                    const auto target_pixel_coordinates = target_spatial_metadata->coordinates( element_index );
                    const auto target_normalized_coordinates = QPointF {
                        static_cast<qreal>( target_pixel_coordinates.x ) / target_spatial_metadata->width,
                        static_cast<qreal>( target_pixel_coordinates.y ) / target_spatial_metadata->height
                    };
                    const auto target_rectangle_coordinates = QPointF {
                        target_rectangle.left() + target_normalized_coordinates.x() * target_rectangle.width(),
                        target_rectangle.top() + target_normalized_coordinates.y() * target_rectangle.height()
                    };
                    const auto source_rectangle_coordinates = _transform.inverted().map( target_rectangle_coordinates );
                    const auto source_normalized_coordinates = QPointF {
                        ( source_rectangle_coordinates.x() - source_rectangle.left() ) / source_rectangle.width(),
                        ( source_rectangle_coordinates.y() - source_rectangle.top() ) / source_rectangle.height()
                    };
                    const auto source_pixel_coordinates = QPointF {
                        source_normalized_coordinates.x() * source_spatial_metadata->width,
                        source_normalized_coordinates.y() * source_spatial_metadata->height
                    };

                    if( _edge_mode == EdgeMode::eZero )
                    {
                        const auto source_coordinates = vec2<int64_t> {
                            static_cast<int64_t>( std::round( source_pixel_coordinates.x() ) ),
                            static_cast<int64_t>( std::round( source_pixel_coordinates.y() ) )
                        };
                        if( source_coordinates.x < 0 || source_coordinates.x >= source_spatial_metadata->width ||
                            source_coordinates.y < 0 || source_coordinates.y >= source_spatial_metadata->height )
                        {
                            continue;
                        }
                    }

                    if( _interpolation_mode == InterpolationMode::eNearestNeighbor )
                    {
                        const auto source_coordinates = vec2<uint32_t> {
                            static_cast<uint32_t>( std::clamp( static_cast<int64_t>( std::round( source_pixel_coordinates.x() ) ), int64_t { 0 }, static_cast<int64_t>( source_spatial_metadata->width - 1 ) ) ),
                            static_cast<uint32_t>( std::clamp( static_cast<int64_t>( std::round( source_pixel_coordinates.y() ) ), int64_t { 0 }, static_cast<int64_t>( source_spatial_metadata->height - 1 ) ) )
                        };
                        const auto source_element_index = source_spatial_metadata->element_index( source_coordinates );

                        for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
                        {
                            intensities.value( { element_index, channel_index } ) = source_intensities.value( { source_element_index, channel_index } );
                        }
                    }
                    else if( _interpolation_mode == InterpolationMode::eBilinear )
                    {
                        const auto src_x = source_pixel_coordinates.x();
                        const auto src_y = source_pixel_coordinates.y();

                        // Get the integer lattice coordinates (top-left)
                        const auto x_floor = std::floor( src_x );
                        const auto y_floor = std::floor( src_y );

                        // Calculate fractional parts for weights
                        const auto dx = src_x - x_floor;
                        const auto dy = src_y - y_floor;

                        // Calculate bilinear weights
                        const auto w_tl = ( 1.0 - dx ) * ( 1.0 - dy ); // Top-Left
                        const auto w_tr = dx * ( 1.0 - dy );           // Top-Right
                        const auto w_bl = ( 1.0 - dx ) * dy;           // Bottom-Left
                        const auto w_br = dx * dy;                     // Bottom-Right

                        // Raw integer coordinates
                        const auto x0 = static_cast<int64_t>( x_floor );
                        const auto y0 = static_cast<int64_t>( y_floor );
                        const auto x1 = x0 + 1;
                        const auto y1 = y0 + 1;

                        // Maximum valid indices
                        const auto max_x = static_cast<int64_t>( source_spatial_metadata->width - 1 );
                        const auto max_y = static_cast<int64_t>( source_spatial_metadata->height - 1 );

                        // Clamp coordinates to handle out-of-bounds (edge extension)
                        const auto cx0 = static_cast<uint32_t>( std::clamp( x0, int64_t { 0 }, max_x ) );
                        const auto cy0 = static_cast<uint32_t>( std::clamp( y0, int64_t { 0 }, max_y ) );
                        const auto cx1 = static_cast<uint32_t>( std::clamp( x1, int64_t { 0 }, max_x ) );
                        const auto cy1 = static_cast<uint32_t>( std::clamp( y1, int64_t { 0 }, max_y ) );

                        // Get 1D element indices for the 4 surrounding pixels
                        const auto idx_tl = source_spatial_metadata->element_index( { cx0, cy0 } );
                        const auto idx_tr = source_spatial_metadata->element_index( { cx1, cy0 } );
                        const auto idx_bl = source_spatial_metadata->element_index( { cx0, cy1 } );
                        const auto idx_br = source_spatial_metadata->element_index( { cx1, cy1 } );

                        for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
                        {
                            // Fetch intensities for the 4 neighbors
                            const auto val_tl = source_intensities.value( { idx_tl, channel_index } );
                            const auto val_tr = source_intensities.value( { idx_tr, channel_index } );
                            const auto val_bl = source_intensities.value( { idx_bl, channel_index } );
                            const auto val_br = source_intensities.value( { idx_br, channel_index } );

                            // Interpolate
                            const auto interpolated = ( val_tl * w_tl ) + ( val_tr * w_tr ) + ( val_bl * w_bl ) + ( val_br * w_br );

                            // Store back into the destination
                            if constexpr( std::is_floating_point_v<value_type> )
                            {
                                intensities.value( { element_index, channel_index } ) = static_cast<value_type>( interpolated );
                            }
                            else
                            {
                                intensities.value( { element_index, channel_index } ) = static_cast<value_type>( std::round( interpolated ) );
                            }
                        }
                    }
                }

                std::cout << "\r[Dataset Alignment] Progress: 100.00%" << std::endl;

                _aligned = QSharedPointer<TensorDataset<value_type>>::create(
                    std::move( intensities ),
                    source_typed.channel_positions()
                );
                _aligned->update_spatial_metadata( std::make_unique<Dataset::SpatialMetadata>( *target_spatial_metadata ) );
                if( _source->override_channel_identifiers().has_value() )
                {
                    _aligned->update_channel_identifiers( _source->override_channel_identifiers().value() );
                }

                this->accept();
            } );
        } );
        context_menu.exec( event->globalPosition().toPoint() );
    }

    _interaction_mode = InteractionMode::eNone;
    _active_point_index = std::nullopt;
}
void DatasetAlignmentDialog::wheelEvent( QWheelEvent* event )
{
    _source_opacity = std::clamp( _source_opacity + event->angleDelta().y() / 120.0 * 0.05, 0.0, 1.0 );
    this->update();
}

QRectF DatasetAlignmentDialog::source_rectangle() const
{
    const auto source_dimensions = _source->spatial_metadata()->dimensions;
    auto source_rectangle = QRectF { 0, 0, static_cast<qreal>( source_dimensions.x ), static_cast<qreal>( source_dimensions.y ) };
    auto source_scaling = 0.95 * std::min( this->width() / source_rectangle.width(), this->height() / source_rectangle.height() );
    source_rectangle.setWidth( source_rectangle.width() * source_scaling );
    source_rectangle.setHeight( source_rectangle.height() * source_scaling );
    source_rectangle.moveCenter( QPointF { this->width() / 2.0, this->height() / 2.0 } );
    return source_rectangle;
}
QRectF DatasetAlignmentDialog::target_rectangle() const
{
    const auto target_dimensions = _target->spatial_metadata()->dimensions;
    auto target_rectangle = QRectF { 0, 0, static_cast<qreal>( target_dimensions.x ), static_cast<qreal>( target_dimensions.y ) };
    auto target_scaling = 0.95 * std::min( this->width() / target_rectangle.width(), this->height() / target_rectangle.height() );
    target_rectangle.setWidth( target_rectangle.width() * target_scaling );
    target_rectangle.setHeight( target_rectangle.height() * target_scaling );
    target_rectangle.moveCenter( QPointF { this->width() / 2.0, this->height() / 2.0 } );
    return target_rectangle;
}