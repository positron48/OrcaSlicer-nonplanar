#pragma once
#include "Collision.hpp"
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
}
