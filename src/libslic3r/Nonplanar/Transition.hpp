#pragma once

#include "Contracts.hpp"
#include "../ExtrusionEntity.hpp"
#include <optional>

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

} // namespace Slic3r::nptop
