#pragma once
#include "NativeJobInputs.hpp"
#include "JobNative.hpp"
#include "JobArtifact.hpp"

namespace Slic3r::nptop {
enum class NativeAnalysisStage { Capture,Body,Hatches,Cap,Material,Motion,Serialize,Lineage,Replay,Admission };
struct NativeAnalysisRequest {
    NativeJobInputsRequest inputs;
    bool millimeters_declared;
    TriangleMesh reservation;
    NativeAffinePassRequest passes;
    AffineHatchPolicy hatches;
    FirstContourPolicy contour;
    SceneBox fill_region;
};
struct NativeAnalysisRequestSnapshot {
    const NativeAnalysisRequest values;
    const std::string canonical_json,sha256;
private:
    NativeAnalysisRequestSnapshot(NativeAnalysisRequest request,std::string json,std::string hash)
        :values(std::move(request)),canonical_json(std::move(json)),sha256(std::move(hash)){}
    friend std::shared_ptr<const NativeAnalysisRequestSnapshot> capture_native_analysis_request(const NativeAnalysisRequest &);
};
// Bounded immutable editing input, independent of a live job. No callback,
// qualification or Print access. run_native_analysis begins a new owned attempt.
std::shared_ptr<const NativeAnalysisRequestSnapshot> capture_native_analysis_request(const NativeAnalysisRequest &);
struct NativeAnalysisLimits : GuardedNativeLimits {
    GuardedNativeBodyLimits body;
    GuardedNativeHatchLimits hatches;
    FirstHatchLayerLimits cap;
    FirstCapMaterialLimits material;
    LinearMotionPlanLimits motion;
    LinearCandidateLimits candidate;
    std::function<void(NativeAnalysisStage)> progress;
};
struct NativeAnalysisResult;
struct NativeAnalysisSnapshot {
    const std::shared_ptr<const NativeAnalysisRequestSnapshot> request;
    const std::shared_ptr<const GuardedNativePlanSnapshot> plan;
    const std::shared_ptr<const GuardedCandidateReportSnapshot> report;
    const size_t replay_evaluations;
private:
    NativeAnalysisSnapshot(std::shared_ptr<const NativeAnalysisRequestSnapshot> input,std::shared_ptr<const GuardedNativePlanSnapshot> path,
        std::shared_ptr<const GuardedCandidateReportSnapshot> validation,size_t work)
        :request(std::move(input)),plan(std::move(path)),report(std::move(validation)),replay_evaluations(work){}
    friend NativeAnalysisResult run_native_analysis(Print &,uint64_t,std::shared_ptr<const NativeAnalysisRequestSnapshot>,
        const std::vector<JobResource> &,const NativeAnalysisLimits &);
};
struct NativeAnalysisResult {
    NativeAnalysisStage stage=NativeAnalysisStage::Capture;
    std::string reason;
    std::shared_ptr<const GuardedJobSnapshot> job;
    std::shared_ptr<const NativeAnalysisSnapshot> snapshot;
};
// Common serialized host/batch controller; actual geometry workers remain
// isolated from live Print state. Every stage consumes the same owned request,
// original source bytes and current job token. One cooperative root covers all
// nested stages; interactive callers must additionally isolate native/CGAL work.
// Returns blocked diagnostics only. Internal candidate bytes are never exported.
NativeAnalysisResult run_native_analysis(Print &,uint64_t job_id,std::shared_ptr<const NativeAnalysisRequestSnapshot>,
    const std::vector<JobResource> &source_files,const NativeAnalysisLimits &limits={});
}
