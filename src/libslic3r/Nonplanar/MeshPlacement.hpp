#pragma once
#include "StlImport.hpp"
#include <array>

namespace Slic3r { class Model; class ModelVolume; }
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

struct CenteredVolumeSnapshot {
    const std::shared_ptr<const StlSourceSnapshot> source;
    const std::shared_ptr<const TriangleMesh> volume_local;
    const Vec3d source_offset;
    const double source_error_upper_mm;
    const uint64_t revision;
};
struct CenteredVolumeResult {
    std::shared_ptr<const CenteredVolumeSnapshot> snapshot;
    MeshAuditResult geometry;
    std::optional<double> centering_error_upper_mm;
    std::optional<double> total_error_upper_mm;
};
// Caller owns/synchronizes the ModelVolume during capture. Establishes only the
// source-to-volume-local centering step, not volume/instance/plate placement.
CenteredVolumeResult capture_centered_volume(const StlImportResult &, const ModelVolume &,
                                             uint64_t revision, const MeshPlacementLimits &limits = {});

struct PlateFrame {
    size_t index=0;
    Vec3d world_origin_mm=Vec3d::Zero();
};
struct ModelPlacementSnapshot {
    const std::shared_ptr<const CenteredVolumeSnapshot> centered;
    const Transform3d volume_to_object, instance_to_world;
    const PlateFrame plate;
};
struct ModelPlacementResult {
    std::shared_ptr<const ModelPlacementSnapshot> snapshot;
    MeshAuditResult geometry;
    std::optional<double> centering_error_upper_mm;
    std::optional<std::array<double,3>> native_transform_errors_upper_mm;
    std::optional<double> total_error_upper_mm;
};
// One native object/volume/instance only. The caller owns model capture and
// supplies the plate frame; GUI plate selection/membership is not certified here.
ModelPlacementResult capture_model_placement(const StlImportResult &, const Model &, const PlateFrame &,
                                             uint64_t revision, const MeshPlacementLimits &limits = {});
}
