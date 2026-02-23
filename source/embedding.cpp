#include "embedding.hpp"

// ----- Embedding ----- //

Embedding::Embedding( std::vector<uint32_t> indices, std::vector<vec2<float>> coordinates )
    : _indices { std::move( indices ) }, _coordinates { std::move( coordinates ) }, _model { py::none {} }
{
    _search_tree.reset( new SearchTree { _coordinates } );
}

const std::vector<uint32_t>& Embedding::indices() const noexcept
{
    return _indices;
}
const std::vector<vec2<float>>& Embedding::coordinates() const noexcept
{
    return _coordinates;
}

void Embedding::update_model( py::object model ) noexcept
{
    _model = model;
    emit this->model_changed( _model );
}
py::object Embedding::model() const noexcept
{
    return _model;
}

const Embedding::SearchTree& Embedding::search_tree() const noexcept
{
    return *_search_tree;
}

// ----- Embedding::SearchTree ----- //

Embedding::SearchTree::SearchTree( const std::vector<vec2<float>>& points ) : _points { points }, _indices( _points.size() )
{
    std::iota( _indices.begin(), _indices.end(), 0 );
    this->construct_partitions_recursive( Partition::Axis::eX, 0, static_cast<uint32_t>( _points.size() ) );
}

const std::vector<vec2<float>>& Embedding::SearchTree::points() const noexcept
{
    return _points;
}
uint32_t Embedding::SearchTree::nearest_neighbor( vec2<float> point ) const
{
    auto current = uint32_t { 0 };
    while( true )
    {
        const auto& partition = _partitions[current];

        if( partition.axis == Partition::Axis::eNone )
        {
            struct
            {
                uint32_t index {};
                double distance {};
            } nearest { std::numeric_limits<uint32_t>::max(), std::numeric_limits<double>::max() };

            for( uint32_t i { partition.begin }; i < partition.end; ++i )
            {
                const double distance { ( _points[_indices[i]] - point ).length() };
                if( distance < nearest.distance ) nearest = { _indices[i], distance };
            }

            return nearest.index;
        }

        if( partition.axis == Partition::Axis::eX ) current = point.x < partition.value ? partition.lower : partition.upper;
        else current = point.y < partition.value ? partition.lower : partition.upper;
    }
}

uint32_t Embedding::SearchTree::construct_partitions_recursive( Partition::Axis axis, uint32_t begin, uint32_t end )
{
    if( end - begin <= 16 )
    {
        const auto index = static_cast<uint32_t>( _partitions.size() );
        const auto partition = Partition {
            begin, end, Partition::Axis::eNone, 0.0, std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()
        };
        _partitions.push_back( partition );
        return index;
    }

    const auto comparison {
        axis == Partition::Axis::eX ?
        std::function<bool( uint32_t, uint32_t )> { [this]( uint32_t a, uint32_t b ) { return _points[a].x < _points[b].x; } } :
        std::function<bool( uint32_t, uint32_t )> { [this]( uint32_t a, uint32_t b ) { return _points[a].y < _points[b].y; } }
    };
    auto middle = ( begin + end ) / 2;
    std::nth_element( _indices.begin() + begin, _indices.begin() + middle, _indices.begin() + end, comparison );

    const auto point_value = [this, axis] ( uint32_t pointIndex )
    {
        return axis == Partition::Axis::eX ? _points[pointIndex].x : _points[pointIndex].y;
    };
    const auto value = point_value( _indices[middle] );
    for( ; middle < end - 1 && point_value( _indices[middle++ + 1] ) == value; );

    const auto partition = Partition {
        begin, end, axis, value, std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()
    };
    const auto index = static_cast<uint32_t>( _partitions.size() );
    _partitions.push_back( partition );

    const auto next_axis = Partition::Axis { axis == Partition::Axis::eX ? Partition::Axis::eY : Partition::Axis::eX };
    _partitions[index].lower = this->construct_partitions_recursive( next_axis, begin, middle );
    _partitions[index].upper = this->construct_partitions_recursive( next_axis, middle, end );

    return index;
}