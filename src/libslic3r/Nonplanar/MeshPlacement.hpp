#pragma once
#include "StlImport.hpp"

namespace Slic3r::nptop {
struct MeshPlacementLimits {
    MeshAuditLimits geometry;
    double max_error_upper_mm=0.01;
    std::function<bool(uint64_t)> is_current;
};
struct MeshPlacementSnapshot {
    const std::shared_ptr<const StlSourceSnapshot> source;
    const std::shared_ptr<const TriangleMesh> model_local;
    const Transform3d model_to_plate;
    const uint64_t revision;
};
struct MeshPlacementResult {
    std::shared_ptr<const MeshPlacementSnapshot> snapshot;
    MeshAuditResult geometry;
    std::optional<double> propagated_source_error_upper_mm;
    std::optional<double> native_transform_error_upper_mm;
    std::optional<double> total_error_upper_mm;
};
// Internal background/worker boundary. Supplied binary64 matrix entries are the
// authoritative affine map; earlier Model centering/composition/plate selection
// and solid occupancy are separate contracts. Returns actual native float mesh
// coordinates in build-plate space only after a new geometry audit.
MeshPlacementResult place_imported_mesh(const StlImportResult &, const Transform3d &model_to_plate,
                                        uint64_t revision, const MeshPlacementLimits &limits = {});
}
