#include "MeshAudit.hpp"
#include "Interval.hpp"
#include "../MeshBoolean.hpp"
#include <array>
#include <map>
#include <set>
#include <vector>

namespace Slic3r::nptop {
namespace {
using Triangle = std::array<float,9>;
std::vector<Triangle> triangles(const indexed_triangle_set &mesh)
{
    std::vector<Triangle> result;
    result.reserve(mesh.indices.size());
    for (const auto &face : mesh.indices) {
        Triangle canonical{};
        for (int rotation=0; rotation<3; ++rotation) {
            Triangle candidate{};
            for (int i=0; i<3; ++i)
                for (int axis=0; axis<3; ++axis)
                    candidate[i*3+axis]=mesh.vertices.at(face((i+rotation)%3))(axis);
            if (rotation==0 || candidate<canonical) canonical=candidate;
        }
        result.push_back(canonical);
    }
    std::sort(result.begin(),result.end());
    return result;
}
}

MeshAuditResult audit_mesh(const TriangleMesh &source, bool millimeters_declared, const MeshAuditLimits &limits)
{
    MeshAuditResult result;
    const auto started=std::chrono::steady_clock::now();
    auto stop = [&] {
        if (limits.cancelled && limits.cancelled()) { result.reason="CANCELLED"; return true; }
        if (std::chrono::steady_clock::now()-started >= limits.timeout) { result.reason="DEADLINE"; return true; }
        return false;
    };
    auto invalid = [&](const char *reason) {
        result.status=MeshAuditStatus::Invalid; result.reason=reason; return result;
    };
    if (!millimeters_declared) { result.reason="UNCONFIRMED_UNITS"; return result; }
    if (limits.max_faces==0 || limits.max_faces>5000 || limits.max_vertices==0 || limits.max_vertices>15000 ||
        !std::isfinite(limits.max_coordinate_mm) ||
        limits.max_coordinate_mm<=0 || limits.max_coordinate_mm>10000 || limits.timeout.count()<=0) {
        result.reason="INVALID_LIMITS"; return result;
    }
    if (source.its.indices.size()>limits.max_faces || source.its.vertices.size()>limits.max_vertices) {
        result.reason="RESOURCE_LIMIT"; return result;
    }
    if (source.its.empty()) return invalid("EMPTY_MESH");
    try {
        detail::require_interval_environment();
        auto mesh=source.its;
        for (const auto &vertex : mesh.vertices) {
            if (stop()) return result;
            for (int axis=0; axis<3; ++axis) {
                if (!std::isfinite(vertex(axis))) return invalid("NONFINITE_VERTEX");
                if (std::abs(double(vertex(axis)))>limits.max_coordinate_mm) {
                    result.reason="COORDINATE_DOMAIN"; return result;
                }
            }
        }
        for (const auto &face : mesh.indices)
            for (int i=0; i<3; ++i)
                if (face(i)<0 || size_t(face(i))>=mesh.vertices.size()) return invalid("INVALID_INDEX");

        // Native normalization merges only equal coordinates, never a
        // tolerance neighborhood. Keep the original mesh and its stats untouched.
        result.exact_duplicate_vertices_removed=its_merge_vertices(mesh);
        struct Edge { std::vector<size_t> faces; int balance=0; };
        std::map<std::pair<int,int>,Edge> edges;
        std::set<std::array<int,3>> faces;
        std::vector<std::vector<size_t>> incident(mesh.vertices.size()), adjacent(mesh.indices.size());
        using detail::Interval;
        Interval volume(0);
        const Vec3d origin=mesh.vertices.front().cast<double>();
        for (size_t index=0; index<mesh.indices.size(); ++index) {
            if (stop()) return result;
            const auto &face=mesh.indices[index];
            std::array<int,3> sorted{face(0),face(1),face(2)};
            std::sort(sorted.begin(),sorted.end());
            if (sorted[0]==sorted[1] || sorted[1]==sorted[2]) return invalid("DEGENERATE_FACE");
            if (!faces.insert(sorted).second) return invalid("DUPLICATE_FACE");
            using IntervalPoint = std::array<Interval,3>;
            std::array<IntervalPoint,3> point{
                IntervalPoint{Interval(0),Interval(0),Interval(0)},
                IntervalPoint{Interval(0),Interval(0),Interval(0)},
                IntervalPoint{Interval(0),Interval(0),Interval(0)}};
            for (int i=0; i<3; ++i)
                for (int axis=0; axis<3; ++axis)
                    point[i][axis]=Interval(double(mesh.vertices[face(i)](axis)))-Interval(origin(axis));
            auto cross = [](const auto &a, const auto &b) {
                return std::array<Interval,3>{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
            };
            const std::array<Interval,3> a{point[1][0]-point[0][0],point[1][1]-point[0][1],point[1][2]-point[0][2]};
            const std::array<Interval,3> b{point[2][0]-point[0][0],point[2][1]-point[0][1],point[2][2]-point[0][2]};
            const auto normal=cross(a,b);
            const auto area_squared=detail::square(normal[0])+detail::square(normal[1])+detail::square(normal[2]);
            if (area_squared.lo<=0) { result.reason="DEGENERATE_OR_UNRESOLVED_AREA"; return result; }
            const auto determinant=cross(point[1],point[2]);
            volume=volume+(point[0][0]*determinant[0]+point[0][1]*determinant[1]+point[0][2]*determinant[2])/Interval(6);
            for (int i=0; i<3; ++i) {
                incident[face(i)].push_back(index);
                int u=face(i),v=face((i+1)%3);
                auto &edge=edges[{std::min(u,v),std::max(u,v)}];
                edge.faces.push_back(index); edge.balance += u<v ? 1 : -1;
            }
        }
        for (const auto &entry : edges) {
            const auto &edge=entry.second;
            if (edge.faces.size()!=2) return invalid("OPEN_OR_NONMANIFOLD_EDGE");
            if (edge.balance!=0) return invalid("INCONSISTENT_ORIENTATION");
            adjacent[edge.faces[0]].push_back(edge.faces[1]);
            adjacent[edge.faces[1]].push_back(edge.faces[0]);
        }
        std::vector<bool> visited(mesh.indices.size());
        for (size_t first=0; first<visited.size(); ++first) {
            if (visited[first]) continue;
            ++result.components;
            std::vector<size_t> todo{first}; visited[first]=true;
            while (!todo.empty()) {
                if (stop()) return result;
                const auto face=todo.back(); todo.pop_back();
                for (auto neighbor : adjacent[face])
                    if (!visited[neighbor]) { visited[neighbor]=true; todo.push_back(neighbor); }
            }
        }
        if (result.components!=1) return invalid("MULTIPLE_COMPONENTS");
        for (size_t vertex=0; vertex<incident.size(); ++vertex) {
            if (stop()) return result;
            std::set<size_t> remaining(incident[vertex].begin(),incident[vertex].end());
            if (remaining.empty()) continue;
            std::vector<size_t> todo{*remaining.begin()}; remaining.erase(todo.front());
            while (!todo.empty()) {
                const auto face=todo.back(); todo.pop_back();
                for (auto neighbor : adjacent[face])
                    if (remaining.erase(neighbor)) todo.push_back(neighbor);
            }
            if (!remaining.empty()) return invalid("NONMANIFOLD_VERTEX");
        }
        result.volume_lower_mm3=volume.lo; result.volume_upper_mm3=volume.hi;
        if (volume.hi<0) return invalid("INWARD_ORIENTATION");
        if (volume.lo<=0) { result.reason="UNRESOLVED_VOLUME"; return result; }
        if (stop()) return result;
        const auto cgal=MeshBoolean::cgal::triangle_mesh_to_cgal(mesh);
        const auto roundtrip=MeshBoolean::cgal::cgal_to_indexed_triangle_set(*cgal);
        if (triangles(mesh)!=triangles(roundtrip)) return invalid("CGAL_CHANGED_GEOMETRY");
        if (stop()) return result;
        const bool intersects=MeshBoolean::cgal::does_self_intersect(*cgal);
        if (stop()) return result;
        if (intersects) return invalid("SELF_INTERSECTION");
        result.normalized=std::make_shared<const TriangleMesh>(std::move(mesh));
        result.reason="CLOSED_ORIENTED_SINGLE_COMPONENT";
        result.status=MeshAuditStatus::ValidGeometry;
    } catch (const std::exception &) {
        result.status=MeshAuditStatus::Unknown;
        result.normalized.reset();
        result.reason="GEOMETRY_EXCEPTION";
    }
    return result;
}
}
