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

inline constexpr unsigned material_integral_contract_version=2;
enum class MaterialIntegralStatus { Bounded, Rejected, Unknown };
class MaterialIntegralProof;
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
    std::shared_ptr<const MaterialIntegralProof> proof;
};
// Refine the signed nominal vertical-cell integral above the highest laid roof.
// Bounded requires the continuous first-pass feasibility proof and the requested
// total interval width. Rounded shoulders and overlaps are included; no selected
// bead amount, perimeter/seam allocation, V-to-E or export permission is provided.
MaterialIntegralResult integrate_material_first_pass(const LowerMaterialView &, const AffineCapCell &,
    double support_plane_z_mm, const TransitionPolicy &, const MaterialIntegralLimits &limits = {});

enum class IntegralSplitAxis { X, Y };
struct IntegralStripVolume { RectangleXY footprint; ScalarBounds volume_mm3; };
struct IntegralStripsSnapshot {
    const std::shared_ptr<const MaterialIntegralProof> source;
    const IntegralSplitAxis axis;
    const std::vector<double> cuts;
    const std::vector<IntegralStripVolume> strips;
    const ScalarBounds total_volume_mm3;
    const size_t proof_cells, evaluations;
};
struct IntegralStripsResult { std::string reason; std::shared_ptr<const IntegralStripsSnapshot> snapshot; };
// Exact, complete strip partition of one owned bounded integral. Reuses all
// continuous leaf roof bounds; does not rerun material queries, select E or
// assert that finite beads/perimeters can fill the strips. Cuts include both
// parent boundaries and must be strictly increasing. Total precision still gates.
IntegralStripsResult split_material_integral(const MaterialIntegralResult &, IntegralSplitAxis,
    const std::vector<double> &cuts, const MaterialIntegralLimits &limits = {});

inline constexpr unsigned affine_pass_stack_contract_version=2;
struct AffinePassStackResult;
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
private:
    AffinePassStackSnapshot(std::shared_ptr<const MaterialPrefixSnapshot> s, AffineCapCell f, AffinePassPolicy p,
        double plane, double offset, ScalarBounds range, MaterialIntegralResult first, std::vector<AffinePassSurface> passes,
        ScalarBounds total, Volume amount, double allocation_error, double numeric)
        : source(std::move(s)), final_surface(f), policy(p), support_plane_z_mm(plane), first_offset_mm(offset), offset_range_mm(range),
          first_pass(std::move(first)), surfaces(std::move(passes)), total_volume_mm3(total), total_allocated_volume(amount),
          total_allocation_error_mm3(allocation_error), numerical_error_upper_mm(numeric) {}
    friend AffinePassStackResult plan_affine_pass_stack(const LowerMaterialView &, const AffineCapCell &,
        double, const AffinePassPolicy &, const MaterialIntegralLimits &);
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

inline constexpr unsigned affine_hatch_contract_version=2;
struct AffineHatchResult;
enum class HatchDirection { AlongX, AlongY };
struct AffineHatchPolicy {
    WidthXY width;
    Length maximum_pitch, boundary_band;
    HatchDirection first_direction;
};
struct AffineHatchLimits {
    MaterialIntegralLimits volumes;
    size_t max_lines=20000;
    std::chrono::milliseconds timeout{1000};
    std::function<bool()> cancelled;
    std::function<bool(uint64_t)> is_current;
};
struct AffineHatchLine {
    PhysicalPosition start, end, reverse_start, reverse_end; // Direction alternatives, never a selected order.
    WidthXY width;
    RectangleXY volume_cell;
    ScalarBounds prospective_cell_volume_mm3, projected_length_mm;
    double coordinate_error_upper_mm;
};
struct AffineHatchPass {
    HatchDirection direction;
    ScalarBounds pitch_mm, prospective_volume_mm3;
    std::vector<AffineHatchLine> lines;
    // Configured outer bands of the parent ROI, not a complete finite-bead or
    // seam remainder. Prospective line cells still own their complete volume.
    std::vector<RectangleXY> boundary_regions;
};
struct AffineHatchSnapshot {
    const std::shared_ptr<const AffinePassStackSnapshot> source;
    const AffineHatchPolicy policy;
    const std::vector<AffineHatchPass> passes;
    const ScalarBounds total_prospective_volume_mm3;
    const size_t line_count;
    const double numerical_error_upper_mm;
private:
    AffineHatchSnapshot(std::shared_ptr<const AffinePassStackSnapshot> s, AffineHatchPolicy p,
        std::vector<AffineHatchPass> passes_, ScalarBounds total, size_t count, double numeric)
        : source(std::move(s)), policy(p), passes(std::move(passes_)), total_prospective_volume_mm3(total),
          line_count(count), numerical_error_upper_mm(numeric) {}
    friend AffineHatchResult plan_affine_hatches(const AffinePassStackResult &, const AffineHatchPolicy &, const AffineHatchLimits &);
};
struct AffineHatchResult { std::string reason; std::shared_ptr<const AffineHatchSnapshot> snapshot; };
// Construct finite fixed-width centerline alternatives on the selected affine
// surfaces, with nominal whole-capsule ROI containment and complete strip volume targets.
// Cells include the unallocated boundary/end bands. No bead volume/E, full fill,
// subsequent support, contact/head clearance, travel or motion order is selected.
AffineHatchResult plan_affine_hatches(const AffinePassStackResult &, const AffineHatchPolicy &,
                                      const AffineHatchLimits &limits = {});

