#pragma once
#include "FullStopReplay.hpp"
#include <cstdint>
#include <memory>
#include <optional>

namespace nptop_verify {
inline constexpr unsigned linear_rate_version=1;
enum class RateStatus { Pass, Fail, Unknown };
enum class RateKinematics { Cartesian, CoreXY };
enum class RateModel { FullStop, FirmwareLookahead };
struct RateBounds {double lower,upper;};
struct LinearRatePolicy {
    uint64_t version=1,profile_id=0,revision=0;
    bool synthetic=true,operator_confirmed_claim=false;
    RateModel model=RateModel::FullStop;
    RateKinematics kinematics=RateKinematics::Cartesian;
    std::array<double,3> position_min{},position_max{},axis_speed{},axis_acceleration{},drive_speed{},drive_acceleration{};
    double initial_acceleration=0,filament_diameter=0,flow=0,filament_speed=0,filament_acceleration=0;
    double max_retraction=0,max_volume_rate=0,max_cross_section=0,max_event_rate=0;
};
struct LinearRateLimits : FullStopReplayLimits {
    size_t max_evaluations=2000000,initial_evaluations=0;
    std::function<bool(uint64_t,uint64_t)> is_current;
};
struct LinearRateStep {
    RateBounds distance,peak,duration,command_volume,nominal_volume;
};
struct LinearRateResult;
struct LinearRateSnapshot {
    const std::string bytes;
    const std::array<double,3> initial_position;
    const LinearRatePolicy policy;
    const std::vector<FullStopMove> moves;
    const std::vector<LinearRateStep> steps;
    const RateBounds command_volume,nominal_volume,duration,final_pressure_debt;
    const size_t evaluations;
private:
    LinearRateSnapshot(std::string text,std::array<double,3> initial,LinearRatePolicy p,std::vector<FullStopMove> parsed,
        std::vector<LinearRateStep> proof,RateBounds v,RateBounds n,RateBounds time,RateBounds debt,size_t work)
        : bytes(std::move(text)),initial_position(initial),policy(p),moves(std::move(parsed)),steps(std::move(proof)),
          command_volume(v),nominal_volume(n),duration(time),final_pressure_debt(debt),evaluations(work) {}
    friend LinearRateResult verify_linear_rates(const std::string &,std::array<double,3>,const LinearRatePolicy &,const LinearRateLimits &);
};
struct LinearRateResult {
    RateStatus status=RateStatus::Unknown;
    std::string reason;
    std::optional<size_t> record,axis;
    std::shared_ptr<const LinearRateSnapshot> snapshot;
    size_t evaluations=0;
};
// Independent final decimal bytes, exact rational rate decisions, enclosed pi
// and ideal rest-to-rest timing. All positions/axes/Cartesian-CoreXY drives/E/Q/
// cross-section/pressure/event rates; pressure contributes no deposited volume.
// Synthetic identity-transform full-stop component only. Not a complete-job
// geometry/contact/delivered-volume certificate or export approval.
LinearRateResult verify_linear_rates(const std::string &,std::array<double,3>,const LinearRatePolicy &,
    const LinearRateLimits &limits={});
}
