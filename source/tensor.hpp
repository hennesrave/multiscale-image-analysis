#pragma once
#include <array>
#include <memory>

class Tensor
{
public:
    template<size_t Rank> class with_rank
    {
    public:
        template<class Type> class with_type;
    };
};

template<size_t Rank> template<class Type> class Tensor::with_rank<Rank>::with_type
{
public:
    using value_type = Type;

    static size_t compute_size( const std::array<size_t, Rank>& dimensions )
    {
        auto size = size_t { 1 };
        for( const auto& dimension : dimensions )
            size *= dimension;
        return size;
    }
    static with_type from_pointer( const std::array<size_t, Rank>& dimensions, value_type* pointer )
    {
        auto result = with_type {};
        result._size = compute_size( dimensions );
        result._dimensions = dimensions;
        result._values = pointer;
        return result;
    }
    static with_type allocate( const std::array<size_t, Rank>& dimensions )
    {
        return from_pointer( dimensions, static_cast<value_type*>( std::malloc( compute_size( dimensions ) * sizeof( value_type ) ) ) );
    }

    with_type() noexcept = default;
    with_type( const std::array<size_t, Rank>& dimensions, const value_type& value ) : _size { this->compute_size( dimensions ) }, _dimensions { dimensions }, _values { static_cast<value_type*>( std::malloc( _size * sizeof( value_type ) ) ) }
    {
        for( size_t i = 0; i < _size; ++i )
            new ( _values + i ) value_type { value };
    }

    with_type( const with_type& other ) : _size { other._size }, _dimensions { other._dimensions }, _values { static_cast<value_type*>( std::malloc( _size * sizeof( value_type ) ) ) }
    {
        for( size_t i = 0; i < _size; ++i )
            new ( _values + i ) value_type { other._values[i] };
    }
    with_type( with_type&& other ) noexcept : _size { other._size }, _dimensions { other._dimensions }, _values { other._values }
    {
        other._size = 0;
        other._dimensions.fill( 0 );
        other._values = nullptr;
    }

    with_type& operator=( const with_type& other )
    {
        this->clear();

        _size = other._size;
        _dimensions = other._dimensions;
        _values = static_cast<value_type*>( std::malloc( _size * sizeof( value_type ) ) );
        for( size_t i = 0; i < _size; ++i )
            new ( _values + i ) value_type { other._values[i] };

        return *this;
    }
    with_type& operator=( with_type&& other ) noexcept
    {
        this->clear();

        _size = other._size;
        _dimensions = other._dimensions;
        _values = other._values;

        other._size = 0;
        other._dimensions.fill( 0 );
        other._values = nullptr;

        return *this;
    }

    ~with_type()
    {
        this->clear();
    }

    static size_t coordinates_to_index( const std::array<size_t, Rank>& coordinates, const std::array<size_t, Rank>& dimensions )
    {
        auto index = size_t { 0 };
        auto multiplier = size_t { 1 };
        for( size_t i = Rank; i-- > 0;)
        {
            index += coordinates[i] * multiplier;
            multiplier *= dimensions[i];
        }
        return index;
    }
    static std::array<size_t, Rank> index_to_coordinates( size_t index, const std::array<size_t, Rank>& dimensions )
    {
        auto coordinates = std::array<size_t, Rank> {};
        for( size_t i = 0; i < Rank; ++i )
        {
            coordinates[i] = index % dimensions[i];
            index /= dimensions[i];
        }
        return coordinates;
    }

    size_t rank() const noexcept
    {
        return Rank;
    }
    size_t size() const noexcept
    {
        return _size;
    }
    size_t bytes() const noexcept
    {
        return _size * sizeof( value_type );
    }
    bool empty() const noexcept
    {
        return _size == 0;
    }

    const std::array<size_t, Rank>& dimensions() const noexcept
    {
        return _dimensions;
    }

    const value_type* data() const noexcept
    {
        return _values;
    }
    value_type* data() noexcept
    {
        return _values;
    }

    const value_type* begin() const noexcept
    {
        return _values;
    }
    value_type* begin() noexcept
    {
        return _values;
    }

    const value_type* end() const noexcept
    {
        return _values + _size;
    }
    value_type* end() noexcept
    {
        return _values + _size;
    }

    size_t coordinates_to_index( const std::array<size_t, Rank>& coordinates ) const
    {
        return Tensor::with_rank<Rank>::with_type<Type>::coordinates_to_index( coordinates, _dimensions );
    }
    std::array<size_t, Rank> index_to_coordinates( size_t index ) const
    {
        return Tensor::with_rank<Rank>::with_type<Type>::index_to_coordinates( index, _dimensions );
    }

    const value_type& value( size_t index ) const
    {
        return _values[index];
    }
    const value_type& value( const std::array<size_t, Rank>& coordinates ) const
    {
        return this->value( this->coordinates_to_index( coordinates ) );
    }
    value_type& value( size_t index )
    {
        return _values[index];
    }
    value_type& value( const std::array<size_t, Rank>& coordinates )
    {
        return this->value( this->coordinates_to_index( coordinates ) );
    }

    void update_value( size_t index, const value_type& value )
    {
        _values[index] = value;
    }
    void update_value( size_t index, value_type&& value )
    {
        _values[index] = std::move( value );
    }
    void update_value( const std::array<size_t, Rank>& coordinates, const value_type& value )
    {
        this->update_value( this->coordinates_to_index( coordinates ), value );
    }
    void update_value( const std::array<size_t, Rank>& coordinates, value_type&& value )
    {
        this->update_value( this->coordinates_to_index( coordinates ), value );
    }

    void clear()
    {
        for( size_t i = 0; i < _size; ++i )
        {
            _values[i].~value_type();
        }
        std::free( _values );

        _size = 0;
        _dimensions.fill( 0 );
        _values = nullptr;
    }

private:
    size_t _size = 0;
    std::array<size_t, Rank> _dimensions {};
    value_type* _values = nullptr;
};

