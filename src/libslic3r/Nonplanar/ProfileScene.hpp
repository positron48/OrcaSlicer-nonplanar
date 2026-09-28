#pragma once
#include "Collision.hpp"
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
}
