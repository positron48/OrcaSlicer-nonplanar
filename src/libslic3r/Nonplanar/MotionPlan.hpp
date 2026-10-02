#pragma once
#include "ProfileScene.hpp"
#include <array>

namespace Slic3r::nptop {
inline constexpr unsigned linear_motion_plan_version=1;
enum class LinearPlannerModel { FullStop, FirmwareLookahead };
enum class LinearKinematics { Cartesian, CoreXY };
struct LinearMotionPolicy {
    uint64_t version,profile_id,revision;
    ProfileOrigin origin;
    bool operator_confirmed_claim;
    LinearPlannerModel model;
    LinearKinematics kinematics;
    SceneBox commanded_domain;
    std::array<double,3> axis_speed_mm_s,axis_acceleration_mm_s2;
    std::array<double,3> drive_speed_mm_s,drive_acceleration_mm_s2;
    Length filament_diameter;
    FlowCompensation flow;
    Speed filament_speed;
    Acceleration filament_acceleration;
    Length max_retraction;
    double max_volume_mm3_s,max_extrude_cross_section_mm2,max_events_per_second;
    std::string fingerprint() const;
};
struct LinearMotionPlanLimits : SimulationMotionPreparationLimits {
    size_t max_records=200000;
    std::function<bool(uint64_t profile_id,uint64_t revision)> is_policy_current;
};
enum class LinearStepCoordinate { XYZ, Filament, Dwell };
struct LinearMotionStep {
    LinearStepCoordinate coordinate;
    ScalarBounds distance_mm,filament_mm,duration_s;
    double peak_speed_mm_s,acceleration_mm_s2;
};
struct LinearMotionPlanResult;
struct LinearMotionPlanSnapshot {
    const std::shared_ptr<const SimulationMotionSourceSnapshot> source,planned;
    const LinearMotionPolicy policy;
    const std::string policy_fingerprint;
    const std::vector<LinearMotionStep> steps;
    const ScalarBounds nominal_volume_mm3,deposition_filament_mm,duration_s;
    const size_t evaluations;
private:
    LinearMotionPlanSnapshot(std::shared_ptr<const SimulationMotionSourceSnapshot> s,
        std::shared_ptr<const SimulationMotionSourceSnapshot> p,LinearMotionPolicy limits,std::string identity,
        std::vector<LinearMotionStep> moves,ScalarBounds v,ScalarBounds e,ScalarBounds time,size_t work)
        : source(std::move(s)),planned(std::move(p)),policy(std::move(limits)),policy_fingerprint(std::move(identity)),
          steps(std::move(moves)),nominal_volume_mm3(v),deposition_filament_mm(e),duration_s(time),evaluations(work) {}
    friend LinearMotionPlanResult plan_linear_motion(const SimulationMotionSourceResult &,const LinearMotionPolicy &,const LinearMotionPlanLimits &);
};
struct LinearMotionPlanResult {
    ClearanceStatus status=ClearanceStatus::Unknown;
    std::string reason;
    std::shared_ptr<const SimulationMotionSourceSnapshot> source;
    std::shared_ptr<const LinearMotionPlanSnapshot> snapshot;
    std::optional<size_t> blocked_record;
    size_t evaluations=0;
};
// Complete ordered journal, commanded straight XYZ and separate pure-E pressure.
// Preserve poses/material; reduce XYZ speed/acceleration for all axes, E, Q,
// cross-section and event frequency. Every event starts/ends at rest; consumers
// must enforce full stops and instant-Travel dwell, not reuse firmware lookahead.
// Only synthetic identity-transform simulation. Geometry/contact, firmware
// qualification, final rounded bytes and whole-job/export approval remain separate.
LinearMotionPlanResult plan_linear_motion(const SimulationMotionSourceResult &,const LinearMotionPolicy &,
    const LinearMotionPlanLimits &limits={});
}
