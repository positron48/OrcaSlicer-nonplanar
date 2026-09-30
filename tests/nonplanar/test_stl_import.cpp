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
#include <libslic3r/Nonplanar/InputSnapshot.hpp>
#include <libslic3r/Nonplanar/VolumePartition.hpp>
#include <libslic3r/Nonplanar/UpperProjection.hpp>
#include <libslic3r/Model.hpp>
#include <libslic3r/Triangulation.hpp>
#include <libslic3r/Format/STL.hpp>
#include <boost/nowide/fstream.hpp>
#include <cfenv>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <boost/filesystem.hpp>
#include <nlohmann/json.hpp>
#include <thread>

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
TEST_CASE("B03 native STL Model placement feeds a subdivided nominal upper projection", "[Nonplanar][B03][UpperProjection]")
{
    boost::nowide::ifstream input(NPTOP_STL_FIXTURE_PATH,std::ios::binary);
    REQUIRE(input);
    const std::string bytes(std::istreambuf_iterator<char>(input),{});
    const auto imported=import_stl_snapshot(bytes,true);
    REQUIRE(imported.geometry.status==MeshAuditStatus::ValidGeometry);
    Model model; REQUIRE(load_stl(NPTOP_STL_FIXTURE_PATH,&model));
    auto *object=model.objects.front();
    REQUIRE(object->volumes.front()->source.mesh_offset.z()==2);
    REQUIRE(object->volumes.front()->get_offset().z()==2);
    object->volumes.front()->scale(Vec3d(2,1,1));
    object->add_instance()->set_offset(Vec3d(60,70,6));
    const auto placed=capture_model_placement(imported,model,PlateFrame{2,Vec3d(20,30,0)},73);
    INFO(placed.geometry.reason);
    REQUIRE(placed.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(placed.total_error_upper_mm);
    const auto projection=analyze_upper_projection(*placed.geometry.normalized,true,73);
    INFO(projection.reason);
    REQUIRE(projection.status==UpperProjectionStatus::NominalHeightfield);
    REQUIRE(projection.snapshot);
    const auto &s=*projection.snapshot;
    REQUIRE(s.revision==placed.snapshot->centered->revision);
    REQUIRE(s.upward_facets.size()==32);
    REQUIRE(s.xy_area_lower_mm2<=1200);
    REQUIRE(s.xy_area_upper_mm2>=1200);
    REQUIRE(s.xy_area_upper_mm2-s.xy_area_lower_mm2<1e-8);
    REQUIRE(s.filtered_area_lower_mm2>1199.999999);
    // Native volume placement restores the centering shift. Original top 4
    // plus instance translation 6 is 10, not centered-local top 2 plus 6.
    REQUIRE(s.minimum_z_mm==10);
    REQUIRE(s.maximum_z_mm==10);
    REQUIRE(same_oriented_triangles(s.geometry->its,placed.geometry.normalized->its));
    REQUIRE(imported.source->bytes==bytes);
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

TEST_CASE("B02 owned native inputs reproduce original source placement after model destruction", "[Nonplanar][B02][InputPlacement]")
{
    const auto cube=make_cube(20,10,2); const auto source=import_stl_snapshot(binary_stl(cube),true);
    REQUIRE(source.geometry.status==MeshAuditStatus::ValidGeometry);
    Model model; auto *object=model.add_object("source","",cube); model.curr_plate_index=7;
    object->volumes.front()->scale(Vec3d(2,1,1));
    auto *instance=object->add_instance(); instance->set_offset(Vec3d(100,80,3));
    instance->set_rotation(Vec3d(0,0,0.3)); instance->set_scaling_factor(Vec3d(2,3,1));
    const PlateFrame plate{7,Vec3d(50,40,0)};
    auto expected=model.mesh(); Transform3d world_to_plate=Transform3d::Identity(); world_to_plate.translation()=-plate.world_origin_mm;
    expected.transform(world_to_plate,false);
    DynamicConfig config; config.set_key_value("nptop_mode",new ConfigOptionString("safe_hybrid"));
    NativeInputBinding binding{42,capture_native_input(model,config)}; REQUIRE(binding.snapshot);
    const auto owned=binding.snapshot; const auto identity=owned->fingerprint;
    MeshPlacementLimits limits; limits.geometry.cancelled=[&] { binding.snapshot.reset(); model.clear_objects(); return false; };
    const auto placed=capture_input_placement(source,binding,plate,limits);
    INFO(placed.geometry.reason); REQUIRE(placed.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(placed.snapshot); REQUIRE(placed.snapshot->native_input==owned);
    REQUIRE(placed.snapshot->native_input->fingerprint==identity);
    REQUIRE(placed.snapshot->centered->revision==42); REQUIRE(placed.snapshot->centered->source==source.source);
    REQUIRE(same_oriented_triangles(placed.geometry.normalized->its,expected.its));
    REQUIRE(placed.total_error_upper_mm); REQUIRE(*placed.total_error_upper_mm<1e-4);
    REQUIRE(placed.native_transform_errors_upper_mm); REQUIRE(placed.centering_error_upper_mm);
    REQUIRE(model.objects.empty()); REQUIRE_FALSE(binding.snapshot);
    const auto projection=analyze_upper_projection(*placed.geometry.normalized,true,42);
    INFO(projection.reason); REQUIRE(projection.status==UpperProjectionStatus::NominalHeightfield);
    REQUIRE(projection.snapshot->revision==42);
}
TEST_CASE("B02 input placement refuses mismatched source plate and stale or unprintable inputs", "[Nonplanar][B02][InputPlacement]")
{
    const auto cube=make_cube(20,10,2); const auto source=import_stl_snapshot(binary_stl(cube),true);
    REQUIRE(source.geometry.status==MeshAuditStatus::ValidGeometry);
    Model model; auto *object=model.add_object("source","",cube); auto *instance=object->add_instance();
    DynamicConfig config; config.set_key_value("nptop_mode",new ConfigOptionString("safe_hybrid"));
    NativeInputBinding binding{3,capture_native_input(model,config)};
    const auto rejected=[](const ModelPlacementResult &result) {
        REQUIRE(result.geometry.status!=MeshAuditStatus::ValidGeometry); REQUIRE_FALSE(result.geometry.normalized);
        REQUIRE_FALSE(result.snapshot); REQUIRE_FALSE(result.total_error_upper_mm);
    };
    rejected(capture_input_placement(source,{0,binding.snapshot},{})); rejected(capture_input_placement(source,{3,{}},{}));
    auto wrong=capture_input_placement(source,binding,PlateFrame{1,Vec3d::Zero()});
    REQUIRE(wrong.geometry.reason=="INPUT_PLATE_MISMATCH"); rejected(wrong);
    const auto unrelated=import_stl_snapshot(binary_stl(make_cube(21,10,2)),true);
    wrong=capture_input_placement(unrelated,binding,{}); rejected(wrong);
    instance->printable=false; binding.snapshot=capture_native_input(model,config);
    wrong=capture_input_placement(source,binding,{}); REQUIRE(wrong.geometry.reason=="REQUIRES_ONE_PRINTABLE_OBJECT_VOLUME_INSTANCE"); rejected(wrong);
    instance->printable=true; binding.snapshot=capture_native_input(model,config);
    MeshPlacementLimits limits; limits.geometry.cancelled=[] { return true; };
    wrong=capture_input_placement(source,binding,{},limits); REQUIRE(wrong.geometry.reason=="CANCELLED"); rejected(wrong);
    limits={}; unsigned polls=0; limits.is_current=[&](uint64_t revision) { REQUIRE(revision==3); return ++polls<3; };
    wrong=capture_input_placement(source,binding,{},limits); REQUIRE(wrong.geometry.reason=="STALE_REVISION"); rejected(wrong);
    limits={}; limits.geometry.timeout=std::chrono::milliseconds(1);
    limits.geometry.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(5)); return false; };
    wrong=capture_input_placement(source,binding,{},limits); REQUIRE(wrong.geometry.reason=="DEADLINE"); rejected(wrong);
    limits={}; limits.geometry.cancelled=[]() -> bool { throw std::runtime_error("callback"); };
    rejected(capture_input_placement(source,binding,{},limits));
}

namespace {
ModelPlacementResult placed_partition_source(const TriangleMesh &mesh, uint64_t revision=19)
{
    const TriangleMesh geometry(mesh.its); // Refresh native bounds after fixture vertex edits.
    const auto imported=import_stl_snapshot(binary_stl(geometry),true);
    INFO(imported.geometry.reason); REQUIRE(imported.geometry.status==MeshAuditStatus::ValidGeometry);
    Model model; auto *object=model.add_object("source","",geometry); object->add_instance();
    DynamicConfig config; config.set_key_value("nptop_mode",new ConfigOptionString("safe_hybrid"));
    const NativeInputBinding input{revision,capture_native_input(model,config)};
    auto placed=capture_input_placement(imported,input,{}); INFO(placed.geometry.reason);
    REQUIRE(placed.geometry.status==MeshAuditStatus::ValidGeometry); return placed;
}
bool inside_z_ray(const TriangleMesh &mesh, const Vec3d &p)
{
    // Independent analytic barycentric +Z ray, no CGAL/planner query. Samples
    // are strict interiors of the fixture cells, never on the physical boundary.
    std::vector<long double> hits;
    for (const auto &f : mesh.its.indices) {
        const auto a=mesh.its.vertices[f[0]].cast<long double>();
        const auto b=mesh.its.vertices[f[1]].cast<long double>();
        const auto c=mesh.its.vertices[f[2]].cast<long double>();
        const long double denominator=(b.y()-c.y())*(a.x()-c.x())+(c.x()-b.x())*(a.y()-c.y());
        if (denominator==0) continue;
        const long double u=((b.y()-c.y())*(p.x()-c.x())+(c.x()-b.x())*(p.y()-c.y()))/denominator;
        const long double v=((c.y()-a.y())*(p.x()-c.x())+(a.x()-c.x())*(p.y()-c.y()))/denominator;
        if (u<0 || v<0 || u+v>1) continue;
        const long double z=u*a.z()+v*b.z()+(1-u-v)*c.z();
        if (z>p.z()) hits.push_back(z);
    }
    std::sort(hits.begin(),hits.end());
    hits.erase(std::unique(hits.begin(),hits.end(),[](long double a,long double b){ return std::abs(a-b)<1e-12L; }),hits.end());
    return hits.size()%2==1;
}
void partition_rejected(const VolumePartitionResult &result)
{
    INFO(result.reason); REQUIRE(result.status!=VolumePartitionStatus::Partitioned);
    REQUIRE_FALSE(result.snapshot);
}
}
TEST_CASE("B04 exact interior cap reservation preserves native source walls and affine volume", "[Nonplanar][B04][VolumePartition]")
{
    auto mesh=make_cube(20,10,2);
    for (auto &v : mesh.its.vertices) if (v.z()>0) v.z()=2.f+v.x()/16.f;
    const auto source=placed_partition_source(mesh);
    const auto original=source.geometry.normalized->its;
    auto reservation=make_cube(8,4,4); reservation.translate(6,3,1);
    const auto result=partition_cap(source,reservation); INFO(result.reason);
    REQUIRE(result.status==VolumePartitionStatus::Partitioned); REQUIRE(result.snapshot);
    const auto &s=*result.snapshot; REQUIRE(s.revision==19); REQUIRE(s.placement==source.snapshot);
    REQUIRE(s.original_exact_volume_mm3.lower<=525); REQUIRE(s.original_exact_volume_mm3.upper>=525);
    REQUIRE(s.body_exact_volume_mm3.lower<=473); REQUIRE(s.body_exact_volume_mm3.upper>=473);
    REQUIRE(s.cap_exact_volume_mm3.lower<=52); REQUIRE(s.cap_exact_volume_mm3.upper>=52);
    REQUIRE(s.total_error_upper_mm<1e-5); REQUIRE(s.fingerprint().size()==64);
    REQUIRE(source.geometry.normalized->its.vertices==original.vertices); REQUIRE(source.geometry.normalized->its.indices==original.indices);
    for (int x=0; x<20; ++x) for (int y=0; y<10; ++y) for (double z : {0.31,1.31,2.31,3.31}) {
        const Vec3d p(x+0.37,y+0.23,z);
        const bool in_original=z<2.+p.x()/16.;
        const bool in_cap=in_original && p.x()>6 && p.x()<14 && p.y()>3 && p.y()<7 && z>1;
        const bool body=inside_z_ray(*s.body,p), cap=inside_z_ray(*s.cap,p);
        REQUIRE(body==(in_original && !in_cap)); REQUIRE(cap==in_cap); REQUIRE_FALSE((body && cap));
    }
    const auto identity=s.fingerprint(); reservation.translate(1,0,0);
    const auto changed=partition_cap(source,reservation); REQUIRE(changed.snapshot); REQUIRE(changed.snapshot->fingerprint()!=identity);
    REQUIRE(s.fingerprint()==identity);
}
TEST_CASE("B04 cap partition owns callback inputs and rejects unusable geometry or stale work", "[Nonplanar][B04][VolumePartition]")
{
    auto source=placed_partition_source(make_cube(20,10,2));
    auto reservation=make_cube(8,4,3); reservation.translate(6,3,1);
    const auto input=source.snapshot;
    VolumePartitionLimits limits; limits.geometry.cancelled=[&] { source={}; reservation=TriangleMesh{}; return false; };
    const auto frozen=partition_cap(source,reservation,limits); INFO(frozen.reason);
    REQUIRE(frozen.status==VolumePartitionStatus::Partitioned); REQUIRE(frozen.snapshot->placement==input);
    REQUIRE(source.geometry.status==MeshAuditStatus::Unknown); REQUIRE(reservation.empty());
    source=placed_partition_source(make_cube(20,10,2)); reservation=make_cube(8,4,3); reservation.translate(6,3,1);
    partition_rejected(partition_cap(ModelPlacementResult{},reservation));
    auto empty=make_cube(1,1,1); empty.translate(100,100,100); partition_rejected(partition_cap(source,empty));
    auto all=make_cube(30,30,30); all.translate(-1,-1,-1); partition_rejected(partition_cap(source,all));
    auto open=reservation; open.its.indices.pop_back(); partition_rejected(partition_cap(source,open));
    limits={}; limits.geometry.cancelled=[] { return true; };
    auto stopped=partition_cap(source,reservation,limits); REQUIRE(stopped.reason=="CANCELLED"); partition_rejected(stopped);
    limits={}; unsigned polls=0; limits.is_current=[&](uint64_t revision) { REQUIRE(revision==19); return ++polls<3; };
    stopped=partition_cap(source,reservation,limits); REQUIRE(stopped.reason=="STALE_REVISION"); partition_rejected(stopped);
    limits={}; limits.geometry.timeout=std::chrono::milliseconds(1);
    limits.geometry.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(5)); return false; };
    stopped=partition_cap(source,reservation,limits); REQUIRE(stopped.reason=="DEADLINE"); partition_rejected(stopped);
    limits={}; limits.geometry.max_faces=1; partition_rejected(partition_cap(source,reservation,limits));
}

