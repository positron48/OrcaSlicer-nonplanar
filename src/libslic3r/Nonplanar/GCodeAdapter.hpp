#pragma once
#include "Contracts.hpp"
#include <string>
#include <vector>

namespace Slic3r::nptop {
// Simulation candidate only. Known initial position, millimetres, identity
// firmware transforms and externally fixed acceleration are prerequisites.
// No file publication, retraction, startup macros or safety approval.
std::string serialize_candidate(const std::vector<MotionEvent> &events,
                                Length filament_diameter, FlowCompensation flow);
}
