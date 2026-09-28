#pragma once
#include "MeshAudit.hpp"
#include <cstdint>
#include <vector>

namespace Slic3r::nptop {
enum class UpperProjectionStatus { NominalHeightfield, Invalid, Unknown };
struct UpperFacet {
    size_t mesh_face;
    double xy_area_lower_mm2, xy_area_upper_mm2;
    double slope_upper;
    bool within_slope_limit;
};
struct UpperProjectionSnapshot {
    const std::shared_ptr<const TriangleMesh> geometry;
    const uint64_t revision;
    const std::vector<UpperFacet> upward_facets;
    const double xy_area_lower_mm2, xy_area_upper_mm2;
    const double filtered_area_lower_mm2, filtered_area_upper_mm2;
    const double minimum_z_mm, maximum_z_mm;
};
struct UpperProjectionLimits {
    MeshAuditLimits geometry;
    // A preliminary nominal slope filter, never a tool-accessibility limit.
    double max_slope = 0.2;
    size_t max_upward_faces = 2500;
    std::function<bool(uint64_t)> is_current;
};
struct UpperProjectionResult {
    UpperProjectionStatus status = UpperProjectionStatus::Unknown;
    std::string reason;
    std::shared_ptr<const UpperProjectionSnapshot> snapshot;
};
// Bounded internal geometry analysis in the supplied coordinate frame. Captures
// and audits the mesh; rejects overlapping upward projections rather than
// trimming away potentially hidden facets. Face IDs refer to the owned audited
// mesh, not CAD faces or an unrelated reload. Bounds describe nominal binary32
// geometry only: import uncertainty, curvature, footprint, tool/scene access,
// transitions, physical qualification and export approval are separate checks.
UpperProjectionResult analyze_upper_projection(const TriangleMesh &, bool millimeters_declared,
                                               uint64_t revision, const UpperProjectionLimits &limits = {});
}
