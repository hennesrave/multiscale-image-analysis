#pragma once
#include "configuration.hpp"
#include "console.hpp"
#include "filestream.hpp"
#include "tensor.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <execution>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <sstream>

#define NOMINMAX
#include <Windows.h>

#include <qcolor.h>
#include <qstring.h>
#include <qobject.h>

namespace utility
{
    double degrees_to_radians( double degrees ) noexcept;
    double radians_to_degrees( double radians ) noexcept;

    int stepsize_to_precision( double stepsize );
    int compute_precision( double value );

    QString request_import_filepath( const QString& title, const QString& filter );

    template<class IndexType> void iterate_parallel( IndexType start, IndexType end, auto&& callable )
    {
        const auto range = std::views::iota( start, end );
        std::for_each( std::execution::seq, range.begin(), range.end(), std::forward<decltype( callable )>( callable ) );
    }
    template<class IndexType> void iterate_parallel( IndexType end, auto&& callable )
    {
        iterate_parallel( IndexType { 0 }, end, std::forward<decltype( callable )>( callable ) );
    }
}

namespace concepts
{
    template<class T> concept NotEqualComparable = requires( T a, T b )
    {
        { a != b } -> std::convertible_to<bool>;
    };
}

// ----- Formatters ----- //

template <> struct std::formatter<std::wstring> : std::formatter<std::string>
{
    auto format( const std::wstring& wstring, auto& context ) const
    {
        const auto required_size = WideCharToMultiByte( CP_UTF8, 0, wstring.data(), (int) wstring.size(), nullptr, 0, nullptr, nullptr );
        auto string = std::string( required_size, 0 );
        WideCharToMultiByte( CP_UTF8, 0, wstring.data(), (int) wstring.size(), &string[0], required_size, nullptr, nullptr );
        return std::formatter<std::string>::format( string, context );
    }
};

// ----- Timer ----- //

class Timer
{
public:
    double milliseconds() const;
    double microseconds() const;
    void reset();

private:
    decltype( std::chrono::high_resolution_clock::now() ) _start { std::chrono::high_resolution_clock::now() };
};

// ----- Range ----- //

template<class T> struct Range
{
    using value_type = T;

    union
    {
        struct
        {
            value_type lower;
            value_type upper;
        };
        struct
        {
            value_type minimum;
            value_type maximum;
        };
        struct
        {
            value_type x;
            value_type y;
        };
    };

    Range( value_type lower = {}, value_type upper = {} ) : lower { lower }, upper { upper } {}

    bool operator==( const Range& other ) const noexcept
    {
        return lower == other.lower && upper == other.upper;
    }
    bool operator!=( const Range& other ) const noexcept
    {
        return lower != other.lower || upper != other.upper;
    }
};

// ----- Vector ----- //

template<class T> struct vec2
{
    using value_type = T;
    value_type x;
    value_type y;

    vec2( value_type x = {}, value_type y = {} ) noexcept : x { x }, y { y } {}

    template<class U> vec2<U> cast()
    {
        return vec2<U> { static_cast<U>( x ), static_cast<U>( y ) };
    }
    value_type operator[]( size_t index ) const
    {
        return reinterpret_cast<const value_type*>( this )[index];
    }

    value_type minimum() const noexcept
    {
        return std::min( x, y );
    }
    value_type maximum() const noexcept
    {
        return std::max( x, y );
    }
    value_type sum() const noexcept
    {
        return x + y;
    }
    value_type product() const noexcept
    {
        return x * y;
    }

    value_type dot( vec2 other ) const noexcept
    {
        return x * other.x + y * other.y;
    }

    value_type length_squared() const noexcept
    {
        return x * x + y * y;
    }
    value_type length() const noexcept
    {
        return std::sqrt( this->length_squared() );
    }
    vec2 normalized() const noexcept
    {
        return *this / this->length();
    }

    vec2 operator+( vec2 other ) const noexcept
    {
        return vec2 { x + other.x, y + other.y };
    }
    vec2 operator-( vec2 other ) const noexcept
    {
        return vec2 { x - other.x, y - other.y };
    }
    vec2 operator*( value_type scalar ) const noexcept
    {
        return vec2 { x * scalar, y * scalar };
    }
    vec2 operator/( value_type scalar ) const noexcept
    {
        return vec2 { x / scalar, y / scalar };
    }

    friend vec2 operator*( value_type scalar, vec2 vector ) noexcept
    {
        return vector * scalar;
    }
    friend vec2 operator/( value_type scalar, vec2 vector ) noexcept
    {
        return vec2 { scalar / vector.x, scalar / vector.y };
    }

    vec2& operator+=( vec2 other ) noexcept
    {
        x += other.x;
        y += other.y;
        return *this;
    }
    vec2& operator-=( vec2 other ) noexcept
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    vec2& operator*=( value_type scalar ) noexcept
    {
        x *= scalar;
        y *= scalar;
        return *this;
    }
    vec2& operator/=( value_type scalar ) noexcept
    {
        x /= scalar;
        y /= scalar;
        return *this;
    }

    bool operator==( vec2 other ) const noexcept
    {
        return x == other.x && y == other.y;
    }
    bool operator!=( vec2 other ) const noexcept
    {
        return x != other.x || y != other.y;
    }
};

