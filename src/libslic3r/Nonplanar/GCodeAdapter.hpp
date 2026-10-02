#pragma once
#include "Contracts.hpp"
#include "MotionPlan.hpp"
#include <string>
#include <vector>

namespace Slic3r::nptop {
// Simulation candidate only. Known initial position, millimetres, identity
// firmware transforms and externally fixed acceleration are prerequisites.
// No file publication, retraction, startup macros or safety approval.
std::string serialize_candidate(const std::vector<MotionEvent> &events,
                                Length filament_diameter, FlowCompensation flow);
inline constexpr unsigned linear_candidate_version=1;
struct LinearCandidatePolicy {
    uint64_t profile_id,revision;
    Acceleration initial_acceleration;
    unsigned xyz_digits=6,e_digits=9,feed_digits=6,acceleration_digits=6,dwell_digits=3;
    std::string fingerprint() const;
};
struct LinearCandidateLimits : LinearMotionPlanLimits {size_t max_bytes=32*1024*1024;};
struct CandidateEventBytes {size_t begin,end;};
struct LinearCandidateResult;
struct LinearCandidateSnapshot {
    const std::shared_ptr<const LinearMotionPlanSnapshot> plan;
    const LinearCandidatePolicy policy;
    const PhysicalPosition initial_position;
    const std::string bytes,sha256,policy_fingerprint;
    const std::vector<CandidateEventBytes> events;
    const double acceleration_mm_s2,coordinate_rounding_error_mm;
    const size_t evaluations;
private:
    LinearCandidateSnapshot(std::shared_ptr<const LinearMotionPlanSnapshot> source,LinearCandidatePolicy p,PhysicalPosition initial,
        std::string text,std::string hash,std::string identity,std::vector<CandidateEventBytes> ranges,double acceleration,double error,size_t work)
        : plan(std::move(source)),policy(p),initial_position(initial),bytes(std::move(text)),sha256(std::move(hash)),policy_fingerprint(std::move(identity)),events(std::move(ranges)),
          acceleration_mm_s2(acceleration),coordinate_rounding_error_mm(error),evaluations(work) {}
    friend LinearCandidateResult serialize_linear_candidate(const LinearMotionPlanResult &,const LinearCandidatePolicy &,const LinearCandidateLimits &);
};
struct LinearCandidateResult {
    std::string reason;
    std::shared_ptr<const LinearMotionPlanSnapshot> plan;
    std::shared_ptr<const LinearCandidateSnapshot> snapshot;
    size_t evaluations=0;
};
// Owned final bytes only, not motion/geometry verification or export approval.
// Native formatting, one bounded acceleration <= known initial and every step's
// ceiling, explicit per-event feed/XYZ or pressure/dwell and complete barriers.
// Precision is isolated from stock. Final rounded geometry/E/rates require replay.
LinearCandidateResult serialize_linear_candidate(const LinearMotionPlanResult &,const LinearCandidatePolicy &,
    const LinearCandidateLimits &limits={});
}
