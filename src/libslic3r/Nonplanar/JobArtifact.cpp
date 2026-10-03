#include "JobArtifact.hpp"
#include "JobNative.hpp"
#include "NativeJobInputs.hpp"
#include "Canonical.hpp"
#include "StlImport.hpp"
#include "Interval.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>
#include <exception>
#include <set>

namespace Slic3r::nptop {
GuardedCandidateBindingResult bind_guarded_candidate(Print &print,const GuardedJobTask &task,const LinearCandidateResult &requested,
    const GuardedJobLimits &requested_limits,std::shared_ptr<const GuardedNativePlanSnapshot> native)
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
        if(task.snapshot->native_inputs){
            require(bool(native),"NATIVE_JOB_BINDING_REQUIRES_LINEAGE");
            require(task.snapshot->native_inputs->matches_candidate(*candidate,stop),"NATIVE_JOB_CANDIDATE_INPUT_MISMATCH");
        }
        if(native)require(native->candidate==candidate && native->hatches->body->job==task.snapshot && native->hatches->body->attempt==task.attempt &&
            sha256_bytes(native->canonical_json)==native->sha256,"JOB_NATIVE_LINEAGE_BINDING");
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
        if(native){writer.append(",\"native_lineage\":");writer.value(native->sha256);}
        writer.append(native && native->later ? ",\"schema\":4,\"scope\":\"owned_native_body_cap_later_candidate_lineage_only\",\"serializer_policy\":" :
            native && native->departure ? ",\"schema\":3,\"scope\":\"owned_native_body_cap_departure_candidate_lineage_only\",\"serializer_policy\":" :
            native ? ",\"schema\":2,\"scope\":\"owned_native_body_cap_candidate_lineage_only\",\"serializer_policy\":" :
            ",\"schema\":1,\"scope\":\"job_context_candidate_byte_identity_only\",\"serializer_policy\":");writer.value(candidate->policy_fingerprint);
        writer.append(",\"source_fingerprint\":");writer.value(ledger.source_fingerprint);writer.append(",\"source_revision\":");writer.append(std::to_string(ledger.revision));writer.append("}");
        auto json=writer.take();auto hash=sha256_bytes(json);stop();
        auto binding=std::shared_ptr<const GuardedCandidateBindingSnapshot>(new GuardedCandidateBindingSnapshot(task.snapshot,candidate,task.attempt,std::move(json),std::move(hash),std::move(native)));stop();
        const auto next=advance_guarded_job(print,task,GuardedJobPhase::Verifying);
        require(next.task && next.snapshot==binding->job,"STALE_JOB_CANDIDATE_PUBLICATION");
        active=next.task;require(!limits.cancelled || !limits.cancelled(),"JOB_BINDING_CANCELLED");
        const auto status=guarded_job_status(print);
        require(active->is_current() && status.snapshot==active->snapshot && status.attempt==active->attempt && status.phase==GuardedJobPhase::Verifying,"STALE_JOB_CANDIDATE_PUBLICATION");
        require(std::chrono::steady_clock::now()-started<limits.timeout,"JOB_BINDING_DEADLINE");
        result.snapshot=std::move(binding);result.task=next.task;
    }catch(const std::exception &e){result.snapshot.reset();result.task.reset();result.reason=*e.what() ? e.what() : "JOB_BINDING_EXCEPTION_WITHOUT_REASON";stop_guarded_job(print,active ? *active : task,GuardedJobPhase::Unknown);}
    catch(...){result.snapshot.reset();result.task.reset();result.reason="JOB_BINDING_UNKNOWN_EXCEPTION";stop_guarded_job(print,active ? *active : task,GuardedJobPhase::Unknown);}
    return result;
}

