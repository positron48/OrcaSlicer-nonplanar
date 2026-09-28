#pragma once

#include "Contracts.hpp"
#include <chrono>
#include <cstddef>
#include <optional>

namespace Slic3r::nptop {

inline constexpr unsigned geometry_contract_version = 1;
using ToolPosition = Position<Frame::ToolLocal>;

struct FiniteTip {
    ToolPosition center;
    Length opening_radius, outer_radius;
};
struct ToolBox { ToolPosition min, max; };
struct SceneBox { PhysicalPosition min, max; };
// Solid z <= gradient_x*x + gradient_y*y + intercept_mm, in physical space.
struct PlaneObstacle { double gradient_x, gradient_y, intercept_mm; };
enum class InteractionClass { RigidForbidden, DepositionContact };
struct ToolComponent {
    uint64_t id;
    std::variant<FiniteTip, ToolBox> geometry;
    InteractionClass interaction = InteractionClass::RigidForbidden;
};
struct SceneObstacle {
    uint64_t id;
    std::variant<PlaneObstacle, SceneBox> geometry;
};

struct ClearancePolicy {
    Length required;
    NumericBudget numeric;
    Length tool_measurement, positioning, material;
};
struct QueryLimits {
    size_t max_evaluations = 4095;
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max();
};
enum class ClearanceStatus { Pass, Fail, Unknown };
enum class ClearanceReason {
    Separated, ClearanceViolation, UncertainBoundary, UnsupportedPair,
    UnsupportedContact, InvalidInput, NumericalFailure, WorkLimit, Timeout
};
struct ClearanceBounds {
    double lower_mm, upper_mm;
    double bound_error_mm; // charged input uncertainty; arithmetic is already in both bounds
};
struct ClearanceWitness {
    double parameter; // position on the original event, not a first-contact estimate
    PhysicalPosition nozzle;
};
struct ClearanceResult {
    ClearanceStatus status = ClearanceStatus::Unknown;
    ClearanceReason reason = ClearanceReason::InvalidInput;
    uint64_t event_id, sequence_index, component_id, obstacle_id;
    InteractionClass interaction;
    double required_clearance_mm;
    double parameter_begin = 0, parameter_end = 1; // checked domain; not a collision interval
    std::optional<ClearanceBounds> bounds;
    std::optional<ClearanceWitness> witness;
    size_t evaluations = 0;
};

// A primitive check only: does not establish profile completeness, support or export permission.
ClearanceResult query_clearance(const MotionEvent &, const ToolComponent &, const SceneObstacle &,
                                const ClearancePolicy &, const QueryLimits & = {});

} // namespace Slic3r::nptop
