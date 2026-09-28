#pragma once

#include "Contracts.hpp"
#include "../ExtrusionEntity.hpp"
#include <optional>
#include <chrono>
#include <functional>
#include <memory>

namespace Slic3r { class LayerRegion; }

namespace Slic3r::nptop {

inline constexpr unsigned transition_contract_version = 1;
struct RectangleXY { double min_x, min_y, max_x, max_y; };
struct ScalarBounds { double lower, upper; };
struct PlanarSupportCore {
    RectangleXY lower_footprint; // eroded flat core, never an inflated collision envelope
    ScalarBounds nominal_top_mm, possible_top_mm;
    uint64_t source_path_id;
};
enum class TransitionStatus { Compatible, Rejected, Unknown };
enum class TransitionReason {
    SupportedCell, GapTooSmall, GapTooLarge, UncertainGap, UnsupportedFootprint,
    UnsupportedPath, InvalidInput, NumericalFailure
};
struct SupportCoreResult {
    TransitionReason reason = TransitionReason::InvalidInput;
    std::optional<PlanarSupportCore> core;
};

// A03 named scale conversion + explicit translation from native object XY.
// This supports only ordinary straight, axis-aligned solid infill in the
// declared rounded-rectangle material model; it is not a full material replay.
SupportCoreResult reconstruct_planar_core(const ExtrusionPath &, size_t segment, double layer_print_z,
    const PhysicalPosition &object_origin, const NativeScale &, uint64_t source_path_id,
    Length nozzle_diameter, Length xy_uncertainty, Length vertical_uncertainty);

struct AffineCapCell {
    RectangleXY footprint;
    double z00, z10, z01; // absolute physical Z; z11 = z10 + z01 - z00
};
struct TransitionPolicy {
    VerticalGap minimum, maximum;
    Length corner_height_error; // independent error on each of the three input heights
};
struct TransitionResult {
    TransitionStatus status = TransitionStatus::Unknown;
    TransitionReason reason = TransitionReason::InvalidInput;
    uint64_t source_path_id = 0;
    // Signed integrals are diagnostic for rejected/unknown cells, not a
    // nonnegative deposition Volume or authorization to emit material.
    std::optional<ScalarBounds> gap_mm, nominal_volume_mm3, possible_volume_mm3;
};

// Simulation primitive only. A compatible cell is not print/export approval.
TransitionResult assess_first_pass(const AffineCapCell &, const PlanarSupportCore &, const TransitionPolicy &);

inline constexpr unsigned planar_region_contract_version = 1;
enum class PlanarEntityKind { Collection, Loop, MultiPath, Path };
struct PlanarEntityRecord {
    size_t parent_id; // One-based node indices, zero for each region root.
    PlanarEntityKind kind;
    bool can_reverse, can_sort;
    int native_inset_index;
    std::optional<ExtrusionLoopRole> loop_role;
};
struct PlanarPathRecord {
    size_t entity_id;
    ExtrusionRole role;
    double width_mm, height_mm, mm3_per_mm;
    Points3 native_points;
    std::vector<Position<Frame::BuildPlate>> points;
    double coordinate_error_upper_mm=0;
    ScalarBounds length_mm{0,0}, native_volume_mm3{0,0};
};
struct PlanarRegionLimits {
    size_t max_entities=20000, max_points=200000, max_depth=32;
    std::chrono::milliseconds timeout{1000};
    std::function<bool()> cancelled;
    std::function<bool(uint64_t)> is_current;
};
struct PlanarRegionSnapshot {
    const uint64_t revision;
    const size_t native_layer_id;
    const double native_layer_height_mm, native_print_z_mm;
    const Position<Frame::BuildPlate> object_origin;
    const NativeScale native_scale;
    // Roots 1 and 2 are perimeters and fills, including empty roots.
    const std::vector<PlanarEntityRecord> entities;
    const std::vector<PlanarPathRecord> paths;
    const ScalarBounds native_volume_mm3;
};
struct PlanarRegionResult {
    std::string reason;
    std::shared_ptr<const PlanarRegionSnapshot> snapshot;
};
// Caller synchronizes the native region only during initial bounded capture;
// no callback runs while reading it. This is unordered nominal path data, not a
// support/partition proof, whole-job binding, motion plan or export approval.
PlanarRegionResult capture_planar_region(const LayerRegion &, Position<Frame::BuildPlate> object_origin,
                                        uint64_t revision, const PlanarRegionLimits &limits = {});

} // namespace Slic3r::nptop
