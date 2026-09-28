#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/UpperProjection.hpp>
#include <cfenv>
#include <limits>
#include <set>
#include <thread>

using namespace Slic3r;
using namespace Slic3r::nptop;
namespace {
TriangleMesh wedge()
{
    auto mesh=make_cube(20,10,2);
    for (auto &v : mesh.its.vertices) if (v.z()>0) v.z() += v.x()/8;
    return mesh;
}
TriangleMesh overhang()
{
    // A connected C-shaped XZ section extruded through Y=0..10. The shelf
    // z=2, x=0..15 is hidden from above by the roof z=10, x=0..20.
    const std::vector<Vec2f> section{{0,0},{20,0},{20,10},{0,10},{0,8},{15,8},{15,2},{0,2}};
    indexed_triangle_set mesh;
    for (float y : {0.f,10.f}) for (auto p : section) mesh.vertices.emplace_back(p.x(),y,p.y());
    for (Vec3i32 f : {Vec3i32(0,1,6),Vec3i32(0,6,7),Vec3i32(1,2,5),Vec3i32(1,5,6),Vec3i32(2,3,4),Vec3i32(2,4,5)}) {
        mesh.indices.push_back(f); mesh.indices.emplace_back(f.x()+8,f.z()+8,f.y()+8);
    }
    for (int a=0; a<8; ++a) {
        const int b=(a+1)%8;
        mesh.indices.emplace_back(a,a+8,b+8); mesh.indices.emplace_back(a,b+8,b);
    }
    return TriangleMesh(std::move(mesh));
}
}
TEST_CASE("B03 nominal upper projection retains the full predefined flat and wedge masks", "[Nonplanar][B03][UpperProjection]")
{
    for (bool sloped : {false,true}) {
        const auto mesh=sloped ? wedge() : make_cube(20,10,2);
        const auto before=mesh.its;
        const auto result=analyze_upper_projection(mesh,true,91);
        INFO(result.reason);
        REQUIRE(result.status==UpperProjectionStatus::NominalHeightfield);
        REQUIRE(result.snapshot);
        const auto &s=*result.snapshot;
        std::set<size_t> expected_faces, observed_faces;
        for (size_t face=0; face<before.indices.size(); ++face) {
            bool on_roof=true;
            for (int i=0; i<3; ++i) {
                const auto &v=before.vertices[before.indices[face](i)];
                on_roof=on_roof && v.z()==(sloped ? 2+v.x()/8 : 2);
            }
            if (on_roof) expected_faces.insert(face);
        }
        REQUIRE(s.revision==91);
        REQUIRE(s.geometry.get()!=&mesh);
        REQUIRE(s.upward_facets.size()==2);
        REQUIRE(s.xy_area_lower_mm2<=200);
        REQUIRE(s.xy_area_upper_mm2>=200);
        REQUIRE(s.xy_area_upper_mm2-s.xy_area_lower_mm2<1e-9);
        REQUIRE(s.filtered_area_lower_mm2>199.999999);
        REQUIRE(s.minimum_z_mm==2);
        REQUIRE(s.maximum_z_mm==(sloped ? 4.5 : 2));
        for (const auto &f : s.upward_facets) {
            REQUIRE(observed_faces.insert(f.mesh_face).second);
            REQUIRE(f.within_slope_limit);
            REQUIRE(f.slope_upper>=(sloped ? 0.125 : 0));
            REQUIRE(f.slope_upper<(sloped ? 0.125 : 0)+1e-12);
            for (int i=0; i<3; ++i) {
                const auto &v=s.geometry->its.vertices[s.geometry->its.indices[f.mesh_face](i)];
                REQUIRE(v.z()==(sloped ? 2+v.x()/8 : 2));
            }
        }
        REQUIRE(observed_faces==expected_faces);
        REQUIRE(mesh.its.indices==before.indices);
        REQUIRE(mesh.its.vertices==before.vertices);
    }
}
TEST_CASE("B03 nominal slope exclusion reports area without changing the source", "[Nonplanar][B03][UpperProjection]")
{
    UpperProjectionLimits limits; limits.max_slope=0.1;
    const auto result=analyze_upper_projection(wedge(),true,1,limits);
    REQUIRE(result.status==UpperProjectionStatus::NominalHeightfield);
    REQUIRE(result.snapshot);
    REQUIRE(result.snapshot->filtered_area_lower_mm2==0);
    REQUIRE(result.snapshot->filtered_area_upper_mm2==0);
    REQUIRE(result.snapshot->upward_facets.size()==2);
    for (const auto &f : result.snapshot->upward_facets) REQUIRE_FALSE(f.within_slope_limit);
}
TEST_CASE("B03 hidden shelf cannot become a visible upper candidate", "[Nonplanar][B03][UpperProjection]")
{
    const auto source=overhang();
    const auto audit=audit_mesh(source,true);
    INFO(audit.reason);
    REQUIRE(audit.status==MeshAuditStatus::ValidGeometry);
    const auto result=analyze_upper_projection(source,true,1);
    REQUIRE(result.status==UpperProjectionStatus::Unknown);
    REQUIRE(result.reason=="OVERLAPPING_UPWARD_PROJECTIONS");
    REQUIRE_FALSE(result.snapshot);
}
TEST_CASE("B03 invalid units mesh budgets and stale callbacks never publish a partial mask", "[Nonplanar][B03][UpperProjection]")
{
    auto source=wedge();
    REQUIRE_FALSE(analyze_upper_projection(source,true,0).snapshot);
    REQUIRE_FALSE(analyze_upper_projection(source,false,1).snapshot);
    auto open=source; open.its.indices.pop_back();
    REQUIRE(analyze_upper_projection(open,true,1).status==UpperProjectionStatus::Invalid);
    UpperProjectionLimits limits; limits.max_upward_faces=1;
    REQUIRE(analyze_upper_projection(source,true,1,limits).reason=="UPWARD_FACE_LIMIT");
    limits={}; limits.geometry.timeout=std::chrono::milliseconds(0);
    REQUIRE_FALSE(analyze_upper_projection(source,true,1,limits).snapshot);
    limits={}; limits.max_slope=std::numeric_limits<double>::quiet_NaN();
    REQUIRE_FALSE(analyze_upper_projection(source,true,1,limits).snapshot);
    limits={}; limits.geometry.cancelled=[] { return true; };
    REQUIRE_FALSE(analyze_upper_projection(source,true,1,limits).snapshot);
    limits={}; limits.is_current=[](uint64_t) { return false; };
    REQUIRE(analyze_upper_projection(source,true,1,limits).reason=="STALE_REVISION");
    limits={};
    bool mutated=false;
    limits.geometry.cancelled=[&] {
        if (!mutated) { mutated=true; source.its={}; limits.max_slope=0.; }
        return false;
    };
    const auto owned=analyze_upper_projection(source,true,1,limits);
    REQUIRE(owned.status==UpperProjectionStatus::NominalHeightfield);
    REQUIRE(owned.snapshot->filtered_area_lower_mm2>199.999999);
    source=wedge(); limits={};
    struct RestoreRounding { ~RestoreRounding() { std::fesetround(FE_TONEAREST); } } restore;
    limits.is_current=[](uint64_t) { std::fesetround(FE_UPWARD); return true; };
    REQUIRE_FALSE(analyze_upper_projection(source,true,1,limits).snapshot);
}
TEST_CASE("B03 late staleness and elapsed deadline discard an otherwise complete mask", "[Nonplanar][B03][UpperProjection]")
{
    const auto source=wedge();
    size_t calls=0;
    UpperProjectionLimits limits;
    limits.is_current=[&](uint64_t) { ++calls; return true; };
    REQUIRE(analyze_upper_projection(source,true,1,limits).snapshot);
    REQUIRE(calls>1);
    const auto final_call=calls; calls=0;
    limits.is_current=[&](uint64_t) { return ++calls<final_call; };
    const auto stale=analyze_upper_projection(source,true,1,limits);
    REQUIRE(stale.reason=="STALE_REVISION");
    REQUIRE_FALSE(stale.snapshot);
    limits={}; limits.geometry.timeout=std::chrono::milliseconds(1);
    limits.geometry.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(3)); return false; };
    const auto expired=analyze_upper_projection(source,true,1,limits);
    REQUIRE(expired.reason=="DEADLINE");
    REQUIRE_FALSE(expired.snapshot);
}
