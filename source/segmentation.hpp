#pragma once
#include "json.hpp"
#include "utility.hpp"

#include <qcolor.h>
#include <qobject.h>
#include <qsharedpointer.h>

class Segment;
class Segmentation;

class Segment : public QObject
{
    Q_OBJECT
public:
    const Segmentation& segmentation() const noexcept;
    Segmentation& segmentation() noexcept;

    uint32_t number() const noexcept;
    uint32_t element_count() const noexcept;

    vec4<float> color() const noexcept;
    void update_color( vec4<float> color );

    const QString& identifier() const noexcept;
    void update_identifier( const std::optional<QString>& identifier );

signals:
    void number_changed( uint32_t number );
    void element_count_changed( uint32_t element_count );
    void identifier_changed( const QString& identifier );
    void color_changed( vec4<float> color );

private:
    friend class Segmentation;

    Segment( Segmentation& segmentation, uint32_t number, uint32_t element_count, vec4<float> color );

    void update_number( uint32_t number );
    void update_element_count( uint32_t element_count );

    Segmentation& _segmentation;
    uint32_t _number;
    uint32_t _element_count;
    Override<QString> _identifier;
    vec4<float> _color;
};

class Segmentation : public QObject
{
    Q_OBJECT
public:
    class Editor
    {
    public:
        Editor( const Editor& ) = delete;
        Editor( Editor&& ) = delete;

        Editor& operator=( const Editor& ) = delete;
        Editor& operator=( Editor&& ) = delete;

        ~Editor();

        void update_value( uint32_t element_index, uint32_t segment_number );

    private:
        friend class Segmentation;

        Editor( Segmentation& segmentation );

        Segmentation& _segmentation;
        std::vector<uint32_t> _element_counts;
    };

    Segmentation( uint32_t element_count );

    uint32_t element_count() const noexcept;

    const Array<uint32_t>& segment_numbers() const noexcept;
    uint32_t segment_number( uint32_t element_index ) const;

    const Array<vec4<float>>& element_colors() const noexcept;
    const Array<std::vector<uint32_t>>& element_indices() const noexcept;

    uint32_t segment_count() const noexcept;
    const QSharedPointer<Segment>& segment( uint32_t segment_number ) const;

    QSharedPointer<Segment> append_segment();
    void remove_segment( QSharedPointer<Segment> segment );

    nlohmann::json serialize() const;
    bool deserialize( const nlohmann::json& json );

    Editor editor();

signals:
    void segment_numbers_changed() const;
    void element_colors_changed() const;
    void element_indices_changed() const;

    void segment_appended( const QSharedPointer<Segment>& segment ) const;
    void segment_removed( const QSharedPointer<Segment>& segment ) const;
    void segment_count_changed( uint32_t segment_count ) const;

    void segment_identifier_changed() const;
    void segment_color_changed() const;

private:
    Array<vec4<float>> compute_element_colors() const;
    Array<std::vector<uint32_t>> compute_element_indices() const;

    Array<uint32_t> _segment_numbers;
    Computed<Array<vec4<float>>> _element_colors;
    Computed<Array<std::vector<uint32_t>>> _element_indices;

    std::vector<QSharedPointer<Segment>> _segments;
    uint32_t _current_preset_color_index = 0;


    static inline std::array<vec4<float>, 9> _preset_colors {
        vec4<float> { 128, 177, 211, 255 } / 255.0f,
        vec4<float> { 253, 180, 98, 255 } / 255.0f,
        vec4<float> { 179, 222, 105, 255 } / 255.0f,
        vec4<float> { 252, 205, 229, 255 } / 255.0f,
        vec4<float> { 188, 128, 189, 255 } / 255.0f,
        vec4<float> { 141, 211, 199, 255 } / 255.0f,
        vec4<float> { 255, 255, 179, 255 } / 255.0f,
        vec4<float> { 190, 186, 218, 255 } / 255.0f,
        vec4<float> { 251, 128, 114, 255 } / 255.0f
    };
};