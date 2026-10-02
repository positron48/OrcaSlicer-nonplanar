#pragma once
// Test-only independent 113-bit formulas on the parsed final coordinates/E/F/A.
// No planner helpers or geometry certificate. Decimal parser inputs are doubles.
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <nonplanar_verify/FullStopReplay.hpp>
#include <catch2/catch_test_macros.hpp>

namespace nptop_test {
template<class Policy> void check_full_stop_rates(const std::vector<nptop_verify::FullStopMove> &moves,const Policy &policy,bool corexy)
{
    using High=boost::multiprecision::cpp_bin_float_quad;
    const High area=acos(High(-1))*High(policy.filament_diameter.value())*High(policy.filament_diameter.value())/4;
    High total_time=0;
    for(size_t i=0;i<moves.size();++i) {
        INFO("final-byte rate oracle record=" << i);
        const auto &m=moves[i];
        if(m.kind==nptop_verify::FullStopKind::Dwell) {
            CHECK(High(m.dwell_seconds)*High(policy.max_events_per_second)>=1);total_time+=High(m.dwell_seconds);continue;
        }
        const std::array<High,3> delta{High(m.end[0])-High(m.start[0]),High(m.end[1])-High(m.start[1]),High(m.end[2])-High(m.start[2])};
        const High length=m.kind==nptop_verify::FullStopKind::Pressure ? abs(High(m.e)) : sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
        REQUIRE(length>0);const High acceleration(m.acceleration),feed=High(m.feed)/60;
        const High peak=std::min(feed,High(sqrt(acceleration*length)));
        CHECK(peak/length<=High(policy.max_events_per_second));
        if(m.kind==nptop_verify::FullStopKind::XYZ) {
            const std::array<High,3> drive=corexy ? std::array<High,3>{delta[0]+delta[1],delta[0]-delta[1],delta[2]} : delta;
            for(size_t axis=0;axis<3;++axis) {
                CHECK(abs(delta[axis])*peak/length<=High(policy.axis_speed_mm_s[axis]));
                CHECK(abs(delta[axis])*acceleration/length<=High(policy.axis_acceleration_mm_s2[axis]));
                CHECK(abs(drive[axis])*peak/length<=High(policy.drive_speed_mm_s[axis]));
                CHECK(abs(drive[axis])*acceleration/length<=High(policy.drive_acceleration_mm_s2[axis]));
            }
            if(m.e>0) {
                const High section=High(m.e)*area/length;
                CHECK(section<=High(policy.max_extrude_cross_section_mm2));CHECK(section*peak<=High(policy.max_volume_mm3_s));
            }
        } else CHECK(abs(High(m.e))<=High(policy.max_retraction.value()));
        CHECK(abs(High(m.e))*peak/length<=High(policy.filament_speed.value()));
        CHECK(abs(High(m.e))*acceleration/length<=High(policy.filament_acceleration.value()));
        total_time+=length/peak+peak/acceleration;
    }
    CHECK(total_time>0);
}
}
