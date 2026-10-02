#pragma once
#include "InputSnapshot.hpp"
#include <atomic>
#include <chrono>
#include <functional>

namespace Slic3r::nptop {
inline constexpr unsigned guarded_job_version=1;
enum class JobResourceKind { SourceFile, Toolhead, Scene, Material, Numeric, Firmware, Algorithms, Software };
struct JobResource {JobResourceKind kind;std::string name,bytes;};
struct JobResourceSnapshot {const JobResourceKind kind;const std::string name,bytes,sha256;};
struct GuardedJobResult;
// Publishable identity views omit only explicit transport/timestamp/log options.
// Original snapshots remain exact in memory; do not persist their raw settings.
struct JobIdentityView {const std::string canonical_json,fingerprint;};
struct GuardedJobSnapshot {
    const uint64_t job_id,input_revision;
    const std::shared_ptr<const NativeInputSnapshot> input,executed_input;
    const std::shared_ptr<const PrintConfigSnapshot> settings;
    const JobIdentityView input_identity,executed_identity,settings_identity;
    const std::vector<JobResourceSnapshot> resources;
    const std::string canonical_json,fingerprint;
private:
    GuardedJobSnapshot(uint64_t id,uint64_t revision,std::shared_ptr<const NativeInputSnapshot> source,
        std::shared_ptr<const NativeInputSnapshot> executed,std::shared_ptr<const PrintConfigSnapshot> config,
        JobIdentityView source_view,JobIdentityView executed_view,JobIdentityView settings_view,
        std::vector<JobResourceSnapshot> dependencies,std::string json,std::string hash)
        :job_id(id),input_revision(revision),input(std::move(source)),executed_input(std::move(executed)),settings(std::move(config)),
        input_identity(std::move(source_view)),executed_identity(std::move(executed_view)),settings_identity(std::move(settings_view)),
        resources(std::move(dependencies)),canonical_json(std::move(json)),fingerprint(std::move(hash)){}
    friend GuardedJobResult begin_guarded_job(Print &,uint64_t,const std::vector<JobResource> &,const struct GuardedJobLimits &);
};
// Same exact version-1 omission registry as the publishable job identity.
// Used only by isolated derived workers; original host snapshots remain exact.
ResolvedConfigSnapshot guarded_slicing_config(const ResolvedConfigSnapshot &);
std::shared_ptr<const NativeInputSnapshot> guarded_slicing_input(const GuardedJobSnapshot &);
// Working phases only. No phase or resource identity is a Verified certificate.
enum class GuardedJobPhase {Editing,Analyzing,Planning,Serializing,Verifying,Failed,Unknown,Cancelled,Stale};
struct GuardedJobTask {
    const std::shared_ptr<const GuardedJobSnapshot> snapshot;
    const uint64_t attempt;
    const GuardedJobPhase phase;
    bool is_current() const {return validity->load(std::memory_order_acquire);}
private:
    const std::shared_ptr<std::atomic<bool>> validity;
    GuardedJobTask(std::shared_ptr<const GuardedJobSnapshot> job,uint64_t generation,GuardedJobPhase step,std::shared_ptr<std::atomic<bool>> flag)
        :snapshot(std::move(job)),attempt(generation),phase(step),validity(std::move(flag)){}
    friend struct GuardedJobOwner;
};
struct GuardedJobLimits {
    std::chrono::milliseconds timeout{1000};
    std::function<bool()> cancelled;
};
struct GuardedJobResult {std::string reason;std::shared_ptr<const GuardedJobSnapshot> snapshot;std::shared_ptr<const GuardedJobTask> task;};
struct GuardedJobStatus {GuardedJobPhase phase=GuardedJobPhase::Editing;uint64_t attempt=0;std::shared_ptr<const GuardedJobSnapshot> snapshot;};
// Caller serializes native Print/Model operations for the whole synchronous
// capture, as for capture_print_config. Resource bytes are copied before any
// callback. Opaque resources are identity inputs, never qualification claims.
// Every begin cancels the previous attempt before resource work; OFF bypasses.
GuardedJobResult begin_guarded_job(Print &,uint64_t job_id,const std::vector<JobResource> &,const GuardedJobLimits &limits={});
// Only the exact current owner/attempt/source/settings/phase can advance.
// Old callbacks, repeated callbacks and mode/plate/input edits cannot resurrect
// an attempt. Resource changes must start a new job, including failed captures.
GuardedJobResult advance_guarded_job(Print &,const GuardedJobTask &,GuardedJobPhase);
bool stop_guarded_job(Print &,const GuardedJobTask &,GuardedJobPhase);
GuardedJobStatus guarded_job_status(Print &);
struct GuardedJobOwner {
    ~GuardedJobOwner();
    void invalidate();
private:
    // Access is serialized by the owning Print's state mutex. No worker mutates
    // native Print state; these short owner operations only bind owned results.
    uint64_t attempt=0;
    GuardedJobPhase phase=GuardedJobPhase::Editing;
    std::shared_ptr<const GuardedJobSnapshot> snapshot;
    bool exhausted=false;
    std::shared_ptr<std::atomic<bool>> validity;
    std::shared_ptr<const GuardedJobTask> task(std::shared_ptr<const GuardedJobSnapshot>,GuardedJobPhase) const;
    void publish(const GuardedJobTask &);
    bool accepts(const GuardedJobTask &) const;
    friend GuardedJobResult begin_guarded_job(Print &,uint64_t,const std::vector<JobResource> &,const GuardedJobLimits &);
    friend GuardedJobResult advance_guarded_job(Print &,const GuardedJobTask &,GuardedJobPhase);
    friend bool stop_guarded_job(Print &,const GuardedJobTask &,GuardedJobPhase);
    friend GuardedJobStatus guarded_job_status(Print &);
};
}
