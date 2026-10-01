#pragma once
#include "Transition.hpp"
#include "Collision.hpp"
#include <string>

namespace Slic3r::nptop {
inline constexpr unsigned deposited_material_contract_version=1;
enum class BeadSectionKind { Rectangle, RoundedRectangle };
struct BeadSection {
    BeadSectionKind kind;
    double gap_begin_mm, gap_end_mm;
    ScalarBounds width_mm; // Permitted range over the whole varying-gap bead.
};
struct MaterialRecord { MotionEvent motion; std::optional<BeadSection> bead; };
struct MaterialModel {
    uint64_t model_id;
    Length outer_xy_growth, outer_z_growth, inner_xy_loss, inner_z_loss;
    Length numerical_coordinate_error;
};
struct MaterialLimits {
    size_t max_records=200000;
    std::chrono::milliseconds timeout{1000};
    std::function<bool()> cancelled;
    std::function<bool(uint64_t)> is_current;
};
struct DepositedBeadGeometry { ScalarBounds xy_length_mm, volume_mm3; };
struct MaterialSequenceSnapshot {
    const uint64_t revision;
    const std::string source_fingerprint;
    const MaterialModel model;
    const std::vector<MaterialRecord> records;
    const std::vector<std::optional<DepositedBeadGeometry>> geometry;
    std::string canonical_context() const;
    std::string canonical_record(size_t) const;
    std::string fingerprint() const;
};
struct MaterialSequenceResult {
    std::string reason;
    std::shared_ptr<const MaterialSequenceSnapshot> snapshot;
};
// Geometric model only: declared inner/outer set assumptions are not measured
// qualification. Caller synchronizes the initial bounded input capture.
MaterialSequenceResult capture_material_sequence(const std::vector<MaterialRecord> &, const MaterialModel &,
    uint64_t revision, const std::string &source_fingerprint, const MaterialLimits &limits = {});
struct MaterialPrefixSnapshot {
    const std::shared_ptr<const MaterialSequenceSnapshot> sequence;
    const size_t completed_records;
    const double current_progress;
    // Cumulative commanded amount, not the measure of the overlapping bead union.
    const ScalarBounds nominal_deposited_volume_mm3;
    std::string canonical() const;
    std::string fingerprint() const;
};
struct NominalMaterialView { std::shared_ptr<const MaterialPrefixSnapshot> snapshot; };
struct UpperMaterialView { std::shared_ptr<const MaterialPrefixSnapshot> snapshot; };
struct LowerMaterialView { std::shared_ptr<const MaterialPrefixSnapshot> snapshot; };
struct MaterialAt {
    std::string reason;
    NominalMaterialView nominal;
    UpperMaterialView upper;
    LowerMaterialView lower;
};
// Include complete previous depositions and only this fraction of the current
// event. At the sequence end progress must be zero. No contact exclusions.
MaterialAt material_at(std::shared_ptr<const MaterialSequenceSnapshot>, size_t completed_records,
                       double current_progress, const MaterialLimits &limits = {});
enum class MaterialMembership { Inside, Outside, Unknown };
struct MaterialQueryLimits {
    size_t max_evaluations=200000;
    std::chrono::milliseconds timeout{1000};
    std::function<bool()> cancelled;
    std::function<bool(uint64_t)> is_current;
};
struct MaterialQueryResult {
    MaterialMembership membership=MaterialMembership::Unknown;
    std::string reason;
    uint64_t source_event_id=0;
    size_t evaluations=0;
};
// Point membership in distinct model representations, not a whole footprint,
// support, continuous tool sweep, physical-state or export approval.
MaterialQueryResult classify_material(const NominalMaterialView &, const PhysicalPosition &, const MaterialQueryLimits &limits = {});
MaterialQueryResult classify_material(const UpperMaterialView &, const PhysicalPosition &, const MaterialQueryLimits &limits = {});
MaterialQueryResult classify_material(const LowerMaterialView &, const PhysicalPosition &, const MaterialQueryLimits &limits = {});
enum class MaterialCoverageStatus { Covered, Uncovered, Unknown };
enum class MaterialRepresentation { Nominal, Upper, Lower };
inline constexpr unsigned material_coverage_contract_version=1;
struct MaterialCoverageLimits : MaterialQueryLimits {
    size_t max_cells=4095, max_depth=32;
};
struct MaterialCoverageResult {
    MaterialCoverageStatus status=MaterialCoverageStatus::Unknown;
    std::string reason;
    std::shared_ptr<const MaterialPrefixSnapshot> source;
    std::optional<SceneBox> domain;
    MaterialRepresentation representation=MaterialRepresentation::Nominal;
    std::optional<PhysicalPosition> witness;
    size_t cells=0, evaluations=0;
};
// Continuous coverage of the complete physical XY footprint x vertical range,
// including zero-thickness planes. A point/vertex sample is never a certificate.
MaterialCoverageResult cover_material(const NominalMaterialView &, const SceneBox &, const MaterialCoverageLimits &limits = {});
MaterialCoverageResult cover_material(const UpperMaterialView &, const SceneBox &, const MaterialCoverageLimits &limits = {});
MaterialCoverageResult cover_material(const LowerMaterialView &, const SceneBox &, const MaterialCoverageLimits &limits = {});

inline constexpr unsigned material_transition_contract_version=1;
struct MaterialTransitionResult {
    TransitionStatus status=TransitionStatus::Unknown;
    std::string reason;
    std::shared_ptr<const MaterialPrefixSnapshot> source;
    std::optional<AffineCapCell> cell;
    std::optional<TransitionPolicy> policy;
    double support_plane_z_mm=0;
    MaterialCoverageResult support;
    // Ceilings over the complete XY footprint, not lower bounds on the roof.
    std::optional<double> nominal_roof_ceiling_mm, upper_roof_ceiling_mm;
    std::optional<ScalarBounds> gap_mm, nominal_volume_mm3;
    size_t roof_evaluations=0;
};
// Bounded affine-cell feasibility from one actual prefix. The requested support
// plane must be continuously covered by D_lower; D_upper bounds the minimum gap.
// Nominal volume bounds integrate the unfilled vertical cell above its highest
// nominal material. They are diagnostic, never a selected bead volume/E, seam,
// tool contact, complete transition plan or export approval. No Z adaptation.
MaterialTransitionResult assess_material_first_pass(const LowerMaterialView &, const AffineCapCell &,
    double support_plane_z_mm, const TransitionPolicy &, const MaterialCoverageLimits &limits = {});

inline constexpr unsigned material_integral_contract_version=1;
enum class MaterialIntegralStatus { Bounded, Rejected, Unknown };
struct MaterialIntegralLimits : MaterialCoverageLimits {
    Volume maximum_interval_width{.001}; // Total mm3 interval width, not per cell.
};
struct MaterialIntegralResult {
    MaterialIntegralStatus status=MaterialIntegralStatus::Unknown;
    std::string reason;
    MaterialTransitionResult first_pass;
    double maximum_interval_width_mm3=0;
    std::optional<ScalarBounds> nominal_volume_mm3;
    size_t cells=0, evaluations=0;
};
// Refine the signed nominal vertical-cell integral above the highest laid roof.
// Bounded requires the continuous first-pass feasibility proof and the requested
// total interval width. Rounded shoulders and overlaps are included; no selected
// bead amount, perimeter/seam allocation, V-to-E or export permission is provided.
MaterialIntegralResult integrate_material_first_pass(const LowerMaterialView &, const AffineCapCell &,
    double support_plane_z_mm, const TransitionPolicy &, const MaterialIntegralLimits &limits = {});

inline constexpr unsigned affine_pass_stack_contract_version=1;
struct AffinePassPolicy {
    size_t passes;
    TransitionPolicy first_gap;
    VerticalGap later_vertical_minimum, later_vertical_maximum;
    NormalGap later_normal_minimum, later_normal_maximum;
    Volume total_volume_error;
};
struct AffinePassSurface {
    AffineCapCell cell;
    std::optional<ScalarBounds> vertical_spacing_mm, normal_spacing_mm;
    ScalarBounds volume_mm3;
    Volume allocated_volume; // Prospective whole-cell quota, never a bead/E.
    double allocation_error_mm3;
};
struct AffinePassStackSnapshot {
    const std::shared_ptr<const MaterialPrefixSnapshot> source;
    const AffineCapCell final_surface;
    const AffinePassPolicy policy;
    const double support_plane_z_mm, first_offset_mm;
    const ScalarBounds offset_range_mm;
    const MaterialIntegralResult first_pass;
    const std::vector<AffinePassSurface> surfaces;
    const ScalarBounds total_volume_mm3;
    const Volume total_allocated_volume;
    const double total_allocation_error_mm3, numerical_error_upper_mm;
};
struct AffinePassStackResult {
    std::string reason;
    std::shared_ptr<const AffinePassStackSnapshot> snapshot;
};
// Select prospective parallel affine surfaces within the declared pass count
// and spacing limits. Preserve the final target, re-prove the selected first
// surface against actual material and bound total nominal cell-volume quotas.
// Later spacing is geometric only: actual cap material, paths, contact/seam,
// full-head motion, target-source binding and export remain separate obligations.
AffinePassStackResult plan_affine_pass_stack(const LowerMaterialView &, const AffineCapCell &final_surface,
    double support_plane_z_mm, const AffinePassPolicy &, const MaterialIntegralLimits &limits = {});
}
