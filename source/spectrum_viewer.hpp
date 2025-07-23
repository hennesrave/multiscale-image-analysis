#pragma once
#include "database.hpp"
#include "plotting_widget.hpp"

class SpectrumViewer : public PlottingWidget
{
    Q_OBJECT
private:
    struct FeatureHandle
    {
        QWeakPointer<DatasetChannelsFeature> feature;
        uint32_t channel_index = 0;
        bool both = false;
    };

public:
    enum class StatisticsMode
    {
        eAverage,
        eMinimum,
        eMaximum
    };
    enum class VisualizationMode
    {
        eLine,
        eLollipop
    };

    SpectrumViewer( Database& database );

    void paintEvent( QPaintEvent* event ) override;

    void mousePressEvent( QMouseEvent* event ) override;
    void mouseReleaseEvent( QMouseEvent* event ) override;

    void mouseMoveEvent( QMouseEvent* event ) override;
    void leaveEvent( QEvent* event ) override;

    void keyPressEvent( QKeyEvent* event ) override;

    uint32_t screen_to_channel_index( double x ) const;

signals:
    void highlighted_channel_index_changed( std::optional<uint32_t> channel_index );
    void hovered_feature_changed( FeatureHandle feature_handle );

private:
    void update_hovered_feature();
    void baseline_correction() const;
    void export_spectra() const;

    Database& _database;

    StatisticsMode _statistics_mode = StatisticsMode::eAverage;
    VisualizationMode _visualization_mode = VisualizationMode::eLine;

    FeatureHandle _hovered_feature;
    FeatureHandle _selected_feature;

    QPointF _cursor_position { -1.0, -1.0 };
};