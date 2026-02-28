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

    struct Viewport
    {
        vec2<uint32_t> offset;
        vec2<uint32_t> extent;
    };

    ChannelGlyphsViewer( Database& database );

    Normalization normalization() const noexcept;
    void update_normalization( Normalization normalization );

    Viewport viewport() const noexcept;
    void update_viewport( Viewport viewport );

signals:
    void normalization_changed( Normalization normalization ) const;
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
    void update_canvas();

    void reset_canvas_rectangle();
    QRectF glyph_rectangle( uint32_t channel_index ) const;

    Database& _database;

    Normalization _normalization = Normalization::eGlobal;
    std::unique_ptr<ColormapTemplate> _colormap_template;

    struct Glyph
    {
        Matrix<double> values;
        QImage image;

        vec2<uint32_t> offset;
    };

    Viewport _viewport;
    std::vector<Glyph> _glyphs;

    QImage _canvas;
    QRectF _canvas_rectangle;

    Array<vec2<double>> _potential_glyph_positions;
    Matrix<double> _distance_matrix;
    Array<vec2<double>> _embedding;
};