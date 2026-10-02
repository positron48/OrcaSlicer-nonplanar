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

inline constexpr unsigned guarded_report_version=1,guarded_check_registry_version=1;
enum class GuardedCheckExecution {Run,NotRun,Skipped,Error};
struct GuardedReportCheck {
    std::string id;
    nptop_verify::RateStatus status;
    GuardedCheckExecution execution;
    std::string reason;
};
const std::vector<std::string> &guarded_mandatory_checks();
struct GuardedCandidateReportResult;
struct GuardedCandidateReportSnapshot {
    const std::shared_ptr<const GuardedCandidateBindingSnapshot> binding;
    const std::shared_ptr<const GuardedJobTask> task;
    const std::shared_ptr<const nptop_verify::LinearRateSnapshot> rates;
    const std::shared_ptr<const nptop_verify::LinearMaterialSnapshot> material;
    const std::vector<GuardedReportCheck> checks;
    const nptop_verify::RateStatus overall_status;
    const bool export_allowed;
    const std::string canonical_json,sha256;
private:
    GuardedCandidateReportSnapshot(std::shared_ptr<const GuardedCandidateBindingSnapshot> context,std::shared_ptr<const GuardedJobTask> token,
        std::shared_ptr<const nptop_verify::LinearRateSnapshot> rate_proof,std::shared_ptr<const nptop_verify::LinearMaterialSnapshot> material_proof,
        std::vector<GuardedReportCheck> results,nptop_verify::RateStatus status,bool allowed,std::string json,std::string hash)
        :binding(std::move(context)),task(std::move(token)),rates(std::move(rate_proof)),material(std::move(material_proof)),
        checks(std::move(results)),overall_status(status),export_allowed(allowed),canonical_json(std::move(json)),sha256(std::move(hash)){}
    friend GuardedCandidateReportResult verify_guarded_candidate_report(const GuardedCandidateBindingResult &,const LinearMaterialOptions &,const LinearCandidateLimits &);
};
struct GuardedCandidateReportResult {
    std::string reason;
    std::shared_ptr<const GuardedCandidateReportSnapshot> snapshot;
    size_t evaluations=0;
};
// Worker owns every input before callbacks and reads no live Print/Model.
// Replays final bytes anew; never consumes caller-supplied PASS/check subsets.
// The fixed full registry retains every unimplemented qualification as NOT_RUN.
GuardedCandidateReportResult verify_guarded_candidate_report(const GuardedCandidateBindingResult &,const LinearMaterialOptions &,
    const LinearCandidateLimits &limits={});
// Serialized host completion; stale/foreign/repeated/late results cannot alter
// another attempt. Only blocked diagnostic reports currently have admission;
// missing mandatory proofs end Unknown/Failed and never produce Verified.
bool accept_guarded_candidate_report(Print &,std::shared_ptr<const GuardedCandidateReportSnapshot>,const GuardedJobLimits &limits={});
}
