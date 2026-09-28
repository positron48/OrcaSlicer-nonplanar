#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/MeshPlacement.hpp>
#include <boost/nowide/fstream.hpp>
#include <cfenv>
#include <thread>

using namespace Slic3r;
using namespace Slic3r::nptop;
namespace {
StlImportResult imported()
{
    boost::nowide::ifstream input(NPTOP_STL_FIXTURE_PATH,std::ios::binary);
    REQUIRE(input);
    const std::string bytes(std::istreambuf_iterator<char>(input),{});
    auto result=import_stl_snapshot(bytes,true);
    REQUIRE(result.geometry.status==MeshAuditStatus::ValidGeometry);
    return result;
}
void rejected(const MeshPlacementResult &result)
{
    INFO(result.geometry.reason);
    REQUIRE(result.geometry.status!=MeshAuditStatus::ValidGeometry);
    REQUIRE_FALSE(result.geometry.normalized);
    REQUIRE_FALSE(result.total_error_upper_mm);
}
}
TEST_CASE("B02 native placement preserves the imported source and proves affine block volume and bounds", "[Nonplanar][B02][Placement]")
{
    const auto source=imported();
    const auto before=source.geometry.normalized->its;
    const auto bytes=source.source->bytes;
    Transform3d matrix=Transform3d::Identity();
    matrix(0,0)=0; matrix(0,1)=-2; matrix(1,0)=2; matrix(1,1)=0; matrix(2,2)=2;
    matrix.translation()=Vec3d(100,50,3);
    const auto result=place_imported_mesh(source,matrix,29);
    INFO(result.geometry.reason);
    REQUIRE(result.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(result.snapshot);
    REQUIRE(result.snapshot->revision==29);
    REQUIRE(result.snapshot->model_to_plate.matrix()==matrix.matrix());
    REQUIRE(result.geometry.volume_lower_mm3<=19200);
    REQUIRE(result.geometry.volume_upper_mm3>=19200);
    const auto bounds=result.geometry.normalized->bounding_box();
    REQUIRE(bounds.min==Vec3d(80,20,3));
    REQUIRE(bounds.max==Vec3d(120,80,11));
    REQUIRE(result.total_error_upper_mm);
    REQUIRE(*result.total_error_upper_mm<1e-10);
    REQUIRE(result.geometry.normalized!=source.geometry.normalized);
    REQUIRE(source.geometry.normalized->its.vertices==before.vertices);
    REQUIRE(source.geometry.normalized->its.indices==before.indices);
    REQUIRE(source.source->bytes==bytes);
}
TEST_CASE("B02 placement bounds actual native float rounding and enforces the measured allowance", "[Nonplanar][B02][Placement]")
{
    const auto source=imported();
    Transform3d matrix=Transform3d::Identity(); matrix.translation()=Vec3d(0.1,0.1,0.1);
    const auto result=place_imported_mesh(source,matrix,1);
    INFO(result.geometry.reason);
    REQUIRE(result.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(result.native_transform_error_upper_mm);
    // Exact independent Fraction oracle for source corner (15,10,4), each axis
    // translated by the authoritative binary64 value 0.1 then stored binary32.
    constexpr double corner_error=30923764531.0/36028797018963968.0;
    REQUIRE(*result.native_transform_error_upper_mm>=corner_error);
    REQUIRE(*result.native_transform_error_upper_mm<1e-6);
    REQUIRE(*result.propagated_source_error_upper_mm==0);
    MeshPlacementLimits limits; limits.max_error_upper_mm=corner_error/2;
    const auto limited=place_imported_mesh(source,matrix,1,limits);
    REQUIRE(limited.geometry.reason=="PLACEMENT_ERROR_BUDGET");
    rejected(limited);
}
TEST_CASE("B02 placement propagates source uncertainty through nonuniform scale without double counting", "[Nonplanar][B02][Placement]")
{
    auto source=imported();
    // A conservative input bound is allowed to exceed the fixture's exact zero.
    source.source_error_upper_mm=0.0001;
    Transform3d matrix=Transform3d::Identity(); matrix(0,0)=2; matrix(1,1)=3; matrix(2,2)=4;
    const auto result=place_imported_mesh(source,matrix,1);
    REQUIRE(result.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(*result.propagated_source_error_upper_mm>=0.0004);
    REQUIRE(*result.propagated_source_error_upper_mm<0.000400000001);
    REQUIRE(*result.total_error_upper_mm>=*result.propagated_source_error_upper_mm);
    REQUIRE(*result.total_error_upper_mm<0.00040000001);
    MeshPlacementLimits limits; limits.max_error_upper_mm=0.0003;
    const auto limited=place_imported_mesh(source,matrix,1,limits);
    REQUIRE(limited.geometry.reason=="PLACEMENT_ERROR_BUDGET");
    rejected(limited);
}
TEST_CASE("B02 placement rejects missing provenance unsupported matrices and collapsed native topology", "[Nonplanar][B02][Placement]")
{
    auto source=imported();
    const auto identity=Transform3d::Identity();
    rejected(place_imported_mesh(source,identity,0));
    source.source_error_upper_mm.reset(); rejected(place_imported_mesh(source,identity,1));
    source=imported(); source.native_geometry_unchanged=false; rejected(place_imported_mesh(source,identity,1));
    source=imported();
    for (double entry : {0.,-1.,101.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
        Transform3d matrix=identity; matrix(0,0)=entry;
        rejected(place_imported_mesh(source,matrix,1));
    }
    Transform3d projective=identity; projective(3,0)=0.01;
    rejected(place_imported_mesh(source,projective,1));
    for (double budget : {-1.,0.051,std::numeric_limits<double>::quiet_NaN()}) {
        MeshPlacementLimits limits; limits.max_error_upper_mm=budget;
        rejected(place_imported_mesh(source,identity,1,limits));
    }
    Transform3d collapse=identity;
    collapse.linear()*=1e-10; collapse.translation()=Vec3d(1000,1000,1000);
    const auto collapsed=place_imported_mesh(source,collapse,1);
    REQUIRE(collapsed.geometry.status==MeshAuditStatus::Invalid);
    REQUIRE(collapsed.geometry.reason=="DEGENERATE_FACE");
    rejected(collapsed);
}
TEST_CASE("B02 placement owns callback inputs and cannot accept cancelled stale or late work", "[Nonplanar][B02][Placement]")
{
    auto source=imported();
    const auto original=source.geometry.normalized;
    Transform3d matrix=Transform3d::Identity();
    MeshPlacementLimits limits;
    limits.geometry.cancelled=[&] {
        matrix.translation()=Vec3d(500,500,500); source.geometry={}; return false;
    };
    const auto owned=place_imported_mesh(source,matrix,17,limits);
    REQUIRE(owned.geometry.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(owned.snapshot->model_to_plate.matrix()==Transform3d::Identity().matrix());
    REQUIRE(owned.snapshot->model_local==original);
    source=imported(); matrix=Transform3d::Identity(); limits={};
    limits.geometry.cancelled=[] { return true; };
    auto cancelled=place_imported_mesh(source,matrix,1,limits);
    REQUIRE(cancelled.geometry.reason=="CANCELLED"); rejected(cancelled);
    limits={}; int polls=0;
    limits.is_current=[&](uint64_t) { return ++polls<3; };
    auto stale=place_imported_mesh(source,matrix,1,limits);
    REQUIRE(stale.geometry.reason=="STALE_REVISION"); rejected(stale);
    limits={}; limits.geometry.timeout=std::chrono::milliseconds(1);
    limits.geometry.cancelled=[&] {
        limits.geometry.timeout=std::chrono::seconds(30);
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); return false;
    };
    auto late=place_imported_mesh(source,matrix,1,limits);
    REQUIRE(late.geometry.reason=="DEADLINE"); rejected(late);
}
TEST_CASE("B02 placement detects an unsupported arithmetic mode introduced by a callback", "[Nonplanar][B02][Placement]")
{
    const auto source=imported();
    struct RestoreRounding {
        int mode=std::fegetround();
        ~RestoreRounding() { std::fesetround(mode); }
    } restore;
    MeshPlacementLimits limits;
    limits.geometry.cancelled=[] { std::fesetround(FE_UPWARD); return false; };
    const auto result=place_imported_mesh(source,Transform3d::Identity(),1,limits);
    rejected(result);
    REQUIRE(result.geometry.reason=="PLACEMENT_EXCEPTION");
}
