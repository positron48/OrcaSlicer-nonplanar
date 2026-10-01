#pragma once
#include "VolumePartition.hpp"
#include "Policy.hpp"
#include "DepositionModel.hpp"
#include "UpperProjection.hpp"

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

inline constexpr unsigned body_material_contract_version=1;
struct BodyMaterialParameters {
    PhysicalPosition plate_origin; // Explicit declared plate -> physical translation.
    MaterialModel model;
    MaterialIds material;
    Speed deposition_speed, travel_speed;
    Acceleration acceleration;
    uint64_t support_reference_id, contact_reference_id; // References, not proof.
};
struct BodyBeadReference {
    size_t record_index, region_index, path_index, segment_index;
    double width_reconciliation_upper_mm, volume_reconciliation_upper_mm3;
};
struct BodyMaterialSnapshot {
    const std::shared_ptr<const PlanarBodySnapshot> body;
    const std::shared_ptr<const MaterialSequenceSnapshot> material;
    const std::string body_fingerprint, material_fingerprint;
    const PhysicalPosition plate_origin;
    const std::vector<BodyBeadReference> references;
    std::string canonical_context() const;
    std::string canonical_reference(size_t) const;
    std::string fingerprint() const;
};
struct BodyMaterialResult {
    std::string reason;
    std::shared_ptr<const BodyMaterialSnapshot> snapshot;
};
// Diagnostic material reconstruction in retained native enumeration order.
// Connecting travel is unqualified; this is not the native G-code order, a
// scheduler, support/contact/firmware proof or an executable motion plan.
BodyMaterialResult reconstruct_planar_body_material(const PlanarBodyResult &, const BodyMaterialParameters &,
                                                    const MaterialLimits &limits = {});

struct NativeAffinePassRequest {
    RectangleXY footprint; // Physical XY, including its whole interior.
    size_t patch;
    double support_plane_z_mm;
    AffinePassPolicy policy;
};
struct NativeAffinePassLimits {
    MaterialIntegralLimits material;
    UpperProjectionLimits projection;
    size_t max_reservation_tests=50000;
};
struct NativeAffinePassSnapshot {
    const std::shared_ptr<const BodyMaterialSnapshot> body_material;
    const std::shared_ptr<const UpperProjectionSnapshot> source_projection;
    const NativeAffinePassRequest request;
    const std::shared_ptr<const AffinePassStackSnapshot> stack;
    const double source_corner_error_upper_mm;
    const size_t reservation_tests;
};
struct NativeAffinePassResult {
    std::string reason;
    std::shared_ptr<const NativeAffinePassSnapshot> snapshot;
};
// Bind prospective surfaces/quotas to an owned native body and its original
// source target. Whole rectangular cell must avoid source holes, lie in one
// exactly affine patch, and fit the convex reservation with numerical error.
// This is not complete curved/seam/path/material scheduling or export approval.
NativeAffinePassResult plan_native_affine_pass_stack(const BodyMaterialResult &, const NativeAffinePassRequest &,
                                                    const NativeAffinePassLimits &limits = {});

struct NativeAffineHatchSnapshot {
    const std::shared_ptr<const NativeAffinePassSnapshot> passes;
    const std::shared_ptr<const AffineHatchSnapshot> hatches;
};
struct NativeAffineHatchResult { std::string reason; std::shared_ptr<const NativeAffineHatchSnapshot> snapshot; };
// Capture/revalidate the actual native parent and original affine target, then
// construct finite hatch alternatives under one whole-call deadline. Candidate
// geometry and prospective cells are not a complete deposited motion plan.
NativeAffineHatchResult plan_native_affine_hatches(const BodyMaterialResult &, const NativeAffinePassRequest &,
    const AffineHatchPolicy &, const NativeAffinePassLimits &pass_limits = {}, const AffineHatchLimits &hatch_limits = {});
}
