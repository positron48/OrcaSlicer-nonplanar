#pragma once
#include "MeshPlacement.hpp"
#include "Transition.hpp"

namespace Slic3r::nptop {
inline constexpr unsigned volume_partition_contract_version=1;
enum class VolumePartitionStatus { Partitioned, Invalid, Unknown };
struct VolumePartitionLimits {
    MeshAuditLimits geometry;
    double max_total_error_mm=0.05;
    std::function<bool(uint64_t)> is_current;
};
struct VolumePartitionSnapshot {
    const uint64_t revision;
    const std::shared_ptr<const ModelPlacementSnapshot> placement;
    const std::shared_ptr<const TriangleMesh> original, reservation, body, cap;
    const ScalarBounds original_exact_volume_mm3, body_exact_volume_mm3, cap_exact_volume_mm3;
    const ScalarBounds body_native_volume_mm3, cap_native_volume_mm3;
    const double partition_error_upper_mm, total_error_upper_mm;
    const double body_volume_conversion_error_upper_mm3, cap_volume_conversion_error_upper_mm3;
    const size_t shared_interface_triangles;
    std::string canonical_json() const;
    std::string fingerprint() const;
};
struct VolumePartitionResult {
    VolumePartitionStatus status=VolumePartitionStatus::Unknown;
    std::string reason;
    std::shared_ptr<const VolumePartitionSnapshot> snapshot;
};
// Nominal geometry partition in build-plate coordinates. Caller synchronizes
// the initial bounded reservation capture. Inputs then remain owned, including
// during callbacks. The reservation must still pass upper-selection, tool and
// interface/material checks before planning; this is never export approval.
VolumePartitionResult partition_cap(const ModelPlacementResult &, const TriangleMesh &reservation,
                                    const VolumePartitionLimits &limits = {});
}