const std::vector<std::string> &guarded_mandatory_checks()
{
    static const std::vector<std::string> registry{
        "actual_support","candidate_manifest_identity","complete_route_order","config_compatibility","continuous_geometry",
        "final_filters","final_material","final_rates","firmware_preconditions","independent_replay","material_delivery_qualification",
        "numeric_algorithm_qualification","profile_complete","scene_coverage","software_identity","source_model_plan_binding","target_volume_seam"};
    return registry;
}
namespace {
using Json=nlohmann::json;
const char *report_status(nptop_verify::RateStatus status)
{return status==nptop_verify::RateStatus::Pass ? "PASS" : status==nptop_verify::RateStatus::Fail ? "FAIL" : "UNKNOWN";}
const char *report_execution(GuardedCheckExecution execution)
{return execution==GuardedCheckExecution::Run ? "RUN" : execution==GuardedCheckExecution::NotRun ? "NOT_RUN" : execution==GuardedCheckExecution::Skipped ? "SKIPPED" : "ERROR";}
unsigned hex_digit(char c)
{
    require((c>='0' && c<='9') || (c>='a' && c<='f'),"JOB_MANIFEST_HEX");return c<='9' ? c-'0' : c-'a'+10;
}
std::string manifest_hash(const Json &value)
{
    require(value.is_string(),"JOB_MANIFEST_HASH_TYPE");const auto text=value.get<std::string>();require(text.size()==128,"JOB_MANIFEST_HASH_SIZE");
    std::string decoded;decoded.reserve(64);for(size_t i=0;i<text.size();i+=2)decoded.push_back(char(16*hex_digit(text[i])+hex_digit(text[i+1])));return decoded;
}
void check_candidate_manifest(const GuardedCandidateBindingSnapshot &binding)
{
    const auto &candidate=*binding.candidate;const auto &job=*binding.job;const auto &ledger=*candidate.plan->planned->material->ledger;
    require(sha256_bytes(candidate.bytes)==candidate.sha256 && sha256_bytes(binding.manifest_json)==binding.manifest_sha256 &&
        sha256_bytes(job.canonical_json)==job.fingerprint,"JOB_REPORT_BYTES_OR_MANIFEST_HASH");
    require(candidate.policy.fingerprint()==candidate.policy_fingerprint && candidate.plan->policy.fingerprint()==candidate.plan->policy_fingerprint,"JOB_REPORT_POLICY_HASH");
    const auto manifest=Json::parse(binding.manifest_json);
    std::set<std::string> expected{"attempt","candidate_sha256","candidate_size","initial_position","job_fingerprint","job_id","material_journal",
        "motion_policy","schema","scope","serializer_policy","source_fingerprint","source_revision"};
    if(binding.native)expected.insert("native_lineage");
    std::set<std::string> keys;require(manifest.is_object(),"JOB_REPORT_MANIFEST_OBJECT");for(auto i=manifest.begin();i!=manifest.end();++i)keys.insert(i.key());require(keys==expected,"JOB_REPORT_MANIFEST_REGISTRY");
    const auto integer=[&](const char *key,uint64_t value){require(manifest.at(key).is_number_unsigned() && manifest.at(key).get<uint64_t>()==value,"JOB_REPORT_MANIFEST_INTEGER_BINDING");};
    integer("schema",binding.native && binding.native->later ? 4 : binding.native && binding.native->departure ? guarded_departure_binding_version : binding.native ? 2 : guarded_candidate_binding_version);
    integer("attempt",binding.attempt);integer("job_id",job.job_id);integer("candidate_size",candidate.bytes.size());integer("source_revision",ledger.revision);
    require(manifest.at("scope")== (binding.native && binding.native->later ? "owned_native_body_cap_later_candidate_lineage_only" :
        binding.native && binding.native->departure ? "owned_native_body_cap_departure_candidate_lineage_only" :
        binding.native ? "owned_native_body_cap_candidate_lineage_only" : "job_context_candidate_byte_identity_only"),"JOB_REPORT_MANIFEST_SCOPE");
    if(binding.native){const auto &native=*binding.native;
        require(native.candidate==binding.candidate && native.hatches->body->job==binding.job && native.hatches->body->attempt==binding.attempt &&
            sha256_bytes(native.canonical_json)==native.sha256 && manifest_hash(manifest.at("native_lineage"))==native.sha256,"JOB_REPORT_NATIVE_LINEAGE");}
    for(const auto &[key,value]:std::vector<std::pair<const char*,std::string>>{{"candidate_sha256",candidate.sha256},{"job_fingerprint",job.fingerprint},
        {"motion_policy",candidate.plan->policy_fingerprint},{"serializer_policy",candidate.policy_fingerprint},{"material_journal",ledger.fingerprint()},{"source_fingerprint",ledger.source_fingerprint}})
        require(manifest_hash(manifest.at(key))==value,"JOB_REPORT_MANIFEST_HASH_BINDING");
    const auto &initial=manifest.at("initial_position");require(initial.is_array() && initial.size()==3,"JOB_REPORT_INITIAL_POSITION");
    const std::array<double,3> pose{candidate.initial_position.x(),candidate.initial_position.y(),candidate.initial_position.z()};
    for(size_t axis=0;axis<3;++axis){require(initial[axis].is_string(),"JOB_REPORT_INITIAL_BITS");const auto encoded=initial[axis].get<std::string>();require(encoded.size()==16,"JOB_REPORT_INITIAL_BITS");
        uint64_t decoded=0,bits=0;for(char c:encoded)decoded=(decoded<<4)|hex_digit(c);std::memcpy(&bits,&pose[axis],sizeof bits);require(decoded==bits,"JOB_REPORT_INITIAL_BINDING");}
}
}
GuardedCandidateReportResult verify_guarded_candidate_report(const GuardedCandidateBindingResult &requested,const LinearMaterialOptions &requested_options,
    const LinearCandidateLimits &requested_limits)
{
    GuardedCandidateReportResult result;
    try {
        const auto binding=requested.snapshot;const auto task=requested.task;const auto options=requested_options;const auto limits=requested_limits;
        const auto started=std::chrono::steady_clock::now();
        require(binding && task && task->phase==GuardedJobPhase::Verifying && task->snapshot==binding->job && task->attempt==binding->attempt,"JOB_REPORT_TASK_BINDING");
        const auto &candidate=*binding->candidate;const auto &ledger=*candidate.plan->planned->material->ledger;const auto &scene=candidate.plan->planned->scene;
        require(limits.timeout.count()>0 && limits.timeout.count()<=1000,"JOB_REPORT_TIMEOUT_DOMAIN");
        std::exception_ptr stopped;
        const auto guard=[&]{
            if(stopped)std::rethrow_exception(stopped);
            try {
            require(!limits.cancelled || !limits.cancelled(),"JOB_REPORT_CANCELLED");require(task->is_current(),"STALE_JOB_REPORT_TASK");
            require(!limits.is_current || limits.is_current(ledger.revision),"STALE_JOB_REPORT_SOURCE");
            require(!limits.is_scene_current || limits.is_scene_current(scene.profile_id,scene.revision),"STALE_JOB_REPORT_SCENE");
            if(limits.is_policy_current)require(limits.is_policy_current(candidate.plan->policy.profile_id,candidate.plan->policy.revision) &&
                limits.is_policy_current(candidate.policy.profile_id,candidate.policy.revision) && limits.is_policy_current(options.policy_id,options.revision),"STALE_JOB_REPORT_POLICY");
            require(task->is_current(),"STALE_JOB_REPORT_TASK");require(std::chrono::steady_clock::now()-started<limits.timeout,"JOB_REPORT_DEADLINE");detail::require_interval_environment();
            }catch(...){stopped=std::current_exception();throw;}
        };
        guard();require(candidate.evaluations<=limits.max_evaluations,"JOB_REPORT_WORK_LIMIT");result.evaluations=candidate.evaluations;
        require(!binding->job->native_inputs || binding->job->native_inputs->matches_replay(options),"NATIVE_JOB_REPLAY_INPUT_MISMATCH");guard();
        const auto charge=[&](size_t count){require(result.evaluations<=limits.max_evaluations && count<=limits.max_evaluations-result.evaluations,"JOB_REPORT_WORK_LIMIT");result.evaluations+=count;};
        charge(20);check_candidate_manifest(*binding);guard();
        std::vector<GuardedReportCheck> checks;for(const auto &id:guarded_mandatory_checks())checks.push_back({id,nptop_verify::RateStatus::Unknown,GuardedCheckExecution::NotRun,"Full job proof is not implemented: "+id});
        const auto set=[&](const char *id,nptop_verify::RateStatus status,GuardedCheckExecution execution,const std::string &reason){
            auto found=std::find_if(checks.begin(),checks.end(),[&](const auto &check){return check.id==id;});require(found!=checks.end(),"JOB_REPORT_CHECK_REGISTRY");*found={id,status,execution,reason};
        };
        set("candidate_manifest_identity",nptop_verify::RateStatus::Pass,GuardedCheckExecution::Run,"Actual bytes, initial pose, job, attempt, original policies and declared journal match the manifest.");
        LinearCandidateResult owned;owned.plan=candidate.plan;owned.snapshot=binding->candidate;
        auto replay_limits=limits;replay_limits.cancelled=[&]{guard();return false;};
        replay_limits.max_evaluations=limits.max_evaluations-(result.evaluations-candidate.evaluations);
        const auto rates=verify_linear_candidate_rates(owned,replay_limits);guard();require(rates.evaluations>=candidate.evaluations,"JOB_REPORT_RATE_WORK_ACCOUNTING");charge(rates.evaluations-candidate.evaluations);
        set("final_rates",rates.status,GuardedCheckExecution::Run,rates.reason);
        nptop_verify::LinearMaterialResult material;
        if(rates.snapshot){
            replay_limits.max_evaluations=limits.max_evaluations-(result.evaluations-candidate.evaluations);
            material=verify_linear_candidate_material(owned,options,replay_limits);guard();require(material.evaluations>=candidate.evaluations,"JOB_REPORT_MATERIAL_WORK_ACCOUNTING");charge(material.evaluations-candidate.evaluations);
            set("final_material",material.status,GuardedCheckExecution::Run,material.reason);
            set("independent_replay",material.status,GuardedCheckExecution::Run,material.reason);
        }else{
            set("final_material",nptop_verify::RateStatus::Unknown,GuardedCheckExecution::Skipped,"Material replay skipped after final-rate refusal.");
            set("independent_replay",nptop_verify::RateStatus::Unknown,GuardedCheckExecution::Skipped,"Complete declared journal replay skipped after final-rate refusal.");
        }
        const bool allowed=std::all_of(checks.begin(),checks.end(),[](const auto &check){return check.status==nptop_verify::RateStatus::Pass && check.execution==GuardedCheckExecution::Run;});
        const auto overall=std::any_of(checks.begin(),checks.end(),[](const auto &check){return check.status==nptop_verify::RateStatus::Fail;}) ? nptop_verify::RateStatus::Fail : allowed ? nptop_verify::RateStatus::Pass : nptop_verify::RateStatus::Unknown;
        Json entries=Json::array();for(const auto &check:checks)entries.push_back({{"id",check.id},{"mandatory",true},{"status",report_status(check.status)},{"execution",report_execution(check.execution)},{"reason",check.reason}});
        const Json validation={{"schema_version","0.1.0-draft"},{"document_example",false},{"job_id",std::to_string(binding->job->job_id)},
            {"job_revision",binding->job->input_revision},{"overall_status",report_status(overall)},{"export_decision",allowed ? "ALLOW" : "BLOCK"},
            {"gcode_sha256",candidate.sha256},{"mandatory_check_ids",guarded_mandatory_checks()},{"checks",entries},
            {"assumptions",{"Declared simulation identity-transform full-stop rates/material only.",binding->native && binding->native->departure ?
                "Protected native body/cap/selected-bead/simulation-departure/linear-candidate lineage checked; full cap/contact/order/final-byte geometry and resource qualification pending." : binding->native ?
                "Protected bounded native body/cap/linear-candidate lineage checked; full source/target, measured resources, machine preconditions and geometry/support/volume/route qualification pending." :
                "No qualified source-to-plan, measured resources, machine preconditions or full geometry/support/volume/route proofs."}}};
        charge(1);Json replay={{"records",rates.snapshot ? rates.snapshot->moves.size() : 0},{"evaluations",result.evaluations},
            {"rate_status",report_status(rates.status)},{"rate_reason",rates.reason},{"material_status",report_status(material.status)},{"material_reason",material.reason}};
        const Json document={{"schema",guarded_report_version},{"registry_version",guarded_check_registry_version},{"scope","declared_linear_final_byte_replay_incomplete_job"},
            {"attempt",binding->attempt},{"job_fingerprint",binding->job->fingerprint},{"manifest_sha256",binding->manifest_sha256},{"replay",replay},{"validation",validation}};
        auto json=document.dump();auto hash=sha256_bytes(json);guard();
        result.snapshot=std::shared_ptr<const GuardedCandidateReportSnapshot>(new GuardedCandidateReportSnapshot(binding,task,rates.snapshot,material.snapshot,
            std::move(checks),overall,allowed,std::move(json),std::move(hash)));guard();result.reason="DECLARED_FINAL_REPLAY_REPORT_FULL_JOB_MANDATORY_PROOFS_PENDING";
    }catch(const std::exception &e){result.snapshot.reset();result.reason=*e.what() ? e.what() : "JOB_REPORT_EXCEPTION_WITHOUT_REASON";}
    catch(...){result.snapshot.reset();result.reason="JOB_REPORT_UNKNOWN_EXCEPTION";}return result;
}
bool accept_guarded_candidate_report(Print &print,std::shared_ptr<const GuardedCandidateReportSnapshot> report,const GuardedJobLimits &requested_limits)
{
    try {
        const auto limits=requested_limits;const auto started=std::chrono::steady_clock::now();
        require(report && report->task && !report->export_allowed && report->overall_status!=nptop_verify::RateStatus::Pass,"JOB_REPORT_NOT_A_BLOCKED_DIAGNOSTIC");
        require(limits.timeout.count()>0 && limits.timeout.count()<=1000,"JOB_REPORT_ADMISSION_TIMEOUT");
        require(!limits.cancelled || !limits.cancelled(),"JOB_REPORT_ADMISSION_CANCELLED");
        require(sha256_bytes(report->canonical_json)==report->sha256,"JOB_REPORT_ADMISSION_HASH");
        require(report->task->is_current(),"STALE_JOB_REPORT_ADMISSION");
        const auto terminal=report->overall_status==nptop_verify::RateStatus::Fail ? GuardedJobPhase::Failed : GuardedJobPhase::Unknown;
        require(std::chrono::steady_clock::now()-started<limits.timeout,"JOB_REPORT_ADMISSION_DEADLINE");
        require(stop_guarded_job(print,*report->task,terminal),"STALE_JOB_REPORT_ADMISSION");
        require(std::chrono::steady_clock::now()-started<limits.timeout,"JOB_REPORT_ADMISSION_DEADLINE");return true;
    }catch(const std::exception &){return false;}
}
}
