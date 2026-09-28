#pragma once
#include "MeshAudit.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace Slic3r::nptop {
struct UpperProjectionLimits;
struct UpperProjectionResult;
enum class UpperProjectionStatus { NominalHeightfield, Invalid, Unknown };
struct UpperFacet {
    size_t mesh_face;
    double xy_area_lower_mm2, xy_area_upper_mm2;
    double slope_upper;
    bool within_slope_limit;
};
struct ProjectionBoundary {
    std::vector<size_t> mesh_vertices; // Closed implicitly; first vertex is not repeated.
    bool hole;
    double signed_xy_area_lower_mm2, signed_xy_area_upper_mm2;
};
struct UpperPatch {
    std::vector<size_t> mesh_faces;
    std::vector<ProjectionBoundary> boundaries;
    double xy_area_lower_mm2, xy_area_upper_mm2;
    double minimum_z_mm = std::numeric_limits<double>::infinity();
    double maximum_z_mm = -std::numeric_limits<double>::infinity();
    double slope_upper = 0;
    struct Crease {
        std::array<size_t,2> mesh_vertices, mesh_faces;
    };
    std::vector<Crease> creases;
    // Zero only for an exactly affine nominal patch, otherwise unknown.
    std::optional<double> nominal_curvature_upper_mm_inv;
};
struct UpperProjectionSnapshot {
    const std::shared_ptr<const TriangleMesh> geometry;
    const uint64_t revision;
    const std::vector<UpperFacet> upward_facets;
    const double xy_area_lower_mm2, xy_area_upper_mm2;
    const double filtered_area_lower_mm2, filtered_area_upper_mm2;
    const double minimum_z_mm, maximum_z_mm;
    const std::vector<UpperPatch> slope_patches;
private:
    // Only audited analysis may create masks consumed by containment queries.
    UpperProjectionSnapshot(std::shared_ptr<const TriangleMesh> mesh, uint64_t rev,
        std::vector<UpperFacet> facets, double area_lo, double area_hi,
        double filtered_lo, double filtered_hi, double z_lo, double z_hi, std::vector<UpperPatch> patches)
        : geometry(std::move(mesh)), revision(rev), upward_facets(std::move(facets)),
          xy_area_lower_mm2(area_lo), xy_area_upper_mm2(area_hi),
          filtered_area_lower_mm2(filtered_lo), filtered_area_upper_mm2(filtered_hi),
          minimum_z_mm(z_lo), maximum_z_mm(z_hi), slope_patches(std::move(patches)) {}
    friend UpperProjectionResult analyze_upper_projection(const TriangleMesh &, bool, uint64_t,
                                                          const UpperProjectionLimits &);
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

struct UpperFootprintQuery {
    size_t patch = 0;
    Vec2d start_mm{0,0}, end_mm{0,0};
    double xy_radius_mm = 0;
    double boundary_uncertainty_mm = 0;
    double transition_inset_mm = 0;
};
struct UpperFootprintLimits {
    std::chrono::milliseconds timeout{1000};
    std::function<bool()> cancelled;
    std::function<bool(uint64_t)> is_current;
};
enum class UpperFootprintStatus { Contained, Outside, Unknown };
struct UpperFootprintResult {
    UpperFootprintStatus status = UpperFootprintStatus::Unknown;
    std::string reason;
    double required_inset_upper_mm = 0;
    std::optional<double> nominal_curvature_upper_mm_inv;
};
// Tests the whole swept disk against one nominal XY patch, preserving holes.
// This is an implicit inset query, not Z contact, curvature or head clearance.
UpperFootprintResult check_upper_footprint(std::shared_ptr<const UpperProjectionSnapshot>,
    const UpperFootprintQuery &, const UpperFootprintLimits &limits = {});
// Additionally requires the whole capsule to avoid every noncoplanar crease.
// Reaching a crease is UNKNOWN; affine nominal curvature alone is not printability.
UpperFootprintResult check_affine_upper_footprint(std::shared_ptr<const UpperProjectionSnapshot>,
    const UpperFootprintQuery &, const UpperFootprintLimits &limits = {});
}
