#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/StlImportError.hpp>
#include <cstring>
#include <cmath>
#include <limits>
#include <cfenv>
#include <boost/nowide/fstream.hpp>

using namespace Slic3r::nptop;
TEST_CASE("B02 decimal conversion bounds match independent exact rational oracles", "[Nonplanar][B02][StlError]")
{
    // Python Fraction(decimal) - Fraction(binary32) oracle, independent of the
    // production decimal parser and binary32 bit decomposition. Exponent 0 with
    // exact=true denotes zero error, otherwise the smallest enclosing 2^exponent.
    struct Oracle { const char *decimal; uint32_t bits; int exponent; bool exact=false; };
    const Oracle cases[]={
        {"0.1",0x3dcccccd,-29}, {"-0.1",0xbdcccccd,-29},
        {"0.5",0x3f000000,0,true}, {"-1.25e2",0xc2fa0000,0,true},
        {"1e-45",0x00000001,-150}, {"1e-100",0x00000000,-332},
        {"1.000000059604644775390625",0x3f800000,-24},
        {"10000.0001",0x461c4000,-13},
        {"0.1000000000000000000000000001",0x3dcccccd,-29},
        {"0.0000000001",0x2edbe6ff,-59},
        {"1.401298464324817e-45",0x00000001,-203}, {"-0",0x80000000,0,true}
    };
    for (const auto &item : cases) {
        float value; std::memcpy(&value,&item.bits,sizeof(value));
        INFO(item.decimal);
        REQUIRE(detail::decimal_float_error_upper(item.decimal,value)==(item.exact ? 0 : std::ldexp(1.,item.exponent)));
    }
    // 0.1f - 1/10 is exactly 1/671088640, enclosed by 2^-29.
    REQUIRE(detail::decimal_float_error_upper("0.1",0.1f)>=1./671088640);
}
TEST_CASE("B02 unbounded or unsupported numeric syntax cannot claim zero conversion error", "[Nonplanar][B02][StlError]")
{
    for (const auto &token : {std::string(""),std::string("."),std::string("+"),std::string("1e"),
         std::string("nan"),std::string("inf"),std::string("0x1p0"),std::string("1.0suffix"),
         std::string("1,5"),std::string("1e101"),std::string("1e-101"),std::string(65,'1')}) {
        INFO(token);
        REQUIRE_THROWS(detail::decimal_float_error_upper(token,1));
    }
    REQUIRE_THROWS(detail::decimal_float_error_upper("1",std::numeric_limits<float>::infinity()));
    REQUIRE_THROWS(detail::decimal_float_error_upper("1",std::numeric_limits<float>::quiet_NaN()));
}
TEST_CASE("B02 source error callbacks cannot bypass the declared arithmetic environment", "[Nonplanar][B02][NumericCallback]")
{
    boost::nowide::ifstream input(NPTOP_STL_FIXTURE_PATH,std::ios::binary);
    REQUIRE(input);
    const std::string binary(std::istreambuf_iterator<char>(input),{});
    const std::string ascii="solid triangle\nfacet normal 0 0 1\nouter loop\nvertex 0.1 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid triangle\n";
    for (const auto &bytes : {binary,ascii}) {
        stl_file parsed;
        REQUIRE(stl_open_from_memory(&parsed,bytes,5000));
        for (int mode : {FE_UPWARD,FE_DOWNWARD}) {
            struct Restore { int mode=std::fegetround(); ~Restore() { std::fesetround(mode); } } restore;
            int called=0;
            CHECK_THROWS(detail::stl_source_error_upper(bytes,parsed,[&] {
                ++called; std::fesetround(mode); return false;
            }));
            REQUIRE(called>0);
        }
    }
}
