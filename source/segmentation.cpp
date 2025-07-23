#include "segmentation.hpp"

#include "json.hpp"
#include "logger.hpp"

// ----- Segment ----- //

const Segmentation& Segment::segmentation() const noexcept
{
    return _segmentation;
}
Segmentation& Segment::segmentation() noexcept
{
    return _segmentation;
}

uint32_t Segment::number() const noexcept
{
    return _number;
}
uint32_t Segment::element_count() const noexcept
{
    return _element_count;
}

vec4<float> Segment::color() const noexcept
{
    return _color;
}
void Segment::update_color( vec4<float> color )
{
    if( _color != color )
    {
        _identifier.update_automatic_value( "Segment " + color.qcolor().name() );
        emit color_changed( _color = color );
    }
}

const QString& Segment::identifier() const noexcept
{
    return _identifier.value();
}
void Segment::update_identifier( const std::optional<QString>& identifier )
{
    _identifier.update_override_value( identifier );
}

Segment::Segment( Segmentation& segmentation, uint32_t number, uint32_t element_count, vec4<float> color )
    : QObject {}
    , _segmentation { segmentation }
    , _number { number }
    , _element_count { element_count }
    , _color { color }
    , _identifier { "Segment " + color.qcolor().name(), std::nullopt }
{
    QObject::connect( &_identifier, &Override<QString>::value_changed, this, [this]
    {
        emit identifier_changed( _identifier.value() );
    } );
}

void Segment::update_number( uint32_t number )
{
    if( _number != number )
    {
        emit number_changed( _number = number );
    }
}
void Segment::update_element_count( uint32_t element_count )
{
    if( _element_count != element_count )
    {
        emit element_count_changed( _element_count = element_count );
    }
}

// ----- Segmentation ----- //

Segmentation::Segmentation( uint32_t element_count ) : _values { element_count, 0 }
{
    auto default_segment = this->append_segment();
    default_segment->update_color( vec4<float> { 0.0f, 0.0f, 0.0f, 0.0f } );
    default_segment->update_element_count( element_count );
    _current_preset_color_index = 0;

    _element_colors.initialize( [this] ( auto& element_colors ) { this->compute_element_colors( element_colors ); } );
    _element_indices.initialize( [this] ( auto& element_indices ) { this->compute_element_indices( element_indices ); } );

    QObject::connect( this, &Segmentation::segment_appended, this, [this]
    {
        emit segment_count_changed( this->segment_count() );
    } );
    QObject::connect( this, &Segmentation::segment_removed, this, [this]
    {
        emit segment_count_changed( this->segment_count() );
    } );

    QObject::connect( this, &Segmentation::values_changed, &_element_colors, &Promise<Array<vec4<float>>>::invalidate );
    QObject::connect( this, &Segmentation::values_changed, &_element_indices, &Promise<Array<std::vector<uint32_t>>>::invalidate );
}

uint32_t Segmentation::element_count() const noexcept
{
    return static_cast<uint32_t>( _values.size() );
}

const Array<uint32_t>& Segmentation::values() const noexcept
{
    return _values;
}
uint32_t Segmentation::value( uint32_t element_index ) const
{
    return _values.value( element_index );
}

const Array<uint32_t>& Segmentation::segment_numbers() const noexcept
{
    return _values;
}
uint32_t Segmentation::segment_number( uint32_t element_index ) const
{
    return _values.value( element_index );
}

uint32_t Segmentation::segment_count() const noexcept
{
    return static_cast<uint32_t>( _segments.size() );
}
const QSharedPointer<Segment>& Segmentation::segment( uint32_t segment_number ) const
{
    return _segments[segment_number];
}

QSharedPointer<Segment> Segmentation::append_segment()
{
    const auto color = _preset_colors[_current_preset_color_index];
    _current_preset_color_index = ( _current_preset_color_index + 1 ) % _preset_colors.size();

    auto segment = QSharedPointer<Segment> { new Segment { *this, this->segment_count(), 0, color } };

    QObject::connect( segment.data(), &Segment::identifier_changed, this, &Segmentation::segment_identifier_changed );
    QObject::connect( segment.data(), &Segment::color_changed, &_element_colors, &Promise<Array<vec4<float>>>::invalidate );
    QObject::connect( segment.data(), &Segment::color_changed, this, &Segmentation::segment_color_changed );

    _segments.push_back( segment );
    emit segment_appended( segment );
    return segment;
}
void Segmentation::remove_segment( QSharedPointer<Segment> segment )
{
    const auto segment_number = segment->number();
    if( _segments[segment_number] == segment && segment_number > 0 )
    {
        auto default_segment_element_count = _segments.front()->element_count();
        for( auto& value : _values )
        {
            if( value == segment_number )
            {
                value = 0;
                ++default_segment_element_count;
            }
            else if( value > segment_number )
            {
                --value;
            }
        }
        _segments.front()->update_element_count( default_segment_element_count );

        _segments.erase( _segments.begin() + segment_number );
        for( uint32_t i = segment_number; i < _segments.size(); ++i )
        {
            _segments[i]->update_number( i );
        }
        emit segment_removed( segment );

        if( segment->element_count() )
        {
            emit values_changed( _values );
        }
    }
}

