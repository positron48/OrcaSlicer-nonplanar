#include "UpperProjection.hpp"
#include "UpperProjectionPredicates.hpp"
#include "Interval.hpp"
#include <algorithm>
#include <array>
#include <limits>

namespace Slic3r::nptop {
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
        auto snapshot=std::make_shared<const UpperProjectionSnapshot>(UpperProjectionSnapshot{
            audited.normalized,revision,std::move(facets),std::max(0.,area.lo),area.hi,
            std::max(0.,filtered_area.lo),filtered_area.hi,minimum_z,maximum_z});
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
}
