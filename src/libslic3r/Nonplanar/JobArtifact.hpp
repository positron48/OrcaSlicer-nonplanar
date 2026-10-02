#pragma once
#include "Job.hpp"
#include "GCodeAdapter.hpp"

namespace Slic3r::nptop {
inline constexpr unsigned guarded_candidate_binding_version=1;
struct GuardedCandidateBindingResult;
struct GuardedCandidateBindingSnapshot {
    const std::shared_ptr<const GuardedJobSnapshot> job;
    const std::shared_ptr<const LinearCandidateSnapshot> candidate;
    const uint64_t attempt;
    const std::string manifest_json,manifest_sha256;
private:
    GuardedCandidateBindingSnapshot(std::shared_ptr<const GuardedJobSnapshot> context,std::shared_ptr<const LinearCandidateSnapshot> bytes,
        uint64_t generation,std::string manifest,std::string hash)
        :job(std::move(context)),candidate(std::move(bytes)),attempt(generation),manifest_json(std::move(manifest)),manifest_sha256(std::move(hash)){}
    friend GuardedCandidateBindingResult bind_guarded_candidate(Print &,const GuardedJobTask &,const LinearCandidateResult &,const GuardedJobLimits &);
};
struct GuardedCandidateBindingResult {
    std::string reason;
    std::shared_ptr<const GuardedCandidateBindingSnapshot> snapshot;
    std::shared_ptr<const GuardedJobTask> task;
};
// Bind protected complete bytes, initial pose, actual ledger and both original
// policies to one current Serializing attempt. Success supplies its Verifying
// token. Neither the association nor the manifest proves model-to-plan geometry,
// final replay, mandatory checks, measured resources or export eligibility.
// No files are published. Caller serializes native host operations at admission.
GuardedCandidateBindingResult bind_guarded_candidate(Print &,const GuardedJobTask &,const LinearCandidateResult &,
    const GuardedJobLimits &limits={});
}
