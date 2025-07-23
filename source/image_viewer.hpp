#pragma once
#include "database.hpp"
#include "utility.hpp"

#include <qsharedpointer.h>
#include <qwidget.h>

class ImageViewer : public QWidget
{
    Q_OBJECT
public:
    enum class SelectionMode
    {
        eNone,
        eGrowSegment,
        eShrinkSegment,
    };

    ImageViewer( Database& database );

    void update_colormap( QSharedPointer<Colormap> colormap );

    void resizeEvent( QResizeEvent* event ) override;
    void paintEvent( QPaintEvent* event ) override;
    void wheelEvent( QWheelEvent* event ) override;

    void mousePressEvent( QMouseEvent* event ) override;
    void mouseReleaseEvent( QMouseEvent* event ) override;

    void mouseMoveEvent( QMouseEvent* event ) override;
    void leaveEvent( QEvent* event ) override;

    QPointF pixel_to_screen( vec2<uint32_t> pixel ) const;
    vec2<uint32_t> screen_to_pixel( QPointF screen ) const;

private:
    void reset_image_rectangle();
    void create_screenshot( uint32_t scaling ) const;
    void export_columns() const;
    void export_matrix() const;

    Database& _database;
    QSharedPointer<const Dataset> _dataset;
    QSharedPointer<Segmentation> _segmentation;
    QWeakPointer<Colormap> _colormap;

    double _image_opacity = 1.0;
    double _segmentation_opacity = 0.5;

    QRectF _image_rectangle;
    QPointF _cursor_position;
    QPolygonF _selection_polygon;
    SelectionMode _selection_mode = SelectionMode::eNone;
};