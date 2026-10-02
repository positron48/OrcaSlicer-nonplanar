#pragma once
#include "Collision.hpp"
#include "DepositionModel.hpp"
#include <functional>
#include <string>
#include <vector>

namespace Slic3r::nptop {
inline constexpr unsigned simulation_scene_version = 1;
enum class ProfileOrigin { Synthetic, OperatorMeasured };
enum class HeadPart { NozzleBody, Heater, Sock, Duct, Sensor, Mount };
struct HeadEnvelope {
    uint64_t id;
    HeadPart part;
    ToolBox outer;
    // A moving part must have an all-configurations outer envelope over the
    // entire declared nozzle domain. This is a synthetic scene assumption.
    bool moving = false;
    bool all_configurations_enclosed = false;
};
struct SimulationScene {
    unsigned version;
    uint64_t profile_id, revision;
    ProfileOrigin origin;
    bool operator_confirmed_claim;
    FiniteTip tip;
    std::vector<HeadEnvelope> head;
    SceneBox nozzle_domain, scene_domain;
    std::vector<SceneBox> obstacles;
    bool obstacle_inventory_complete;
    Length unmodelled_parts_min_local_z;
    Length uncertainty;
};
enum class SceneApplicability { SimulationOnly, InvalidInput, UnsupportedVersion,
                                UnsupportedQualification, IncompleteGeometry,
                                OutsideCoverage, NumericalFailure };
// Checks only declared coverage for straight nozzle motions and fixed-axis
// outer boxes. SimulationOnly is NOT collision clearance or export permission.
SceneApplicability assess_simulation_scene(const SimulationScene &, const std::vector<MotionEvent> &);

struct SimulationTravelLimits {
    size_t max_pairs=100000, max_evaluations_per_pair=4095;
    std::chrono::milliseconds timeout{1000};
    std::function<bool()> cancelled;
    std::function<bool(uint64_t profile_id, uint64_t revision)> is_current;
};
struct SimulationTravelResult {
    SceneApplicability applicability=SceneApplicability::InvalidInput;
    ClearanceStatus status=ClearanceStatus::Unknown;
    std::string reason;
    uint64_t profile_id=0, revision=0, tip_component_id=0;
    size_t required_pairs=0, checked_pairs=0;
    std::optional<ClearanceResult> limiting_check;
};
// Owned whole-head Travel-only analysis against the declared static inventory.
// Obstacle IDs are one-based captured indices. PASS remains SimulationOnly.
SimulationTravelResult check_simulation_travel(const SimulationScene &, const std::vector<MotionEvent> &,
    const ClearancePolicy &, const SimulationTravelLimits &limits = {});
inline constexpr unsigned simulation_material_motion_version=1;
struct SimulationMotionPreparationLimits : MaterialQueryLimits {
    std::function<bool(uint64_t profile_id,uint64_t revision)> is_scene_current;
};
struct SimulationMotionSourceResult;
struct SimulationMotionSourceSnapshot {
    const SimulationScene scene;
    const std::shared_ptr<const MaterialMotionSourceSnapshot> material;
    const ClearancePolicy policy,static_policy,material_policy;
    const std::vector<ToolComponent> components;
    const uint64_t tip_component_id;
private:
    SimulationMotionSourceSnapshot(SimulationScene s,std::shared_ptr<const MaterialMotionSourceSnapshot> m,ClearancePolicy p,
        ClearancePolicy stat,ClearancePolicy deposited,std::vector<ToolComponent> tools,uint64_t tip)
        : scene(std::move(s)),material(std::move(m)),policy(p),static_policy(stat),material_policy(deposited),components(std::move(tools)),tip_component_id(tip) {}
    friend SimulationMotionSourceResult prepare_simulation_motion(const SimulationScene &,const MaterialMotionSourceResult &,
        const ClearancePolicy &,const SimulationMotionPreparationLimits &);
};
struct SimulationMotionSourceResult {
    SceneApplicability applicability=SceneApplicability::InvalidInput;
    std::string reason;std::shared_ptr<const SimulationMotionSourceSnapshot> snapshot;size_t evaluations=0;
};
// Capture the complete declared synthetic head/inventory and the exact original
// ledger. Upper Z bounds constrain omitted upper parts; future rows are coverage
// inputs only. This never qualifies an operator/measured printer profile.
SimulationMotionSourceResult prepare_simulation_motion(const SimulationScene &,const MaterialMotionSourceResult &,
    const ClearancePolicy &,const SimulationMotionPreparationLimits &limits={});
struct SimulationMotionLimits : MaterialMotionLimits {
    size_t max_scene_pairs=100000,max_evaluations_per_scene_pair=4095;
    std::function<bool(uint64_t profile_id,uint64_t revision)> is_scene_current;
};
struct SimulationMotionResult;
struct SimulationMotionSnapshot {
    const std::shared_ptr<const SimulationMotionSourceSnapshot> source;
    const std::shared_ptr<const MaterialMotionSnapshot> material;
    const std::vector<ClearanceResult> scene_checks;
    const size_t evaluations;
private:
    SimulationMotionSnapshot(std::shared_ptr<const SimulationMotionSourceSnapshot> s,std::shared_ptr<const MaterialMotionSnapshot> m,
        std::vector<ClearanceResult> pairs,size_t work) : source(std::move(s)),material(std::move(m)),scene_checks(std::move(pairs)),evaluations(work) {}
    friend SimulationMotionResult check_simulation_motion(const SimulationMotionSourceResult &,size_t,const SimulationMotionLimits &);
};
struct SimulationMotionResult {
    ClearanceStatus status=ClearanceStatus::Unknown;std::string reason;
    size_t event_index=0;
    std::shared_ptr<const SimulationMotionSourceSnapshot> source;
    std::shared_ptr<const SimulationMotionSnapshot> snapshot;
    std::optional<ClearanceResult> scene_check;
    std::optional<MaterialMotionResult> material_check;
    size_t checked_scene_pairs=0,evaluations=0;
};
// Same original event for every complete head/static pair and for the coupled
// material proof. One shared work/deadline; partial diagnostics never grant the
// protected complete result. PASS remains SimulationOnly, not job/export approval.
SimulationMotionResult check_simulation_motion(const SimulationMotionSourceResult &,size_t event_index,const SimulationMotionLimits &limits={});
inline constexpr unsigned simulation_lift_route_version=1;
struct SimulationLiftRouteLimits : SimulationMotionLimits {size_t max_records=200000;};
struct SimulationLiftRouteResult;
struct SimulationLiftRouteSnapshot {
    const std::shared_ptr<const SimulationMotionSourceSnapshot> source,planned;
    const size_t original_event_index;
    const double lift_z_mm;
    const std::vector<size_t> source_records;
    const std::vector<std::shared_ptr<const SimulationMotionSnapshot>> legs;
    const size_t cells,checked_scene_pairs,evaluations;
private:
    SimulationLiftRouteSnapshot(std::shared_ptr<const SimulationMotionSourceSnapshot> original,
        std::shared_ptr<const SimulationMotionSourceSnapshot> route,size_t index,double z,std::vector<size_t> origins,
        std::vector<std::shared_ptr<const SimulationMotionSnapshot>> checks,size_t count,size_t pairs,size_t work)
        : source(std::move(original)),planned(std::move(route)),original_event_index(index),lift_z_mm(z),source_records(std::move(origins)),
          legs(std::move(checks)),cells(count),checked_scene_pairs(pairs),evaluations(work) {}
    friend SimulationLiftRouteResult plan_simulation_lifted_travel(const SimulationMotionSourceResult &,size_t,double,const SimulationLiftRouteLimits &);
};
struct SimulationLiftRouteResult {
    ClearanceStatus status=ClearanceStatus::Unknown;std::string reason;
    std::shared_ptr<const SimulationMotionSourceSnapshot> source;
    std::shared_ptr<const SimulationLiftRouteSnapshot> snapshot;
    std::optional<size_t> blocked_leg;
    std::optional<SimulationMotionResult> motion_check;
    size_t cells=0,checked_scene_pairs=0,evaluations=0;
};
// Replace one original Travel by exit/lift, level transfer and descent/entry at
// an explicit physical Z. Revalidate the complete candidate ledger, preserving
// every other payload/pose/ID and its order; only sequence indices shift.
// Each leg uses its original actual prefix with every captured head/static pair.
// Shared source/preparation/pair/material work/cells/deadline; no unchecked lift,
// automatic Z-hop, deposition contact, whole-job or export qualification.
SimulationLiftRouteResult plan_simulation_lifted_travel(const SimulationMotionSourceResult &,size_t event_index,double lift_z_mm,
    const SimulationLiftRouteLimits &limits={});

inline constexpr unsigned simulation_cap_departure_version=1;
struct SimulationCapDepartureRequest {
    PhysicalPosition destination;
    double lift_z_mm;
    Speed travel_speed;
    Acceleration travel_acceleration;
};
struct SimulationCapDepartureLimits : SimulationLiftRouteLimits { FirstCapMaterialLimits material; };
struct SimulationCapDepartureResult;
struct SimulationCapDepartureSnapshot {
    const std::shared_ptr<const FirstCapMaterialSnapshot> before,material;
    const std::shared_ptr<const NextCapBeadSnapshot> bead;
    const std::shared_ptr<const SimulationLiftRouteSnapshot> route;
    const SimulationCapDepartureRequest request;
    const size_t evaluations;
private:
    SimulationCapDepartureSnapshot(std::shared_ptr<const FirstCapMaterialSnapshot> original,
        std::shared_ptr<const FirstCapMaterialSnapshot> laid,std::shared_ptr<const NextCapBeadSnapshot> path,
        std::shared_ptr<const SimulationLiftRouteSnapshot> travel,SimulationCapDepartureRequest options,size_t work)
        :before(std::move(original)),material(std::move(laid)),bead(std::move(path)),route(std::move(travel)),request(options),evaluations(work) {}
    friend SimulationCapDepartureResult plan_simulation_cap_departure(const FirstCapMaterialResult &,const NextCapBeadResult &,
        const SimulationScene &,const ClearancePolicy &,const SimulationCapDepartureRequest &,const SimulationCapDepartureLimits &);
};
struct SimulationCapDepartureResult {
    ClearanceStatus status=ClearanceStatus::Unknown;std::string reason;
    std::shared_ptr<const FirstCapMaterialSnapshot> before;
    std::shared_ptr<const NextCapBeadSnapshot> bead;
    std::shared_ptr<const SimulationCapDepartureSnapshot> snapshot;
    std::optional<SimulationLiftRouteResult> route_check;
    size_t evaluations=0;
};
// Append one already calculated prospective bead on its exact complete parent,
// preserving all old records and original cap/cell targets. Then append and
// continuously prove its complete exit/transfer/entry with the whole head and
// actual new prefix. All assembly/preparation/route stages share original limits.
// Does not select widths, repair already-laid material, qualify the bead's print
// motion/contact, full target fill, whole job, independent byte replay or export.
SimulationCapDepartureResult plan_simulation_cap_departure(const FirstCapMaterialResult &,const NextCapBeadResult &,
    const SimulationScene &,const ClearancePolicy &,const SimulationCapDepartureRequest &,const SimulationCapDepartureLimits &limits={});
}