template<class T> struct vec3
{
    using value_type = T;
    value_type x;
    value_type y;
    value_type z;

    vec3( value_type x = {}, value_type y = {}, value_type z = {} ) noexcept : x { x }, y { y }, z { z } {}

    template<class U> vec3<U> cast()
    {
        return vec3<U> { static_cast<U>( x ), static_cast<U>( y ), static_cast<U>( z ) };
    }
    value_type operator[]( size_t index ) const
    {
        return reinterpret_cast<const value_type*>( this )[index];
    }

    value_type minimum() const noexcept
    {
        return std::min( { x, y, z } );
    }
    value_type maximum() const noexcept
    {
        return std::max( { x, y, z } );
    }
    value_type sum() const noexcept
    {
        return x + y + z;
    }
    value_type product() const noexcept
    {
        return x * y * z;
    }

    value_type dot( vec3 other ) const noexcept
    {
        return x * other.x + y * other.y + z * other.z;
    }
    vec3 cross( vec3 other ) const noexcept
    {
        return vec3 {
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        };
    }

    value_type length_squared() const noexcept
    {
        return x * x + y * y + z * z;
    }
    value_type length() const noexcept
    {
        return std::sqrt( this->length_squared() );
    }
    vec3 normalized() const noexcept
    {
        return *this / this->length();
    }

    vec3 operator+( vec3 other ) const noexcept
    {
        return vec3 { x + other.x, y + other.y, z + other.z };
    }
    vec3 operator-( vec3 other ) const noexcept
    {
        return vec3 { x - other.x, y - other.y, z - other.z };
    }
    vec3 operator*( value_type scalar ) const noexcept
    {
        return vec3 { x * scalar, y * scalar, z * scalar };
    }
    vec3 operator/( value_type scalar ) const noexcept
    {
        return vec3 { x / scalar, y / scalar, z / scalar };
    }

    friend vec3 operator*( value_type scalar, vec3 vector ) noexcept
    {
        return vector * scalar;
    }
    friend vec3 operator/( value_type scalar, vec3 vector ) noexcept
    {
        return vec3 { scalar / vector.x, scalar / vector.y, scalar / vector.z };
    }

    vec3& operator+=( vec3 other ) noexcept
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    vec3& operator-=( vec3 other ) noexcept
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
    vec3& operator*=( value_type scalar ) noexcept
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }
    vec3& operator/=( value_type scalar ) noexcept
    {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    bool operator==( vec3 other ) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }
    bool operator!=( vec3 other ) const noexcept
    {
        return x != other.x || y != other.y || z != other.z;
    }
};

template<class T> struct vec4
{
    using value_type = T;
    union
    {
        struct { value_type x, y, z, w; };
        struct { value_type r, g, b, a; };
    };

    vec4( value_type x = {}, value_type y = {}, value_type z = {}, value_type w = {} ) noexcept : x { x }, y { y }, z { z }, w { w }
    {
    }

    template<class U> vec4<U> cast() const
    {
        return vec4<U> { static_cast<U>( x ), static_cast<U>( y ), static_cast<U>( z ), static_cast<U>( w ) };
    }
    value_type operator[]( size_t index ) const
    {
        return reinterpret_cast<const value_type*>( this )[index];
    }

    QColor qcolor() const noexcept
    {
        return QColor { static_cast<int>( x * 255 ), static_cast<int>( y * 255 ), static_cast<int>( z * 255 ), static_cast<int>( w * 255 ) };
    }

    value_type minimum() const noexcept
    {
        return std::min( { x, y, z, w } );
    }
    value_type maximum() const noexcept
    {
        return std::max( { x, y, z, w } );
    }
    value_type sum() const noexcept
    {
        return x + y + z + w;
    }
    value_type product() const noexcept
    {
        return x * y * z * w;
    }

    value_type dot( vec4 other ) const noexcept
    {
        return x * other.x + y * other.y + z * other.z + w * other.w;
    }

    value_type length_squared() const noexcept
    {
        return x * x + y * y + z * z + w * w;
    }
    value_type length() const noexcept
    {
        return std::sqrt( this->length_squared() );
    }
    vec4 normalized() const noexcept
    {
        return *this / this->length();
    }

    vec4 operator+( vec4 other ) const noexcept
    {
        return vec4 { x + other.x, y + other.y, z + other.z, w + other.w };
    }
    vec4 operator-( vec4 other ) const noexcept
    {
        return vec4 { x - other.x, y - other.y, z - other.z, w - other.w };
    }
    vec4 operator*( value_type scalar ) const noexcept
    {
        return vec4 { x * scalar, y * scalar, z * scalar, w * scalar };
    }
    vec4 operator/( value_type scalar ) const noexcept
    {
        return vec4 { x / scalar, y / scalar, z / scalar, w / scalar };
    }

    friend vec4 operator*( value_type scalar, vec4 vector ) noexcept
    {
        return vector * scalar;
    }
    friend vec4 operator/( value_type scalar, vec4 vector ) noexcept
    {
        return vec4 { scalar / vector.x, scalar / vector.y, scalar / vector.z, scalar / vector.w };
    }

    vec4& operator+=( vec4 other ) noexcept
    {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
        return *this;
    }
    vec4& operator-=( vec4 other ) noexcept
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }
    vec4& operator*=( value_type scalar ) noexcept
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        w *= scalar;
        return *this;
    }
    vec4& operator/=( value_type scalar ) noexcept
    {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        w /= scalar;
        return *this;
    }

    bool operator==( vec4 other ) const noexcept
    {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }
    bool operator!=( vec4 other ) const noexcept
    {
        return x != other.x || y != other.y || z != other.z || w != other.w;
    }
};