inline constexpr unsigned affine_hatch_cells_contract_version=1;
struct AffineHatchCellPass {
    RectangleXY finite_footprint;
    std::vector<IntegralStripVolume> finite_cells, remainder_cells;
    ScalarBounds finite_volume_mm3, remainder_volume_mm3, total_volume_mm3;
};
struct AffineHatchCellsResult;
struct AffineHatchCellsSnapshot {
    const std::shared_ptr<const AffineHatchSnapshot> source;
    const std::vector<AffineHatchCellPass> passes;
    const ScalarBounds finite_volume_mm3, remainder_volume_mm3, total_volume_mm3;
    const size_t proof_cells, evaluations;
private:
    AffineHatchCellsSnapshot(std::shared_ptr<const AffineHatchSnapshot> s, std::vector<AffineHatchCellPass> p,
        ScalarBounds finite, ScalarBounds remainder, ScalarBounds total, size_t cells, size_t work)
        : source(std::move(s)), passes(std::move(p)), finite_volume_mm3(finite), remainder_volume_mm3(remainder),
          total_volume_mm3(total), proof_cells(cells), evaluations(work) {}
    friend AffineHatchCellsResult allocate_affine_hatch_cells(const AffineHatchResult &, const MaterialIntegralLimits &);
};
struct AffineHatchCellsResult { std::string reason; std::shared_ptr<const AffineHatchCellsSnapshot> snapshot; };
// Partition every whole-pass volume into finite rectangular path-owned cells
// and an explicit boundary/end remainder. No amount outside a finite footprint
// is charged to that path. Rounded-section/overlap corrections, filled beads/E,
// seam construction, actual later support and motion order remain separate.
AffineHatchCellsResult allocate_affine_hatch_cells(const AffineHatchResult &, const MaterialIntegralLimits &limits = {});

inline constexpr unsigned fixed_width_bead_contract_version=1;
struct FixedWidthBeadRequest {
    PhysicalPosition start, end;
    WidthXY width;
    VerticalGap gap_begin, gap_end;
    BeadSectionKind kind;
    uint64_t revision;
    std::string source_fingerprint;
};
struct FixedWidthBeadLimits {
    Length maximum_width_error{.005}; // Approximation of one fixed nominal width.
    Volume maximum_volume_error{.001}; // Whole path, not per packet.
    size_t max_segments=4096, max_depth=24;
    std::chrono::milliseconds timeout{1000};
    std::function<bool()> cancelled;
    std::function<bool(uint64_t)> is_current;
};
struct FixedWidthBeadPiece {
    PhysicalPosition start, end;
    WidthXY nominal_width;
    Volume volume;
    BeadSection section; // Actual constant-flux width range, not variable design width.
    double width_error_upper_mm, coordinate_error_upper_mm;
};
struct FixedWidthBeadResult;
struct FixedWidthBeadSnapshot {
    const FixedWidthBeadRequest request;
    const std::vector<FixedWidthBeadPiece> pieces;
    const ScalarBounds target_volume_mm3, deposited_volume_mm3;
    const double total_volume_error_mm3, maximum_width_error_mm, numerical_error_upper_mm;
private:
    FixedWidthBeadSnapshot(FixedWidthBeadRequest r, std::vector<FixedWidthBeadPiece> p, ScalarBounds target,
        ScalarBounds deposited, double volume_error, double width_error, double numeric)
        : request(std::move(r)), pieces(std::move(p)), target_volume_mm3(target), deposited_volume_mm3(deposited),
          total_volume_error_mm3(volume_error), maximum_width_error_mm(width_error), numerical_error_upper_mm(numeric) {}
    friend FixedWidthBeadResult plan_fixed_width_bead(const FixedWidthBeadRequest &, const FixedWidthBeadLimits &);
};
struct FixedWidthBeadResult { std::string reason; std::shared_ptr<const FixedWidthBeadSnapshot> snapshot; };
// Subdivide a declared straight affine-gap path into constant-flux G1 amounts.
// Bound departure from its fixed nominal XY width and whole analytic volume.
// Butt ends and the existing vertical section model; no 3D slope multiplier.
// Source identity is declared context, not actual support/contact/order proof.
// Cell filling/overlap, actual gap reconstruction and export remain separate.
FixedWidthBeadResult plan_fixed_width_bead(const FixedWidthBeadRequest &, const FixedWidthBeadLimits &limits = {});

