#pragma once
#include "VolumePartition.hpp"
#include "Policy.hpp"

namespace Slic3r::nptop {
inline constexpr unsigned planar_body_contract_version=1;
struct PlanarBodyLimits {
    PlanarRegionLimits paths;
    size_t max_layers=1000, max_regions=1000;
};
struct PlanarBodyRegion {
    const int native_region_id;
    const ResolvedConfigSnapshot config;
    const std::shared_ptr<const PlanarRegionSnapshot> geometry;
};
struct PlanarBodySnapshot {
    const uint64_t revision;
    const std::shared_ptr<const VolumePartitionSnapshot> partition;
    const std::shared_ptr<const PrintConfigSnapshot> guarded_settings;
    const ResolvedConfigSnapshot executed_full_config, executed_print_config, executed_object_config;
    const NativeScale native_scale;
    const double origin_error_upper_mm;
    const std::vector<PlanarBodyRegion> regions;
    const ScalarBounds native_volume_mm3;
    std::string canonical_json() const;
    std::string fingerprint() const;
};
struct PlanarBodyResult {
    std::string reason;
    std::shared_ptr<const PlanarBodySnapshot> snapshot;
};
// Run in an isolated native worker (or serialized host test): native Model
// construction/config timestamps must not race GUI edits. Owned source settings
// are checked before only the private derived body engine switches to OFF.
// All nominal layers/roles/widths/volumes are retained; no ordered motions,
// deposited-material coverage, seam/tool proof or export approval is supplied.
PlanarBodyResult generate_planar_body(const VolumePartitionResult &, const PlanarBodyLimits &limits = {});
}
