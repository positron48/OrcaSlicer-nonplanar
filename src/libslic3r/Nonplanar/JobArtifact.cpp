#include "JobArtifact.hpp"
#include "Canonical.hpp"
#include "StlImport.hpp"

namespace Slic3r::nptop {
GuardedCandidateBindingResult bind_guarded_candidate(Print &print,const GuardedJobTask &task,const LinearCandidateResult &requested,
    const GuardedJobLimits &requested_limits)
{
    GuardedCandidateBindingResult result;
    std::shared_ptr<const GuardedJobTask> active;
    try {
        const auto candidate=requested.snapshot;const auto plan=requested.plan;const auto limits=requested_limits;
        const auto started=std::chrono::steady_clock::now();
        require(limits.timeout.count()>0 && limits.timeout.count()<=1000,"INVALID_JOB_BINDING_LIMIT");
        const auto stop=[&]{require(!limits.cancelled || !limits.cancelled(),"JOB_BINDING_CANCELLED");require(task.is_current(),"STALE_JOB_BINDING_TASK");require(std::chrono::steady_clock::now()-started<limits.timeout,"JOB_BINDING_DEADLINE");};stop();
        require(task.phase==GuardedJobPhase::Serializing,"JOB_BINDING_REQUIRES_SERIALIZING");
        require(candidate && candidate->plan==plan,"MISSING_JOB_CANDIDATE");
        require(!candidate->bytes.empty() && candidate->bytes.size()<=32*1024*1024,"JOB_CANDIDATE_BYTE_LIMIT");
        require(sha256_bytes(candidate->bytes)==candidate->sha256 && candidate->policy.fingerprint()==candidate->policy_fingerprint &&
            plan->policy.fingerprint()==plan->policy_fingerprint,"JOB_CANDIDATE_IDENTITY_MISMATCH");stop();
        const auto &ledger=*plan->planned->material->ledger;
        const auto journal=ledger.fingerprint();stop();
        detail::CanonicalConfigWriter writer;
        writer.append("{\"attempt\":");writer.append(std::to_string(task.attempt));writer.append(",\"candidate_sha256\":");writer.value(candidate->sha256);
        writer.append(",\"candidate_size\":");writer.append(std::to_string(candidate->bytes.size()));
        writer.append(",\"initial_position\":");writer.value(Vec3d(candidate->initial_position.x(),candidate->initial_position.y(),candidate->initial_position.z()));
        writer.append(",\"job_fingerprint\":");writer.value(task.snapshot->fingerprint);writer.append(",\"job_id\":");writer.append(std::to_string(task.snapshot->job_id));
        writer.append(",\"material_journal\":");writer.value(journal);writer.append(",\"motion_policy\":");writer.value(plan->policy_fingerprint);
        writer.append(",\"schema\":1,\"scope\":\"job_context_candidate_byte_identity_only\",\"serializer_policy\":");writer.value(candidate->policy_fingerprint);
        writer.append(",\"source_fingerprint\":");writer.value(ledger.source_fingerprint);writer.append(",\"source_revision\":");writer.append(std::to_string(ledger.revision));writer.append("}");
        auto json=writer.take();auto hash=sha256_bytes(json);stop();
        auto binding=std::shared_ptr<const GuardedCandidateBindingSnapshot>(new GuardedCandidateBindingSnapshot(task.snapshot,candidate,task.attempt,std::move(json),std::move(hash)));stop();
        const auto next=advance_guarded_job(print,task,GuardedJobPhase::Verifying);
        require(next.task && next.snapshot==binding->job,"STALE_JOB_CANDIDATE_PUBLICATION");
        active=next.task;require(!limits.cancelled || !limits.cancelled(),"JOB_BINDING_CANCELLED");
        const auto status=guarded_job_status(print);
        require(active->is_current() && status.snapshot==active->snapshot && status.attempt==active->attempt && status.phase==GuardedJobPhase::Verifying,"STALE_JOB_CANDIDATE_PUBLICATION");
        require(std::chrono::steady_clock::now()-started<limits.timeout,"JOB_BINDING_DEADLINE");
        result.snapshot=std::move(binding);result.task=next.task;
    }catch(const std::exception &e){result.snapshot.reset();result.task.reset();result.reason=e.what();stop_guarded_job(print,active ? *active : task,GuardedJobPhase::Unknown);}
    return result;
}
}
