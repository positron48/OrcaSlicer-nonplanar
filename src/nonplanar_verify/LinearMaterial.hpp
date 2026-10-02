#pragma once
#include "LinearRates.hpp"
namespace nptop_verify {
inline constexpr unsigned linear_material_version=1;
enum class MaterialEventKind { Deposit,Travel,Retraction,Restore,Dwell };
enum class MaterialSectionKind { Rectangle,RoundedRectangle };
struct MaterialSection {MaterialSectionKind kind;double gap_begin_mm,gap_end_mm;};
struct MaterialDeclaration {
 uint64_t event_id;size_t sequence_index;MaterialEventKind kind;
 std::array<double,3> start,end;
 double expected_nominal_volume_mm3,expected_filament_mm;
 std::optional<MaterialSection> section;
};
struct LinearMaterialPolicy {
 uint64_t version=1,model_id=0,policy_id=0,revision=0,source_revision=0;
 std::string source_fingerprint;
 bool synthetic=true,operator_confirmed_claim=false;
 double outer_xy_growth_mm=0,outer_z_growth_mm=0,inner_xy_loss_mm=0,inner_z_loss_mm=0,numerical_coordinate_error_mm=0;
 double max_coordinate_delta_mm=0,max_nominal_delta_mm3=0,max_total_nominal_delta_mm3=0,max_filament_delta_mm=0;
 double relative_dose_error=0,absolute_dose_error_mm3=0;
};
struct LinearMaterialLimits : LinearRateLimits {
 std::function<bool(uint64_t)> is_source_current;
 std::function<bool(uint64_t,uint64_t)> is_rate_current;
};
struct MaterialBox {std::array<RateBounds,3> coordinate;}; // Outer bounds, never a solid/coverage proof.
struct ReplayedBead {
 size_t record;uint64_t event_id;
 std::array<RateBounds,3> start,end;
 RateBounds xy_length,nominal_volume,delivered_volume,nominal_area,delivered_area,nominal_width,delivered_width;
 MaterialBox nominal_bounds,upper_bounds;
 bool empty_inner; // Finite eroded butt interval is empty; other lower-shape tests remain necessary.
};
struct MaterialReplayData;
struct LinearMaterialResult;
struct LinearMaterialPrefixResult;
enum class MaterialRepresentation { Nominal,Upper,Lower };
enum class MaterialMembership { Inside,Outside,Unknown };
struct MaterialRegion {std::array<double,3> min,max;};
struct MaterialBoxResult;
struct MaterialCoverResult;
struct MaterialCoverLimits;
struct LinearMaterialPrefixSnapshot;
struct JoinedMaterialPolicy;
struct JoinedMaterialLimits;
struct JoinedMaterialResult;
struct JoinedMaterialSnapshot;
struct JoinedMaterialCoverResult;
struct LinearMaterialSnapshot {
 const std::shared_ptr<const LinearRateSnapshot> rates;
 const std::vector<MaterialDeclaration> declarations;
 const LinearMaterialPolicy policy;
 const std::vector<std::optional<ReplayedBead>> beads;
 const RateBounds nominal_volume,delivered_volume,maximum_nominal_delta_mm3,total_nominal_delta_mm3;
 const size_t evaluations;
private:
 const std::shared_ptr<const MaterialReplayData> exact;
 LinearMaterialSnapshot(std::shared_ptr<const LinearRateSnapshot> r,std::vector<MaterialDeclaration> d,LinearMaterialPolicy p,
  std::vector<std::optional<ReplayedBead>> b,RateBounds n,RateBounds v,RateBounds error,RateBounds total,size_t work,std::shared_ptr<const MaterialReplayData> e)
  :rates(std::move(r)),declarations(std::move(d)),policy(std::move(p)),beads(std::move(b)),nominal_volume(n),delivered_volume(v),
   maximum_nominal_delta_mm3(error),total_nominal_delta_mm3(total),evaluations(work),exact(std::move(e)){}
 friend LinearMaterialResult reconstruct_linear_material(std::shared_ptr<const LinearRateSnapshot>,const std::vector<MaterialDeclaration>&,const LinearMaterialPolicy&,const LinearMaterialLimits&);
 friend LinearMaterialPrefixResult linear_material_at(std::shared_ptr<const LinearMaterialSnapshot>,size_t,double,const LinearMaterialLimits&);
 friend MaterialBoxResult classify_linear_material(std::shared_ptr<const LinearMaterialPrefixSnapshot>,const MaterialRegion&,MaterialRepresentation,const LinearMaterialLimits&);
 friend MaterialCoverResult cover_linear_material(std::shared_ptr<const LinearMaterialPrefixSnapshot>,const MaterialRegion&,MaterialRepresentation,const MaterialCoverLimits&);
 friend JoinedMaterialResult reconstruct_joined_linear_material(std::shared_ptr<const LinearMaterialPrefixSnapshot>,const JoinedMaterialPolicy&,const JoinedMaterialLimits&);
 friend JoinedMaterialCoverResult cover_joined_linear_material_lower(std::shared_ptr<const JoinedMaterialSnapshot>,const MaterialRegion&,const JoinedMaterialLimits&);
};
struct LinearMaterialResult {
 RateStatus status=RateStatus::Unknown;std::string reason;std::optional<size_t> record;
 std::shared_ptr<const LinearMaterialSnapshot> snapshot;size_t evaluations=0;
};
struct LinearMaterialPrefixSnapshot {
 const std::shared_ptr<const LinearMaterialSnapshot> source;
 const size_t completed_records;const double current_progress;
 const std::vector<ReplayedBead> pieces;
 const RateBounds nominal_volume,delivered_volume;const size_t evaluations;
private:
 LinearMaterialPrefixSnapshot(std::shared_ptr<const LinearMaterialSnapshot> s,size_t n,double t,std::vector<ReplayedBead> b,RateBounds v,RateBounds delivered,size_t work)
  :source(std::move(s)),completed_records(n),current_progress(t),pieces(std::move(b)),nominal_volume(v),delivered_volume(delivered),evaluations(work){}
 friend LinearMaterialPrefixResult linear_material_at(std::shared_ptr<const LinearMaterialSnapshot>,size_t,double,const LinearMaterialLimits&);
};
struct LinearMaterialPrefixResult {
 RateStatus status=RateStatus::Unknown;std::string reason;std::shared_ptr<const LinearMaterialPrefixSnapshot> snapshot;size_t evaluations=0;
};
struct MaterialBoxResult {
 MaterialMembership membership=MaterialMembership::Unknown;std::string reason;
 std::optional<uint64_t> event_id;size_t evaluations=0;
};
struct MaterialCoverLimits : LinearMaterialLimits {size_t max_cells=100000;unsigned max_depth=32;};
struct MaterialCoverLeaf {MaterialRegion region;uint64_t event_id;};
struct MaterialCoverSnapshot {
 const std::shared_ptr<const LinearMaterialPrefixSnapshot> source;
 const MaterialRegion region;const MaterialRepresentation representation;
 const std::vector<MaterialCoverLeaf> leaves;const size_t evaluations,cells;
private:
 MaterialCoverSnapshot(std::shared_ptr<const LinearMaterialPrefixSnapshot> s,MaterialRegion b,MaterialRepresentation r,
  std::vector<MaterialCoverLeaf> l,size_t work,size_t count)
  :source(std::move(s)),region(b),representation(r),leaves(std::move(l)),evaluations(work),cells(count){}
 friend MaterialCoverResult cover_linear_material(std::shared_ptr<const LinearMaterialPrefixSnapshot>,const MaterialRegion&,MaterialRepresentation,const MaterialCoverLimits&);
};
struct MaterialCoverResult {
 RateStatus status=RateStatus::Unknown;std::string reason;std::shared_ptr<const MaterialCoverSnapshot> snapshot;
 std::optional<MaterialRegion> uncovered;size_t evaluations=0,cells=0;
};
// Declared constant-flux fixed-gap section model, synthetic only. Reconstructs
// final decimal poses/dose; shape declarations do not prove support/contact.
// Delivered-dose intervals are explicit assumptions, not physical calibration.
LinearMaterialResult reconstruct_linear_material(std::shared_ptr<const LinearRateSnapshot>,const std::vector<MaterialDeclaration>&,
 const LinearMaterialPolicy&,const LinearMaterialLimits &limits={});
// All previous actual depositions plus only the requested current fraction.
// No future rows, pressure material or blanket filled boxes/clearance PASS.
LinearMaterialPrefixResult linear_material_at(std::shared_ptr<const LinearMaterialSnapshot>,size_t,double,const LinearMaterialLimits &limits={});
// Whole closed region, not vertex sampling. Upper/lower use respectively the
// largest/smallest declared delivered dose and local XY/Z growth/erosion.
// Per-bead erosion retains finite butt gaps; no assumed packet seam repair.
MaterialBoxResult classify_linear_material(std::shared_ptr<const LinearMaterialPrefixSnapshot>,const MaterialRegion&,
 MaterialRepresentation,const LinearMaterialLimits &limits={});
// A protected partition covers the entire requested region with actual-prefix
// bead witnesses. An uncovered box proves a missing part; exhaustion is UNKNOWN.
MaterialCoverResult cover_linear_material(std::shared_ptr<const LinearMaterialPrefixSnapshot>,const MaterialRegion&,
 MaterialRepresentation,const MaterialCoverLimits &limits={});
inline constexpr unsigned joined_material_version=1;
enum class JoinedMaterialModel { CommonRunEnvelope };
struct JoinedMaterialPolicy {
 uint64_t version=1,policy_id=0,revision=0;bool synthetic=true,operator_confirmed_claim=false;
 JoinedMaterialModel model=JoinedMaterialModel::CommonRunEnvelope;
};
struct JoinedMaterialLimits : MaterialCoverLimits {std::function<bool(uint64_t,uint64_t)> is_join_current;};
struct JoinedMaterialRun {size_t first_record,last_record;MaterialBox outer_bounds;};
struct JoinedMaterialSnapshot {
 const std::shared_ptr<const LinearMaterialPrefixSnapshot> source;
 const JoinedMaterialPolicy policy;const std::vector<JoinedMaterialRun> runs;const size_t evaluations;
private:
 JoinedMaterialSnapshot(std::shared_ptr<const LinearMaterialPrefixSnapshot> s,JoinedMaterialPolicy p,std::vector<JoinedMaterialRun> r,size_t work)
  :source(std::move(s)),policy(p),runs(std::move(r)),evaluations(work){}
 friend JoinedMaterialResult reconstruct_joined_linear_material(std::shared_ptr<const LinearMaterialPrefixSnapshot>,const JoinedMaterialPolicy&,const JoinedMaterialLimits&);
};
struct JoinedMaterialResult {
 RateStatus status=RateStatus::Unknown;std::string reason;std::shared_ptr<const JoinedMaterialSnapshot> snapshot;size_t evaluations=0;
};
struct JoinedMaterialCoverLeaf {MaterialRegion region;size_t run_index;};
struct JoinedMaterialCoverSnapshot {
 const std::shared_ptr<const JoinedMaterialSnapshot> source;const MaterialRegion region;
 const std::vector<JoinedMaterialCoverLeaf> leaves;const size_t evaluations,cells;
private:
 JoinedMaterialCoverSnapshot(std::shared_ptr<const JoinedMaterialSnapshot> s,MaterialRegion b,std::vector<JoinedMaterialCoverLeaf> l,size_t work,size_t count)
  :source(std::move(s)),region(b),leaves(std::move(l)),evaluations(work),cells(count){}
 friend JoinedMaterialCoverResult cover_joined_linear_material_lower(std::shared_ptr<const JoinedMaterialSnapshot>,const MaterialRegion&,const JoinedMaterialLimits&);
};
struct JoinedMaterialCoverResult {
 RateStatus status=RateStatus::Unknown;std::string reason;std::shared_ptr<const JoinedMaterialCoverSnapshot> snapshot;
 std::optional<MaterialRegion> uncovered;size_t evaluations=0,cells=0;
};
// Explicit synthetic continuous-run envelope, distinct from per-event erosion.
// Only adjacent deposits with exact final XYZ continuity, collinear forward XY
// and matching section kind join. Interruptions/turns/reversals split the run.
JoinedMaterialResult reconstruct_joined_linear_material(std::shared_ptr<const LinearMaterialPrefixSnapshot>,
 const JoinedMaterialPolicy&,const JoinedMaterialLimits &limits={});
// Inflate the complete query with unchanged loss/error, clip at every actual
// packet boundary and prove every complete slice in its smallest-dose solid.
// Actual run ends/front stay strict; no future or pressure material appears.
JoinedMaterialCoverResult cover_joined_linear_material_lower(std::shared_ptr<const JoinedMaterialSnapshot>,
 const MaterialRegion&,const JoinedMaterialLimits &limits={});
}