namespace {
TriangleMesh partition_ring()
{
    indexed_triangle_set mesh;
    const std::array<Vec2f,8> xy{Vec2f(0,0),Vec2f(20,0),Vec2f(20,10),Vec2f(0,10),
        Vec2f(8,4),Vec2f(12,4),Vec2f(12,6),Vec2f(8,6)};
    for (float z : {0.f,2.f}) for (const auto &p : xy) mesh.vertices.emplace_back(p.x(),p.y(),z);
    for (int i=0; i<4; ++i) {
        const int j=(i+1)%4;
        for (const Vec3i32 f : {Vec3i32(i,j,j+4),Vec3i32(i,j+4,i+4)}) {
            mesh.indices.emplace_back(f.x()+8,f.y()+8,f.z()+8); mesh.indices.emplace_back(f.x(),f.z(),f.y());
        }
        mesh.indices.emplace_back(i,j,j+8); mesh.indices.emplace_back(i,j+8,i+8);
        mesh.indices.emplace_back(j+4,i+4,i+12); mesh.indices.emplace_back(j+4,i+12,j+12);
    }
    return TriangleMesh(std::move(mesh));
}
TriangleMesh partition_section(const std::vector<Vec2f> &section, float y0, float y1)
{
    Polygon polygon; indexed_triangle_set mesh; const int count=int(section.size());
    for (const auto &p : section) polygon.points.emplace_back(scale_(double(p.x())),scale_(double(p.y())));
    for (float y : {y0,y1}) for (const auto &p : section) mesh.vertices.emplace_back(p.x(),y,p.y());
    for (const auto &f : Triangulation::triangulate(polygon)) {
        mesh.indices.push_back(f); mesh.indices.emplace_back(f.x()+count,f.z()+count,f.y()+count);
    }
    for (int a=0; a<count; ++a) {
        const int b=(a+1)%count;
        mesh.indices.emplace_back(a,a+count,b+count); mesh.indices.emplace_back(a,b+count,b);
    }
    return TriangleMesh(std::move(mesh));
}
}
TEST_CASE("B04 partition preserves a through hole and handles a stepped reserved interface", "[Nonplanar][B04][VolumePartition]")
{
    auto source=placed_partition_source(partition_ring());
    auto reservation=make_cube(22,12,3); reservation.translate(-1,-1,1);
    auto result=partition_cap(source,reservation); INFO(result.reason);
    REQUIRE(result.status==VolumePartitionStatus::Partitioned);
    const auto &ring=*result.snapshot;
    REQUIRE(ring.body_exact_volume_mm3.lower<=192); REQUIRE(ring.body_exact_volume_mm3.upper>=192);
    REQUIRE(ring.cap_exact_volume_mm3.lower<=192); REQUIRE(ring.cap_exact_volume_mm3.upper>=192);
    REQUIRE(ring.shared_interface_triangles>0);
    for (int x=0; x<20; ++x) for (int y=0; y<10; ++y) {
        const Vec3d low(x+0.37,y+0.23,0.3), high(x+0.37,y+0.23,1.3);
        const bool material=!(low.x()>8 && low.x()<12 && low.y()>4 && low.y()<6);
        REQUIRE(inside_z_ray(*ring.body,low)==material); REQUIRE(inside_z_ray(*ring.cap,high)==material);
        REQUIRE_FALSE(inside_z_ray(*ring.body,high)); REQUIRE_FALSE(inside_z_ray(*ring.cap,low));
    }
    auto stepped=partition_section({{0,0},{20,0},{20,3},{10,3},{10,2},{0,2}},0,10);
    source=placed_partition_source(stepped);
    reservation=partition_section({{6,1},{10,1},{10,1.5},{14,1.5},{14,5},{6,5}},3,7);
    result=partition_cap(source,reservation); INFO(result.reason);
    REQUIRE(result.status==VolumePartitionStatus::Partitioned);
    const auto &s=*result.snapshot;
    REQUIRE(s.original_exact_volume_mm3.lower<=500); REQUIRE(s.original_exact_volume_mm3.upper>=500);
    REQUIRE(s.body_exact_volume_mm3.lower<=460); REQUIRE(s.body_exact_volume_mm3.upper>=460);
    REQUIRE(s.cap_exact_volume_mm3.lower<=40); REQUIRE(s.cap_exact_volume_mm3.upper>=40);
    for (int x=0; x<20; ++x) for (int y=0; y<10; ++y) for (double z : {0.3,1.3,2.3,3.3}) {
        const Vec3d p(x+0.37,y+0.23,z);
        const bool original=z<(p.x()<10 ? 2 : 3);
        const bool cap=original && p.x()>6 && p.x()<14 && p.y()>3 && p.y()<7 && z>(p.x()<10 ? 1 : 1.5);
        REQUIRE(inside_z_ray(*s.body,p)==(original && !cap)); REQUIRE(inside_z_ray(*s.cap,p)==cap);
    }
}
TEST_CASE("B04 partition enforces measured native conversion and inherited error budget", "[Nonplanar][B04][VolumePartition]")
{
    auto mesh=make_cube(20,10,2); for (auto &v : mesh.its.vertices) if (v.z()>0) v.z()=2.f+v.x()/16.f;
    const auto source=placed_partition_source(mesh);
    auto reservation=make_cube(8,4,4); reservation.translate(0.1f,0.3f,1.f);
    const auto result=partition_cap(source,reservation); INFO(result.reason);
    REQUIRE(result.status==VolumePartitionStatus::Partitioned); const auto &s=*result.snapshot;
    REQUIRE(s.partition_error_upper_mm>0); REQUIRE(s.total_error_upper_mm>*source.total_error_upper_mm);
    REQUIRE(s.body_volume_conversion_error_upper_mm3>=0); REQUIRE(s.cap_volume_conversion_error_upper_mm3>=0);
    VolumePartitionLimits limits;
    limits.max_total_error_mm=*source.total_error_upper_mm+s.partition_error_upper_mm/2;
    auto rejected=partition_cap(source,reservation,limits); REQUIRE(rejected.reason=="PARTITION_ERROR_BUDGET"); partition_rejected(rejected);
    limits={}; limits.max_total_error_mm=0.051; partition_rejected(partition_cap(source,reservation,limits));
}

