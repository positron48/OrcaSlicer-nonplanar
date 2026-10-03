#pragma once
#include "NativeAnalysisWorker.hpp"
#include "../PrintConfig.hpp"

namespace Slic3r::nptop {
struct NativeAnalysisViewInput {
    const std::shared_ptr<const NativeAnalysisRequestSnapshot> request;
    const std::shared_ptr<const NativeInputSnapshot> source;
    const Vec3d plate_origin;
    const std::string mode, editing_bytes, source_sha256, identity_sha256;
private:
    NativeAnalysisViewInput(std::shared_ptr<const NativeAnalysisRequestSnapshot> r,
        std::shared_ptr<const NativeInputSnapshot> s, Vec3d origin, std::string m,
        std::string bytes, std::string identity)
        : request(std::move(r)), source(std::move(s)), plate_origin(origin), mode(std::move(m)), editing_bytes(std::move(bytes)),
          source_sha256(source->fingerprint), identity_sha256(std::move(identity)) {}
    friend std::shared_ptr<const NativeAnalysisViewInput> capture_native_analysis_view_input(
        const Model &, const DynamicPrintConfig &, int, const Vec3d &, const std::string &, const std::string &);
};
// Serialized GUI capture. The explicit analysis mode is applied to a copy;
// the original mode, all original settings, placement and exact editing bytes
// remain identity inputs. This is a display session, not a saved project format.
std::shared_ptr<const NativeAnalysisViewInput> capture_native_analysis_view_input(
    const Model &, const DynamicPrintConfig &, int plate, const Vec3d &origin,
    const std::string &mode, const std::string &editing_bytes);
// Main-thread completion only. Both the recaptured live GUI identity and the
// private native owner must still match. Stops the host attempt as Unknown or
// Cancelled; a diagnostic can never advance it or authorize export.
NativeAnalysisWorkerResult finish_native_analysis_view(Print &, const GuardedJobTask &,
    const NativeAnalysisViewInput &, const NativeAnalysisViewInput *current,
    uint64_t revision, uint64_t current_revision, bool cancelled, NativeAnalysisWorkerResult);

struct NativeAnalysisViewOwner;
// Private source-only display attempt. It can only be revoked; it is never a
// GuardedJobTask or an export credential. Source is captured before Print.apply.
struct NativeAnalysisViewTask {
    const std::shared_ptr<const NativeAnalysisViewInput> input;
    const uint64_t revision,attempt;
    bool is_current() const {return validity.load(std::memory_order_acquire);}
private:
    mutable std::atomic<bool> validity{true};
    NativeAnalysisViewTask(std::shared_ptr<const NativeAnalysisViewInput> value,uint64_t r,uint64_t a)
        :input(std::move(value)),revision(r),attempt(a){}
    friend struct NativeAnalysisViewOwner;
    friend std::shared_ptr<const NativeAnalysisViewTask> begin_native_analysis_view(
        NativeAnalysisViewOwner &,std::shared_ptr<const NativeAnalysisViewInput>,uint64_t);
};
// Begin/invalidate/finish are serialized by the GUI host. Only the atomic flag
// crosses into the worker/monitor; no live model, owner or widgets escape.
struct NativeAnalysisViewOwner {
    ~NativeAnalysisViewOwner(){invalidate();}
    NativeAnalysisViewOwner()=default;
    NativeAnalysisViewOwner(const NativeAnalysisViewOwner &)=delete;
    NativeAnalysisViewOwner &operator=(const NativeAnalysisViewOwner &)=delete;
    void invalidate();
private:
    uint64_t attempt=0;
    std::shared_ptr<const NativeAnalysisViewTask> current;
    friend std::shared_ptr<const NativeAnalysisViewTask> begin_native_analysis_view(
        NativeAnalysisViewOwner &,std::shared_ptr<const NativeAnalysisViewInput>,uint64_t);
    friend NativeAnalysisWorkerResult finish_native_analysis_view(NativeAnalysisViewOwner &,
        const NativeAnalysisViewTask &,const NativeAnalysisViewInput *,uint64_t,bool,NativeAnalysisWorkerResult);
};
std::shared_ptr<const NativeAnalysisViewTask> begin_native_analysis_view(
    NativeAnalysisViewOwner &,std::shared_ptr<const NativeAnalysisViewInput>,uint64_t revision);
NativeAnalysisWorkerResult finish_native_analysis_view(NativeAnalysisViewOwner &,const NativeAnalysisViewTask &,
    const NativeAnalysisViewInput *current,uint64_t current_revision,bool cancelled,NativeAnalysisWorkerResult);
}
