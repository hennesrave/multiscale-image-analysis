#pragma once
#include "database.hpp"
#include "utility.hpp"

#include <qwidget.h>

class ColormapTemplate;

class ChannelSegmentAbundances : public QObject
{
    Q_OBJECT
public:
    ChannelSegmentAbundances( QSharedPointer<const Dataset> dataset, QSharedPointer<const Segmentation> segmentation );

    QSharedPointer<const Dataset> dataset() const;
    QSharedPointer<const Segmentation> segmentation() const;

    const Array<Array<double>>& abundances() const;

signals:
    void abundances_changed() const;

private:
    Array<Array<double>> compute_abundances();

    QWeakPointer<const Dataset> _dataset;
    QWeakPointer<const Segmentation> _segmentation;

    Computed<Array<Array<double>>> _abundances;
};

class ChannelGlyphsViewer : public QWidget
{
    Q_OBJECT
public:
    struct Viewport
    {
        vec2<uint32_t> offset;
        vec2<uint32_t> extent;
    };

    enum class Normalization
    {
        eGlobal,
        eGlyph
    };

    enum class Positioning
    {
        eLinear,
        eEmbedding,
        eGridified
    };

    enum class Abundances
    {
        eDisabled,
        eEnabled,
        eSegmentsOnly
    };

    struct Glyph
    {
        Matrix<double> values;
        QImage image;

        vec2<uint32_t> offset;
    };

    ChannelGlyphsViewer( Database& database );

    Viewport viewport() const noexcept;
    void update_viewport( Viewport viewport );

    Normalization normalization() const noexcept;
    void update_normalization( Normalization normalization );

    Positioning positioning() const noexcept;
    void update_positioning( Positioning positioning );

    Abundances abundances() const noexcept;
    void update_abundances( Abundances abundances );

signals:
    void viewport_changed( Viewport viewport ) const;
    void normalization_changed( Normalization normalization ) const;
    void positioning_changed( Positioning positioning ) const;
    void abundances_changed( Abundances abundances ) const;

protected:
    void resizeEvent( QResizeEvent* event ) override;

    void paintEvent( QPaintEvent* event ) override;
    void mouseMoveEvent( QMouseEvent* event ) override;
    void mousePressEvent( QMouseEvent* event ) override;

    void leaveEvent( QEvent* event ) override;
    void wheelEvent( QWheelEvent* event ) override;

    void keyPressEvent( QKeyEvent* event ) override;

private:
    void on_create_distance_matrix();
    void on_export_distance_matrix( QString filepath = {} );
    void on_import_distance_matrix();

    void on_create_embedding();
    void on_export_embedding( QString filepath = {} );
    void on_import_embedding();

    void update_glyph_values();
    void update_glyph_images();
    void update_glyph_layout();
    void update_canvas();

    void reset_canvas_rectangle();
    QRectF glyph_rectangle( uint32_t channel_index ) const;

    Database& _database;

    ChannelSegmentAbundances _channel_segment_abundances;

    Viewport _viewport;
    Normalization _normalization            = Normalization::eGlobal;
    const ColormapTemplate* _colormap       = nullptr;
    Override<double> _colormap_lower        = { 0.0, std::nullopt };
    Override<double> _colormap_upper        = { 1.0, std::nullopt };
    Positioning _positioning                = Positioning::eLinear;
    Abundances _abundances                  = Abundances::eDisabled;

    QImage _canvas;
    std::vector<Glyph> _glyphs;

    Matrix<double> _distance_matrix;
    Array<vec2<double>> _embedding;

    QRectF _canvas_rectangle;
};