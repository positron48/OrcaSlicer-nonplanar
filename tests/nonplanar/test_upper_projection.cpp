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

TEST_CASE("B03 whole footprint fits the predefined block and affine wedge ROI", "[Nonplanar][B03][UpperFootprint]")
{
    for (const auto &mesh : {make_cube(20,10,2),wedge()}) {
        const auto projection=analyze_upper_projection(mesh,true,19);
        REQUIRE(projection.snapshot);
        UpperFootprintQuery query{0,{2,2},{18,8},1,0,0};
        auto result=check_upper_footprint(projection.snapshot,query);
        INFO(result.reason);
        REQUIRE(result.status==UpperFootprintStatus::Contained);
        REQUIRE(result.required_inset_upper_mm==1);
        std::swap(query.start_mm,query.end_mm);
        REQUIRE(check_upper_footprint(projection.snapshot,query).status==UpperFootprintStatus::Contained);
        query.xy_radius_mm=2;
        REQUIRE(check_upper_footprint(projection.snapshot,query).status==UpperFootprintStatus::Outside);
    }
}
TEST_CASE("B03 whole footprint cannot jump a hole between individually valid endpoints", "[Nonplanar][B03][UpperFootprint]")
{
    const auto projection=analyze_upper_projection(gridded_plate(3,true),true,2);
    REQUIRE(projection.snapshot);
    UpperFootprintQuery query{0,{0.5,1.5},{2.5,1.5},0.25,0,0};
    for (auto point : {query.start_mm,query.end_mm}) {
        auto endpoint=query; endpoint.start_mm=point; endpoint.end_mm=point;
        REQUIRE(check_upper_footprint(projection.snapshot,endpoint).status==UpperFootprintStatus::Contained);
    }
    REQUIRE(check_upper_footprint(projection.snapshot,query).status==UpperFootprintStatus::Outside);
    query.start_mm.y()=query.end_mm.y()=0.5;
    REQUIRE(check_upper_footprint(projection.snapshot,query).status==UpperFootprintStatus::Contained);
    query.xy_radius_mm=0.5;
    REQUIRE(check_upper_footprint(projection.snapshot,query).status==UpperFootprintStatus::Outside);
    query.xy_radius_mm=0.25; query.boundary_uncertainty_mm=0.125; query.transition_inset_mm=0.125;
    REQUIRE(check_upper_footprint(projection.snapshot,query).status==UpperFootprintStatus::Outside);
    query={0,{1.5,1.5},{1.5,1.5},0.125,0,0};
    REQUIRE(check_upper_footprint(projection.snapshot,query).status==UpperFootprintStatus::Outside);
    query.start_mm=query.end_mm=Vec2d(-1,1.5);
    REQUIRE(check_upper_footprint(projection.snapshot,query).status==UpperFootprintStatus::Outside);
}
TEST_CASE("B03 exact footprint boundary rejects tangency without an arbitrary epsilon", "[Nonplanar][B03][UpperFootprint]")
{
    const auto projection=analyze_upper_projection(gridded_plate(3,true),true,3);
    REQUIRE(projection.snapshot);
    UpperFootprintQuery query{0,{0.5,0.5},{0.5,0.5},0.5,0,0};
    REQUIRE(check_upper_footprint(projection.snapshot,query).status==UpperFootprintStatus::Outside);
    query.xy_radius_mm=std::nextafter(0.5,0.);
    REQUIRE(check_upper_footprint(projection.snapshot,query).status==UpperFootprintStatus::Contained);
    query.xy_radius_mm=std::nextafter(0.5,1.);
    REQUIRE(check_upper_footprint(projection.snapshot,query).status==UpperFootprintStatus::Outside);
}

