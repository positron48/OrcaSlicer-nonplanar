#include "GCodeAdapter.hpp"
#include "../GCodeWriter.hpp"
#include <set>

namespace Slic3r::nptop {
std::string serialize_candidate(const std::vector<MotionEvent> &events,
                                Length diameter, FlowCompensation flow)
{
    require(!events.empty() && events.size() <= 10000, "candidate event limit");
    (void) filament_feed(Volume(1), diameter, flow);
    GCodeWriter writer;
    writer.config.use_relative_e_distances.value = true;
    writer.set_extruders({0});
    writer.set_extruder(0);
    // IR is already in machine physical coordinates. Never subtract plate
    // placement a second time. Identity firmware mapping is the spike domain.
    writer.set_xy_offset(0, 0);
    const auto xyz = [](const PhysicalPosition &p) { return Vec3d(p.x(), p.y(), p.z()); };
    writer.set_position(xyz(events.front().start));
    std::set<uint64_t> ids;
    std::string bytes = "G90\nM83\n";
    for (size_t i = 0; i < events.size(); ++i) {
        const auto &event = events[i];
        validate_event(event);
        require(event.sequence_index == i && ids.insert(event.event_id).second, "invalid event order or identity");
        require(xyz(event.start) == writer.get_position(), "discontinuous candidate");
        require(xyz(event.start).cwiseAbs().maxCoeff() <= 10000 &&
                xyz(event.end).cwiseAbs().maxCoeff() <= 10000, "candidate coordinate limit");
        require(!std::holds_alternative<Retraction>(event.payload), "retraction not implemented in spike");
        require(xyz(event.start) != xyz(event.end), "stationary candidate move");
        bool quantized_motion = false;
        for (int axis = 0; axis < 3; ++axis)
            quantized_motion |= GCodeG1Formatter::quantize_xyzf(xyz(event.start)[axis]) !=
                                GCodeG1Formatter::quantize_xyzf(xyz(event.end)[axis]);
        require(quantized_motion, "candidate movement collapses at export precision");
        const double feed = event.speed_limit.value() * 60;
        require(feed >= 0.001 && feed < 100000, "candidate feed outside writer domain");
        double e = 0;
        if (const auto *bead = std::get_if<Deposition>(&event.payload)) {
            e = filament_feed(bead->volume, diameter, flow).value();
            require(e >= 0.00001 && e <= 10000, "candidate extrusion outside writer domain");
        }
        bytes += writer.set_speed(feed);
        // This low-level formatter accepts absolute XYZ without the GCode ZAA
        // branch. force_no_extrusion emits a straight G1 travel at our feed,
        // avoiding travel_to_xyz's implicit hop and alternate speed policy.
        bytes += writer.extrude_to_xyz(xyz(event.end), e, "", e == 0);
    }
    return bytes;
}
}
