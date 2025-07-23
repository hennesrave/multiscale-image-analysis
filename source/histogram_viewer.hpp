#pragma once
#include "database.hpp"
#include "histogram.hpp"
#include "plotting_widget.hpp"

class HistogramViewer : public PlottingWidget
{
	Q_OBJECT
public:
	HistogramViewer( Database& database );

	QSharedPointer<const Feature> feature() const;
	void update_feature( QSharedPointer<const Feature> feature );

	uint32_t bincount() const noexcept;
	void update_bincount( uint32_t bincount );

	void paintEvent( QPaintEvent* event ) override;

	void mousePressEvent( QMouseEvent* event ) override;
	void mouseMoveEvent( QMouseEvent* event ) override;
	void leaveEvent( QEvent* event ) override;

private:
	void export_histograms() const;

	Database& _database;
	Histogram _histogram { 20 };
	StackedHistogram _segmentation_histogram { 20 };

	QPointF _cursor_position;
};