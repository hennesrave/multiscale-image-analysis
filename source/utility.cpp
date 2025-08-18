#include "utility.hpp"

#include <numbers>
#include <limits>

namespace utility
{
    double degrees_to_radians( double degrees ) noexcept
    {
        return degrees * ( std::numbers::pi / 180.0 );
    }
    double radians_to_degrees( double radians ) noexcept
    {
        return radians * ( 180.0 / std::numbers::pi );
    }

    int stepsize_to_precision( double stepsize )
    {
        return std::max( 0, static_cast<int>( std::ceil( -std::log10( stepsize ) ) ) );
    }
    int compute_precision( double value )
    {
        constexpr auto tolerance = 100.0 * std::numeric_limits<double>::epsilon();
        value = std::abs( value );

        auto precision = 0;
        auto fractional = value - std::floor( value );
        while( fractional > tolerance && precision < 17 )
        {
            value *= 10.0;
            fractional = value - std::floor( value );
            ++precision;
        }
        return precision;
    }
}

// ----- Timer ----- //

double Timer::milliseconds() const
{
    const auto current = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>( current - _start ).count() / 1000.0;
}
double Timer::microseconds() const
{
    const auto current = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>( current - _start ).count() / 1000.0;
}
void Timer::reset()
{
    _start = std::chrono::high_resolution_clock::now();
}