TEST_CASE("B03 whole footprint agrees with independent rectangle inequalities", "[Nonplanar][B03][UpperFootprint]")
{
    const auto projection=analyze_upper_projection(make_cube(20,10,2),true,4);
    REQUIRE(projection.snapshot);
    // A convex rectangle contains a capsule iff both endpoints satisfy all
    // four strict inset half-planes. This oracle uses no polygon/distance code.
    for (int seed=0; seed<120; ++seed) {
        const Vec2d a((seed*7)%24-2,(seed*3)%14-2), b((seed*11)%24-2,(seed*5)%14-2);
        const double radius=0.25*((seed%8)+1);
        const auto inside=[&](Vec2d p) {
            return p.x()>radius && p.x()<20-radius && p.y()>radius && p.y()<10-radius;
        };
        const auto result=check_upper_footprint(projection.snapshot,{0,a,b,radius,0,0});
        INFO(seed);
        REQUIRE(result.status==(inside(a) && inside(b) ? UpperFootprintStatus::Contained : UpperFootprintStatus::Outside));
    }
}
TEST_CASE("B03 footprint captures ownership and rejects invalid cancelled stale or late queries", "[Nonplanar][B03][UpperFootprint]")
{
    auto snapshot=analyze_upper_projection(gridded_plate(3,true),true,5).snapshot;
    REQUIRE(snapshot);
    const UpperFootprintQuery valid{0,{0.5,0.5},{2.5,0.5},0.25,0,0};
    REQUIRE(check_upper_footprint(nullptr,valid).status==UpperFootprintStatus::Unknown);
    for (double invalid : {-1.,0.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
        auto query=valid; query.xy_radius_mm=invalid;
        REQUIRE(check_upper_footprint(snapshot,query).status==UpperFootprintStatus::Unknown);
    }
    for (int field=0; field<5; ++field) {
        auto query=valid;
        if (field==0) query.patch=1;
        if (field==1) query.start_mm.x()=std::numeric_limits<double>::quiet_NaN();
        if (field==2) query.end_mm.y()=10001;
        if (field==3) query.boundary_uncertainty_mm=-1;
        if (field==4) query.transition_inset_mm=std::numeric_limits<double>::infinity();
        REQUIRE(check_upper_footprint(snapshot,query).status==UpperFootprintStatus::Unknown);
    }
    UpperFootprintLimits limits; limits.timeout=std::chrono::milliseconds(0);
    REQUIRE(check_upper_footprint(snapshot,valid,limits).status==UpperFootprintStatus::Unknown);
    limits={}; limits.cancelled=[] { return true; };
    REQUIRE(check_upper_footprint(snapshot,valid,limits).reason=="CANCELLED");
    limits={}; limits.is_current=[](uint64_t) { return false; };
    REQUIRE(check_upper_footprint(snapshot,valid,limits).reason=="STALE_REVISION");
    limits={}; size_t calls=0;
    limits.is_current=[&](uint64_t revision) { REQUIRE(revision==5); ++calls; return true; };
    REQUIRE(check_upper_footprint(snapshot,valid,limits).status==UpperFootprintStatus::Contained);
    const auto final_call=calls; REQUIRE(final_call>1); calls=0;
    limits.is_current=[&](uint64_t) { return ++calls<final_call; };
    const auto stale=check_upper_footprint(snapshot,valid,limits);
    REQUIRE(stale.status==UpperFootprintStatus::Unknown);
    REQUIRE(stale.reason=="STALE_REVISION");
    limits={}; limits.timeout=std::chrono::milliseconds(1);
    limits.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(3)); return false; };
    const auto late=check_upper_footprint(snapshot,valid,limits);
    REQUIRE(late.status==UpperFootprintStatus::Unknown);
    REQUIRE(late.reason=="DEADLINE");
    limits={}; limits.cancelled=[]() -> bool { throw std::runtime_error("cancel failure"); };
    REQUIRE(check_upper_footprint(snapshot,valid,limits).status==UpperFootprintStatus::Unknown);
    struct RestoreRounding { ~RestoreRounding() { std::fesetround(FE_TONEAREST); } } restore;
    limits={}; limits.is_current=[](uint64_t) { std::fesetround(FE_UPWARD); return true; };
    REQUIRE(check_upper_footprint(snapshot,valid,limits).status==UpperFootprintStatus::Unknown);
    std::fesetround(FE_TONEAREST);
    limits={}; auto query=valid; bool mutated=false;
    limits.cancelled=[&] {
        if (!mutated) {
            mutated=true; snapshot.reset(); query.xy_radius_mm=10;
            limits.timeout=std::chrono::milliseconds(0); limits.is_current=[](uint64_t) { return false; };
        }
        return false;
    };
    REQUIRE(check_upper_footprint(snapshot,query,limits).status==UpperFootprintStatus::Contained);
    REQUIRE(mutated);
    REQUIRE_FALSE(snapshot);
}

