#pragma once
#include "database.hpp"
#include "boxplot.hpp"
#include "plotting_widget.hpp"

class BoxplotViewer : public PlottingWidget
{
    Q_OBJECT
public:
    BoxplotViewer( Database& database );

    QSharedPointer<Feature> feature() const noexcept;
    void update_feature( QSharedPointer<Feature> feature );

    void paintEvent( QPaintEvent* event ) override;

    void mousePressEvent( QMouseEvent* event ) override;
    void mouseMoveEvent( QMouseEvent* event ) override;
    void leaveEvent( QEvent* event ) override;

private:
    void on_feature_extremes_changed();
    void export_boxplots() const;

    Database& _database;
    QWeakPointer<Feature> _feature;
    GroupedBoxplot _boxplot;

    QPointF _cursor_position;
};