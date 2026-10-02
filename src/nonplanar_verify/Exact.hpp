#pragma once
// Independent exact arithmetic, shared only by final-byte verifier components.
#include "LinearRates.hpp"
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <cstring>
#include <limits>
namespace nptop_verify::exact {
using Int=boost::multiprecision::cpp_int;
using Q=boost::multiprecision::number<boost::multiprecision::cpp_rational_backend,boost::multiprecision::et_off>;
using Wide=boost::multiprecision::cpp_bin_float_quad;
inline void numeric_error(const char *message){throw std::runtime_error(message);}
inline Q square(const Q &q) {return q*q;}
inline Q binary(double value)
{
    if(!std::isfinite(value)) numeric_error("NONFINITE_RATE_INPUT");
    uint64_t bits;std::memcpy(&bits,&value,sizeof bits);
    const unsigned exponent=unsigned((bits>>52)&2047);
    Int numerator=bits&((uint64_t(1)<<52)-1);
    if(exponent)numerator+=uint64_t(1)<<52;
    const int shift=exponent ? int(exponent)-1075 : -1074;
    Q result=shift>=0 ? Q(numerator<<shift) : Q(numerator,Int(1)<<(-shift));
    return bits>>63 ? -result : result;
}
inline Q decimal(const std::string &word)
{
    size_t i=0;bool negative=false,dot=false;unsigned digits=0;
    if(word[i]=='-' || word[i]=='+'){negative=word[i]=='-';++i;}
    Int number=0,scale=1;
    for(;i<word.size();++i) {
        const char c=word[i];if(c=='.'){dot=true;continue;}
        number=number*10+(c-'0');
        if(dot){if(++digits>9)numeric_error("UNSUPPORTED_DECIMAL_PRECISION");scale*=10;}
    }
    return Q(negative ? -number : number,scale);
}
struct Range {Q lo,hi;};
inline Range pi_bounds()
{
    // Alternating atan series encloses each value. Machin's identity follows
    // from tan(4*atan(1/5)-atan(1/239))=1 and its angle in (0,pi/2).
    const auto atan_bounds=[](unsigned denominator) {
        const Q x(1,Int(denominator)),x2=x*x;Q power=x,sum=0;
        for(unsigned i=0;i<40;++i){const Q term=power/(2*i+1);sum+=i%2 ? -term : term;power*=x2;}
        return Range{sum,sum+power/81};
    };
    const auto a=atan_bounds(5),b=atan_bounds(239);
    const Q lo=16*a.lo-4*b.hi,hi=16*a.hi-4*b.lo;
    // Retain the exact enclosure while keeping per-event rationals small.
    Int scale=1;for(unsigned i=0;i<24;++i)scale*=10;
    const Q l=lo*scale,h=hi*scale;
    const Int lower=numerator(l)/denominator(l),upper=numerator(h)/denominator(h)+1;
    return {Q(lower,scale),Q(upper,scale)};
}
// A nonnegative exact value's nearest-double estimate is only a seed. Exact
// rational comparisons bracket it; floating conversion/sqrt is not the oracle.
inline RateBounds enclose(const Q &value,bool root,const std::function<void()> &work)
{
    if(value<0)numeric_error("INVALID_NONNEGATIVE_RATE_BOUND");
    if(value==0)return {0,0};
    double seed=root ? sqrt(Wide(numerator(value))/Wide(denominator(value))).convert_to<double>() : value.convert_to<double>();
    if(!std::isfinite(seed) || seed<0)numeric_error("RATE_BOUND_OUTSIDE_BINARY64");
    const auto exact=[&](double d) {const Q q=binary(d);return root ? square(q) : q;};
    while(exact(seed)>value){work();seed=std::nextafter(seed,0.);}
    for(;;) {
        const double next=std::nextafter(seed,std::numeric_limits<double>::infinity());
        if(!std::isfinite(next) || exact(next)>value)break;
        work();seed=next;
    }
    const double upper=exact(seed)==value ? seed : std::nextafter(seed,std::numeric_limits<double>::infinity());
    if(!std::isfinite(upper))numeric_error("RATE_BOUND_OUTSIDE_BINARY64");
    return {seed,upper};
}
}
