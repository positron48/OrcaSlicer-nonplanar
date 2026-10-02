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
struct MaterialBoundsNode {MaterialBox bounds;size_t begin,end,left=0,right=0;};
struct MaterialBoundsIndex {std::vector<size_t> order;std::vector<MaterialBoundsNode> nodes;};
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
struct LinearRunSupportPolicy;
struct LinearRunSupportLimits;
struct LinearRunSupportResult;
struct LinearTravelScene;
struct LinearTravelLimits;
struct LinearTravelResult;
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
 friend JoinedMaterialCoverResult cover_joined_linear_material(std::shared_ptr<const JoinedMaterialSnapshot>,const MaterialRegion&,MaterialRepresentation,const JoinedMaterialLimits&);
 friend LinearRunSupportResult verify_linear_run_support(std::shared_ptr<const JoinedMaterialSnapshot>,size_t,const LinearRunSupportPolicy&,const LinearRunSupportLimits&);
 friend LinearTravelResult verify_linear_travel_geometry(std::shared_ptr<const LinearMaterialSnapshot>,size_t,size_t,const LinearTravelScene&,const LinearTravelLimits&);
};
struct LinearMaterialResult {
 RateStatus status=RateStatus::Unknown;std::string reason;std::optional<size_t> record;
 std::shared_ptr<const LinearMaterialSnapshot> snapshot;size_t evaluations=0;
};
struct LinearMaterialPrefixSnapshot {
 const std::shared_ptr<const LinearMaterialSnapshot> source;
 const size_t completed_records;const double current_progress;
 const std::vector<ReplayedBead> pieces;
 const MaterialBoundsIndex nominal_index; // protected broad-phase only, never filled material
 const RateBounds nominal_volume,delivered_volume;const size_t evaluations;
private:
 LinearMaterialPrefixSnapshot(std::shared_ptr<const LinearMaterialSnapshot> s,size_t n,double t,std::vector<ReplayedBead> b,MaterialBoundsIndex index,RateBounds v,RateBounds delivered,size_t work)
  :source(std::move(s)),completed_records(n),current_progress(t),pieces(std::move(b)),nominal_index(std::move(index)),nominal_volume(v),delivered_volume(delivered),evaluations(work){}
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
 const JoinedMaterialPolicy policy;const std::vector<JoinedMaterialRun> runs;const MaterialBoundsIndex outer_index;const size_t evaluations;
private:
 JoinedMaterialSnapshot(std::shared_ptr<const LinearMaterialPrefixSnapshot> s,JoinedMaterialPolicy p,std::vector<JoinedMaterialRun> r,MaterialBoundsIndex index,size_t work)
  :source(std::move(s)),policy(p),runs(std::move(r)),outer_index(std::move(index)),evaluations(work){}
 friend JoinedMaterialResult reconstruct_joined_linear_material(std::shared_ptr<const LinearMaterialPrefixSnapshot>,const JoinedMaterialPolicy&,const JoinedMaterialLimits&);
};
struct JoinedMaterialResult {
 RateStatus status=RateStatus::Unknown;std::string reason;std::shared_ptr<const JoinedMaterialSnapshot> snapshot;size_t evaluations=0;
};
struct JoinedMaterialCoverLeaf {MaterialRegion region;size_t run_index;};
struct JoinedMaterialCoverSnapshot {
 const std::shared_ptr<const JoinedMaterialSnapshot> source;const MaterialRegion region;const MaterialRepresentation representation;
 const std::vector<JoinedMaterialCoverLeaf> leaves;const size_t evaluations,cells;
private:
 JoinedMaterialCoverSnapshot(std::shared_ptr<const JoinedMaterialSnapshot> s,MaterialRegion b,MaterialRepresentation r,std::vector<JoinedMaterialCoverLeaf> l,size_t work,size_t count)
  :source(std::move(s)),region(b),representation(r),leaves(std::move(l)),evaluations(work),cells(count){}
 friend JoinedMaterialCoverResult cover_joined_linear_material_lower(std::shared_ptr<const JoinedMaterialSnapshot>,const MaterialRegion&,const JoinedMaterialLimits&);
 friend JoinedMaterialCoverResult cover_joined_linear_material(std::shared_ptr<const JoinedMaterialSnapshot>,const MaterialRegion&,MaterialRepresentation,const JoinedMaterialLimits&);
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
// Exact closed Nominal union across actual packet cuts, or original eroded
// common-run Lower. The protected representation tag prevents role confusion.
// Upper remains a separate query; no whole-run physical bonding is inferred.
JoinedMaterialCoverResult cover_joined_linear_material(std::shared_ptr<const JoinedMaterialSnapshot>,
 const MaterialRegion&,MaterialRepresentation,const JoinedMaterialLimits &limits={});
inline constexpr unsigned linear_run_support_version=1;
struct LinearRunSupportPolicy {
 uint64_t version=1,policy_id=0,revision=0;bool synthetic=true,operator_confirmed_claim=false;
 double cross_slope=0,vertical_min=0,vertical_max=0,normal_min=0,normal_max=0;
};
struct LinearRunSupportLimits : JoinedMaterialLimits {std::function<bool(uint64_t,uint64_t)> is_support_current;};
struct LinearRunSupportLeaf {
 size_t target_record;RateBounds progress,transverse;
 MaterialRegion vertical_near,vertical_terminal,normal_near,normal_terminal;
 std::shared_ptr<const JoinedMaterialCoverSnapshot> lower_anchor;
 std::shared_ptr<const JoinedMaterialCoverSnapshot> nominal_terminal;
};
struct LinearRunSupportResult;
struct LinearRunSupportSnapshot {
 const std::shared_ptr<const JoinedMaterialSnapshot> source,support;
 const size_t run_index;const LinearRunSupportPolicy policy;
 const double query_error_mm;const std::vector<LinearRunSupportLeaf> leaves;
 const size_t evaluations,cells;
private:
 LinearRunSupportSnapshot(std::shared_ptr<const JoinedMaterialSnapshot> s,std::shared_ptr<const JoinedMaterialSnapshot> old,size_t index,
  LinearRunSupportPolicy p,double error,std::vector<LinearRunSupportLeaf> parts,size_t work,size_t count)
  :source(std::move(s)),support(std::move(old)),run_index(index),policy(p),query_error_mm(error),leaves(std::move(parts)),evaluations(work),cells(count){}
 friend LinearRunSupportResult verify_linear_run_support(std::shared_ptr<const JoinedMaterialSnapshot>,size_t,const LinearRunSupportPolicy&,const LinearRunSupportLimits&);
};
struct LinearRunSupportWitness {size_t target_record;RateBounds progress,transverse;MaterialRegion region;};
struct LinearRunSupportResult {
 RateStatus status=RateStatus::Unknown;std::string reason;std::shared_ptr<const LinearRunSupportSnapshot> snapshot;
 std::optional<LinearRunSupportWitness> witness;size_t evaluations=0,cells=0;
};
// Whole actual run footprint and explicit affine carrier slope. Only material
// before this run can supply nominal gap bands and a guaranteed Lower anchor.
// All source/errors/bands/limits remain; this is not contact/export approval.
LinearRunSupportResult verify_linear_run_support(std::shared_ptr<const JoinedMaterialSnapshot>,size_t,
 const LinearRunSupportPolicy&,const LinearRunSupportLimits &limits={});

inline constexpr unsigned linear_travel_version=1;
enum class TravelHeadRole { NozzleBody,Heater,Sock,Duct,Sensor,Mount };
struct TravelHeadBox {uint64_t id;TravelHeadRole role;MaterialRegion local;bool moving=false,all_configurations_enclosed=false;};
struct LinearTravelScene {
 uint64_t version=1,profile_id=0,revision=0;bool synthetic=true,operator_confirmed_claim=false;
 std::array<double,3> tip_center{};double opening_radius_mm=0,outer_radius_mm=0;
 std::vector<TravelHeadBox> head;std::vector<MaterialRegion> obstacles;
 MaterialRegion nozzle_domain,scene_domain;
 bool obstacle_inventory_complete=false;double unmodelled_parts_min_local_z_mm=0,uncertainty_mm=0;
 // required, import, chord, distance, conversion, tool, positioning, material,
 // scene. All are retained; the independent exact query adds them conservatively.
 std::array<double,9> clearance_mm{};
};
struct LinearTravelLimits : MaterialCoverLimits {std::function<bool(uint64_t,uint64_t)> is_scene_current;};
struct LinearTravelLeaf {size_t record,component;RateBounds progress;MaterialRegion local,world;bool outside_annulus=false;};
struct LinearTravelWitness {
 size_t record,component;RateBounds progress;MaterialRegion point;
 std::optional<uint64_t> material_event;std::optional<size_t> obstacle;
};
struct LinearTravelSnapshot {
 const std::shared_ptr<const LinearMaterialSnapshot> source;
 const std::shared_ptr<const LinearMaterialPrefixSnapshot> prefix;
 const size_t first_record,record_count;const LinearTravelScene scene;
 const std::vector<LinearTravelLeaf> leaves;const size_t evaluations,cells;
private:
 LinearTravelSnapshot(std::shared_ptr<const LinearMaterialSnapshot> s,std::shared_ptr<const LinearMaterialPrefixSnapshot> p,
  size_t first,size_t count,LinearTravelScene head,std::vector<LinearTravelLeaf> parts,size_t work,size_t n)
  :source(std::move(s)),prefix(std::move(p)),first_record(first),record_count(count),scene(std::move(head)),leaves(std::move(parts)),evaluations(work),cells(n){}
 friend LinearTravelResult verify_linear_travel_geometry(std::shared_ptr<const LinearMaterialSnapshot>,size_t,size_t,const LinearTravelScene&,const LinearTravelLimits&);
};
struct LinearTravelResult {
 RateStatus status=RateStatus::Unknown;std::string reason;std::shared_ptr<const LinearTravelSnapshot> snapshot;
 std::optional<LinearTravelWitness> witness;size_t evaluations=0,cells=0;
};
// Independently parsed final decimals and reconstructed Upper material before
// one complete contiguous Travel block. Whole fixed-axis annulus/head/static
// sweeps, exact rational bounds and complete partitions; no sampled PASS,
// contact exception, future material, planner geometry or whole-job/export claim.
LinearTravelResult verify_linear_travel_geometry(std::shared_ptr<const LinearMaterialSnapshot>,size_t first_record,size_t record_count,
 const LinearTravelScene&,const LinearTravelLimits &limits={});
}