// ----- Override ----- //

class OverrideObject : public QObject
{
    Q_OBJECT
signals:
    void value_changed();
};

template<class T> class Override : public OverrideObject
{
public:
    using value_type = T;

    Override( value_type automatic_value, std::optional<value_type> override_value ) noexcept : _automatic_value { automatic_value }, _override_value { override_value }
    {
    }

    const value_type& automatic_value() const noexcept
    {
        return _automatic_value;
    }
    void update_automatic_value( value_type automatic_value ) noexcept requires( concepts::NotEqualComparable<value_type> )
    {
        if( _automatic_value != automatic_value )
        {
            _automatic_value = automatic_value;

            if( !_override_value.has_value() )
            {
                emit value_changed();
            }
        }
    }
    void update_automatic_value( value_type automatic_value ) noexcept requires( !concepts::NotEqualComparable<value_type> )
    {
        _automatic_value = automatic_value;

        if( !_override_value.has_value() )
        {
            emit value_changed();
        }
    }

    const std::optional<value_type>& override_value() const noexcept
    {
        return _override_value;
    }
    void update_override_value( std::optional<value_type> override_value ) noexcept requires( concepts::NotEqualComparable<value_type> )
    {
        if( _override_value != override_value )
        {
            _override_value = override_value;
            emit value_changed();
        }
    }
    void update_override_value( std::optional<value_type> override_value ) noexcept requires( !concepts::NotEqualComparable<value_type> )
    {
        _override_value = override_value;
        emit value_changed();
    }

    const value_type& value() const noexcept
    {
        return _override_value.has_value() ? *_override_value : _automatic_value;
    }

private:
    value_type _automatic_value;
    std::optional<value_type> _override_value;
};

// ----- Computed ----- //

class ComputedObject : public QObject
{
    Q_OBJECT
public:
    virtual void invalidate() = 0;

signals:
    void changed() const;
};

template<class T> class Computed : public ComputedObject
{
public:
    using value_type = T;

    Computed() noexcept = default;
    Computed( std::function<value_type()> compute_function ) noexcept : _compute_function { std::move( compute_function ) }
    {
    }

    void initialize( std::function<value_type()> compute_function ) noexcept
    {
        if( _compute_function )
        {
            Console::warning( "Computed value already initialized, overwriting." );
        }
        _compute_function = std::move( compute_function );
    }

    bool present() const noexcept
    {
        return _value.has_value();
    }
    operator bool() const noexcept
    {
        return this->present();
    }

    const value_type& value() const
    {
        if( !_value.has_value() )
        {
            if( !_compute_function )
            {
                Console::critical( "Computed value requested without a compute function." );
            }

            _value = _compute_function();
        }
        return *_value;
    }
    const value_type& operator*() const
    {
        return this->value();
    }
    const value_type* operator->() const
    {
        return std::addressof( this->value() );
    }

    void invalidate()
    {
        _value.reset();
        emit ComputedObject::changed();
    }
    void write( const value_type& value )
    {
        _value = value;
        emit ComputedObject::changed();
    }
    void write( value_type&& value )
    {
        _value = std::move( value );
        emit ComputedObject::changed();
    }

private:
    mutable std::optional<value_type> _value;
    std::function<value_type()> _compute_function;
};

