#pragma once
#include "Job.hpp"
#include "PlanarBody.hpp"
#include "GCodeAdapter.hpp"

namespace Slic3r::nptop {
// Actual worker inputs, copied by begin_guarded_job before any callback.
// Identity only: these declarations do not qualify geometry or firmware.
struct NativeJobInputsRequest {
    BodyMaterialParameters body;
    SimulationScene scene;
    ClearancePolicy clearance;
    LinearMotionPolicy motion;
    LinearCandidatePolicy serializer;
    LinearMaterialOptions replay;
};
struct NativeJobInputsSnapshot {
    const NativeJobInputsRequest values;
    const std::vector<JobResource> resources;
    bool matches_body(const BodyMaterialParameters &) const;
    bool matches_candidate(const LinearCandidateSnapshot &,const std::function<void()> &poll) const;
    bool matches_replay(const LinearMaterialOptions &) const;
private:
    NativeJobInputsSnapshot(NativeJobInputsRequest inputs,std::vector<JobResource> encoded)
        :values(std::move(inputs)),resources(std::move(encoded)){}
    friend std::shared_ptr<const NativeJobInputsSnapshot> capture_native_job_inputs(NativeJobInputsRequest,const std::function<void()> &);
};
// Input has already been bounded and owned by the job root. Polls belong to
// that original capture deadline; no second timeout or host access is created.
std::shared_ptr<const NativeJobInputsSnapshot> capture_native_job_inputs(NativeJobInputsRequest,const std::function<void()> &poll);
}
