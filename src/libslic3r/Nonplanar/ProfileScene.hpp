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
}
