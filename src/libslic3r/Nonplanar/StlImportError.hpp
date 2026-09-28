#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <admesh/stl.h>

namespace Slic3r::nptop::detail {
// Exact decimal/binary32 difference, rounded outward to a binary64 power of two.
// Finite binary32 only; decimal token <=80 bytes, <=64 digits, exponent +/-100.
// Unsupported syntax/domain throws; it never yields a fabricated zero bound.
double decimal_float_error_upper(std::string_view decimal, float parsed);
// Maximum vertex L1 displacement bounds Euclidean displacement of corresponding
// points on every source triangle. Input is the bounded, unrepaired native read.
double stl_source_error_upper(std::string_view bytes, const stl_file &parsed,
                              const std::function<bool()> &cancelled);
}
