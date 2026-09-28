#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/MeshAudit.hpp>
#include <limits>

using namespace Slic3r;
using namespace Slic3r::nptop;
TEST_CASE("B02 mesh audit preserves source and accepts a bounded native cube geometry", "[Nonplanar][B02][MeshAudit]")
{
    const auto source=make_cube(20,10,2);
    const auto before=source.its;
    const auto result=audit_mesh(source,true);
    INFO(result.reason);
    REQUIRE(result.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(result.normalized);
    REQUIRE(result.components==1);
    REQUIRE(result.volume_lower_mm3<=400);
    REQUIRE(result.volume_upper_mm3>=400);
    REQUIRE(result.volume_upper_mm3-result.volume_lower_mm3<1e-8);
    REQUIRE(result.normalized.get()!=&source);
    REQUIRE(source.its.indices==before.indices);
    REQUIRE(source.its.vertices==before.vertices);
}
TEST_CASE("B02 mesh audit rejects missing faces duplicates orientation and invalid coordinates", "[Nonplanar][B02][MeshAudit]")
{
    auto rejected=[](const TriangleMesh &mesh) {
        const auto result=audit_mesh(mesh,true);
        REQUIRE(result.status!=MeshAuditStatus::ValidGeometry);
        REQUIRE_FALSE(result.normalized);
    };
    auto mesh=make_cube(20,10,2); mesh.its.indices.pop_back(); rejected(mesh);
    mesh=make_cube(20,10,2); mesh.its.indices.push_back(mesh.its.indices.front()); rejected(mesh);
    mesh=make_cube(20,10,2); std::swap(mesh.its.indices.front()(0),mesh.its.indices.front()(1)); rejected(mesh);
    mesh=make_cube(20,10,2); mesh.flip_triangles(); rejected(mesh);
    mesh=make_cube(20,10,2); mesh.its.indices.front()(0)=-1; rejected(mesh);
    mesh=make_cube(20,10,2); mesh.its.indices.front()(0)=100000; rejected(mesh);
    mesh=make_cube(20,10,2); mesh.its.vertices.front().x()=std::numeric_limits<float>::quiet_NaN(); rejected(mesh);
    mesh=make_cube(20,10,2); mesh.its.vertices.front().y()=std::numeric_limits<float>::infinity(); rejected(mesh);
    mesh=make_cube(20,10,2); mesh.its.indices.front()(0)=mesh.its.indices.front()(1); rejected(mesh);
    rejected(TriangleMesh{});
}
TEST_CASE("B02 exact vertex normalization changes only the owned derived mesh", "[Nonplanar][B02][MeshAudit]")
{
    auto source=make_cube(20,10,2);
    const auto face=source.its.indices.front();
    for (int i=0; i<3; ++i) {
        const auto copy=source.its.vertices[face(i)];
        source.its.indices.front()(i)=int(source.its.vertices.size());
        source.its.vertices.push_back(copy);
    }
    const auto before=source.its;
    const auto result=audit_mesh(source,true);
    INFO(result.reason);
    REQUIRE(result.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(result.exact_duplicate_vertices_removed==3);
    REQUIRE(source.its.vertices==before.vertices);
    REQUIRE(source.its.indices==before.indices);
}
TEST_CASE("B02 mesh audit fails closed on units cancellation and finite work limits", "[Nonplanar][B02][MeshAudit]")
{
    const auto mesh=make_cube(20,10,2);
    REQUIRE(audit_mesh(mesh,false).reason=="UNCONFIRMED_UNITS");
    MeshAuditLimits limits; limits.max_faces=1;
    REQUIRE(audit_mesh(mesh,true,limits).reason=="RESOURCE_LIMIT");
    limits={}; limits.timeout=std::chrono::milliseconds(0);
    REQUIRE(audit_mesh(mesh,true,limits).reason=="INVALID_LIMITS");
    limits={}; limits.max_faces=5001;
    REQUIRE(audit_mesh(mesh,true,limits).reason=="INVALID_LIMITS");
    limits={}; limits.max_coordinate_mm=1;
    REQUIRE(audit_mesh(mesh,true,limits).reason=="COORDINATE_DOMAIN");
    limits={}; limits.cancelled=[] { return true; };
    const auto cancelled=audit_mesh(mesh,true,limits);
    REQUIRE(cancelled.reason=="CANCELLED");
    REQUIRE_FALSE(cancelled.normalized);
    limits.cancelled=[]() -> bool { throw std::runtime_error("interrupted callback"); };
    const auto interrupted=audit_mesh(mesh,true,limits);
    REQUIRE(interrupted.status==MeshAuditStatus::Unknown);
    REQUIRE(interrupted.reason=="GEOMETRY_EXCEPTION");
    REQUIRE_FALSE(interrupted.normalized);
}
TEST_CASE("B02 separate and overlapping closed shells are not silently united", "[Nonplanar][B02][MeshAudit]")
{
    for (double offset : {10.,30.}) {
        auto source=make_cube(20,20,20), other=make_cube(20,20,20);
        other.translate(float(offset),0,0); source.merge(other);
        const auto result=audit_mesh(source,true);
        REQUIRE(result.status==MeshAuditStatus::Invalid);
        REQUIRE(result.reason=="MULTIPLE_COMPONENTS");
        REQUIRE_FALSE(result.normalized);
    }
}
TEST_CASE("B02 closed connected bowtie prism is rejected for self intersection", "[Nonplanar][B02][MeshAudit]")
{
    // Extruded bowtie has positive signed volume 1.5 and closed, consistently
    // oriented abstract topology. Opposite walls cross at (1.2,1.2,z).
    indexed_triangle_set mesh;
    mesh.vertices={{0,0,0},{3,3,0},{0,3,0},{2,0,0},
                   {0,0,1},{3,3,1},{0,3,1},{2,0,1}};
    mesh.indices={{0,2,1},{0,3,2},{4,5,6},{4,6,7}};
    for (int i=0; i<4; ++i) {
        const int j=(i+1)%4;
        mesh.indices.emplace_back(i,j,j+4);
        mesh.indices.emplace_back(i,j+4,i+4);
    }
    const auto result=audit_mesh(TriangleMesh(std::move(mesh)),true);
    INFO(result.reason);
    REQUIRE(result.status==MeshAuditStatus::Invalid);
    REQUIRE(result.components==1);
    REQUIRE(result.volume_lower_mm3>0);
    REQUIRE(result.reason=="SELF_INTERSECTION");
    REQUIRE_FALSE(result.normalized);
}
