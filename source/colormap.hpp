#pragma once
#include "utility.hpp"

#include <qobject.h>
#include <qscopedpointer.h>
#include <qsharedpointer.h>

class Feature;

// ----- ColormapTemplate ---- //

class ColormapTemplate : public QObject
{
    Q_OBJECT
public:
    static const ColormapTemplate gray;
    static const ColormapTemplate red;
    static const ColormapTemplate green;
    static const ColormapTemplate blue;
    static const ColormapTemplate viridis;
    static const ColormapTemplate inferno;
    static const ColormapTemplate plasma;
    static const ColormapTemplate rainbow;
    static const ColormapTemplate coolwarm;
    static const ColormapTemplate seismic;
    static const ColormapTemplate tab10;
    static const std::vector<std::pair<const char*, const ColormapTemplate&>> registry;

    struct Node
    {
        double position;
        vec4<float> color;
    };

    ColormapTemplate( std::vector<Node> nodes ) noexcept;
    std::unique_ptr<ColormapTemplate> clone() const;

    vec4<float> color( double value ) const;
    void invert();

private:
    std::vector<Node> _nodes;

signals:
    void colors_changed();
};

// ----- Colormap ---- //

class Colormap : public QObject
{
    Q_OBJECT
public:
    Colormap();

    virtual uint32_t element_count() const = 0;
    const Promise<Array<vec4<float>>>& colors() const noexcept;

signals:
    void colors_changed();

protected:
    virtual void compute_colors( Array<vec4<float>>& colors ) const = 0;
    Promise<Array<vec4<float>>> _colors;
};

// ----- Colormap1D ---- //

class Colormap1D : public Colormap
{
    Q_OBJECT
public:
    Colormap1D( std::unique_ptr<ColormapTemplate> colormap_template );

    const std::unique_ptr<ColormapTemplate>& colormap_template() const noexcept;
    void update_colormap_template( std::unique_ptr<ColormapTemplate> colormap_template );

    QSharedPointer<Feature> feature() const noexcept;
    void update_feature( QSharedPointer<Feature> feature );

    const Override<double>& lower() const noexcept;
    Override<double>& lower() noexcept;

    const Override<double>& upper() const noexcept;
    Override<double>& upper() noexcept;

    uint32_t element_count() const override;

signals:
    void template_changed( const std::unique_ptr<ColormapTemplate>& colormap_template );
    void feature_changed( QSharedPointer<Feature> feature );

private:
    void compute_colors( Array<vec4<float>>& colors ) const override;

    std::unique_ptr<ColormapTemplate> _colormap_template;
    QWeakPointer<Feature> _feature;
    Override<double> _lower { 0.0, std::nullopt };
    Override<double> _upper { 1.0, std::nullopt };
};

// ----- ColormapRGB ---- //

class ColormapRGB : public Colormap
{
    Q_OBJECT
public:
    ColormapRGB();

    const Colormap1D& colormap_r() const noexcept;
    const Colormap1D& colormap_g() const noexcept;
    const Colormap1D& colormap_b() const noexcept;

    uint32_t element_count() const override;

private:
    void compute_colors( Array<vec4<float>>& colors ) const override;

    Colormap1D _colormap_r { ColormapTemplate::red.clone() };
    Colormap1D _colormap_g { ColormapTemplate::green.clone() };
    Colormap1D _colormap_b { ColormapTemplate::blue.clone() };
};