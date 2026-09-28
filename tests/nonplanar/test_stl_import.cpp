#include <libslic3r/libslic3r.h>
#define NANOSVG_IMPLEMENTATION
#include "nanosvg/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg/nanosvgrast.h"
// Model integration pulls native SVG consumers into this test executable;
// provide the same implementation units as the upstream FFF test target.
#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/StlImport.hpp>
#include <libslic3r/Nonplanar/MeshPlacement.hpp>
#include <libslic3r/Model.hpp>
#include <libslic3r/Format/STL.hpp>
#include <boost/nowide/fstream.hpp>
#include <cfenv>
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
TEST_CASE("B02 centered native volume retains source provenance and independent rounding allowance", "[Nonplanar][B02][Centering]")
{
    auto cube=make_cube(20,10,2); cube.translate(0.1f,0.1f,0.1f);
    const auto source=import_stl_snapshot(binary_stl(cube),true);
    REQUIRE(source.geometry.status==MeshAuditStatus::ValidGeometry);
    Model model;
    auto *object=model.add_object("source","",cube);
    auto *volume=object->volumes.front();
    const auto before=volume->mesh().its;
    const auto offset=volume->source.mesh_offset;
    const auto result=capture_centered_volume(source,*volume,41);
    INFO(result.geometry.reason);
    REQUIRE(result.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(result.snapshot);
    REQUIRE(result.snapshot->revision==41);
    REQUIRE(result.snapshot->source==source.source);
    REQUIRE(result.snapshot->source_offset==offset);
    REQUIRE(result.snapshot->volume_local.get()!=&volume->mesh());
    REQUIRE(result.centering_error_upper_mm);
    // Independent Python Fraction oracle for all eight corners, retained in
    // B02-centering evidence. Source is binary STL, so import error is zero.
    constexpr double expected=147./268435456.;
    REQUIRE(*result.centering_error_upper_mm>=expected);
    REQUIRE(*result.centering_error_upper_mm<expected+1e-12);
    REQUIRE(*result.total_error_upper_mm>=expected);
    REQUIRE(*source.source_error_upper_mm==0);
    REQUIRE(result.snapshot->source_error_upper_mm==0);
    REQUIRE(volume->mesh().its.vertices==before.vertices);
    REQUIRE(volume->mesh().its.indices==before.indices);
    REQUIRE(volume->source.mesh_offset==offset);
    MeshPlacementLimits limits; limits.max_error_upper_mm=expected/2;
    const auto rejected=capture_centered_volume(source,*volume,41,limits);
    REQUIRE(rejected.geometry.status!=MeshAuditStatus::ValidGeometry);
    REQUIRE_FALSE(rejected.geometry.normalized);
    REQUIRE_FALSE(rejected.total_error_upper_mm);
    auto conservative=source; conservative.source_error_upper_mm=0.0001;
    const auto propagated=capture_centered_volume(conservative,*volume,41);
    REQUIRE(propagated.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(propagated.snapshot->source_error_upper_mm==0.0001);
    REQUIRE(*propagated.total_error_upper_mm>=0.0001+expected);
    REQUIRE(*propagated.total_error_upper_mm<0.0001+expected+1e-12);
}
TEST_CASE("B02 actual native STL to ModelVolume path matches the captured original source", "[Nonplanar][B02][Centering]")
{
    boost::nowide::ifstream input(NPTOP_STL_FIXTURE_PATH,std::ios::binary);
    REQUIRE(input);
    const std::string bytes(std::istreambuf_iterator<char>(input),{});
    auto source=import_stl_snapshot(bytes,true);
    REQUIRE(source.geometry.status==MeshAuditStatus::ValidGeometry);
    Model model;
    REQUIRE(load_stl(NPTOP_STL_FIXTURE_PATH,&model));
    REQUIRE(model.objects.size()==1);
    REQUIRE(model.objects.front()->volumes.size()==1);
    const auto *volume=model.objects.front()->volumes.front();
    const auto result=capture_centered_volume(source,*volume,1);
    REQUIRE(result.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(result.snapshot->source->bytes==bytes);
    REQUIRE(same_oriented_triangles(result.geometry.normalized->its,volume->mesh().its));
    REQUIRE(*result.total_error_upper_mm<1e-12);
    REQUIRE(capture_centered_volume(source,*volume,0).geometry.status==MeshAuditStatus::Unknown);
    source.source_error_upper_mm.reset();
    REQUIRE(capture_centered_volume(source,*volume,1).geometry.reason=="MISSING_IMPORT_PROVENANCE");
}
TEST_CASE("B02 model placement reproduces native volume then instance geometry in an explicit plate frame", "[Nonplanar][B02][ModelPlacement]")
{
    const auto cube=make_cube(20,10,2);
    const auto source=import_stl_snapshot(binary_stl(cube),true);
    Model model; auto *object=model.add_object("source","",cube);
    object->volumes.front()->scale(Vec3d(2,1,1));
    auto *instance=object->add_instance();
    instance->set_offset(Vec3d(100,80,3));
    instance->set_rotation(Vec3d(0,0,0.3));
    instance->set_scaling_factor(Vec3d(2,3,1));
    const PlateFrame plate{7,Vec3d(50,40,0)};
    auto expected=model.mesh();
    Transform3d world_to_plate=Transform3d::Identity(); world_to_plate.translation()=-plate.world_origin_mm;
    expected.transform(world_to_plate,false);
    const auto result=capture_model_placement(source,model,plate,21);
    INFO(result.geometry.reason);
    REQUIRE(result.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(result.snapshot);
    REQUIRE(result.snapshot->centered->revision==21);
    REQUIRE(result.snapshot->plate.index==7);
    REQUIRE(result.snapshot->volume_to_object.matrix()==object->volumes.front()->get_matrix().matrix());
    REQUIRE(result.snapshot->instance_to_world.matrix()==instance->get_matrix().matrix());
    REQUIRE(same_oriented_triangles(result.geometry.normalized->its,expected.its));
    REQUIRE(result.total_error_upper_mm);
    REQUIRE(*result.total_error_upper_mm<1e-4);
    REQUIRE(result.native_transform_errors_upper_mm);
    REQUIRE(result.centering_error_upper_mm);
}
TEST_CASE("B02 model placement propagates prior error through every native stage", "[Nonplanar][B02][ModelPlacement]")
{
    const auto cube=make_cube(20,10,2);
    auto source=import_stl_snapshot(binary_stl(cube),true);
    source.source_error_upper_mm=0.0001;
    Model model; auto *object=model.add_object("source","",cube);
    object->volumes.front()->scale(2);
    auto *instance=object->add_instance(); instance->set_scaling_factor(Vec3d(3,3,3));
    const auto result=capture_model_placement(source,model,PlateFrame{},1);
    INFO(result.geometry.reason);
    REQUIRE(result.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(*result.total_error_upper_mm>=0.0006);
    REQUIRE(*result.total_error_upper_mm<0.00060000001);
    MeshPlacementLimits limits; limits.max_error_upper_mm=0.0003;
    const auto refused=capture_model_placement(source,model,PlateFrame{},1,limits);
    REQUIRE(refused.geometry.status!=MeshAuditStatus::ValidGeometry);
    REQUIRE_FALSE(refused.geometry.normalized);
    REQUIRE_FALSE(refused.total_error_upper_mm);
}
TEST_CASE("B02 model placement rejects ambiguous selection and unsupported native transforms", "[Nonplanar][B02][ModelPlacement]")
{
    const auto cube=make_cube(20,10,2);
    const auto source=import_stl_snapshot(binary_stl(cube),true);
    for (int mutation=0; mutation<8; ++mutation) {
        INFO(mutation);
        Model model; auto *object=model.add_object("source","",cube);
        auto *instance=object->add_instance(); PlateFrame plate;
        if (mutation==0) model.add_object("other","",cube);
        if (mutation==1) object->add_volume(cube);
        if (mutation==2) object->add_instance();
        if (mutation==3) object->clear_instances();
        if (mutation==4) instance->printable=false;
        if (mutation==5) instance->set_scaling_factor(Vec3d(0,1,1));
        if (mutation==6) object->volumes.front()->mirror(X);
        if (mutation==7) plate.world_origin_mm.x()=std::numeric_limits<double>::quiet_NaN();
        const auto result=capture_model_placement(source,model,plate,1);
        REQUIRE(result.geometry.status!=MeshAuditStatus::ValidGeometry);
        REQUIRE_FALSE(result.geometry.normalized);
        REQUIRE_FALSE(result.total_error_upper_mm);
    }
}
TEST_CASE("B02 model placement freezes matrices frame and source before callbacks", "[Nonplanar][B02][ModelPlacement]")
{
    const auto cube=make_cube(20,10,2);
    auto source=import_stl_snapshot(binary_stl(cube),true);
    Model model; auto *object=model.add_object("source","",cube); object->add_instance();
    const auto expected=model.mesh(); PlateFrame plate;
    MeshPlacementLimits limits;
    limits.geometry.cancelled=[&] {
        model.clear_objects(); source.geometry={}; plate.world_origin_mm=Vec3d(999,999,999);
        limits.max_error_upper_mm=-1; return false;
    };
    const auto result=capture_model_placement(source,model,plate,11,limits);
    INFO(result.geometry.reason);
    REQUIRE(result.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(result.snapshot->plate.world_origin_mm==Vec3d::Zero());
    REQUIRE(same_oriented_triangles(result.geometry.normalized->its,expected.its));
    Model stable; stable.add_object("source","",cube)->add_instance();
    source=import_stl_snapshot(binary_stl(cube),true); plate={}; limits={};
    int polls=0; limits.is_current=[&](uint64_t) { return ++polls<40; };
    const auto stale=capture_model_placement(source,stable,plate,11,limits);
    REQUIRE(stale.geometry.status!=MeshAuditStatus::ValidGeometry);
    REQUIRE_FALSE(stale.geometry.normalized);
    REQUIRE_FALSE(stale.total_error_upper_mm);
}
TEST_CASE("B02 centered source capture rejects altered geometry offsets units and volume roles", "[Nonplanar][B02][Centering]")
{
    const auto cube=make_cube(20,10,2);
    const auto source=import_stl_snapshot(binary_stl(cube),true);
    REQUIRE(source.geometry.status==MeshAuditStatus::ValidGeometry);
    for (int mutation=0; mutation<7; ++mutation) {
        INFO(mutation);
        Model model; auto *volume=model.add_object("source","",cube)->volumes.front();
        if (mutation==0) { auto changed=volume->mesh(); changed.translate(0.1f,0,0); volume->set_mesh(std::move(changed)); }
        if (mutation==1) volume->source.mesh_offset.x()+=0.1;
        if (mutation==2) volume->source.is_converted_from_inches=true;
        if (mutation==3) volume->source.is_converted_from_meters=true;
        if (mutation==4) volume->set_type(ModelVolumeType::PARAMETER_MODIFIER);
        if (mutation==5) volume->source.transform.set_offset(Vec3d(1,0,0));
        if (mutation==6) { auto changed=volume->mesh(); changed.its.indices.front()(0)=-1; volume->set_mesh(std::move(changed)); }
        const auto result=capture_centered_volume(source,*volume,1);
        REQUIRE(result.geometry.status!=MeshAuditStatus::ValidGeometry);
        REQUIRE_FALSE(result.geometry.normalized);
        REQUIRE_FALSE(result.total_error_upper_mm);
    }
}
TEST_CASE("B02 centered volume is frozen before callbacks and stale results cannot accept geometry", "[Nonplanar][B02][Centering]")
{
    const auto cube=make_cube(20,10,2);
    auto source=import_stl_snapshot(binary_stl(cube),true);
    Model model; auto *volume=model.add_object("source","",cube)->volumes.front();
    const auto original=volume->mesh().its;
    MeshPlacementLimits limits;
    limits.geometry.cancelled=[&] {
        volume->reset_mesh(); volume->source.mesh_offset=Vec3d(999,999,999);
        source.geometry={}; limits.max_error_upper_mm=-1; return false;
    };
    const auto frozen=capture_centered_volume(source,*volume,9,limits);
    INFO(frozen.geometry.reason);
    REQUIRE(frozen.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(same_oriented_triangles(frozen.snapshot->volume_local->its,original));
    source=import_stl_snapshot(binary_stl(cube),true);
    Model stable; volume=stable.add_object("source","",cube)->volumes.front();
    for (int mode=0; mode<4; ++mode) {
        struct Restore { int mode=std::fegetround(); ~Restore() { std::fesetround(mode); } } restore;
        limits={}; int polls=0;
        if (mode==0) limits.geometry.cancelled=[] { return true; };
        if (mode==1) limits.is_current=[&](uint64_t) { return ++polls<3; };
        if (mode==2) limits.geometry.cancelled=[] { std::fesetround(FE_UPWARD); return false; };
        if (mode==3) limits.geometry.timeout=std::chrono::milliseconds(0);
        const auto rejected=capture_centered_volume(source,*volume,9,limits);
        REQUIRE(rejected.geometry.status!=MeshAuditStatus::ValidGeometry);
        REQUIRE_FALSE(rejected.geometry.normalized);
        REQUIRE_FALSE(rejected.total_error_upper_mm);
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
