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
// Declared constant-flux fixed-gap section model, synthetic only. Reconstructs
// final decimal poses/dose; shape declarations do not prove support/contact.
// Delivered-dose intervals are explicit assumptions, not physical calibration.
LinearMaterialResult reconstruct_linear_material(std::shared_ptr<const LinearRateSnapshot>,const std::vector<MaterialDeclaration>&,
 const LinearMaterialPolicy&,const LinearMaterialLimits &limits={});
// All previous actual depositions plus only the requested current fraction.
// No future rows, pressure material or blanket filled boxes/clearance PASS.
LinearMaterialPrefixResult linear_material_at(std::shared_ptr<const LinearMaterialSnapshot>,size_t,double,const LinearMaterialLimits &limits={});
}
