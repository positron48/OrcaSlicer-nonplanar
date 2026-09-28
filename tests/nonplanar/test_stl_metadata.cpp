#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <libslic3r/TriangleMesh.hpp>
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <array>
#include <algorithm>

using namespace Slic3r;
namespace {
struct StlFiles {
    const boost::filesystem::path directory = boost::filesystem::temp_directory_path()/boost::filesystem::unique_path("nptop-stl-%%%%-%%%%");
    std::string body;
    StlFiles() {
        boost::filesystem::create_directory(directory);
        const auto original=(directory/"original.stl").string();
        REQUIRE(make_cube(20,20,20).write_ascii(original.c_str()));
        boost::nowide::ifstream input(original);
        body.assign(std::istreambuf_iterator<char>(input),{});
        body.erase(0,body.find('\n')+1);
    }
    ~StlFiles() { boost::system::error_code ec; boost::filesystem::remove_all(directory,ec); }
    std::string with_header(const std::string &name, const std::string &header) const {
        const auto path=(directory/name).string();
        boost::nowide::ofstream output(path);
        output << "solid " << header << '\n' << body;
        return path;
    }
};
}
TEST_CASE("B02 binary STL metadata reads exactly the bounded header", "[Nonplanar][B02][stl]")
{
    std::array<char,80> bytes;
    bytes.fill(' ');
    const std::string prefix="MW 1.0 model US";
    std::copy(prefix.begin(),prefix.end(),bytes.begin());
    auto result=stl_parse_source_metadata(std::string_view(bytes.data(),bytes.size()));
    REQUIRE(result.model_id=="model"); REQUIRE(result.country_code=="US");
    bytes[prefix.size()]='\0';
    bytes.back()='X';
    result=stl_parse_source_metadata(std::string_view(bytes.data(),bytes.size()));
    REQUIRE(result.model_id=="model"); REQUIRE(result.country_code=="US");
    for (const auto &header : {std::string("MW"), std::string("MW 1.0 id"), std::string("MW 1.1 id US"),
                             std::string(256,'X'), std::string("MW 1.0 id ")+std::string(16,'C')}) {
        const auto invalid=stl_parse_source_metadata(header);
        REQUIRE(invalid.model_id.empty()); REQUIRE(invalid.country_code.empty());
    }
}
TEST_CASE("B02 native STL metadata bounds preserve geometry for long and malformed headers", "[Nonplanar][B02][stl]")
{
    StlFiles files;
    const std::vector<std::string> headers = {
        "ordinary", "MW", std::string(4096,'A'), std::string(253,'A')+"MW",
        "MW "+std::string(40,'v')+" id US", "MW 1.0 "+std::string(180,'i')+" US",
        "MW 1.0 id "+std::string(40,'c'), "MW 1.1 id US",
        "MW 1.0 id US"+std::string(243,' ')+"\r continuation"
    };
    for (const auto &header : headers) {
        INFO(header.size());
        const auto path=files.with_header("input.stl",header);
        TriangleMesh mesh;
        size_t callbacks=0;
        REQUIRE(mesh.ReadSTLFile(path.c_str(),true,[&](int,int,bool&,std::string &id,std::string &country) {
            ++callbacks; REQUIRE(id.empty()); REQUIRE(country.empty());
        }));
        REQUIRE(callbacks>0);
        REQUIRE(mesh.facets_count()==12);
        REQUIRE(mesh.volume()==Catch::Approx(8000));
        REQUIRE(mesh.size().x()==Catch::Approx(20));
        REQUIRE(mesh.size().y()==Catch::Approx(20));
        REQUIRE(mesh.size().z()==Catch::Approx(20));
    }
}
TEST_CASE("B02 nested native STL imports retain metadata belonging to each file", "[Nonplanar][B02][stl]")
{
    StlFiles files;
    const auto outer_path=files.with_header("outer.stl","MW 1.0 outer US\r");
    const auto inner_path=files.with_header("inner.stl","MW 1.0 inner ES");
    TriangleMesh outer;
    bool nested=false;
    size_t outer_callbacks=0, inner_callbacks=0;
    REQUIRE(outer.ReadSTLFile(outer_path.c_str(),true,[&](int,int,bool&,std::string &id,std::string &country) {
        ++outer_callbacks;
        REQUIRE(id=="outer"); REQUIRE(country=="US");
        if (!nested) {
            nested=true;
            TriangleMesh inner;
            REQUIRE(inner.ReadSTLFile(inner_path.c_str(),true,[&](int,int,bool&,std::string &nested_id,std::string &nested_country) {
                ++inner_callbacks; REQUIRE(nested_id=="inner"); REQUIRE(nested_country=="ES");
            }));
            REQUIRE(inner.volume()==Catch::Approx(8000));
            REQUIRE(id=="outer"); REQUIRE(country=="US");
        }
    }));
    REQUIRE(outer_callbacks>1);
    REQUIRE(inner_callbacks>0);
    REQUIRE(outer.volume()==Catch::Approx(8000));
}
