#pragma once
#include "NativeAnalysisJson.hpp"

namespace Slic3r::nptop {
inline constexpr unsigned native_analysis_worker_protocol=1;
inline constexpr size_t native_analysis_worker_byte_limit=32*1024*1024;
struct NativeAnalysisWorkerInput {
    const std::shared_ptr<const GuardedJobTask> task;
    const std::string bytes,sha256,input_fingerprint,request_sha256;
private:
    NativeAnalysisWorkerInput(std::shared_ptr<const GuardedJobTask> owner,std::string payload,std::string source,std::string request)
        :task(std::move(owner)),bytes(std::move(payload)),sha256(sha256_bytes(bytes)),input_fingerprint(std::move(source)),request_sha256(std::move(request)){}
    friend std::shared_ptr<const NativeAnalysisWorkerInput> capture_native_analysis_worker_input(
        std::shared_ptr<const GuardedJobTask>,std::shared_ptr<const NativeAnalysisRequestSnapshot>);
};
// Called under the host's existing Model/Print serialization. Consumes only
// captured source and a request already bound into the current host job.
std::shared_ptr<const NativeAnalysisWorkerInput> capture_native_analysis_worker_input(
    std::shared_ptr<const GuardedJobTask>,std::shared_ptr<const NativeAnalysisRequestSnapshot>);
struct NativeAnalysisWorkerOptions {
    std::chrono::milliseconds timeout{30000};
    uint64_t max_peak_rss_bytes=512*1024*1024;
    std::function<bool()> cancelled;
    std::function<void(NativeAnalysisStage)> progress;
};
struct NativeAnalysisWorkerResult {
    std::string reason,diagnostic;
    uint64_t peak_rss_bytes=0;
    size_t progress_stages=0;
};
// Blocking background-job API. Trusted bundled executable, direct argv, private
// workspace, no shell or profile-selected executable. Child output is display
// data only: never a native proof snapshot or an export credential. Cancellation,
// stale host token, timeout, memory/protocol failures expose no partial report.
// RSS is supervised/observed, not instantaneous hard macOS RSS containment.
NativeAnalysisWorkerResult run_native_analysis_worker(const std::string &executable,
    std::shared_ptr<const NativeAnalysisWorkerInput>,const NativeAnalysisWorkerOptions &options={});
// Internal child entry; called only by the bundled executable after OS limits
// are installed. Does not read source paths, user presets or host Print state.
std::string execute_native_analysis_worker(const std::string &bytes,const std::function<void(NativeAnalysisStage)> &progress);
}
