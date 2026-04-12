#pragma once
#include <qdialog.h>
#include <qsharedpointer.h>

class Colormap1D;
class Dataset;
class DatasetChannelsFeature;

class DatasetAlignmentDialog : public QDialog
{
    Q_OBJECT
public:
    enum class InteractionMode
    {
        eNone,
        eMovePoint
    };

    enum class InterpolationMode
    {
        eNearestNeighbor,
        eBilinear
    };

    enum class EdgeMode
    {
        eZero,
        eClamp
    };

    DatasetAlignmentDialog( QSharedPointer<Dataset> source, QSharedPointer<Dataset> target );

    QSharedPointer<Dataset> source() const noexcept;
    QSharedPointer<Dataset> target() const noexcept;
    QSharedPointer<Dataset> aligned() const noexcept;

    void paintEvent( QPaintEvent* event ) override;

    void mousePressEvent( QMouseEvent* event ) override;
    void mouseMoveEvent( QMouseEvent* event ) override;
    void mouseReleaseEvent( QMouseEvent* event ) override;

    void wheelEvent( QWheelEvent* event ) override;

private:
    QRectF source_rectangle() const;
    QRectF target_rectangle() const;

    QSharedPointer<Dataset> _source;
    QSharedPointer<Dataset> _target;
    QSharedPointer<Dataset> _aligned;

    QSharedPointer<DatasetChannelsFeature> _source_feature;
    QSharedPointer<DatasetChannelsFeature> _target_feature;

    QSharedPointer<Colormap1D> _source_colormap;
    QSharedPointer<Colormap1D> _target_colormap;

    InteractionMode _interaction_mode           = InteractionMode::eNone;
    std::optional<size_t> _active_point_index   = std::nullopt;

    InterpolationMode _interpolation_mode   = InterpolationMode::eBilinear;
    EdgeMode _edge_mode                     = EdgeMode::eZero;

    qreal _source_opacity = 0.5;

    std::array<QPointF, 3> _source_points {
        QPointF { 0.0, 0.0 }, QPointF { 1.0, 0.0 }, QPointF { 0.0, 1.0 }
    };
    std::array<QPointF, 3> _target_points {
        QPointF { 0.0, 0.0 }, QPointF { 1.0, 0.0 }, QPointF { 0.0, 1.0 }
    };

    QTransform _transform;
};