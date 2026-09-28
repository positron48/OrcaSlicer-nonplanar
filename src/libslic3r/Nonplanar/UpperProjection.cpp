#include "UpperProjection.hpp"
#include "UpperProjectionPredicates.hpp"
#include "Interval.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <optional>
#include <set>

namespace Slic3r::nptop {
namespace {
template<class Stop> std::optional<std::vector<UpperPatch>> build_patches(
    const indexed_triangle_set &mesh, const std::vector<UpperFacet> &facets, Stop &&stop, std::string &reason)
{
    using Edge = std::pair<int,int>;
    const auto edge_key=[](int a,int b) { return Edge{std::min(a,b),std::max(a,b)}; };
    std::map<Edge,std::vector<size_t>> incidence;
    std::vector<std::vector<size_t>> adjacent(facets.size());
    for (size_t face=0; face<facets.size(); ++face) if (facets[face].within_slope_limit) {
        if (stop()) return {};
        const auto &ids=mesh.indices[facets[face].mesh_face];
        for (int edge=0; edge<3; ++edge) incidence[edge_key(ids(edge),ids((edge+1)%3))].push_back(face);
    }
    for (const auto &[edge,uses] : incidence) {
        if (stop()) return {};
        if (uses.size()>2) { reason="NONMANIFOLD_MASK_EDGE"; return {}; }
        if (uses.size()==2) {
            adjacent[uses[0]].push_back(uses[1]); adjacent[uses[1]].push_back(uses[0]);
        }
    }
    struct BoundaryEdge { size_t from, to, patch, loop, order, loop_size; };
    std::vector<BoundaryEdge> boundary_edges;
    std::vector<bool> visited(facets.size(),false);
    std::vector<UpperPatch> patches;
    using detail::Interval;
    for (size_t seed=0; seed<facets.size(); ++seed) if (facets[seed].within_slope_limit && !visited[seed]) {
        UpperPatch patch;
        Interval area(0), boundary_area(0);
        std::vector<size_t> todo{seed}; visited[seed]=true;
        std::map<int,int> next;
        std::set<int> incoming;
        while (!todo.empty()) {
            if (stop()) return {};
            const auto index=todo.back(); todo.pop_back();
            const auto &facet=facets[index];
            patch.mesh_faces.push_back(facet.mesh_face);
            patch.slope_upper=std::max(patch.slope_upper,facet.slope_upper);
            area=area+Interval(facet.xy_area_lower_mm2,facet.xy_area_upper_mm2);
            for (auto neighbor : adjacent[index]) if (!visited[neighbor]) { visited[neighbor]=true; todo.push_back(neighbor); }
            const auto &ids=mesh.indices[facet.mesh_face];
            for (int vertex=0; vertex<3; ++vertex) {
                const double z=mesh.vertices[ids(vertex)].z();
                patch.minimum_z_mm=std::min(patch.minimum_z_mm,z);
                patch.maximum_z_mm=std::max(patch.maximum_z_mm,z);
            }
            for (int edge=0; edge<3; ++edge) {
                const int from=ids(edge), to=ids((edge+1)%3);
                const auto &uses=incidence.at(edge_key(from,to));
                if (uses.size()==2 && uses.front()==index) {
                    const auto other=facets[uses.back()].mesh_face;
                    const auto &neighbor=mesh.indices[other];
                    if (!detail::coplanar_faces({mesh.vertices[ids(0)],mesh.vertices[ids(1)],mesh.vertices[ids(2)]},
                        {mesh.vertices[neighbor(0)],mesh.vertices[neighbor(1)],mesh.vertices[neighbor(2)]}))
                        patch.creases.push_back({{size_t(from),size_t(to)},{facet.mesh_face,other}});
                }
                if (uses.size()==1 &&
                    (!next.emplace(from,to).second || !incoming.insert(to).second)) {
                    reason="AMBIGUOUS_MASK_BOUNDARY"; return {};
                }
            }
        }
        if (next.empty()) { reason="MISSING_MASK_BOUNDARY"; return {}; }
        for (const auto &[from,to] : next)
            if (!incoming.count(from) || !next.count(to)) { reason="OPEN_MASK_BOUNDARY"; return {}; }
        size_t outer_loops=0;
        while (!next.empty()) {
            std::vector<size_t> vertices;
            const int first=next.begin()->first;
            int current=first;
            do {
                if (stop()) return {};
                auto edge=next.find(current);
                if (edge==next.end()) { reason="OPEN_MASK_BOUNDARY"; return {}; }
                vertices.push_back(size_t(current));
                current=edge->second; next.erase(edge);
            } while (current!=first);
            if (vertices.size()<3) { reason="DEGENERATE_MASK_BOUNDARY"; return {}; }
            Interval twice_area(0);
            const auto &origin=mesh.vertices[vertices.front()];
            for (size_t i=0; i<vertices.size(); ++i) {
                if (stop()) return {};
                const auto &a=mesh.vertices[vertices[i]], &b=mesh.vertices[vertices[(i+1)%vertices.size()]];
                const auto ax=Interval(double(a.x()))-Interval(double(origin.x()));
                const auto ay=Interval(double(a.y()))-Interval(double(origin.y()));
                const auto bx=Interval(double(b.x()))-Interval(double(origin.x()));
                const auto by=Interval(double(b.y()))-Interval(double(origin.y()));
                twice_area=twice_area+ax*by-ay*bx;
                boundary_edges.push_back({vertices[i],vertices[(i+1)%vertices.size()],patches.size(),
                                          patch.boundaries.size(),i,vertices.size()});
            }
            const auto signed_area=twice_area/Interval(2);
            if (signed_area.lo<=0 && signed_area.hi>=0) { reason="UNRESOLVED_MASK_AREA"; return {}; }
            const bool hole=signed_area.hi<0;
            if (!hole) ++outer_loops;
            boundary_area=boundary_area+signed_area;
            patch.boundaries.push_back({std::move(vertices),hole,signed_area.lo,signed_area.hi});
        }
        if (outer_loops!=1 || boundary_area.lo>area.hi || boundary_area.hi<area.lo) {
            reason="MASK_AREA_OR_OWNERSHIP_MISMATCH"; return {};
        }
        std::sort(patch.mesh_faces.begin(),patch.mesh_faces.end());
        if (patch.creases.empty()) patch.nominal_curvature_upper_mm_inv=0.;
        patch.xy_area_lower_mm2=std::max(0.,area.lo); patch.xy_area_upper_mm2=area.hi;
        patches.push_back(std::move(patch));
    }
    for (size_t i=0; i<boundary_edges.size(); ++i)
        for (size_t j=i+1; j<boundary_edges.size(); ++j) {
            if (stop()) return {};
            const auto &a=boundary_edges[i], &b=boundary_edges[j];
            const bool neighbors=a.patch==b.patch && a.loop==b.loop &&
                ((a.order+1)%a.loop_size==b.order || (b.order+1)%b.loop_size==a.order);
            if (detail::projected_boundary_conflict(mesh.vertices[a.from],mesh.vertices[a.to],
                                                   mesh.vertices[b.from],mesh.vertices[b.to],neighbors)) {
                reason="INTERSECTING_MASK_BOUNDARY"; return {};
            }
        }
    return patches;
}
}

UpperProjectionResult analyze_upper_projection(const TriangleMesh &source, bool millimeters_declared,
                                               uint64_t revision, const UpperProjectionLimits &requested_limits)
{
    const UpperProjectionLimits limits=requested_limits;
    UpperProjectionResult result;
    const auto started=std::chrono::steady_clock::now();
    const auto stop=[&] {
        if (limits.geometry.cancelled && limits.geometry.cancelled()) { result.reason="CANCELLED"; return true; }
        if (limits.is_current && !limits.is_current(revision)) { result.reason="STALE_REVISION"; return true; }
        detail::require_interval_environment();
        if (std::chrono::steady_clock::now()-started >= limits.geometry.timeout) { result.reason="DEADLINE"; return true; }
        return false;
    };
    try {
        detail::require_interval_environment();
        if (revision==0 || !limits.geometry.valid() || !std::isfinite(limits.max_slope) || limits.max_slope<0 ||
            limits.max_upward_faces==0 || limits.max_upward_faces>5000) {
            result.reason="INVALID_LIMITS"; return result;
        }
        auto audit_limits=limits.geometry;
        audit_limits.cancelled=stop;
        // audit_mesh owns its copy before invoking this cancellation wrapper.
        // No source or requested-limits reads follow that boundary.
        const auto audited=audit_mesh(source,millimeters_declared,audit_limits);
        if (audited.status!=MeshAuditStatus::ValidGeometry || !audited.normalized) {
            if (result.reason.empty()) result.reason=audited.reason;
            if (audited.status==MeshAuditStatus::Invalid) result.status=UpperProjectionStatus::Invalid;
            return result;
        }
        if (stop()) return result;
        const auto &mesh=audited.normalized->its;
        struct Projection {
            detail::ProjectionTriangle vertices;
            double min_x, max_x, min_y, max_y;
        };
        std::vector<Projection> projections;
        std::vector<UpperFacet> facets;
        using detail::Interval;
        Interval area(0), filtered_area(0);
        double minimum_z=std::numeric_limits<double>::infinity(), maximum_z=-minimum_z;
        for (size_t face=0; face<mesh.indices.size(); ++face) {
            if (stop()) return result;
            const auto &ids=mesh.indices[face];
            const detail::ProjectionTriangle vertices{mesh.vertices[ids(0)],mesh.vertices[ids(1)],mesh.vertices[ids(2)]};
            if (detail::projection_orientation(vertices)<=0) continue;
            if (facets.size()>=limits.max_upward_faces) { result.reason="UPWARD_FACE_LIMIT"; return result; }
            std::array<Interval,3> a{Interval(0),Interval(0),Interval(0)}, b=a;
            for (int axis=0; axis<3; ++axis) {
                a[axis]=Interval(double(vertices[1](axis)))-Interval(double(vertices[0](axis)));
                b[axis]=Interval(double(vertices[2](axis)))-Interval(double(vertices[0](axis)));
            }
            const std::array<Interval,3> normal{a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
            if (normal[2].lo<=0) { result.reason="UNRESOLVED_UPWARD_NORMAL"; return result; }
            const Interval face_area=normal[2]/Interval(2);
            const bool horizontal=vertices[0].z()==vertices[1].z() && vertices[0].z()==vertices[2].z();
            const double slope=horizontal ? 0. : (detail::root(detail::square(normal[0])+detail::square(normal[1]))/normal[2]).hi;
            const bool within_limit=slope<=limits.max_slope;
            facets.push_back({face,face_area.lo,face_area.hi,slope,within_limit});
            area=area+face_area;
            if (within_limit) filtered_area=filtered_area+face_area;
            Projection projection{vertices,vertices[0].x(),vertices[0].x(),vertices[0].y(),vertices[0].y()};
            for (const auto &v : vertices) {
                projection.min_x=std::min(projection.min_x,double(v.x()));
                projection.max_x=std::max(projection.max_x,double(v.x()));
                projection.min_y=std::min(projection.min_y,double(v.y()));
                projection.max_y=std::max(projection.max_y,double(v.y()));
                minimum_z=std::min(minimum_z,double(v.z()));
                maximum_z=std::max(maximum_z,double(v.z()));
            }
            projections.push_back(std::move(projection));
        }
        if (facets.empty()) { result.reason="NO_UPWARD_FACETS"; return result; }
        for (size_t i=0; i<projections.size(); ++i)
            for (size_t j=i+1; j<projections.size(); ++j) {
                if (stop()) return result;
                const auto &a=projections[i], &b=projections[j];
                // Exact comparisons of binary32 coordinates represented in
                // binary64: touching AABBs cannot have overlapping interiors.
                if (a.max_x<=b.min_x || b.max_x<=a.min_x || a.max_y<=b.min_y || b.max_y<=a.min_y) continue;
                if (detail::projection_interiors_overlap(a.vertices,b.vertices)) {
                    result.reason="OVERLAPPING_UPWARD_PROJECTIONS"; return result;
                }
            }
        auto patches=build_patches(mesh,facets,stop,result.reason);
        if (!patches) return result;
        auto snapshot=std::shared_ptr<const UpperProjectionSnapshot>(new UpperProjectionSnapshot(
            audited.normalized,revision,std::move(facets),std::max(0.,area.lo),area.hi,
            std::max(0.,filtered_area.lo),filtered_area.hi,minimum_z,maximum_z,std::move(*patches)));
        if (stop()) return result;
        result.snapshot=std::move(snapshot);
        result.status=UpperProjectionStatus::NominalHeightfield;
        result.reason="NOMINAL_UPPER_PROJECTION_ONLY";
    } catch (const std::exception &) {
        result.snapshot.reset();
        result.status=UpperProjectionStatus::Unknown;
        result.reason="PROJECTION_EXCEPTION";
    }
    return result;
}

namespace {
UpperFootprintResult check_footprint(std::shared_ptr<const UpperProjectionSnapshot> snapshot,
    const UpperFootprintQuery &requested_query, const UpperFootprintLimits &requested_limits, bool require_affine)
{
    const UpperFootprintQuery query=requested_query;
    const UpperFootprintLimits limits=requested_limits;
    UpperFootprintResult result;
    const auto started=std::chrono::steady_clock::now();
    const auto stop=[&] {
        if (limits.cancelled && limits.cancelled()) { result.reason="CANCELLED"; return true; }
        if (limits.is_current && !limits.is_current(snapshot->revision)) { result.reason="STALE_REVISION"; return true; }
        detail::require_interval_environment();
        if (std::chrono::steady_clock::now()-started>=limits.timeout) { result.reason="DEADLINE"; return true; }
        return false;
    };
    try {
        detail::require_interval_environment();
        const auto bounded=[](double value) { return std::isfinite(value) && std::abs(value)<=10000; };
        if (!snapshot || query.patch>=snapshot->slope_patches.size() || limits.timeout.count()<=0 ||
            !bounded(query.start_mm.x()) || !bounded(query.start_mm.y()) ||
            !bounded(query.end_mm.x()) || !bounded(query.end_mm.y()) ||
            !bounded(query.xy_radius_mm) || query.xy_radius_mm<=0 ||
            !bounded(query.boundary_uncertainty_mm) || query.boundary_uncertainty_mm<0 ||
            !bounded(query.transition_inset_mm) || query.transition_inset_mm<0) {
            result.reason="INVALID_FOOTPRINT_QUERY"; return result;
        }
        detail::Interval inset(query.xy_radius_mm);
        for (double term : {query.boundary_uncertainty_mm,query.transition_inset_mm})
            if (term!=0) inset=inset+detail::Interval(term);
        if (inset.hi>10000) { result.reason="FOOTPRINT_INSET_LIMIT"; return result; }
        result.required_inset_upper_mm=inset.hi;
        if (stop()) return result;
        bool contained=true;
        const char *reason="NOMINAL_XY_FOOTPRINT_ONLY";
        for (const auto &boundary : snapshot->slope_patches[query.patch].boundaries) {
            if (stop()) return result;
            std::vector<Vec3f> vertices;
            vertices.reserve(boundary.mesh_vertices.size());
            for (auto id : boundary.mesh_vertices) vertices.push_back(snapshot->geometry->its.vertices[id]);
            const int side=detail::projected_bounded_side(vertices,query.start_mm);
            if (side!=(boundary.hole ? -1 : 1)) {
                contained=false; reason="CENTER_OUTSIDE_PATCH"; break;
            }
            for (size_t i=0; i<vertices.size(); ++i) {
                if (stop()) return result;
                if (!detail::projected_clearance_exceeds(query.start_mm,query.end_mm,vertices[i],
                                                        vertices[(i+1)%vertices.size()],inset.hi)) {
                    contained=false; reason="FOOTPRINT_REACHES_BOUNDARY"; break;
                }
            }
            if (!contained) break;
        }
        // Start membership plus positive whole-segment boundary clearance
        // implies that the connected capsule stays inside this one patch.
        if (contained && require_affine) {
            const auto &vertices=snapshot->geometry->its.vertices;
            for (const auto &crease : snapshot->slope_patches[query.patch].creases) {
                if (stop()) return result;
                if (!detail::projected_clearance_exceeds(query.start_mm,query.end_mm,
                        vertices[crease.mesh_vertices[0]],vertices[crease.mesh_vertices[1]],inset.hi)) {
                    if (!stop()) result.reason="UNQUALIFIED_CREASE_IN_FOOTPRINT";
                    return result;
                }
            }
        }
        if (stop()) return result;
        result.status=contained ? UpperFootprintStatus::Contained : UpperFootprintStatus::Outside;
        result.reason=reason;
        if (contained && require_affine) {
            result.nominal_curvature_upper_mm_inv=0.;
            result.reason="NOMINAL_AFFINE_FOOTPRINT_ONLY";
        }
    } catch (const std::exception &) {
        result.reason="FOOTPRINT_EXCEPTION";
    } catch (...) {
        result.reason="FOOTPRINT_EXCEPTION";
    }
    return result;
}
}
UpperFootprintResult check_upper_footprint(std::shared_ptr<const UpperProjectionSnapshot> snapshot,
    const UpperFootprintQuery &query, const UpperFootprintLimits &limits)
{
    return check_footprint(std::move(snapshot),query,limits,false);
}
UpperFootprintResult check_affine_upper_footprint(std::shared_ptr<const UpperProjectionSnapshot> snapshot,
    const UpperFootprintQuery &query, const UpperFootprintLimits &limits)
{
    return check_footprint(std::move(snapshot),query,limits,true);
}
}