template<> template<class Type> class Tensor::with_rank<1>::with_type
{
public:
    using value_type = Type;

    static with_type from_pointer( size_t size, value_type* pointer )
    {
        auto result = with_type {};
        result._size = size;
        result._values = pointer;
        return result;
    }
    static with_type allocate( size_t size )
    {
        return from_pointer( size, static_cast<value_type*>( std::malloc( size * sizeof( value_type ) ) ) );
    }

    with_type() noexcept = default;
    with_type( size_t size, const value_type& value ) : _size { size }, _values { static_cast<value_type*>( std::malloc( _size * sizeof( value_type ) ) ) }
    {
        for( size_t i = 0; i < _size; ++i )
            new ( _values + i ) value_type { value };
    }

    with_type( const with_type& other ) : _size { other._size }, _values { static_cast<value_type*>( std::malloc( _size * sizeof( value_type ) ) ) }
    {
        for( size_t i = 0; i < _size; ++i )
            new ( _values + i ) value_type { other._values[i] };
    }
    with_type( with_type&& other ) noexcept : _size { other._size }, _values { other._values }
    {
        other._size = 0;
        other._values = nullptr;
    }

    with_type& operator=( const with_type& other )
    {
        this->clear();

        _size = other._size;
        _values = static_cast<value_type*>( std::malloc( _size * sizeof( value_type ) ) );
        for( size_t i = 0; i < _size; ++i )
            new ( _values + i ) value_type { other._values[i] };

        return *this;
    }
    with_type& operator=( with_type&& other ) noexcept
    {
        this->clear();

        _size = other._size;
        _values = other._values;

        other._size = 0;
        other._values = nullptr;

        return *this;
    }

    ~with_type()
    {
        this->clear();
    }

    size_t size() const noexcept
    {
        return _size;
    }
    size_t bytes() const noexcept
    {
        return _size * sizeof( value_type );
    }
    bool empty() const noexcept
    {
        return _size == 0;
    }

    const value_type* data() const noexcept
    {
        return _values;
    }
    value_type* data() noexcept
    {
        return _values;
    }

    const value_type* begin() const noexcept
    {
        return _values;
    }
    value_type* begin() noexcept
    {
        return _values;
    }

    const value_type* end() const noexcept
    {
        return _values + _size;
    }
    value_type* end() noexcept
    {
        return _values + _size;
    }

    const value_type& first() const
    {
        return _values[0];
    }
    value_type& first()
    {
        return _values[0];
    }

    const value_type& last() const
    {
        return _values[_size - 1];
    }
    value_type& last()
    {
        return _values[_size - 1];
    }

    const value_type& value( size_t index ) const
    {
        return _values[index];
    }
    value_type& value( size_t index )
    {
        return _values[index];
    }

    const value_type& operator[]( size_t index ) const
    {
        return this->value( index );
    }
    value_type& operator[]( size_t index )
    {
        return this->value( index );
    }

    void update_value( size_t index, const value_type& value )
    {
        _values[index] = value;
    }
    void update_value( size_t index, value_type&& value )
    {
        _values[index] = std::move( value );
    }

    void clear()
    {
        for( size_t i = 0; i < _size; ++i )
        {
            _values[i].~value_type();
        }
        std::free( _values );

        _size = 0;
        _values = nullptr;
    }

private:
    size_t _size = 0;
    value_type* _values = nullptr;
};



template<class Type> using Array = Tensor::with_rank<1>::template with_type<Type>;
template<class Type> using Matrix = Tensor::with_rank<2>::template with_type<Type>;