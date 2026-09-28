#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/StlImport.hpp>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

using namespace Slic3r;
using namespace Slic3r::nptop;
namespace {
void append_u32(std::string &bytes, uint32_t value)
{
    for (int i=0; i<4; ++i) bytes += char((value>>(8*i))&255);
}
std::string binary_stl(const TriangleMesh &mesh)
{
    std::string bytes(80,' ');
    bytes.replace(0,5,"solid"); // A legal binary header is not an ASCII discriminator.
    append_u32(bytes,uint32_t(mesh.its.indices.size()));
    for (const auto &face : mesh.its.indices) {
        bytes.append(12,'\0'); // Normals are derived from geometry by native repair.
        for (int i=0; i<3; ++i)
            for (int axis=0; axis<3; ++axis) {
                const float value=mesh.its.vertices[face(i)](axis);
                uint32_t bits; std::memcpy(&bits,&value,sizeof(bits)); append_u32(bytes,bits);
            }
        bytes.append(2,'\0');
    }
    return bytes;
}
std::string ascii_stl(const TriangleMesh &mesh)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10) << "solid cube\n";
    for (const auto &face : mesh.its.indices) {
        out << "facet normal 0 0 0\nouter loop\n";
        for (int i=0; i<3; ++i) {
            const auto &v=mesh.its.vertices[face(i)];
            out << "vertex " << v.x() << ' ' << v.y() << ' ' << v.z() << '\n';
        }
        out << "endloop\nendfacet\n";
    }
    out << "endsolid cube\n";
    return out.str();
}
}
TEST_CASE("B02 ASCII and binary snapshots preserve bytes and native geometry", "[Nonplanar][B02][StlImport]")
{
    const auto cube=make_cube(20,10,2);
    const auto ascii=ascii_stl(cube), binary=binary_stl(cube);
    for (const auto &bytes : {ascii,binary}) {
        const auto result=import_stl_snapshot(bytes,true);
        INFO(result.geometry.reason);
        REQUIRE(result.geometry.status==MeshAuditStatus::ValidGeometry);
        REQUIRE(result.source->bytes==bytes);
        REQUIRE(result.source->sha256.size()==64);
        REQUIRE(result.source->millimeters_declared);
        REQUIRE(result.parsed->facets_count()==12);
        REQUIRE(result.native_repair.original_num_facets==12);
        REQUIRE(result.native_repair.facets_removed==0);
        REQUIRE(result.native_repair.facets_reversed==0);
        REQUIRE(result.native_geometry_unchanged);
        REQUIRE(result.source_error_upper_mm);
        REQUIRE(*result.source_error_upper_mm==0);
        REQUIRE(same_oriented_triangles(cube.its,result.parsed->its));
        REQUIRE(same_oriented_triangles(cube.its,result.geometry.normalized->its));
        REQUIRE(result.geometry.volume_lower_mm3<=400);
        REQUIRE(result.geometry.volume_upper_mm3>=400);
    }
    const auto invalid=import_stl_snapshot("abc",true);
    REQUIRE(invalid.source->sha256=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    REQUIRE(invalid.geometry.status==MeshAuditStatus::Unknown);
    REQUIRE_FALSE(invalid.geometry.normalized);
}
TEST_CASE("B02 caller mutation cannot replace the captured STL during callbacks", "[Nonplanar][B02][StlImport]")
{
    auto input=binary_stl(make_cube(20,10,2));
    const auto before=input;
    MeshAuditLimits limits;
    limits.cancelled=[&] { input="replaced during import"; return false; };
    const auto result=import_stl_snapshot(input,true,limits);
    REQUIRE(result.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(result.source->bytes==before);
    REQUIRE(input!=before);
    REQUIRE(result.native_geometry_unchanged);
}
TEST_CASE("B02 source limits units cancellation and malformed binary reject", "[Nonplanar][B02][StlImport]")
{
    const auto bytes=binary_stl(make_cube(20,10,2));
    auto rejected=[](const std::string &input, bool mm=true, const MeshAuditLimits &limits=MeshAuditLimits{}) {
        const auto result=import_stl_snapshot(input,mm,limits);
        REQUIRE(result.geometry.status!=MeshAuditStatus::ValidGeometry);
        REQUIRE_FALSE(result.geometry.normalized);
        REQUIRE_FALSE(result.native_geometry_unchanged);
    };
    auto changed=bytes; changed[80]=char(255); rejected(changed);
    changed=bytes; changed.pop_back(); rejected(changed);
    changed=bytes+"ignored trailing bytes"; rejected(changed);
    rejected(std::string(2*1024*1024+1,'x'));
    rejected(bytes,false);
    MeshAuditLimits limits; limits.max_faces=11; rejected(bytes,true,limits);
    limits={}; limits.max_faces=5001; rejected(bytes,true,limits);
    limits={}; limits.max_coordinate_mm=1; rejected(bytes,true,limits);
    limits={}; limits.cancelled=[] { return true; }; rejected(bytes,true,limits);
    limits={}; limits.cancelled=[]() -> bool { throw std::runtime_error("cancel callback failed"); }; rejected(bytes,true,limits);
    for (float bad : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        auto cube=make_cube(20,10,2); cube.its.vertices.front().x()=bad;
        rejected(binary_stl(cube));
    }
}
TEST_CASE("B02 strict ASCII records reject missing values and unconsumed source data", "[Nonplanar][B02][StlImport]")
{
    const auto bytes=ascii_stl(make_cube(20,10,2));
    std::vector<std::string> invalid{bytes+"other content\n",bytes+std::string(1,'\0'),bytes.substr(0,bytes.find("endsolid"))};
    for (const auto &value : {"vertex nan 0 0", "vertex inf 0 0", "vertex nope", "vertex 0 0", "vertex 0 0 0 ignored"}) {
        auto changed=bytes;
        const auto begin=changed.find("vertex"), end=changed.find('\n',begin);
        changed.replace(begin,end-begin,value);
        invalid.push_back(std::move(changed));
    }
    for (const auto &changed : invalid) {
        const auto result=import_stl_snapshot(changed,true);
        REQUIRE(result.geometry.status!=MeshAuditStatus::ValidGeometry);
        REQUIRE_FALSE(result.geometry.normalized);
        REQUIRE(result.source->bytes==changed);
    }
    auto crlf=bytes;
    size_t offset=0;
    while ((offset=crlf.find('\n',offset))!=std::string::npos) { crlf.insert(offset,1,'\r'); offset+=2; }
    REQUIRE(import_stl_snapshot(crlf,true).geometry.status==MeshAuditStatus::ValidGeometry);
}
TEST_CASE("B02 source topology is checked before native repair can hide it", "[Nonplanar][B02][StlImport]")
{
    auto reject=[](const TriangleMesh &mesh) {
        const auto bytes=binary_stl(mesh);
        const auto result=import_stl_snapshot(bytes,true);
        REQUIRE(result.geometry.status==MeshAuditStatus::Invalid);
        REQUIRE(result.parsed);
        REQUIRE(result.source->bytes==bytes);
        REQUIRE_FALSE(result.geometry.normalized);
        REQUIRE_FALSE(result.native_geometry_unchanged);
    };
    auto cube=make_cube(20,10,2); cube.flip_triangles(); reject(cube);
    cube=make_cube(20,10,2); cube.its.indices.pop_back(); reject(cube);
    cube=make_cube(20,10,2); cube.its.indices.push_back(cube.its.indices.front()); reject(cube);
}
TEST_CASE("B02 native repair comparison preserves oriented triangle multisets", "[Nonplanar][B02][StlImport]")
{
    const auto cube=make_cube(20,10,2);
    auto changed=cube.its;
    std::reverse(changed.indices.begin(),changed.indices.end());
    for (auto &face : changed.indices) face=Vec3i32(face(1),face(2),face(0));
    REQUIRE(same_oriented_triangles(cube.its,changed));
    std::swap(changed.indices.front()(0),changed.indices.front()(1));
    REQUIRE_FALSE(same_oriented_triangles(cube.its,changed));
    changed=cube.its; changed.vertices.front().x()+=0.0001f;
    REQUIRE_FALSE(same_oriented_triangles(cube.its,changed));
    changed=cube.its; changed.indices.pop_back();
    REQUIRE_FALSE(same_oriented_triangles(cube.its,changed));
}
TEST_CASE("B02 callback cannot relax captured geometry or import limits", "[Nonplanar][B02][StlImport][LimitSnapshot]")
{
    const auto cube=make_cube(20,10,2);
    MeshAuditLimits limits;
    limits.max_coordinate_mm=1;
    limits.cancelled=[&] { limits.max_coordinate_mm=10000; return false; };
    const auto audited=audit_mesh(cube,true,limits);
    limits.max_coordinate_mm=1;
    const auto imported=import_stl_snapshot(binary_stl(cube),true,limits);
    CHECK(audited.status==MeshAuditStatus::Unknown);
    CHECK(audited.reason=="COORDINATE_DOMAIN");
    CHECK_FALSE(audited.normalized);
    CHECK(imported.geometry.status==MeshAuditStatus::Unknown);
    CHECK(imported.geometry.reason=="COORDINATE_DOMAIN");
    CHECK_FALSE(imported.geometry.normalized);
}
TEST_CASE("B02 import reports actual decimal conversion error and rejects unbounded syntax", "[Nonplanar][B02][StlImport][StlError]")
{
    const auto original=ascii_stl(make_cube(20,10,2));
    auto decimal=original;
    size_t offset=0;
    while ((offset=decimal.find("vertex 0 ",offset))!=std::string::npos) {
        decimal.replace(offset,9,"vertex 0.1 "); offset+=11;
    }
    const auto imported=import_stl_snapshot(decimal,true);
    INFO(imported.geometry.reason);
    REQUIRE(imported.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(imported.source_error_upper_mm);
    REQUIRE(*imported.source_error_upper_mm==std::nextafter(std::ldexp(1.,-29),std::numeric_limits<double>::infinity()));
    REQUIRE(imported.native_geometry_unchanged);
    auto hex=original;
    offset=0;
    while ((offset=hex.find("vertex 0 ",offset))!=std::string::npos) {
        hex.replace(offset,9,"vertex 0x1p0 "); offset+=13;
    }
    const auto unsupported=import_stl_snapshot(hex,true);
    REQUIRE(unsupported.geometry.status==MeshAuditStatus::Unknown);
    REQUIRE(unsupported.geometry.reason=="SOURCE_ERROR_UNKNOWN");
    REQUIRE_FALSE(unsupported.source_error_upper_mm);
    REQUIRE_FALSE(unsupported.geometry.normalized);
}
