#include "colormap.hpp"

#include "feature.hpp"

// ----- ColormapTemplate ----- //

ColormapTemplate::ColormapTemplate( std::vector<Node> nodes ) noexcept : _nodes { std::move( nodes ) }
{
}

std::unique_ptr<ColormapTemplate> ColormapTemplate::clone() const
{
    return std::unique_ptr<ColormapTemplate>( new ColormapTemplate( _nodes ) );
}

vec4<float> ColormapTemplate::color( double value ) const
{
    if( _nodes.empty() )
    {
        return vec4<float> { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    if( value <= _nodes.front().position )
    {
        return _nodes.front().color;
    }
    if( value >= _nodes.back().position )
    {
        return _nodes.back().color;
    }
    for( size_t i = 1; i < _nodes.size(); ++i )
    {
        if( value < _nodes[i].position )
        {
            const auto& node_a = _nodes[i - 1];
            const auto& node_b = _nodes[i];
            const auto t = ( value - node_a.position ) / ( node_b.position - node_a.position );
            return node_a.color + t * ( node_b.color - node_a.color );
        }
    }
    return vec4<float> { 0.0f, 0.0f, 0.0f, 0.0f };
}
void ColormapTemplate::invert()
{
    std::reverse( _nodes.begin(), _nodes.end() );
    for( auto& node : _nodes )
    {
        node.position = 1.0 - node.position;
    }
    emit colors_changed();
}

const ColormapTemplate ColormapTemplate::gray { std::vector<ColormapTemplate::Node> {
    { 0.0, vec4<float> { 0.0f, 0.0f, 0.0f, 1.0f } },
    { 1.0, vec4<float> { 1.0f, 1.0f, 1.0f, 1.0f } }
} };
const ColormapTemplate ColormapTemplate::red { std::vector<ColormapTemplate::Node> {
    { 0.0, vec4<float> { 0.0f, 0.0f, 0.0f, 1.0f } },
    { 1.0, vec4<float> { 1.0f, 0.0f, 0.0f, 1.0f } }
} };
const ColormapTemplate ColormapTemplate::green { std::vector<ColormapTemplate::Node> {
    { 0.0, vec4<float> { 0.0f, 0.0f, 0.0f, 1.0f } },
    { 1.0, vec4<float> { 0.0f, 1.0f, 0.0f, 1.0f } }
} };
const ColormapTemplate ColormapTemplate::blue { std::vector<ColormapTemplate::Node> {
    { 0.0, vec4<float> { 0.0f, 0.0f, 0.0f, 1.0f } },
    { 1.0, vec4<float> { 0.0f, 0.0f, 1.0f, 1.0f } }
} };
const ColormapTemplate ColormapTemplate::viridis { std::vector<ColormapTemplate::Node> {
    { 0.00f, vec4<float> { 0.267004f, 0.004874f, 0.329415f, 1.0f } },
    { 0.14f, vec4<float> { 0.275191f, 0.194905f, 0.496005f, 1.0f } },
    { 0.29f, vec4<float> { 0.212395f, 0.359683f, 0.551710f, 1.0f } },
    { 0.43f, vec4<float> { 0.153364f, 0.497000f, 0.557724f, 1.0f } },
    { 0.57f, vec4<float> { 0.122312f, 0.633153f, 0.530398f, 1.0f } },
    { 0.71f, vec4<float> { 0.288921f, 0.758394f, 0.428426f, 1.0f } },
    { 0.86f, vec4<float> { 0.626579f, 0.854645f, 0.223353f, 1.0f } },
    { 1.00f, vec4<float> { 0.993248f, 0.906157f, 0.143936f, 1.0f } }
} };
const ColormapTemplate ColormapTemplate::inferno { std::vector<ColormapTemplate::Node> {
    { 0.00f, vec4<float> { 0.001462f, 0.000466f, 0.013866f, 1.0f } },
    { 0.14f, vec4<float> { 0.185228f, 0.011644f, 0.105980f, 1.0f } },
    { 0.29f, vec4<float> { 0.370983f, 0.099166f, 0.175706f, 1.0f } },
    { 0.43f, vec4<float> { 0.566949f, 0.206756f, 0.207364f, 1.0f } },
    { 0.57f, vec4<float> { 0.748651f, 0.318106f, 0.222177f, 1.0f } },
    { 0.71f, vec4<float> { 0.894114f, 0.444937f, 0.228141f, 1.0f } },
    { 0.86f, vec4<float> { 0.984264f, 0.624027f, 0.248885f, 1.0f } },
    { 1.00f, vec4<float> { 0.988362f, 0.998364f, 0.644924f, 1.0f } }
} };

const ColormapTemplate ColormapTemplate::plasma { std::vector<ColormapTemplate::Node> {
    { 0.00f, vec4<float> { 0.050383f, 0.029803f, 0.527975f, 1.0f } },
    { 0.14f, vec4<float> { 0.261206f, 0.018345f, 0.600792f, 1.0f } },
    { 0.29f, vec4<float> { 0.477504f, 0.015566f, 0.550321f, 1.0f } },
    { 0.43f, vec4<float> { 0.676281f, 0.075353f, 0.446213f, 1.0f } },
    { 0.57f, vec4<float> { 0.844566f, 0.191209f, 0.300591f, 1.0f } },
    { 0.71f, vec4<float> { 0.961340f, 0.396249f, 0.181863f, 1.0f } },
    { 0.86f, vec4<float> { 0.998354f, 0.624904f, 0.151478f, 1.0f } },
    { 1.00f, vec4<float> { 0.940015f, 0.975158f, 0.131326f, 1.0f } }
} };
const ColormapTemplate ColormapTemplate::rainbow { std::vector<ColormapTemplate::Node> {
    { 0.0f, vec4<float> { 0.5f, 0.0f, 1.0f, 1.0f } },
    { 0.17f, vec4<float> { 0.0f, 0.0f, 1.0f, 1.0f } },
    { 0.33f, vec4<float> { 0.0f, 1.0f, 1.0f, 1.0f } },
    { 0.50f, vec4<float> { 0.0f, 1.0f, 0.0f, 1.0f } },
    { 0.67f, vec4<float> { 1.0f, 1.0f, 0.0f, 1.0f } },
    { 0.83f, vec4<float> { 1.0f, 0.0f, 0.0f, 1.0f } },
    { 1.0f, vec4<float> { 0.5f, 0.0f, 0.0f, 1.0f } }
} };
const ColormapTemplate ColormapTemplate::coolwarm { std::vector<ColormapTemplate::Node> {
    { 0.0, vec4<float> { 0.2298057f, 0.298717966f, 0.753683153f, 1.0f } },
    { 0.03125, vec4<float> { 0.26623388f, 0.353094838f, 0.801466763f, 1.0f } },
    { 0.0625, vec4<float> { 0.30386891f, 0.406535296f, 0.84495867f, 1.0f } },
    { 0.09375, vec4<float> { 0.342804478f, 0.458757618f, 0.883725899f, 1.0f } },
    { 0.125, vec4<float> { 0.38301334f, 0.50941904f, 0.917387822f, 1.0f } },
    { 0.15625, vec4<float> { 0.424369608f, 0.558148092f, 0.945619588f, 1.0f } },
    { 0.1875, vec4<float> { 0.46666708f, 0.604562568f, 0.968154911f, 1.0f } },
    { 0.21875, vec4<float> { 0.509635204f, 0.648280772f, 0.98478814f, 1.0f } },
    { 0.25, vec4<float> { 0.552953156f, 0.688929332f, 0.995375608f, 1.0f } },
    { 0.28125, vec4<float> { 0.596262162f, 0.726149107f, 0.999836203f, 1.0f } },
    { 0.3125, vec4<float> { 0.639176211f, 0.759599947f, 0.998151185f, 1.0f } },
    { 0.34375, vec4<float> { 0.681291281f, 0.788964712f, 0.990363227f, 1.0f } },
    { 0.375, vec4<float> { 0.722193294f, 0.813952739f, 0.976574709f, 1.0f } },
    { 0.40625, vec4<float> { 0.761464949f, 0.834302879f, 0.956945269f, 1.0f } },
    { 0.4375, vec4<float> { 0.798691636f, 0.849786142f, 0.931688648f, 1.0f } },
    { 0.46875, vec4<float> { 0.833466556f, 0.860207984f, 0.901068838f, 1.0f } },
    { 0.5, vec4<float> { 0.865395197f, 0.86541021f, 0.865395561f, 1.0f } },
    { 0.53125, vec4<float> { 0.897787179f, 0.848937047f, 0.820880546f, 1.0f } },
    { 0.5625, vec4<float> { 0.924127593f, 0.827384882f, 0.774508472f, 1.0f } },
    { 0.59375, vec4<float> { 0.944468518f, 0.800927443f, 0.726736146f, 1.0f } },
    { 0.625, vec4<float> { 0.958852946f, 0.769767752f, 0.678007945f, 1.0f } },
    { 0.65625, vec4<float> { 0.96732803f, 0.734132809f, 0.628751763f, 1.0f } },
    { 0.6875, vec4<float> { 0.969954137f, 0.694266682f, 0.579375448f, 1.0f } },
    { 0.71875, vec4<float> { 0.966811177f, 0.650421156f, 0.530263762f, 1.0f } },
    { 0.75, vec4<float> { 0.958003065f, 0.602842431f, 0.481775914f, 1.0f } },
    { 0.78125, vec4<float> { 0.943660866f, 0.551750968f, 0.434243684f, 1.0f } },
    { 0.8125, vec4<float> { 0.923944917f, 0.49730856f, 0.387970225f, 1.0f } },
    { 0.84375, vec4<float> { 0.89904617f, 0.439559467f, 0.343229596f, 1.0f } },
    { 0.875, vec4<float> { 0.869186849f, 0.378313092f, 0.300267182f, 1.0f } },
    { 0.90625, vec4<float> { 0.834620542f, 0.312874446f, 0.259301199f, 1.0f } },
    { 0.9375, vec4<float> { 0.795631745f, 0.24128379f, 0.220525627f, 1.0f } },
    { 0.96875, vec4<float> { 0.752534934f, 0.157246067f, 0.184115123f, 1.0f } },
    { 1.0, vec4<float> { 0.705673158f, 0.01555616f, 0.150232812f, 1.0f } }
} };
const ColormapTemplate ColormapTemplate::seismic { std::vector<ColormapTemplate::Node> {
    { 0.0f, vec4<float> { 0.0f, 0.0f, 0.3f, 1.0f } },
    { 0.25f, vec4<float> { 0.0f, 0.0f, 1.0f, 1.0f } },
    { 0.5f, vec4<float> { 1.0f, 1.0f, 1.0f, 1.0f } },
    { 0.75f, vec4<float> { 1.0f, 0.0f, 0.0f, 1.0f } },
    { 1.0f, vec4<float> { 0.3f, 0.0f, 0.0f, 1.0f } }
} };
const ColormapTemplate ColormapTemplate::tab10 { std::vector<ColormapTemplate::Node> {
    { 0.0f, vec4<float> { 0.121f, 0.466f, 0.705f, 1.0f } },     // blue
    { 0.1f, vec4<float> { 0.121f, 0.466f, 0.705f, 1.0f } },

    { 0.1f, vec4<float> { 1.0f, 0.498f, 0.054f, 1.0f } },       // orange
    { 0.2f, vec4<float> { 1.0f, 0.498f, 0.054f, 1.0f } },

    { 0.2f, vec4<float> { 0.172f, 0.627f, 0.172f, 1.0f } },     // green
    { 0.3f, vec4<float> { 0.172f, 0.627f, 0.172f, 1.0f } },

    { 0.3f, vec4<float> { 0.839f, 0.153f, 0.157f, 1.0f } },     // red
    { 0.4f, vec4<float> { 0.839f, 0.153f, 0.157f, 1.0f } },

    { 0.4f, vec4<float> { 0.580f, 0.404f, 0.741f, 1.0f } },     // purple
    { 0.5f, vec4<float> { 0.580f, 0.404f, 0.741f, 1.0f } },

    { 0.5f, vec4<float> { 0.549f, 0.337f, 0.294f, 1.0f } },     // brown
    { 0.6f, vec4<float> { 0.549f, 0.337f, 0.294f, 1.0f } },

    { 0.6f, vec4<float> { 0.890f, 0.466f, 0.760f, 1.0f } },     // pink
    { 0.7f, vec4<float> { 0.890f, 0.466f, 0.760f, 1.0f } },

    { 0.7f, vec4<float> { 0.498f, 0.498f, 0.498f, 1.0f } },     // gray
    { 0.8f, vec4<float> { 0.498f, 0.498f, 0.498f, 1.0f } },

    { 0.8f, vec4<float> { 0.737f, 0.741f, 0.133f, 1.0f } },     // olive
    { 0.9f, vec4<float> { 0.737f, 0.741f, 0.133f, 1.0f } },

    { 0.9f, vec4<float> { 0.090f, 0.745f, 0.811f, 1.0f } },     // cyan
    { 1.0f, vec4<float> { 0.090f, 0.745f, 0.811f, 1.0f } }
} };

const std::vector<std::pair<const char*, const ColormapTemplate&>> ColormapTemplate::registry {
    { "Gray \u2605", ColormapTemplate::gray },
    { "Red", ColormapTemplate::red },
    { "Green", ColormapTemplate::green },
    { "Blue", ColormapTemplate::blue },
    { "Viridis \u2605", ColormapTemplate::viridis },
    { "Inferno", ColormapTemplate::inferno },
    { "Plasma", ColormapTemplate::plasma },
    { "Rainbow", ColormapTemplate::rainbow },
    { "Coolwarm \u2605", ColormapTemplate::coolwarm },
    { "Seismic", ColormapTemplate::seismic },
    { "Tab10 \u2605", ColormapTemplate::tab10 }
};

// ----- Colormap ----- //

Colormap::Colormap()
{
    _colors.initialize( std::bind( &Colormap::compute_colors, this ) );
    QObject::connect( &_colors, &ComputedObject::changed, this, &Colormap::colors_changed );
}

const Array<vec4<float>>& Colormap::colors() const
{
    return *_colors;
}

// ----- Colormap1D ----- //

Colormap1D::Colormap1D( std::unique_ptr<ColormapTemplate> colormap_template ) : _colormap_template { std::move( colormap_template ) }
{
    if( !_colormap_template )
    {
        Console::critical( "Invalid colormap template provided." );
    }

    QObject::connect( _colormap_template.get(), &ColormapTemplate::colors_changed, &_colors, &ComputedObject::invalidate );
    QObject::connect( &_lower, &Override<double>::value_changed, this, [this]
    {
        const auto lower = _lower.value();
        if( _upper.value() < lower )
        {
            if( _upper.automatic_value() > lower ) _upper.update_override_value( std::nullopt );
            else _upper.update_override_value( lower + 1.0 );
        }
        _colors.invalidate();
    } );
    QObject::connect( &_upper, &Override<double>::value_changed, this, [this]
    {
        const auto upper = _upper.value();
        if( _lower.value() > upper )
        {
            if( _lower.automatic_value() < upper ) _lower.update_override_value( std::nullopt );
            else _lower.update_override_value( upper - 1.0 );
        }
        _colors.invalidate();
    } );
    QObject::connect( this, &Colormap1D::template_changed, &_colors, &ComputedObject::invalidate );
    QObject::connect( this, &Colormap1D::feature_changed, &_colors, &ComputedObject::invalidate );
}

uint32_t Colormap1D::element_count() const
{
    if( const auto feature = _feature.lock() )
    {
        return feature->element_count();
    }
    return 0;
}

const std::unique_ptr<ColormapTemplate>& Colormap1D::colormap_template() const noexcept
{
    return _colormap_template;
}
void Colormap1D::update_colormap_template( std::unique_ptr<ColormapTemplate> colormap_template )
{
    if( colormap_template )
    {
        if( _colormap_template )
        {
            QObject::disconnect( _colormap_template.get(), nullptr, this, nullptr );
        }

        if( _colormap_template = std::move( colormap_template ) )
        {
            QObject::connect( _colormap_template.get(), &ColormapTemplate::colors_changed, this, &Colormap1D::colors_changed );
        }
        emit template_changed( _colormap_template );
    }
    else
    {
        Console::critical( "Invalid colormap template provided." );
    }
}

QSharedPointer<Feature> Colormap1D::feature() const noexcept
{
    return _feature.lock();
}
void Colormap1D::update_feature( QSharedPointer<Feature> feature )
{
    if( _feature != feature )
    {
        if( auto feature = _feature.lock() )
        {
            QObject::disconnect( feature.get(), nullptr, this, nullptr );
        }

        if( _feature = feature )
        {
            QObject::connect( feature.get(), &Feature::values_changed, &_colors, &ComputedObject::invalidate );
            QObject::connect( feature.get(), &Feature::extremes_changed, this, &Colormap1D::on_feature_extremes_changed );
            QObject::connect( feature.get(), &QObject::destroyed, this, [this] { emit feature_changed( nullptr ); } );
            this->on_feature_extremes_changed();
        }

        emit feature_changed( _feature );
    }
}

const Override<double>& Colormap1D::lower() const noexcept
{
    return _lower;
}
Override<double>& Colormap1D::lower() noexcept
{
    return _lower;
}

const Override<double>& Colormap1D::upper() const noexcept
{
    return _upper;
}
Override<double>& Colormap1D::upper() noexcept
{
    return _upper;
}

void Colormap1D::on_feature_extremes_changed()
{
    if( auto feature = _feature.lock() )
    {
        const auto lower_override = _lower.override_value();
        const auto upper_override = _upper.override_value();

        const auto& extremes = feature->extremes();
        _lower.update_automatic_value( extremes.minimum );
        _upper.update_automatic_value( extremes.maximum );

        _lower.update_override_value( lower_override );
        _upper.update_override_value( upper_override );
    }
    else
    {
        _lower.update_automatic_value( 0.0 );
        _upper.update_automatic_value( 1.0 );
    }
}

Array<vec4<float>> Colormap1D::compute_colors() const
{
    auto colors = Array<vec4<float>> { this->element_count(), vec4<float>{ 0.0f, 0.0f, 0.0f, 1.0f } };

    if( const auto feature = _feature.lock() )
    {
        const auto& feature_values = feature->values();

        const auto lower = _lower.value();
        const auto upper = _upper.value();

        if( lower == upper )
        {
            utility::iterate_parallel( this->element_count(), [this, &colors] ( uint32_t element_index )
            {
                colors[element_index] = _colormap_template->color( 0.5 );
            } );
        }
        else
        {
            const auto range = upper - lower;
            utility::iterate_parallel( this->element_count(), [this, &colors, &feature_values, lower, range] ( uint32_t element_index )
            {
                const auto value = feature_values[element_index];
                const auto normalized = ( value - lower ) / range;
                colors[element_index] = _colormap_template->color( normalized );
            } );
        }
    }

    return colors;
}

// ----- ColormapRGB ----- //

ColormapRGB::ColormapRGB()
{
    QObject::connect( &_colormap_r, &Colormap::colors_changed, &_colors, &ComputedObject::invalidate );
    QObject::connect( &_colormap_g, &Colormap::colors_changed, &_colors, &ComputedObject::invalidate );
    QObject::connect( &_colormap_b, &Colormap::colors_changed, &_colors, &ComputedObject::invalidate );
}

uint32_t ColormapRGB::element_count() const
{
    if( _colormap_r.element_count() == _colormap_g.element_count() && _colormap_r.element_count() == _colormap_b.element_count() )
    {
        return _colormap_r.element_count();
    }
    return 0;
}

const Colormap1D& ColormapRGB::colormap_r() const noexcept
{
    return _colormap_r;
}
const Colormap1D& ColormapRGB::colormap_g() const noexcept
{
    return _colormap_g;
}
const Colormap1D& ColormapRGB::colormap_b() const noexcept
{
    return _colormap_b;
}

Array<vec4<float>> ColormapRGB::compute_colors() const
{
    const auto& colors_r = _colormap_r.colors();
    const auto& colors_g = _colormap_g.colors();
    const auto& colors_b = _colormap_b.colors();

    auto colors = Array<vec4<float>>::allocate( this->element_count() );
    utility::iterate_parallel( this->element_count(), [&] ( uint32_t element_index )
    {
        const auto r = colors_r.value( element_index ).r;
        const auto g = colors_g.value( element_index ).g;
        const auto b = colors_b.value( element_index ).b;
        colors[element_index] = vec4<float> { r, g, b, 1.0f };
    } );
    return colors;
}