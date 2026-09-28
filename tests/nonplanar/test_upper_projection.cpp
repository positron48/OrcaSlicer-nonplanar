#include <catch2/catch_test_macros.hpp>
#include <array>
#include <libslic3r/Nonplanar/UpperProjection.hpp>
#include <libslic3r/Nonplanar/UpperProjectionPredicates.hpp>
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
TriangleMesh gridded_plate(int side, bool hole, bool ramp=false, bool pinched=false)
{
    indexed_triangle_set mesh;
    const int stride=side+1, layer=stride*stride;
    const auto present=[&](int x,int y) { return x>=0 && x<side && y>=0 && y<side && !(hole && x==1 && y==1); };
    for (int z=0; z<2; ++z)
        for (int y=0; y<=side; ++y)
            for (int x=0; x<=side; ++x) {
                float top=ramp && x>=2 ? 3.f : 2.f;
                if (pinched && ((x==0 && y==0) || (x==2 && y==2))) top=3;
                mesh.vertices.emplace_back(float(x),float(y),z ? top : 0.f);
            }
    for (int y=0; y<side; ++y) for (int x=0; x<side; ++x) if (present(x,y)) {
        const std::array<int,4> v{y*stride+x,y*stride+x+1,(y+1)*stride+x+1,(y+1)*stride+x};
        mesh.indices.emplace_back(v[0]+layer,v[1]+layer,v[2]+layer);
        mesh.indices.emplace_back(v[0]+layer,v[2]+layer,v[3]+layer);
        mesh.indices.emplace_back(v[0],v[2],v[1]); mesh.indices.emplace_back(v[0],v[3],v[2]);
        const std::array<bool,4> exterior{!present(x,y-1),!present(x+1,y),!present(x,y+1),!present(x-1,y)};
        for (int edge=0; edge<4; ++edge) if (exterior[edge]) {
            const int a=v[edge], b=v[(edge+1)%4];
            mesh.indices.emplace_back(a,b,b+layer); mesh.indices.emplace_back(a,b+layer,a+layer);
        }
    }
    return TriangleMesh(std::move(mesh));
}
}
TEST_CASE("B03 connected upper mask keeps the through-hole boundary and exact area", "[Nonplanar][B03][UpperProjection][UpperPatches]")
{
    const auto source=gridded_plate(3,true);
    const auto result=analyze_upper_projection(source,true,1);
    INFO(result.reason);
    REQUIRE(result.status==UpperProjectionStatus::NominalHeightfield);
    REQUIRE(result.snapshot->slope_patches.size()==1);
    const auto &patch=result.snapshot->slope_patches.front();
    REQUIRE(patch.mesh_faces.size()==16);
    REQUIRE(patch.xy_area_lower_mm2<=8);
    REQUIRE(patch.xy_area_upper_mm2>=8);
    REQUIRE(patch.xy_area_upper_mm2-patch.xy_area_lower_mm2<1e-10);
    REQUIRE(patch.boundaries.size()==2);
    size_t holes=0;
    for (const auto &boundary : patch.boundaries) {
        const double expected=boundary.hole ? -1 : 9;
        holes += boundary.hole;
        REQUIRE(boundary.signed_xy_area_lower_mm2<=expected);
        REQUIRE(boundary.signed_xy_area_upper_mm2>=expected);
        REQUIRE(boundary.mesh_vertices.size()==(boundary.hole ? 4 : 12));
    }
    REQUIRE(holes==1);
}
TEST_CASE("B03 slope mask separates flat strips without erasing the steep connecting geometry", "[Nonplanar][B03][UpperProjection][UpperPatches]")
{
    const auto source=gridded_plate(3,false,true);
    UpperProjectionLimits limits; limits.max_slope=0.1;
    const auto result=analyze_upper_projection(source,true,1,limits);
    INFO(result.reason);
    REQUIRE(result.status==UpperProjectionStatus::NominalHeightfield);
    REQUIRE(result.snapshot->slope_patches.size()==2);
    REQUIRE(result.snapshot->upward_facets.size()==18);
    REQUIRE(result.snapshot->filtered_area_lower_mm2>5.999999999);
    for (const auto &patch : result.snapshot->slope_patches) {
        REQUIRE(patch.mesh_faces.size()==6);
        REQUIRE(patch.xy_area_lower_mm2<=3);
        REQUIRE(patch.xy_area_upper_mm2>=3);
        REQUIRE(patch.boundaries.size()==1);
        REQUIRE_FALSE(patch.boundaries.front().hole);
    }
    REQUIRE(same_oriented_triangles(result.snapshot->geometry->its,source.its));
}
TEST_CASE("B03 a valid heightfield with a pinched slope mask is not silently repaired", "[Nonplanar][B03][UpperProjection][UpperPatches]")
{
    const auto source=gridded_plate(4,false,false,true);
    REQUIRE(audit_mesh(source,true).status==MeshAuditStatus::ValidGeometry);
    UpperProjectionLimits limits; limits.max_slope=0.1;
    const auto result=analyze_upper_projection(source,true,1,limits);
    REQUIRE(result.status==UpperProjectionStatus::Unknown);
    REQUIRE(result.reason=="AMBIGUOUS_MASK_BOUNDARY");
    REQUIRE_FALSE(result.snapshot);
}
TEST_CASE("B03 exact boundary predicates distinguish endpoint adjacency from projected overlap", "[Nonplanar][B03][UpperProjection][UpperPatches]")
{
    for (bool reverse_first : {false,true}) for (bool reverse_second : {false,true}) {
        Vec3f a(0,0,0), b(2,0,0), c(2,0,0), d(3,0,0);
        if (reverse_first) std::swap(a,b);
        if (reverse_second) std::swap(c,d);
        REQUIRE_FALSE(detail::projected_boundary_conflict(a,b,c,d,true));
        REQUIRE(detail::projected_boundary_conflict(a,b,c,d,false));
        c=Vec3f(2,0,0); d=Vec3f(1,0,0);
        if (reverse_second) std::swap(c,d);
        REQUIRE(detail::projected_boundary_conflict(a,b,c,d,true));
    }
    REQUIRE(detail::projected_boundary_conflict(Vec3f(0,0,0),Vec3f(2,0,0),Vec3f(1,-1,9),Vec3f(1,1,9),false));
    REQUIRE_FALSE(detail::projected_boundary_conflict(Vec3f(0,0,0),Vec3f(2,0,0),Vec3f(1,1e-30f,0),Vec3f(2,1e-30f,0),false));
    REQUIRE_FALSE(detail::projected_boundary_conflict(Vec3f(0,0,0),Vec3f(2,0,0),Vec3f(2,0,0),Vec3f(2,2,0),true));
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
