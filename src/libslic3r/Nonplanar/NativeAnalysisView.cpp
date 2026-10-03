#include "NativeAnalysisView.hpp"
#include "Canonical.hpp"
#include "../Print.hpp"
#include <limits>

namespace Slic3r::nptop {
std::shared_ptr<const NativeAnalysisViewInput> capture_native_analysis_view_input(
    const Model &model, const DynamicPrintConfig &config, int plate, const Vec3d &origin,
    const std::string &mode, const std::string &bytes)
{
    require(mode == "safe_hybrid" || mode == "strict_nonplanar", "VIEW_ANALYSIS_MODE_REFUSED");
    require(plate >= 0 && model.curr_plate_index == plate && origin.allFinite(), "VIEW_PLATE_REFUSED");
    const auto request = parse_native_analysis_document(bytes);
    DynamicPrintConfig analysis_config = config;
    analysis_config.set_key_value("nptop_mode", new ConfigOptionString(mode));
    const auto source = capture_native_input(model, analysis_config);
    require(bool(source), "VIEW_SOURCE_REFUSED");
    detail::CanonicalConfigWriter identity;
    identity.append("[1,"); identity.value(source->fingerprint); identity.append(",");
    const auto *original_mode = config.option("nptop_mode");
    identity.value(original_mode ? original_mode->serialize() : std::string()); identity.append(",");
    identity.value(origin); identity.append(","); identity.value(sha256_bytes(bytes)); identity.append("]");
    return std::shared_ptr<const NativeAnalysisViewInput>(new NativeAnalysisViewInput(
        request, source, origin, mode, bytes, sha256_bytes(identity.take())));
}
NativeAnalysisWorkerResult finish_native_analysis_view(Print &print, const GuardedJobTask &task,
    const NativeAnalysisViewInput &input, const NativeAnalysisViewInput *current,
    uint64_t revision, uint64_t current_revision, bool cancelled, NativeAnalysisWorkerResult result)
{
    const auto refuse = [&](const char *reason) { result.diagnostic.clear(); result.reason = reason; };
    if (cancelled) {
        stop_guarded_job(print, task, GuardedJobPhase::Cancelled);
        refuse("VIEW_CANCELLED"); return result;
    }
    const bool fresh = revision != 0 && revision == current_revision && current &&
        input.identity_sha256 == current->identity_sha256 && task.is_current() &&
        task.phase == GuardedJobPhase::Analyzing && task.snapshot->input->fingerprint == input.source_sha256;
    if (!fresh) {
        stop_guarded_job(print, task, GuardedJobPhase::Unknown);
        refuse("VIEW_STALE_INPUTS"); return result;
    }
    if (!stop_guarded_job(print, task, GuardedJobPhase::Unknown)) {
        refuse("VIEW_STALE_OWNER"); return result;
    }
    if (result.reason != "WORKER_BLOCKED_DIAGNOSTIC_COMPLETE" && result.reason != "WORKER_ANALYSIS_REFUSED")
        result.diagnostic.clear();
    return result;
}
void NativeAnalysisViewOwner::invalidate()
{
    if(current)current->validity.store(false,std::memory_order_release);
    current.reset();
}
std::shared_ptr<const NativeAnalysisViewTask> begin_native_analysis_view(
    NativeAnalysisViewOwner &owner,std::shared_ptr<const NativeAnalysisViewInput> input,uint64_t revision)
{
    owner.invalidate();
    require(owner.attempt!=std::numeric_limits<uint64_t>::max(),"VIEW_ATTEMPT_EXHAUSTED");++owner.attempt;
    require(input && input->source && revision,"VIEW_CURRENT_SOURCE_REQUIRED");
    owner.current=std::shared_ptr<const NativeAnalysisViewTask>(new NativeAnalysisViewTask(std::move(input),revision,owner.attempt));
    return owner.current;
}
NativeAnalysisWorkerResult finish_native_analysis_view(NativeAnalysisViewOwner &owner,const NativeAnalysisViewTask &task,
    const NativeAnalysisViewInput *current,uint64_t revision,bool cancelled,NativeAnalysisWorkerResult result)
{
    const bool owned=owner.current.get()==&task;
    const bool fresh=owned && task.is_current() && revision==task.revision && revision && current &&
        current->identity_sha256==task.input->identity_sha256;
    if(owned)owner.invalidate();
    if(cancelled || !fresh){result.diagnostic.clear();result.reason=cancelled ? "VIEW_CANCELLED" : owned ? "VIEW_STALE_INPUTS" : "VIEW_STALE_OWNER";}
    else if(result.reason!="WORKER_BLOCKED_DIAGNOSTIC_COMPLETE" && result.reason!="WORKER_ANALYSIS_REFUSED")result.diagnostic.clear();
    return result;
}
}