inline constexpr unsigned first_hatch_bead_contract_version=2;
enum class FirstHatchRoofDomain { Centerline, FiniteWidth };
struct FirstHatchBeadLimits : MaterialQueryLimits {
    FixedWidthBeadLimits packets;
    Length maximum_gap_error{.001};
    size_t max_roof_segments=4096, max_depth=24;
};
struct FirstHatchBeadResult;
struct FirstHatchBeadSnapshot {
    const std::shared_ptr<const AffineHatchSnapshot> source;
    const size_t line_index;
    const FirstHatchRoofDomain roof_domain;
    const std::vector<FixedWidthBeadPiece> pieces;
    const ScalarBounds actual_target_volume_mm3, deposited_volume_mm3;
    const double maximum_gap_error_mm, maximum_width_error_mm, total_volume_error_mm3, numerical_error_upper_mm;
    const size_t roof_segments, evaluations;
private:
    FirstHatchBeadSnapshot(std::shared_ptr<const AffineHatchSnapshot> s, size_t index, FirstHatchRoofDomain domain, std::vector<FixedWidthBeadPiece> p,
        ScalarBounds target, ScalarBounds deposited, double gap_error, double width_error, double volume_error,
        double numeric, size_t segments, size_t work)
        : source(std::move(s)), line_index(index), roof_domain(domain), pieces(std::move(p)), actual_target_volume_mm3(target),
          deposited_volume_mm3(deposited), maximum_gap_error_mm(gap_error), maximum_width_error_mm(width_error),
          total_volume_error_mm3(volume_error), numerical_error_upper_mm(numeric), roof_segments(segments), evaluations(work) {}
    friend FirstHatchBeadResult plan_first_hatch_bead(const AffineHatchResult &, size_t, const FirstHatchBeadLimits &);
    friend FirstHatchBeadResult plan_first_hatch_footprint_bead(const AffineHatchResult &, size_t, const FirstHatchBeadLimits &);
    static FirstHatchBeadResult plan(const AffineHatchResult &, size_t, const FirstHatchBeadLimits &, FirstHatchRoofDomain);
};
struct FirstHatchBeadResult { std::string reason; std::shared_ptr<const FirstHatchBeadSnapshot> snapshot; };
// Reconstruct the continuous highest nominal laid roof under one first-pass
// centerline, then bound constant-flux amounts against its actual varying gap.
// Piece sections are affine approximants: charge the reported gap/width/numeric
// errors before material use. Finite-width contact, filled union/overlap, travel,
// ordering, later support and export remain separate obligations.
FirstHatchBeadResult plan_first_hatch_bead(const AffineHatchResult &, size_t line_index,
                                        const FirstHatchBeadLimits &limits = {});
// Plan against the continuous nominal roof over the whole finite butt-ended
// strip, enclosing nominal width +/- the permitted width error. That strip must
// lie inside the owned ROI whose support plane is covered by D_lower. Bounds
// the axial section-gap approximation across its full width; rounded side-floor
// contact, target conformity, D_upper/head clearance, order and export are separate.
FirstHatchBeadResult plan_first_hatch_footprint_bead(const AffineHatchResult &, size_t line_index,
                                                  const FirstHatchBeadLimits &limits = {});

inline constexpr unsigned material_union_contract_version=1;
struct MaterialUnionLimits : MaterialIntegralLimits {};
struct MaterialUnionResult;
struct MaterialUnionSnapshot {
    const std::shared_ptr<const MaterialPrefixSnapshot> source;
    const SceneBox domain;
    const ScalarBounds union_volume_mm3, individual_volume_mm3, repeated_volume_mm3;
    const size_t cells, evaluations;
private:
    MaterialUnionSnapshot(std::shared_ptr<const MaterialPrefixSnapshot> s, SceneBox box,
        ScalarBounds occupied, ScalarBounds sum, ScalarBounds repeated, size_t count, size_t work)
        : source(std::move(s)), domain(box), union_volume_mm3(occupied), individual_volume_mm3(sum),
          repeated_volume_mm3(repeated), cells(count), evaluations(work) {}
    friend MaterialUnionResult integrate_material_union(const NominalMaterialView &, const SceneBox &, const MaterialUnionLimits &);
};
struct MaterialUnionResult {
    std::string reason;
    std::shared_ptr<const MaterialUnionSnapshot> snapshot;
    size_t cells=0, evaluations=0; // Diagnostics also retained after UNKNOWN.
    std::optional<ScalarBounds> provisional_union_mm3, provisional_excess_mm3; // Never a certificate.
};
// Continuous clipped nominal occupancy, sum of individual bead occupancies and
// their difference (multiplicity excess, not pairwise intersection volume).
// Future material is absent. All three intervals meet one whole-domain precision
// limit. Declared geometric union only; no target fill, physical contact or export.
MaterialUnionResult integrate_material_union(const NominalMaterialView &, const SceneBox &,
                                             const MaterialUnionLimits &limits = {});