nlohmann::json Segmentation::serialize() const
{
    auto json_segments = nlohmann::json::array();
    for( const auto& segment : _segments )
    {
        auto json_segment = nlohmann::json {
            { "number", segment->number() },
            { "element_count", segment->element_count() },
            { "identifier", segment->_identifier.override_value().value_or( "" ).toStdString() },
            { "color", segment->color().qcolor().name().toStdString() }
        };
        json_segments.push_back( json_segment );
    }

    auto segment_numbers = nlohmann::json::array();
    for( const auto value : _values )
    {
        segment_numbers.push_back( value );
    }

    auto json = nlohmann::json {};
    json["element_count"] = this->element_count();
    json["segments"] = json_segments;
    json["current_preset_color_index"] = _current_preset_color_index;
    json["segment_numbers"] = segment_numbers;
    return json;
}
bool Segmentation::deserialize( const nlohmann::json& json )
{
    const auto element_count = json.value( "element_count", 0u );
    if( element_count != this->element_count() )
    {
        Logger::error() << "Invalid element_count " << element_count;
        return false;
    }

    const auto segments = json.value( "segments", nlohmann::json::array() );
    while( this->segment_count() < segments.size() )
    {
        this->append_segment();
    }

    auto element_counts = std::vector<uint32_t> {};
    for( const auto json_segment : segments )
    {
        const auto number = json_segment["number"].get<uint32_t>();
        const auto element_count = json_segment["element_count"].get<uint32_t>();
        const auto identifier = QString::fromStdString( json_segment["identifier"].get<std::string>() );
        const auto color = QColor { QString::fromStdString( json_segment["color"].get<std::string>() ) };

        element_counts.push_back( element_count );
        _segments[number]->update_identifier( identifier.isEmpty() ? std::nullopt : std::optional<QString> { identifier } );
        _segments[number]->update_color( number == 0 ? vec4<float> {} : vec4<float> { color.redF(), color.greenF(), color.blueF(), color.alphaF() } );
    }

    _current_preset_color_index = json["current_preset_color_index"].get<uint32_t>();
    const auto segment_numbers = json["segment_numbers"];
    auto current_index = 0;
    for( const uint32_t segment_number : segment_numbers )
    {
        _values[current_index++] = segment_number;
    }

    for( uint32_t segment_number = 0; segment_number < element_counts.size(); ++segment_number )
    {
        _segments[segment_number]->update_element_count( element_counts[segment_number] );
    }

    while( this->segment_count() > segments.size() )
    {
        this->remove_segment( _segments.back() );
    }

    emit values_changed( _values );
    return true;
}

Segmentation::Editor Segmentation::editor()
{
    return Editor { *this };
}

const Promise<Array<vec4<float>>>& Segmentation::element_colors() const noexcept
{
    return _element_colors;
}
const Promise<Array<std::vector<uint32_t>>>& Segmentation::element_indices() const noexcept
{
    return _element_indices;
}

void Segmentation::compute_element_colors( Array<vec4<float>>& element_colors ) const
{
    if( element_colors.size() != this->element_count() )
    {
        element_colors = Array<vec4<float>>::allocate( this->element_count() );
    }

    utility::parallel_for( 0u, this->element_count(), [this, &element_colors] ( uint32_t element_index )
    {
        element_colors[element_index] = this->segment( _values[element_index] )->color();
    } );
}
void Segmentation::compute_element_indices( Array<std::vector<uint32_t>>& element_indices ) const
{
    if( element_indices.size() != this->segment_count() )
    {
        element_indices = Array<std::vector<uint32_t>> { this->segment_count(), std::vector<uint32_t> {} };
    }

    for( uint32_t element_index = 0; element_index < this->element_count(); ++element_index )
    {
        const auto segment_number = _values[element_index];
        element_indices[segment_number].push_back( element_index );
    }
}

// ----- Segmentation::Editor ----- //

Segmentation::Editor::~Editor()
{
    for( uint32_t segment_number = 0; segment_number < _segmentation.segment_count(); ++segment_number )
    {
        _segmentation.segment( segment_number )->update_element_count( _element_counts[segment_number] );
    }
    emit _segmentation.values_changed( _segmentation._values );
}

void Segmentation::Editor::update_value( uint32_t element_index, uint32_t segment_number )
{
    auto& current_segment_number = _segmentation._values.value( element_index );
    --_element_counts[current_segment_number];
    ++_element_counts[current_segment_number = segment_number];
}

Segmentation::Editor::Editor( Segmentation& segmentation ) : _segmentation { segmentation }, _element_counts( _segmentation.segment_count() )
{
    for( uint32_t segment_number = 0; segment_number < _segmentation.segment_count(); ++segment_number )
    {
        _element_counts[segment_number] = _segmentation.segment( segment_number )->element_count();
    }
}