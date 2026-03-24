#pragma once
#include "python.hpp"
#include "utility.hpp"

#include <qobject.h>

// ----- Embedding ----- //

class Embedding : public QObject
{
    Q_OBJECT
public:
    class SearchTree
    {
    public:
        struct Partition
        {
            uint32_t begin { 0 };
            uint32_t end { 0 };

            enum Axis { eNone, eX, eY } axis { eNone };
            float value { 0.0f };
            uint32_t lower { 0 };
            uint32_t upper { 0 };
        };

        SearchTree( const std::vector<vec2<float>>& points );

        const std::vector<vec2<float>>& points() const noexcept;
        uint32_t nearest_neighbor( vec2<float> point ) const;

    private:
        uint32_t construct_partitions_recursive( Partition::Axis axis, uint32_t begin, uint32_t end );

        const std::vector<vec2<float>>& _points;
        std::vector<uint32_t> _indices;
        std::vector<Partition> _partitions;
    };

    Embedding( std::vector<uint32_t> indices, std::vector<vec2<float>> coordinates );

    const std::vector<uint32_t>& indices() const noexcept;
    const std::vector<vec2<float>>& coordinates() const noexcept;

    void update_model( py::object model ) noexcept;
    py::object model() const noexcept;

    const SearchTree& search_tree() const noexcept;

signals:
    void model_changed( py::object model ) const;

private:
    std::vector<uint32_t> _indices;
    std::vector<vec2<float>> _coordinates;
    py::object _model { py::none {} };

    std::unique_ptr<SearchTree> _search_tree;
};