TEST_CASE("B04 partition identity matches independent canonical and hash framing", "[Nonplanar][B04][VolumePartition]")
{
    const auto path=boost::filesystem::path(__FILE__).parent_path()/"data/partition-fingerprint-v1.json";
    boost::nowide::ifstream input(path.string()); REQUIRE(input.good()); nlohmann::json oracle; input>>oracle;
    const auto mesh=std::make_shared<const TriangleMesh>(); DynamicConfig config;
    const auto native=std::make_shared<const NativeInputSnapshot>(NativeInputSnapshot{ResolvedConfigSnapshot(config),{},{},0,"","input"});
    const auto bytes=std::make_shared<const StlSourceSnapshot>(StlSourceSnapshot{"","source",true});
    const auto centered=std::make_shared<const CenteredVolumeSnapshot>(CenteredVolumeSnapshot{bytes,mesh,Vec3d::Zero(),0,19});
    const auto placement=std::make_shared<const ModelPlacementSnapshot>(ModelPlacementSnapshot{
        centered,Transform3d::Identity(),Transform3d::Identity(),{2,Vec3d(0.1,-0.,2.)},native});
    // Serialization vector only: empty meshes/placeholder IDs never enter the
    // partition algorithm and do not constitute a geometry/qualification result.
    const VolumePartitionSnapshot snapshot{19,placement,mesh,mesh,mesh,mesh,{0,0},{0,0},{0,0},{0,0},{0,0},0.1,0.2,0,0,0};
    REQUIRE(snapshot.canonical_json()==oracle.at("canonical").get<std::string>());
    REQUIRE(snapshot.fingerprint()==oracle.at("sha256").get<std::string>());
}
