#pragma once
#include "database.hpp"

#include <qwidget.h>

class ColormapTemplate;

class ChannelGlyphsViewer : public QWidget
{
    Q_OBJECT
public:
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

    struct Viewport
    {
        vec2<uint32_t> offset;
        vec2<uint32_t> extent;
    };

    struct Glyph
    {
        Matrix<double> values;
        QImage image;

        vec2<uint32_t> offset;
    };

    ChannelGlyphsViewer( Database& database );

    Normalization normalization() const noexcept;
    void update_normalization( Normalization normalization );

    Positioning positioning() const noexcept;
    void update_positioning( Positioning positioning );

    Viewport viewport() const noexcept;
    void update_viewport( Viewport viewport );

signals:
    void normalization_changed( Normalization normalization ) const;
    void positioning_changed( Positioning positioning ) const;
    void viewport_changed( Viewport viewport ) const;

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

    Viewport _viewport;
    Normalization _normalization        = Normalization::eGlobal;
    const ColormapTemplate* _colormap   = nullptr;
    Positioning _positioning            = Positioning::eLinear;

    QImage _canvas;
    std::vector<Glyph> _glyphs;

    Matrix<double> _distance_matrix;
    Array<vec2<double>> _embedding;

    QRectF _canvas_rectangle;
};