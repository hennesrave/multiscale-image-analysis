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
const ColormapTemplate ColormapTemplate::turbo { std::vector<ColormapTemplate::Node> {
    { 0.0f, vec4<float> { 0.18995f, 0.07176f, 0.23217f, 1.0f } },
    { 0.00392156862745098f, vec4<float> { 0.19483f, 0.08339f, 0.26149f, 1.0f } },
    { 0.00784313725490196f, vec4<float> { 0.19956f, 0.09498f, 0.29024f, 1.0f } },
    { 0.011764705882352941f, vec4<float> { 0.20415f, 0.10652f, 0.31844f, 1.0f } },
    { 0.01568627450980392f, vec4<float> { 0.2086f, 0.11802f, 0.34607f, 1.0f } },
    { 0.0196078431372549f, vec4<float> { 0.21291f, 0.12947f, 0.37314f, 1.0f } },
    { 0.023529411764705882f, vec4<float> { 0.21708f, 0.14087f, 0.39964f, 1.0f } },
    { 0.027450980392156862f, vec4<float> { 0.22111f, 0.15223f, 0.42558f, 1.0f } },
    { 0.03137254901960784f, vec4<float> { 0.225f, 0.16354f, 0.45096f, 1.0f } },
    { 0.03529411764705882f, vec4<float> { 0.22875f, 0.17481f, 0.47578f, 1.0f } },
    { 0.0392156862745098f, vec4<float> { 0.23236f, 0.18603f, 0.50004f, 1.0f } },
    { 0.043137254901960784f, vec4<float> { 0.23582f, 0.1972f, 0.52373f, 1.0f } },
    { 0.047058823529411764f, vec4<float> { 0.23915f, 0.20833f, 0.54686f, 1.0f } },
    { 0.050980392156862744f, vec4<float> { 0.24234f, 0.21941f, 0.56942f, 1.0f } },
    { 0.054901960784313725f, vec4<float> { 0.24539f, 0.23044f, 0.59142f, 1.0f } },
    { 0.058823529411764705f, vec4<float> { 0.2483f, 0.24143f, 0.61286f, 1.0f } },
    { 0.06274509803921569f, vec4<float> { 0.25107f, 0.25237f, 0.63374f, 1.0f } },
    { 0.06666666666666667f, vec4<float> { 0.25369f, 0.26327f, 0.65406f, 1.0f } },
    { 0.07058823529411765f, vec4<float> { 0.25618f, 0.27412f, 0.67381f, 1.0f } },
    { 0.07450980392156863f, vec4<float> { 0.25853f, 0.28492f, 0.693f, 1.0f } },
    { 0.0784313725490196f, vec4<float> { 0.26074f, 0.29568f, 0.71162f, 1.0f } },
    { 0.08235294117647059f, vec4<float> { 0.2628f, 0.30639f, 0.72968f, 1.0f } },
    { 0.08627450980392157f, vec4<float> { 0.26473f, 0.31706f, 0.74718f, 1.0f } },
    { 0.09019607843137255f, vec4<float> { 0.26652f, 0.32768f, 0.76412f, 1.0f } },
    { 0.09411764705882353f, vec4<float> { 0.26816f, 0.33825f, 0.7805f, 1.0f } },
    { 0.09803921568627451f, vec4<float> { 0.26967f, 0.34878f, 0.79631f, 1.0f } },
    { 0.10196078431372549f, vec4<float> { 0.27103f, 0.35926f, 0.81156f, 1.0f } },
    { 0.10588235294117647f, vec4<float> { 0.27226f, 0.3697f, 0.82624f, 1.0f } },
    { 0.10980392156862745f, vec4<float> { 0.27334f, 0.38008f, 0.84037f, 1.0f } },
    { 0.11372549019607843f, vec4<float> { 0.27429f, 0.39043f, 0.85393f, 1.0f } },
    { 0.11764705882352941f, vec4<float> { 0.27509f, 0.40072f, 0.86692f, 1.0f } },
    { 0.12156862745098039f, vec4<float> { 0.27576f, 0.41097f, 0.87936f, 1.0f } },
    { 0.12549019607843137f, vec4<float> { 0.27628f, 0.42118f, 0.89123f, 1.0f } },
    { 0.12941176470588234f, vec4<float> { 0.27667f, 0.43134f, 0.90254f, 1.0f } },
    { 0.13333333333333333f, vec4<float> { 0.27691f, 0.44145f, 0.91328f, 1.0f } },
    { 0.13725490196078433f, vec4<float> { 0.27701f, 0.45152f, 0.92347f, 1.0f } },
    { 0.1411764705882353f, vec4<float> { 0.27698f, 0.46153f, 0.93309f, 1.0f } },
    { 0.14509803921568626f, vec4<float> { 0.2768f, 0.47151f, 0.94214f, 1.0f } },
    { 0.14901960784313725f, vec4<float> { 0.27648f, 0.48144f, 0.95064f, 1.0f } },
    { 0.15294117647058825f, vec4<float> { 0.27603f, 0.49132f, 0.95857f, 1.0f } },
    { 0.1568627450980392f, vec4<float> { 0.27543f, 0.50115f, 0.96594f, 1.0f } },
    { 0.16078431372549018f, vec4<float> { 0.27469f, 0.51094f, 0.97275f, 1.0f } },
    { 0.16470588235294117f, vec4<float> { 0.27381f, 0.52069f, 0.97899f, 1.0f } },
    { 0.16862745098039217f, vec4<float> { 0.27273f, 0.5304f, 0.98461f, 1.0f } },
    { 0.17254901960784313f, vec4<float> { 0.27106f, 0.54015f, 0.9893f, 1.0f } },
    { 0.1764705882352941f, vec4<float> { 0.26878f, 0.54995f, 0.99303f, 1.0f } },
    { 0.1803921568627451f, vec4<float> { 0.26592f, 0.55979f, 0.99583f, 1.0f } },
    { 0.1843137254901961f, vec4<float> { 0.26252f, 0.56967f, 0.99773f, 1.0f } },
    { 0.18823529411764706f, vec4<float> { 0.25862f, 0.57958f, 0.99876f, 1.0f } },
    { 0.19215686274509802f, vec4<float> { 0.25425f, 0.5895f, 0.99896f, 1.0f } },
    { 0.19607843137254902f, vec4<float> { 0.24946f, 0.59943f, 0.99835f, 1.0f } },
    { 0.2f, vec4<float> { 0.24427f, 0.60937f, 0.99697f, 1.0f } },
    { 0.20392156862745098f, vec4<float> { 0.23874f, 0.61931f, 0.99485f, 1.0f } },
    { 0.20784313725490194f, vec4<float> { 0.23288f, 0.62923f, 0.99202f, 1.0f } },
    { 0.21176470588235294f, vec4<float> { 0.22676f, 0.63913f, 0.98851f, 1.0f } },
    { 0.21568627450980393f, vec4<float> { 0.22039f, 0.64901f, 0.98436f, 1.0f } },
    { 0.2196078431372549f, vec4<float> { 0.21382f, 0.65886f, 0.97959f, 1.0f } },
    { 0.22352941176470587f, vec4<float> { 0.20708f, 0.66866f, 0.97423f, 1.0f } },
    { 0.22745098039215686f, vec4<float> { 0.20021f, 0.67842f, 0.96833f, 1.0f } },
    { 0.23137254901960785f, vec4<float> { 0.19326f, 0.68812f, 0.9619f, 1.0f } },
    { 0.23529411764705882f, vec4<float> { 0.18625f, 0.69775f, 0.95498f, 1.0f } },
    { 0.2392156862745098f, vec4<float> { 0.17923f, 0.70732f, 0.94761f, 1.0f } },
    { 0.24313725490196078f, vec4<float> { 0.17223f, 0.7168f, 0.93981f, 1.0f } },
    { 0.24705882352941178f, vec4<float> { 0.16529f, 0.7262f, 0.93161f, 1.0f } },
    { 0.25098039215686274f, vec4<float> { 0.15844f, 0.73551f, 0.92305f, 1.0f } },
    { 0.2549019607843137f, vec4<float> { 0.15173f, 0.74472f, 0.91416f, 1.0f } },
    { 0.2588235294117647f, vec4<float> { 0.14519f, 0.75381f, 0.90496f, 1.0f } },
    { 0.2627450980392157f, vec4<float> { 0.13886f, 0.76279f, 0.8955f, 1.0f } },
    { 0.26666666666666666f, vec4<float> { 0.13278f, 0.77165f, 0.8858f, 1.0f } },
    { 0.27058823529411763f, vec4<float> { 0.12698f, 0.78037f, 0.8759f, 1.0f } },
    { 0.27450980392156865f, vec4<float> { 0.12151f, 0.78896f, 0.86581f, 1.0f } },
    { 0.2784313725490196f, vec4<float> { 0.11639f, 0.7974f, 0.85559f, 1.0f } },
    { 0.2823529411764706f, vec4<float> { 0.11167f, 0.80569f, 0.84525f, 1.0f } },
    { 0.28627450980392155f, vec4<float> { 0.10738f, 0.81381f, 0.83484f, 1.0f } },
    { 0.2901960784313725f, vec4<float> { 0.10357f, 0.82177f, 0.82437f, 1.0f } },
    { 0.29411764705882354f, vec4<float> { 0.10026f, 0.82955f, 0.81389f, 1.0f } },
    { 0.2980392156862745f, vec4<float> { 0.0975f, 0.83714f, 0.80342f, 1.0f } },
    { 0.30196078431372547f, vec4<float> { 0.09532f, 0.84455f, 0.79299f, 1.0f } },
    { 0.3058823529411765f, vec4<float> { 0.09377f, 0.85175f, 0.78264f, 1.0f } },
    { 0.30980392156862746f, vec4<float> { 0.09287f, 0.85875f, 0.7724f, 1.0f } },
    { 0.3137254901960784f, vec4<float> { 0.09267f, 0.86554f, 0.7623f, 1.0f } },
    { 0.3176470588235294f, vec4<float> { 0.0932f, 0.87211f, 0.75237f, 1.0f } },
    { 0.32156862745098036f, vec4<float> { 0.09451f, 0.87844f, 0.74265f, 1.0f } },
    { 0.3254901960784314f, vec4<float> { 0.09662f, 0.88454f, 0.73316f, 1.0f } },
    { 0.32941176470588235f, vec4<float> { 0.09958f, 0.8904f, 0.72393f, 1.0f } },
    { 0.3333333333333333f, vec4<float> { 0.10342f, 0.896f, 0.715f, 1.0f } },
    { 0.33725490196078434f, vec4<float> { 0.10815f, 0.90142f, 0.70599f, 1.0f } },
    { 0.3411764705882353f, vec4<float> { 0.11374f, 0.90673f, 0.69651f, 1.0f } },
    { 0.34509803921568627f, vec4<float> { 0.12014f, 0.91193f, 0.6866f, 1.0f } },
    { 0.34901960784313724f, vec4<float> { 0.12733f, 0.91701f, 0.67627f, 1.0f } },
    { 0.3529411764705882f, vec4<float> { 0.13526f, 0.92197f, 0.66556f, 1.0f } },
    { 0.3568627450980392f, vec4<float> { 0.14391f, 0.9268f, 0.65448f, 1.0f } },
    { 0.3607843137254902f, vec4<float> { 0.15323f, 0.93151f, 0.64308f, 1.0f } },
    { 0.36470588235294116f, vec4<float> { 0.16319f, 0.93609f, 0.63137f, 1.0f } },
    { 0.3686274509803922f, vec4<float> { 0.17377f, 0.94053f, 0.61938f, 1.0f } },
    { 0.37254901960784315f, vec4<float> { 0.18491f, 0.94484f, 0.60713f, 1.0f } },
    { 0.3764705882352941f, vec4<float> { 0.19659f, 0.94901f, 0.59466f, 1.0f } },
    { 0.3803921568627451f, vec4<float> { 0.20877f, 0.95304f, 0.58199f, 1.0f } },
    { 0.38431372549019605f, vec4<float> { 0.22142f, 0.95692f, 0.56914f, 1.0f } },
    { 0.38823529411764707f, vec4<float> { 0.23449f, 0.96065f, 0.55614f, 1.0f } },
    { 0.39215686274509803f, vec4<float> { 0.24797f, 0.96423f, 0.54303f, 1.0f } },
    { 0.396078431372549f, vec4<float> { 0.2618f, 0.96765f, 0.52981f, 1.0f } },
    { 0.4f, vec4<float> { 0.27597f, 0.97092f, 0.51653f, 1.0f } },
    { 0.403921568627451f, vec4<float> { 0.29042f, 0.97403f, 0.50321f, 1.0f } },
    { 0.40784313725490196f, vec4<float> { 0.30513f, 0.97697f, 0.48987f, 1.0f } },
    { 0.4117647058823529f, vec4<float> { 0.32006f, 0.97974f, 0.47654f, 1.0f } },
    { 0.4156862745098039f, vec4<float> { 0.33517f, 0.98234f, 0.46325f, 1.0f } },
    { 0.4196078431372549f, vec4<float> { 0.35043f, 0.98477f, 0.45002f, 1.0f } },
    { 0.4235294117647059f, vec4<float> { 0.36581f, 0.98702f, 0.43688f, 1.0f } },
    { 0.42745098039215684f, vec4<float> { 0.38127f, 0.98909f, 0.42386f, 1.0f } },
    { 0.43137254901960786f, vec4<float> { 0.39678f, 0.99098f, 0.41098f, 1.0f } },
    { 0.43529411764705883f, vec4<float> { 0.41229f, 0.99268f, 0.39826f, 1.0f } },
    { 0.4392156862745098f, vec4<float> { 0.42778f, 0.99419f, 0.38575f, 1.0f } },
    { 0.44313725490196076f, vec4<float> { 0.44321f, 0.99551f, 0.37345f, 1.0f } },
    { 0.44705882352941173f, vec4<float> { 0.45854f, 0.99663f, 0.3614f, 1.0f } },
    { 0.45098039215686275f, vec4<float> { 0.47375f, 0.99755f, 0.34963f, 1.0f } },
    { 0.4549019607843137f, vec4<float> { 0.48879f, 0.99828f, 0.33816f, 1.0f } },
    { 0.4588235294117647f, vec4<float> { 0.50362f, 0.99879f, 0.32701f, 1.0f } },
    { 0.4627450980392157f, vec4<float> { 0.51822f, 0.9991f, 0.31622f, 1.0f } },
    { 0.4666666666666667f, vec4<float> { 0.53255f, 0.99919f, 0.30581f, 1.0f } },
    { 0.47058823529411764f, vec4<float> { 0.54658f, 0.99907f, 0.29581f, 1.0f } },
    { 0.4745098039215686f, vec4<float> { 0.56026f, 0.99873f, 0.28623f, 1.0f } },
    { 0.4784313725490196f, vec4<float> { 0.57357f, 0.99817f, 0.27712f, 1.0f } },
    { 0.4823529411764706f, vec4<float> { 0.58646f, 0.99739f, 0.26849f, 1.0f } },
    { 0.48627450980392156f, vec4<float> { 0.59891f, 0.99638f, 0.26038f, 1.0f } },
    { 0.49019607843137253f, vec4<float> { 0.61088f, 0.99514f, 0.2528f, 1.0f } },
    { 0.49411764705882355f, vec4<float> { 0.62233f, 0.99366f, 0.24579f, 1.0f } },
    { 0.4980392156862745f, vec4<float> { 0.63323f, 0.99195f, 0.23937f, 1.0f } },
    { 0.5019607843137255f, vec4<float> { 0.64362f, 0.98999f, 0.23356f, 1.0f } },
    { 0.5058823529411764f, vec4<float> { 0.65394f, 0.98775f, 0.22835f, 1.0f } },
    { 0.5098039215686274f, vec4<float> { 0.66428f, 0.98524f, 0.2237f, 1.0f } },
    { 0.5137254901960784f, vec4<float> { 0.67462f, 0.98246f, 0.2196f, 1.0f } },
    { 0.5176470588235293f, vec4<float> { 0.68494f, 0.97941f, 0.21602f, 1.0f } },
    { 0.5215686274509804f, vec4<float> { 0.69525f, 0.9761f, 0.21294f, 1.0f } },
    { 0.5254901960784314f, vec4<float> { 0.70553f, 0.97255f, 0.21032f, 1.0f } },
    { 0.5294117647058824f, vec4<float> { 0.71577f, 0.96875f, 0.20815f, 1.0f } },
    { 0.5333333333333333f, vec4<float> { 0.72596f, 0.9647f, 0.2064f, 1.0f } },
    { 0.5372549019607843f, vec4<float> { 0.7361f, 0.96043f, 0.20504f, 1.0f } },
    { 0.5411764705882353f, vec4<float> { 0.74617f, 0.95593f, 0.20406f, 1.0f } },
    { 0.5450980392156862f, vec4<float> { 0.75617f, 0.95121f, 0.20343f, 1.0f } },
    { 0.5490196078431373f, vec4<float> { 0.76608f, 0.94627f, 0.20311f, 1.0f } },
    { 0.5529411764705883f, vec4<float> { 0.77591f, 0.94113f, 0.2031f, 1.0f } },
    { 0.5568627450980392f, vec4<float> { 0.78563f, 0.93579f, 0.20336f, 1.0f } },
    { 0.5607843137254902f, vec4<float> { 0.79524f, 0.93025f, 0.20386f, 1.0f } },
    { 0.5647058823529412f, vec4<float> { 0.80473f, 0.92452f, 0.20459f, 1.0f } },
    { 0.5686274509803921f, vec4<float> { 0.8141f, 0.91861f, 0.20552f, 1.0f } },
    { 0.5725490196078431f, vec4<float> { 0.82333f, 0.91253f, 0.20663f, 1.0f } },
    { 0.5764705882352941f, vec4<float> { 0.83241f, 0.90627f, 0.20788f, 1.0f } },
    { 0.580392156862745f, vec4<float> { 0.84133f, 0.89986f, 0.20926f, 1.0f } },
    { 0.5843137254901961f, vec4<float> { 0.8501f, 0.89328f, 0.21074f, 1.0f } },
    { 0.5882352941176471f, vec4<float> { 0.85868f, 0.88655f, 0.2123f, 1.0f } },
    { 0.592156862745098f, vec4<float> { 0.86709f, 0.87968f, 0.21391f, 1.0f } },
    { 0.596078431372549f, vec4<float> { 0.8753f, 0.87267f, 0.21555f, 1.0f } },
    { 0.6f, vec4<float> { 0.88331f, 0.86553f, 0.21719f, 1.0f } },
    { 0.6039215686274509f, vec4<float> { 0.89112f, 0.85826f, 0.2188f, 1.0f } },
    { 0.6078431372549019f, vec4<float> { 0.8987f, 0.85087f, 0.22038f, 1.0f } },
    { 0.611764705882353f, vec4<float> { 0.90605f, 0.84337f, 0.22188f, 1.0f } },
    { 0.615686274509804f, vec4<float> { 0.91317f, 0.83576f, 0.22328f, 1.0f } },
    { 0.6196078431372549f, vec4<float> { 0.92004f, 0.82806f, 0.22456f, 1.0f } },
    { 0.6235294117647059f, vec4<float> { 0.92666f, 0.82025f, 0.2257f, 1.0f } },
    { 0.6274509803921569f, vec4<float> { 0.93301f, 0.81236f, 0.22667f, 1.0f } },
    { 0.6313725490196078f, vec4<float> { 0.93909f, 0.80439f, 0.22744f, 1.0f } },
    { 0.6352941176470588f, vec4<float> { 0.94489f, 0.79634f, 0.228f, 1.0f } },
    { 0.6392156862745098f, vec4<float> { 0.95039f, 0.78823f, 0.22831f, 1.0f } },
    { 0.6431372549019607f, vec4<float> { 0.9556f, 0.78005f, 0.22836f, 1.0f } },
    { 0.6470588235294118f, vec4<float> { 0.96049f, 0.77181f, 0.22811f, 1.0f } },
    { 0.6509803921568628f, vec4<float> { 0.96507f, 0.76352f, 0.22754f, 1.0f } },
    { 0.6549019607843137f, vec4<float> { 0.96931f, 0.75519f, 0.22663f, 1.0f } },
    { 0.6588235294117647f, vec4<float> { 0.97323f, 0.74682f, 0.22536f, 1.0f } },
    { 0.6627450980392157f, vec4<float> { 0.97679f, 0.73842f, 0.22369f, 1.0f } },
    { 0.6666666666666666f, vec4<float> { 0.98f, 0.73f, 0.22161f, 1.0f } },
    { 0.6705882352941176f, vec4<float> { 0.98289f, 0.7214f, 0.21918f, 1.0f } },
    { 0.6745098039215687f, vec4<float> { 0.98549f, 0.7125f, 0.2165f, 1.0f } },
    { 0.6784313725490196f, vec4<float> { 0.98781f, 0.7033f, 0.21358f, 1.0f } },
    { 0.6823529411764706f, vec4<float> { 0.98986f, 0.69382f, 0.21043f, 1.0f } },
    { 0.6862745098039216f, vec4<float> { 0.99163f, 0.68408f, 0.20706f, 1.0f } },
    { 0.6901960784313725f, vec4<float> { 0.99314f, 0.67408f, 0.20348f, 1.0f } },
    { 0.6941176470588235f, vec4<float> { 0.99438f, 0.66386f, 0.19971f, 1.0f } },
    { 0.6980392156862745f, vec4<float> { 0.99535f, 0.65341f, 0.19577f, 1.0f } },
    { 0.7019607843137254f, vec4<float> { 0.99607f, 0.64277f, 0.19165f, 1.0f } },
    { 0.7058823529411764f, vec4<float> { 0.99654f, 0.63193f, 0.18738f, 1.0f } },
    { 0.7098039215686275f, vec4<float> { 0.99675f, 0.62093f, 0.18297f, 1.0f } },
    { 0.7137254901960784f, vec4<float> { 0.99672f, 0.60977f, 0.17842f, 1.0f } },
    { 0.7176470588235294f, vec4<float> { 0.99644f, 0.59846f, 0.17376f, 1.0f } },
    { 0.7215686274509804f, vec4<float> { 0.99593f, 0.58703f, 0.16899f, 1.0f } },
    { 0.7254901960784313f, vec4<float> { 0.99517f, 0.57549f, 0.16412f, 1.0f } },
    { 0.7294117647058823f, vec4<float> { 0.99419f, 0.56386f, 0.15918f, 1.0f } },
    { 0.7333333333333333f, vec4<float> { 0.99297f, 0.55214f, 0.15417f, 1.0f } },
    { 0.7372549019607844f, vec4<float> { 0.99153f, 0.54036f, 0.1491f, 1.0f } },
    { 0.7411764705882353f, vec4<float> { 0.98987f, 0.52854f, 0.14398f, 1.0f } },
    { 0.7450980392156863f, vec4<float> { 0.98799f, 0.51667f, 0.13883f, 1.0f } },
    { 0.7490196078431373f, vec4<float> { 0.9859f, 0.50479f, 0.13367f, 1.0f } },
    { 0.7529411764705882f, vec4<float> { 0.9836f, 0.49291f, 0.12849f, 1.0f } },
    { 0.7568627450980392f, vec4<float> { 0.98108f, 0.48104f, 0.12332f, 1.0f } },
    { 0.7607843137254902f, vec4<float> { 0.97837f, 0.4692f, 0.11817f, 1.0f } },
    { 0.7647058823529411f, vec4<float> { 0.97545f, 0.4574f, 0.11305f, 1.0f } },
    { 0.7686274509803921f, vec4<float> { 0.97234f, 0.44565f, 0.10797f, 1.0f } },
    { 0.7725490196078432f, vec4<float> { 0.96904f, 0.43399f, 0.10294f, 1.0f } },
    { 0.7764705882352941f, vec4<float> { 0.96555f, 0.42241f, 0.09798f, 1.0f } },
    { 0.7803921568627451f, vec4<float> { 0.96187f, 0.41093f, 0.0931f, 1.0f } },
    { 0.7843137254901961f, vec4<float> { 0.95801f, 0.39958f, 0.08831f, 1.0f } },
    { 0.788235294117647f, vec4<float> { 0.95398f, 0.38836f, 0.08362f, 1.0f } },
    { 0.792156862745098f, vec4<float> { 0.94977f, 0.37729f, 0.07905f, 1.0f } },
    { 0.796078431372549f, vec4<float> { 0.94538f, 0.36638f, 0.07461f, 1.0f } },
    { 0.8f, vec4<float> { 0.94084f, 0.35566f, 0.07031f, 1.0f } },
    { 0.803921568627451f, vec4<float> { 0.93612f, 0.34513f, 0.06616f, 1.0f } },
    { 0.807843137254902f, vec4<float> { 0.93125f, 0.33482f, 0.06218f, 1.0f } },
    { 0.8117647058823529f, vec4<float> { 0.92623f, 0.32473f, 0.05837f, 1.0f } },
    { 0.8156862745098039f, vec4<float> { 0.92105f, 0.31489f, 0.05475f, 1.0f } },
    { 0.8196078431372549f, vec4<float> { 0.91572f, 0.3053f, 0.05134f, 1.0f } },
    { 0.8235294117647058f, vec4<float> { 0.91024f, 0.29599f, 0.04814f, 1.0f } },
    { 0.8274509803921568f, vec4<float> { 0.90463f, 0.28696f, 0.04516f, 1.0f } },
    { 0.8313725490196078f, vec4<float> { 0.89888f, 0.27824f, 0.04243f, 1.0f } },
    { 0.8352941176470589f, vec4<float> { 0.89298f, 0.26981f, 0.03993f, 1.0f } },
    { 0.8392156862745098f, vec4<float> { 0.88691f, 0.26152f, 0.03753f, 1.0f } },
    { 0.8431372549019608f, vec4<float> { 0.88066f, 0.25334f, 0.03521f, 1.0f } },
    { 0.8470588235294118f, vec4<float> { 0.87422f, 0.24526f, 0.03297f, 1.0f } },
    { 0.8509803921568627f, vec4<float> { 0.8676f, 0.2373f, 0.03082f, 1.0f } },
    { 0.8549019607843137f, vec4<float> { 0.86079f, 0.22945f, 0.02875f, 1.0f } },
    { 0.8588235294117647f, vec4<float> { 0.8538f, 0.2217f, 0.02677f, 1.0f } },
    { 0.8627450980392157f, vec4<float> { 0.84662f, 0.21407f, 0.02487f, 1.0f } },
    { 0.8666666666666667f, vec4<float> { 0.83926f, 0.20654f, 0.02305f, 1.0f } },
    { 0.8705882352941177f, vec4<float> { 0.83172f, 0.19912f, 0.02131f, 1.0f } },
    { 0.8745098039215686f, vec4<float> { 0.82399f, 0.19182f, 0.01966f, 1.0f } },
    { 0.8784313725490196f, vec4<float> { 0.81608f, 0.18462f, 0.01809f, 1.0f } },
    { 0.8823529411764706f, vec4<float> { 0.80799f, 0.17753f, 0.0166f, 1.0f } },
    { 0.8862745098039215f, vec4<float> { 0.79971f, 0.17055f, 0.0152f, 1.0f } },
    { 0.8901960784313725f, vec4<float> { 0.79125f, 0.16368f, 0.01387f, 1.0f } },
    { 0.8941176470588235f, vec4<float> { 0.7826f, 0.15693f, 0.01264f, 1.0f } },
    { 0.8980392156862745f, vec4<float> { 0.77377f, 0.15028f, 0.01148f, 1.0f } },
    { 0.9019607843137255f, vec4<float> { 0.76476f, 0.14374f, 0.01041f, 1.0f } },
    { 0.9058823529411765f, vec4<float> { 0.75556f, 0.13731f, 0.00942f, 1.0f } },
    { 0.9098039215686274f, vec4<float> { 0.74617f, 0.13098f, 0.00851f, 1.0f } },
    { 0.9137254901960784f, vec4<float> { 0.73661f, 0.12477f, 0.00769f, 1.0f } },
    { 0.9176470588235294f, vec4<float> { 0.72686f, 0.11867f, 0.00695f, 1.0f } },
    { 0.9215686274509803f, vec4<float> { 0.71692f, 0.11268f, 0.00629f, 1.0f } },
    { 0.9254901960784314f, vec4<float> { 0.7068f, 0.1068f, 0.00571f, 1.0f } },
    { 0.9294117647058824f, vec4<float> { 0.6965f, 0.10102f, 0.00522f, 1.0f } },
    { 0.9333333333333333f, vec4<float> { 0.68602f, 0.09536f, 0.00481f, 1.0f } },
    { 0.9372549019607843f, vec4<float> { 0.67535f, 0.0898f, 0.00449f, 1.0f } },
    { 0.9411764705882353f, vec4<float> { 0.66449f, 0.08436f, 0.00424f, 1.0f } },
    { 0.9450980392156862f, vec4<float> { 0.65345f, 0.07902f, 0.00408f, 1.0f } },
    { 0.9490196078431372f, vec4<float> { 0.64223f, 0.0738f, 0.00401f, 1.0f } },
    { 0.9529411764705882f, vec4<float> { 0.63082f, 0.06868f, 0.00401f, 1.0f } },
    { 0.9568627450980391f, vec4<float> { 0.61923f, 0.06367f, 0.0041f, 1.0f } },
    { 0.9607843137254902f, vec4<float> { 0.60746f, 0.05878f, 0.00427f, 1.0f } },
    { 0.9647058823529412f, vec4<float> { 0.5955f, 0.05399f, 0.00453f, 1.0f } },
    { 0.9686274509803922f, vec4<float> { 0.58336f, 0.04931f, 0.00486f, 1.0f } },
    { 0.9725490196078431f, vec4<float> { 0.57103f, 0.04474f, 0.00529f, 1.0f } },
    { 0.9764705882352941f, vec4<float> { 0.55852f, 0.04028f, 0.00579f, 1.0f } },
    { 0.9803921568627451f, vec4<float> { 0.54583f, 0.03593f, 0.00638f, 1.0f } },
    { 0.984313725490196f, vec4<float> { 0.53295f, 0.03169f, 0.00705f, 1.0f } },
    { 0.9882352941176471f, vec4<float> { 0.51989f, 0.02756f, 0.0078f, 1.0f } },
    { 0.9921568627450981f, vec4<float> { 0.50664f, 0.02354f, 0.00863f, 1.0f } },
    { 0.996078431372549f, vec4<float> { 0.49321f, 0.01963f, 0.00955f, 1.0f } },
    { 1.0f, vec4<float> { 0.4796f, 0.01583f, 0.01055f, 1.0f } },
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
    { "Turbo", ColormapTemplate::turbo },
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