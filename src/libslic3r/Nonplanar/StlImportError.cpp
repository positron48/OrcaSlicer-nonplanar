#include "StlImportError.hpp"
#include "Interval.hpp"
#include <boost/multiprecision/cpp_int.hpp>
#include <cstring>
#include <locale>
#include <sstream>

namespace Slic3r::nptop::detail {
namespace {
using boost::multiprecision::cpp_int;
struct Rational { cpp_int numerator=0, denominator=1; };
void check(bool condition)
{
    if (!condition) throw std::invalid_argument("unsupported STL conversion error domain");
}
Rational decimal_value(std::string_view token)
{
    check(!token.empty() && token.size()<=80);
    size_t i=0;
    const bool negative=token[i]=='-';
    if (token[i]=='-' || token[i]=='+') ++i;
    Rational value;
    int digits=0, fractional=0, exponent=0;
    bool point=false;
    for (; i<token.size() && token[i]!='e' && token[i]!='E'; ++i) {
        const char c=token[i];
        if (c=='.' && !point) { point=true; continue; }
        check(c>='0' && c<='9' && ++digits<=64);
        value.numerator=value.numerator*10+(c-'0');
        if (point) ++fractional;
    }
    check(digits>0);
    if (i<token.size()) {
        ++i;
        bool minus=false;
        if (i<token.size() && (token[i]=='-' || token[i]=='+')) { minus=token[i]=='-'; ++i; }
        const auto start=i;
        for (; i<token.size(); ++i) {
            check(token[i]>='0' && token[i]<='9');
            exponent=exponent*10+(token[i]-'0');
            check(exponent<=100);
        }
        check(i>start);
        if (minus) exponent=-exponent;
    }
    exponent-=fractional;
    for (int j=0; j<std::abs(exponent); ++j)
        (exponent<0 ? value.denominator : value.numerator)*=10;
    if (negative) value.numerator=-value.numerator;
    return value;
}
Rational binary32_value(float parsed)
{
    static_assert(sizeof(float)==sizeof(uint32_t), "binary32 storage required");
    check(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits==24);
    uint32_t bits;
    std::memcpy(&bits,&parsed,sizeof(bits));
    const auto exponent=(bits>>23)&255;
    check(exponent!=255);
    Rational value;
    value.numerator=bits&0x7fffff;
    if (exponent) value.numerator+=1u<<23;
    const int power=exponent ? int(exponent)-150 : -149;
    if (power>=0) value.numerator<<=power;
    else value.denominator<<=-power;
    if (bits>>31) value.numerator=-value.numerator;
    return value;
}
}
double decimal_float_error_upper(std::string_view token, float parsed)
{
    const auto decimal=decimal_value(token), binary=binary32_value(parsed);
    cpp_int numerator=decimal.numerator*binary.denominator-binary.numerator*decimal.denominator;
    if (numerator<0) numerator=-numerator;
    if (numerator==0) return 0;
    const cpp_int denominator=decimal.denominator*binary.denominator;
    int exponent=int(boost::multiprecision::msb(numerator))-int(boost::multiprecision::msb(denominator));
    // Integer comparisons select ceil(log2(error)) without converting the exact
    // rational to floating point or assuming the native parser rounded correctly.
    if (exponent>=0 ? numerator>(denominator<<exponent) : (numerator<<-exponent)>denominator) ++exponent;
    check(exponent>=-1022 && exponent<=1023 && std::numeric_limits<double>::is_iec559);
    return std::ldexp(1.,exponent);
}
double stl_source_error_upper(std::string_view bytes, const stl_file &parsed,
                              const std::function<bool()> &cancelled)
{
    require_interval_environment();
    check(bytes.size()<=2*1024*1024 && parsed.facet_start.size()<=5000 && !parsed.facet_start.empty());
    if (cancelled && cancelled()) throw std::runtime_error("cancelled STL error bound");
    // The bounded native binary reader copies the actual IEEE binary32 values;
    // no decimal surface, unit scaling or tessellation is inferred from them.
    if (parsed.stats.type==binary) return 0;
    check(parsed.stats.type==ascii);
    size_t offset=0, vertex=0;
    double maximum=0;
    while (offset<bytes.size()) {
        if (cancelled && cancelled()) throw std::runtime_error("cancelled STL error bound");
        auto end=bytes.find('\n',offset);
        if (end==std::string_view::npos) end=bytes.size();
        const auto line=bytes.substr(offset,end-offset);
        offset=end==bytes.size() ? end : end+1;
        check(line.size()<=255);
        std::istringstream row{std::string(line)};
        row.imbue(std::locale::classic());
        std::string word;
        if (!(row>>word) || word!="vertex") continue;
        check(vertex<3*parsed.facet_start.size());
        const auto &point=parsed.facet_start[vertex/3].vertex[vertex%3];
        double error=0;
        for (int axis=0; axis<3; ++axis) {
            check(bool(row>>word));
            const double bound=decimal_float_error_upper(word,point(axis));
            if (bound>0) error=up(error+bound);
        }
        check(!(row>>word));
        maximum=std::max(maximum,error);
        ++vertex;
    }
    check(vertex==3*parsed.facet_start.size());
    return maximum;
}
}
