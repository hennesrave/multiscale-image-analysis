#pragma once
#include "configuration.hpp"
#include "console.hpp"
#include "tensor.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <execution>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <thread>

#include <qcolor.h>
#include <qstring.h>
#include <qobject.h>

namespace utility
{
    double degrees_to_radians( double degrees ) noexcept;
    double radians_to_degrees( double radians ) noexcept;

    int stepsize_to_precision( double stepsize );
    int compute_precision( double value );

    template<class IndexType> void parallel_for( IndexType start, IndexType end, auto&& callable )
    {
        const auto range = std::views::iota( start, end );
        std::for_each( std::execution::seq, range.begin(), range.end(), std::forward<decltype( callable )>( callable ) );
    }
}

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
    void update_automatic_value( value_type automatic_value ) noexcept
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

    const std::optional<value_type>& override_value() const noexcept
    {
        return _override_value;
    }
    void update_override_value( std::optional<value_type> override_value ) noexcept
    {
        if( _override_value != override_value )
        {
            _override_value = override_value;
            emit value_changed();
        }
    }

    const value_type& value() const noexcept
    {
        return _override_value.has_value() ? *_override_value : _automatic_value;
    }

private:
    value_type _automatic_value;
    std::optional<value_type> _override_value;
};

// ----- Promise ----- //

class PromiseObject : public QObject
{
    Q_OBJECT
signals:
    void invalidated();
    void finished();
};

template<class T> class Promise : public PromiseObject
{
public:
    using value_type = T;

    Promise() noexcept = default;
    Promise( const QString& identifier, auto&& compute_function )
    {
        this->initialize( identifier, std::forward<decltype( compute_function )>( compute_function ) );
    }

    Promise( const Promise& ) = delete;
    Promise( Promise&& ) = delete;

    Promise& operator=( const Promise& ) = delete;
    Promise& operator=( Promise&& ) = delete;

    ~Promise()
    {
        if( _compute_thread.joinable() )
        {
            {
                auto flags_lock = std::unique_lock<std::mutex> { _flags_mutex };
                _terminate = true;
            }
            _flags_condition_variable.notify_one();
            _compute_thread.join();
        }
    }

    void initialize( const QString& identifier, auto&& compute_function )
    {
        if( _compute_thread.joinable() )
        {
            throw;
        }

        this->setObjectName( identifier );
        _compute_thread = std::thread { [this, compute_function = std::forward<decltype( compute_function )>( compute_function )]
        {
            auto flags_lock = std::unique_lock<std::mutex> { _flags_mutex };

            while( true )
            {
                Console::info( std::format( "Promise going to sleep: {}", this->objectName().toStdString() ) );
                _flags_condition_variable.wait( flags_lock, [this] { return _execute || _terminate; } );
                Console::info( std::format( "Promise waking up: {}", this->objectName().toStdString() ) );

                if( _terminate )
                {
                    break;
                }
                _invalidated = false;

                flags_lock.unlock();
                {
                    Console::info( std::format( "Promise acquiring write lock: {}", this->objectName().toStdString() ) );
                    auto value_lock = std::unique_lock<std::shared_mutex> { _value_mutex };
                    Console::info( std::format( "Promise starting computation: {}", this->objectName().toStdString() ) );
                    compute_function( _value );
                    Console::info( std::format( "Promise finished computation: {}", this->objectName().toStdString() ) );
                }
                flags_lock.lock();
                Console::info( std::format( "Promise flags: {}, {}, {}", _terminate, _invalidated, _finished ) );

                if( _terminate )
                {
                    break;
                }
                else if( _invalidated )
                {
                    _invalidated = false;
                    _execute = true;
                }
                else
                {
                    _execute = false;
                    _finished = true;

                    flags_lock.unlock();

                    Console::info( std::format( "Promise emitting finished signal: {}", this->objectName().toStdString() ) );
                    emit finished();
                    _finished_condition_variable.notify_all();
                    flags_lock.lock();
                }
            }
        } };
    }

    std::pair<const value_type&, std::shared_lock<std::shared_mutex>> await_value() const
    {
        Console::info( std::format( "Awaiting value for promise: {}", this->objectName().toStdString() ) );
        auto flags_lock = std::unique_lock<std::mutex> { _flags_mutex };
        if( !_finished )
        {
            _execute = true;
            flags_lock.unlock();
            _flags_condition_variable.notify_one();

            flags_lock.lock();
            _finished_condition_variable.wait( flags_lock, [this] { return _finished; } );
        }

        auto value_lock = std::shared_lock<std::shared_mutex> { _value_mutex };
        return std::pair<const value_type&, std::shared_lock<std::shared_mutex>> { _value, std::move( value_lock ) };
    }
    std::pair<const value_type*, std::shared_lock<std::shared_mutex>> request_value() const
    {
        Console::info( std::format( "Requesting value for promise: {}", this->objectName().toStdString() ) );
        auto flags_lock = std::unique_lock<std::mutex> { _flags_mutex };
        if( _finished )
        {
            auto value_lock = std::shared_lock<std::shared_mutex> { _value_mutex };
            return std::pair<const value_type*, std::shared_lock<std::shared_mutex>> { std::addressof( _value ), std::move( value_lock ) };
        }
        else
        {
            _execute = true;
            flags_lock.unlock();
            _flags_condition_variable.notify_one();
            Console::info( std::format( "Requesting computation for promise: {}", this->objectName().toStdString() ) );

            return std::pair<const value_type*, std::shared_lock<std::shared_mutex>> { nullptr, std::shared_lock<std::shared_mutex> {} };
        }
    }
    value_type value() const
    {
        const auto [value, lock] = this->await_value();
        return value;
    }

    void subscribe( const QObject* context, const auto& callable ) const
    {
        QObject::connect( this, &Promise::finished, context, callable, Qt::QueuedConnection );

        auto flags_lock = std::unique_lock<std::mutex> { _flags_mutex };
        if( _finished )
        {
            flags_lock.unlock();
            callable();
        }
        else
        {
            _execute = true;
            flags_lock.unlock();
            _flags_condition_variable.notify_one();
        }
    }
    void unsubscribe( const QObject* context ) const
    {
        QObject::disconnect( this, &Promise::finished, context, nullptr );
    }

    void invalidate()
    {
        Console::info( std::format( "Invalidating promise: {}", this->objectName().toStdString() ) );
        {
            auto flags_lock = std::unique_lock<std::mutex> { _flags_mutex };
            if( !_invalidated )
            {
                _finished = false;
                _invalidated = true;
                _execute = true;

                flags_lock.unlock();
                Console::info( std::format( "Invalidated promise: {}", this->objectName().toStdString() ) );
                emit invalidated();
            }
        }

        _flags_condition_variable.notify_one();
    }

private:
    value_type _value;
    mutable std::shared_mutex _value_mutex;

    mutable bool _execute = false;
    bool _invalidated = false;
    bool _terminate = false;
    mutable std::mutex _flags_mutex;
    mutable std::condition_variable _flags_condition_variable;

    bool _finished = false;
    mutable std::condition_variable_any _finished_condition_variable;

    std::thread _compute_thread;
};