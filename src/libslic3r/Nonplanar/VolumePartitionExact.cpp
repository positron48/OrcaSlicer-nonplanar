#include "VolumePartitionExact.hpp"
#include "Interval.hpp"
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Side_of_triangle_mesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <map>
#include <set>

namespace Slic3r::nptop::detail {
namespace {
using Kernel=CGAL::Exact_predicates_exact_constructions_kernel;
using Mesh=CGAL::Surface_mesh<Kernel::Point_3>;
using Triangle=std::array<size_t,3>;
namespace PMP=CGAL::Polygon_mesh_processing;
Mesh exact_mesh(const TriangleMesh &input, const std::function<void()> &stop)
{
    Mesh mesh;
    for (const auto &v : input.its.vertices) { stop(); mesh.add_vertex(Kernel::Point_3(double(v.x()),double(v.y()),double(v.z()))); }
    for (const auto &f : input.its.indices) {
        stop();
        if (mesh.add_face(Mesh::Vertex_index(f[0]),Mesh::Vertex_index(f[1]),Mesh::Vertex_index(f[2]))==Mesh::null_face())
            throw std::runtime_error("EXACT_MESH_CONSTRUCTION");
    }
    return mesh;
}
ScalarBounds scalar(const Kernel::FT &value)
{
    const auto interval=CGAL::to_interval(value);
    if (!std::isfinite(interval.first) || !std::isfinite(interval.second)) throw std::runtime_error("EXACT_VOLUME_DOMAIN");
    require_interval_environment(); return {interval.first,interval.second};
}
struct ConvertedPoint { Vec3f point; size_t id; };
using ConversionMap=std::map<Kernel::Point_3,ConvertedPoint,Kernel::Less_xyz_3>;
struct ConvertedMesh { TriangleMesh mesh; std::map<Triangle,Kernel::Point_3> faces; };
ConvertedMesh convert(const Mesh &input, ConversionMap &shared, double &error, const MeshAuditLimits &limits,
                      const std::function<void()> &stop)
{
    if (input.number_of_faces()>limits.max_faces || input.number_of_vertices()>limits.max_vertices)
        throw std::runtime_error("EXACT_OUTPUT_RESOURCE_LIMIT");
    indexed_triangle_set native;
    std::map<Mesh::Vertex_index,int> indices;
    std::map<Mesh::Vertex_index,size_t> identities;
    for (const auto index : input.vertices()) {
        stop(); const auto &p=input.point(index);
        auto existing=shared.find(p);
        if (existing==shared.end()) {
            const std::array<Kernel::FT,3> axes{p.x(),p.y(),p.z()}; Vec3f value; Interval displacement(0);
            for (size_t axis=0; axis<3; ++axis) {
                const auto interval=CGAL::to_interval(axes[axis]);
                value[axis]=float(CGAL::to_double(axes[axis]));
                require_interval_environment();
                if (!std::isfinite(value[axis]) || std::abs(double(value[axis]))>limits.max_coordinate_mm ||
                    !std::isfinite(interval.first) || !std::isfinite(interval.second)) throw std::runtime_error("EXACT_POINT_DOMAIN");
                const auto delta=Interval(interval.first,interval.second)-Interval(double(value[axis]));
                displacement=displacement+Interval(std::max(std::abs(delta.lo),std::abs(delta.hi)));
            }
            error=std::max(error,displacement.hi);
            existing=shared.emplace(p,ConvertedPoint{value,shared.size()}).first;
        }
        indices.emplace(index,int(native.vertices.size())); identities.emplace(index,existing->second.id);
        native.vertices.push_back(existing->second.point);
    }
    std::map<Triangle,Kernel::Point_3> faces;
    for (const auto face : input.faces()) {
        stop(); std::array<Mesh::Vertex_index,3> vertices; size_t count=0;
        for (const auto vertex : input.vertices_around_face(input.halfedge(face))) {
            if (count>=3) throw std::runtime_error("NONTRIANGULAR_EXACT_OUTPUT"); vertices[count++]=vertex;
        }
        if (count!=3) throw std::runtime_error("DEGENERATE_EXACT_OUTPUT");
        native.indices.emplace_back(indices.at(vertices[0]),indices.at(vertices[1]),indices.at(vertices[2]));
        Triangle key{identities.at(vertices[0]),identities.at(vertices[1]),identities.at(vertices[2])}; std::sort(key.begin(),key.end());
        const auto &a=input.point(vertices[0]), &b=input.point(vertices[1]), &c=input.point(vertices[2]);
        const Kernel::Point_3 center((a.x()+b.x()+c.x())/3,(a.y()+b.y()+c.y())/3,(a.z()+b.z()+c.z())/3);
        if (!faces.emplace(key,center).second) throw std::runtime_error("DUPLICATE_EXACT_OUTPUT_FACE");
    }
    return {TriangleMesh(std::move(native)),std::move(faces)};
}
}

ExactPartition exact_partition(const TriangleMesh &original, const TriangleMesh &reservation, const MeshAuditLimits &limits,
                               const std::function<void()> &stop)
{
    auto source=exact_mesh(original,stop), selection=exact_mesh(reservation,stop);
    const Mesh original_mesh=source;
    const auto original_volume=PMP::volume(source);
    Mesh body, cap;
    std::array<boost::optional<Mesh *>,4> outputs;
    outputs[PMP::Corefinement::INTERSECTION]=&cap;
    outputs[PMP::Corefinement::TM1_MINUS_TM2]=&body;
    stop();
    const auto statuses=PMP::corefine_and_compute_boolean_operations(source,selection,outputs,
        CGAL::parameters::throw_on_self_intersection(true),CGAL::parameters::throw_on_self_intersection(true));
    stop();
    if (!statuses[PMP::Corefinement::INTERSECTION] || !statuses[PMP::Corefinement::TM1_MINUS_TM2] ||
        cap.is_empty() || body.is_empty()) throw std::runtime_error("EMPTY_OR_FAILED_EXACT_PARTITION");
    const auto body_volume=PMP::volume(body), cap_volume=PMP::volume(cap);
    if (original_volume<=0 || body_volume<=0 || cap_volume<=0 || original_volume!=body_volume+cap_volume)
        throw std::runtime_error("EXACT_VOLUME_PARTITION_MISMATCH");
    double error=0; ConversionMap shared(Kernel().less_xyz_3_object());
    auto native_body=convert(body,shared,error,limits,stop), native_cap=convert(cap,shared,error,limits,stop);
    // Exact corefinement must also give the same interface triangulation. Share
    // native point conversions, and reject a differently triangulated internal
    // face before float rounding could turn it into a gap or overlap.
    CGAL::Side_of_triangle_mesh<Mesh,Kernel> side(original_mesh);
    size_t shared_faces=0;
    for (const auto &[key,center] : native_body.faces) {
        stop();
        if (native_cap.faces.count(key)) ++shared_faces;
        else if (side(center)!=CGAL::ON_BOUNDARY) throw std::runtime_error("BODY_INTERFACE_TRIANGULATION_MISMATCH");
    }
    for (const auto &[key,center] : native_cap.faces) {
        stop();
        if (!native_body.faces.count(key) && side(center)!=CGAL::ON_BOUNDARY)
            throw std::runtime_error("CAP_INTERFACE_TRIANGULATION_MISMATCH");
    }
    if (shared_faces==0) throw std::runtime_error("MISSING_SHARED_PARTITION_INTERFACE");
    stop();
    return {std::move(native_body.mesh),std::move(native_cap.mesh),scalar(original_volume),scalar(body_volume),
        scalar(cap_volume),error,shared_faces};
}
}