TEST_CASE("B03 affine curvature survives coplanar triangulation edges", "[Nonplanar][B03][UpperCurvature]")
{
    const std::array<TriangleMesh,3> meshes{make_cube(20,10,2),wedge(),gridded_plate(3,false)};
    for (size_t index=0; index<meshes.size(); ++index) {
        const auto projection=analyze_upper_projection(meshes[index],true,6);
        REQUIRE(projection.snapshot);
        const auto &patch=projection.snapshot->slope_patches.front();
        REQUIRE(patch.creases.empty());
        REQUIRE(patch.nominal_curvature_upper_mm_inv==0);
        REQUIRE(patch.minimum_z_mm==2);
        REQUIRE(patch.maximum_z_mm==(index==1 ? 4.5 : 2));
        const double expected_slope=index==1 ? 0.125 : 0.;
        REQUIRE(patch.slope_upper>=expected_slope);
        REQUIRE(patch.slope_upper-expected_slope<1e-10);
        const auto result=check_affine_upper_footprint(projection.snapshot,{0,{0.5,0.5},{2.5,2.5},0.125,0,0});
        INFO(result.reason);
        REQUIRE(result.status==UpperFootprintStatus::Contained);
        REQUIRE(result.nominal_curvature_upper_mm_inv==0);
        REQUIRE_FALSE(check_upper_footprint(projection.snapshot,{0,{0.5,0.5},{2.5,2.5},0.125,0,0}).nominal_curvature_upper_mm_inv);
    }
}
TEST_CASE("B03 crease curvature cannot be inferred from safe endpoint footprints", "[Nonplanar][B03][UpperCurvature]")
{
    UpperProjectionLimits limits; limits.max_slope=2;
    const auto projection=analyze_upper_projection(gridded_plate(3,false,true),true,7,limits);
    REQUIRE(projection.snapshot);
    REQUIRE(projection.snapshot->slope_patches.size()==1);
    const auto &patch=projection.snapshot->slope_patches.front();
    REQUIRE(patch.creases.size()==6);
    REQUIRE_FALSE(patch.nominal_curvature_upper_mm_inv);
    REQUIRE(patch.minimum_z_mm==2);
    REQUIRE(patch.maximum_z_mm==3);
    REQUIRE(patch.slope_upper>=1);
    REQUIRE(patch.slope_upper<1.00000001);
    std::set<std::array<int,3>> expected_edges;
    for (const auto &crease : patch.creases) {
        const auto &a=projection.snapshot->geometry->its.vertices[crease.mesh_vertices[0]];
        const auto &b=projection.snapshot->geometry->its.vertices[crease.mesh_vertices[1]];
        REQUIRE(a.x()==b.x());
        REQUIRE((a.x()==1 || a.x()==2));
        expected_edges.insert({int(a.x()),int(std::min(a.y(),b.y())),int(std::max(a.y(),b.y()))});
    }
    const std::set<std::array<int,3>> predefined{{1,0,1},{1,1,2},{1,2,3},{2,0,1},{2,1,2},{2,2,3}};
    REQUIRE(expected_edges==predefined);
    for (double x : {0.5,1.5,2.5}) {
        const auto result=check_affine_upper_footprint(projection.snapshot,{0,{x,0.5},{x,2.5},0.125,0,0});
        REQUIRE(result.status==UpperFootprintStatus::Contained);
        REQUIRE(result.nominal_curvature_upper_mm_inv==0);
    }
    for (const auto &query : {UpperFootprintQuery{0,{0.5,1.5},{2.5,1.5},0.125,0,0},
                              UpperFootprintQuery{0,{0.75,0.5},{0.75,0.5},0.25,0,0}}) {
        REQUIRE(check_upper_footprint(projection.snapshot,query).status==UpperFootprintStatus::Contained);
        const auto result=check_affine_upper_footprint(projection.snapshot,query);
        REQUIRE(result.status==UpperFootprintStatus::Unknown);
        REQUIRE(result.reason=="UNQUALIFIED_CREASE_IN_FOOTPRINT");
        REQUIRE_FALSE(result.nominal_curvature_upper_mm_inv);
    }
}
TEST_CASE("B03 one ULP nominal crease is not smoothed away", "[Nonplanar][B03][UpperCurvature]")
{
    auto source=gridded_plate(3,false,true);
    for (auto &v : source.its.vertices) if (v.z()==3) v.z()=std::nextafter(2.f,3.f);
    const auto projection=analyze_upper_projection(source,true,8);
    REQUIRE(projection.snapshot);
    REQUIRE(projection.snapshot->slope_patches.front().creases.size()==6);
    REQUIRE(check_affine_upper_footprint(projection.snapshot,{0,{0.5,1.5},{2.5,1.5},0.125,0,0}).status==UpperFootprintStatus::Unknown);
}
TEST_CASE("B03 affine footprint never retains a curvature bound after late invalidation", "[Nonplanar][B03][UpperCurvature]")
{
    UpperProjectionLimits projection_limits; projection_limits.max_slope=2;
    const auto projection=analyze_upper_projection(gridded_plate(3,false,true),true,9,projection_limits);
    REQUIRE(projection.snapshot);
    const UpperFootprintQuery query{0,{0.5,0.5},{0.5,2.5},0.125,0,0};
    size_t calls=0;
    UpperFootprintLimits limits;
    limits.is_current=[&](uint64_t) { ++calls; return true; };
    REQUIRE(check_affine_upper_footprint(projection.snapshot,query,limits).nominal_curvature_upper_mm_inv==0);
    const auto final_call=calls; REQUIRE(final_call>6); calls=0;
    limits.is_current=[&](uint64_t) { return ++calls<final_call; };
    const auto stale=check_affine_upper_footprint(projection.snapshot,query,limits);
    REQUIRE(stale.status==UpperFootprintStatus::Unknown);
    REQUIRE(stale.reason=="STALE_REVISION");
    REQUIRE_FALSE(stale.nominal_curvature_upper_mm_inv);
    REQUIRE_FALSE(stale.nominal_heights);
    calls=0; limits={}; limits.cancelled=[&] { return ++calls==final_call; };
    const auto cancelled=check_affine_upper_footprint(projection.snapshot,query,limits);
    REQUIRE(cancelled.status==UpperFootprintStatus::Unknown);
    REQUIRE(cancelled.reason=="CANCELLED");
    REQUIRE_FALSE(cancelled.nominal_curvature_upper_mm_inv);
    REQUIRE_FALSE(cancelled.nominal_heights);
    limits={}; limits.timeout=std::chrono::milliseconds(1);
    limits.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(3)); return false; };
    const auto expired=check_affine_upper_footprint(projection.snapshot,query,limits);
    REQUIRE(expired.status==UpperFootprintStatus::Unknown);
    REQUIRE(expired.reason=="DEADLINE");
    REQUIRE_FALSE(expired.nominal_curvature_upper_mm_inv);
    REQUIRE_FALSE(expired.nominal_heights);
}
TEST_CASE("B03 affine endpoint heights and gradients enclose independent dyadic and rational planes", "[Nonplanar][B03][UpperHeights]")
{
    for (bool rational : {false,true}) {
        auto source=rational ? make_cube(3,3,2) : wedge();
        if (rational) for (auto &v : source.its.vertices) if (v.z()>0) v.z()+=v.x()/3;
        UpperProjectionLimits limits; limits.max_slope=.5;
        const auto projection=analyze_upper_projection(source,true,72,limits);
        REQUIRE(projection.snapshot);
        for (const auto &endpoints : std::vector<std::pair<Vec2d,Vec2d>>{
            {{.5,.5},{2.5,2.5}},{{2.5,2.5},{.5,.5}},{{1.5,1.5},{1.5,1.5}}}) {
            const UpperFootprintQuery query{0,endpoints.first,endpoints.second,.125,0,0};
            const auto result=check_affine_upper_footprint(projection.snapshot,query);
            INFO(result.reason); REQUIRE(result.status==UpperFootprintStatus::Contained);
            REQUIRE(result.nominal_heights);
            const auto &height=*result.nominal_heights;
            REQUIRE(height.revision==72);
            REQUIRE(height.reference_mesh_face<projection.snapshot->geometry->its.indices.size());
            const long double divisor=rational ? 3.L : 8.L;
            const auto encloses=[](const std::array<double,2> &range,long double expected) {
                CHECK(range[0]<=expected); CHECK(range[1]>=expected);
                CHECK(range[1]-range[0]<1e-12);
            };
            encloses(height.start_z_mm,2.L+static_cast<long double>(endpoints.first.x())/divisor);
            encloses(height.end_z_mm,2.L+static_cast<long double>(endpoints.second.x())/divisor);
            encloses(height.gradient_x,1.L/divisor); encloses(height.gradient_y,0.L);
            REQUIRE_FALSE(check_upper_footprint(projection.snapshot,query).nominal_heights);
        }
    }
}
TEST_CASE("B03 local affine height selects the actual sheet and never crosses a crease", "[Nonplanar][B03][UpperHeights]")
{
    UpperProjectionLimits limits; limits.max_slope=2;
    const auto projection=analyze_upper_projection(gridded_plate(3,false,true),true,73,limits);
    REQUIRE(projection.snapshot);
    const auto ramp=check_affine_upper_footprint(projection.snapshot,{0,{1.25,.5},{1.75,2.5},.125,0,0});
    REQUIRE(ramp.nominal_heights);
    REQUIRE(ramp.nominal_heights->start_z_mm[0]<=2.25);
    REQUIRE(ramp.nominal_heights->start_z_mm[1]>=2.25);
    REQUIRE(ramp.nominal_heights->end_z_mm[0]<=2.75);
    REQUIRE(ramp.nominal_heights->end_z_mm[1]>=2.75);
    REQUIRE(ramp.nominal_heights->gradient_x[0]<=1);
    REQUIRE(ramp.nominal_heights->gradient_x[1]>=1);
    for (const auto &query : {UpperFootprintQuery{0,{.5,1.5},{2.5,1.5},.125,0,0},
                              UpperFootprintQuery{0,{.5,1.5},{3.,1.5},.125,0,0}}) {
        const auto result=check_affine_upper_footprint(projection.snapshot,query);
        REQUIRE(result.status!=UpperFootprintStatus::Contained);
        REQUIRE_FALSE(result.nominal_heights);
        REQUIRE_FALSE(result.nominal_curvature_upper_mm_inv);
    }
}