inline constexpr unsigned material_fill_contract_version=1;
struct MaterialFillLimits : MaterialUnionLimits {};
struct MaterialFillResult;
struct MaterialFillSnapshot {
    const std::shared_ptr<const MaterialIntegralProof> target;
    const std::shared_ptr<const MaterialUnionSnapshot> occupied;
    const ScalarBounds target_volume_mm3, covered_target_mm3, missing_target_mm3;
    const ScalarBounds outside_target_mm3, below_roof_mm3, above_surface_mm3;
    const size_t cells, evaluations;
private:
    MaterialFillSnapshot(std::shared_ptr<const MaterialIntegralProof> t, std::shared_ptr<const MaterialUnionSnapshot> u,
        ScalarBounds volume, ScalarBounds covered, ScalarBounds missing, ScalarBounds outside,
        ScalarBounds below, ScalarBounds above, size_t count, size_t work)
        : target(std::move(t)), occupied(std::move(u)), target_volume_mm3(volume), covered_target_mm3(covered),
          missing_target_mm3(missing), outside_target_mm3(outside), below_roof_mm3(below), above_surface_mm3(above),
          cells(count), evaluations(work) {}
    friend MaterialFillResult reconcile_material_fill(const MaterialIntegralResult &, const MaterialUnionResult &, const MaterialFillLimits &);
};
struct MaterialFillResult {
    std::string reason;
    std::shared_ptr<const MaterialFillSnapshot> snapshot;
    size_t cells=0, evaluations=0;
    std::optional<ScalarBounds> provisional_below_roof_mm3, provisional_above_surface_mm3; // Never a certificate.
};
// Separate nonnegative target deficit from Z spill within the owned union box.
// The box must match the target XY cell and contain its complete vertical volume.
// Material outside this XY box is a separate obligation. Reuses protected roof
// and union proofs; wrapper fields cannot replace them. Geometric measures only.
MaterialFillResult reconcile_material_fill(const MaterialIntegralResult &, const MaterialUnionResult &,
                                          const MaterialFillLimits &limits = {});
inline constexpr unsigned material_deficit_contract_version=1;
struct MaterialDeficitLimits : MaterialIntegralLimits {size_t max_regions=256;};
struct MaterialDeficitCell {
    RectangleXY footprint;
    ScalarBounds target_volume_mm3;
    // Entire amounts of intersecting finite prefix beads: a conservative upper
    // bound, not local occupied volume. A zero deficit lower bound does not certify fill.
    double candidate_amount_upper_mm3,covered_upper_mm3,missing_lower_mm3;
};
struct MaterialDeficitResult;
struct MaterialDeficitSnapshot {
    const std::shared_ptr<const MaterialFillSnapshot> source;
    const std::vector<MaterialDeficitCell> cells;
    const ScalarBounds target_volume_mm3;
    const double localized_missing_lower_mm3;
    const size_t proof_cells,evaluations;
private:
    MaterialDeficitSnapshot(std::shared_ptr<const MaterialFillSnapshot> s,std::vector<MaterialDeficitCell> c,
        ScalarBounds volume,double missing,size_t fragments,size_t work)
        : source(std::move(s)),cells(std::move(c)),target_volume_mm3(volume),localized_missing_lower_mm3(missing),
          proof_cells(fragments),evaluations(work) {}
    friend MaterialDeficitResult locate_material_deficit(const MaterialFillResult &,const std::vector<double> &,
        const std::vector<double> &,const MaterialDeficitLimits &);
};
struct MaterialDeficitResult {std::string reason;std::shared_ptr<const MaterialDeficitSnapshot> snapshot;};
// Complete exact XY grid of conservative missing-volume lower bounds, within
// the owned fill domain. Cuts include both ends. No local-fill or path approval.
MaterialDeficitResult locate_material_deficit(const MaterialFillResult &,const std::vector<double> &x_cuts,
    const std::vector<double> &y_cuts,const MaterialDeficitLimits &limits = {});

}
