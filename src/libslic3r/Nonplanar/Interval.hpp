#pragma once

#include <algorithm>
#include <array>
#include <cfenv>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Slic3r::nptop::detail {

// These bounds require IEEE binary64 without fast math or contraction. Keep
// each rounded operation separate; do not replace this with point estimates.
constexpr double infinity = std::numeric_limits<double>::infinity();
inline double down(double value) { return std::nextafter(value, -infinity); }
inline double up(double value) { return std::nextafter(value, infinity); }
struct Interval {
    double lo, hi;
    Interval(double a, double b) : lo(a), hi(b)
    {
        if (!std::isfinite(a) || !std::isfinite(b) || a > b)
            throw std::overflow_error("invalid interval arithmetic");
    }
    explicit Interval(double value) : Interval(value, value) {}
};
inline Interval operator+(Interval a, Interval b) { return {down(a.lo+b.lo), up(a.hi+b.hi)}; }
inline Interval operator-(Interval a, Interval b) { return {down(a.lo-b.hi), up(a.hi-b.lo)}; }
inline Interval operator*(Interval a, Interval b)
{
    const std::array<double,4> products{a.lo*b.lo, a.lo*b.hi, a.hi*b.lo, a.hi*b.hi};
    return {down(*std::min_element(products.begin(), products.end())),
            up(*std::max_element(products.begin(), products.end()))};
}
inline Interval operator/(Interval a, Interval b)
{
    if (b.lo <= 0) throw std::overflow_error("invalid interval arithmetic");
    return a * Interval(down(1/b.hi), up(1/b.lo));
}
inline Interval minimum(Interval a, Interval b) { return {std::min(a.lo,b.lo), std::min(a.hi,b.hi)}; }
inline Interval maximum(Interval a, Interval b) { return {std::max(a.lo,b.lo), std::max(a.hi,b.hi)}; }
inline Interval square(Interval a)
{
    const double low = a.lo <= 0 && a.hi >= 0 ? 0 : std::min(a.lo*a.lo, a.hi*a.hi);
    return {std::max(0., down(low)), up(std::max(a.lo*a.lo, a.hi*a.hi))};
}
inline Interval root(Interval a)
{
    if (a.hi < 0) throw std::overflow_error("invalid interval arithmetic");
    return {std::max(0., down(std::sqrt(std::max(0.,a.lo)))), up(std::sqrt(a.hi))};
}

inline void require_interval_environment()
{
    if (!std::numeric_limits<double>::is_iec559 || std::fegetround() != FE_TONEAREST)
        throw std::overflow_error("unsupported interval rounding");
    volatile double minimum_normal = std::numeric_limits<double>::min();
    volatile double minimum_subnormal = std::numeric_limits<double>::denorm_min();
    if (minimum_normal/2 == 0 || minimum_subnormal+minimum_subnormal == 0)
        throw std::overflow_error("unsupported interval underflow");
}

} // namespace Slic3r::nptop::detail
