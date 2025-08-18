#pragma once
#include "collection.hpp"
#include "number_input.hpp"
#include "utility.hpp"

#include <qwidget.h>

class Colormap;
class Colormap1D;
class ColormapRGB;
class Database;
class Feature;

class FeatureSelector;
class PlottingWidget;

// ----- Colormap1DPreview ----- //

class Colormap1DPreview : public QWidget
{
    Q_OBJECT
public:
    Colormap1DPreview( Colormap1D& colormap );
    Colormap1D& colormap() const noexcept;

    QSize sizeHint() const override;
    void paintEvent( QPaintEvent* event ) override;

    void mousePressEvent( QMouseEvent* event ) override;

private:
    Colormap1D& _colormap;

    static constexpr auto _colormap_height = 20;
};

// ----- Colormap1DViewer ----- //

class Colormap1DViewer : public QWidget
{
    Q_OBJECT
public:
    Colormap1DViewer( Colormap1D& colormap, Database& database );
    Colormap1D& colormap() const noexcept;

private:
    void colormap_feature_changed( const Feature* feature );

    Colormap1D& _colormap;
    FeatureSelector* _feature_selector = nullptr;
    RangeInput* _colormap_range = nullptr;
    Colormap1DPreview* _colormap_preview = nullptr;
    PlottingWidget* _colormap_axis = nullptr;
};

// ----- ColormapRGBViewer ----- //

class ColormapRGBViewer : public QWidget
{
    Q_OBJECT
public:
    ColormapRGBViewer( ColormapRGB& colormap );

private:
    ColormapRGB& _colormap;
};

// ----- ColormapViewer ----- //

class ColormapViewer : public QWidget
{
    Q_OBJECT
public:
    ColormapViewer( Database& database );

    QSharedPointer<Colormap> colormap() const noexcept;
    void update_colormap( QSharedPointer<Colormap> colormap );

signals:
    void colormap_changed( QSharedPointer<Colormap> colormap );

private:
    void colormap_appended( QSharedPointer<QObject> object );
    void colormap_removed( QSharedPointer<QObject> object );

    Database& _database;
    QSharedPointer<Colormap> _colormap;
    QWidget* _colormap_widget = nullptr;
};