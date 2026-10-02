#include "Job.hpp"
#include "Canonical.hpp"
#include "Contracts.hpp"
#include "StlImport.hpp"
#include "../Print.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
#include <nlohmann/json.hpp>

namespace Slic3r::nptop {
namespace {
const std::set<std::string> &non_slicing_options()
{
    static const std::set<std::string> keys{"printer_agent","print_host","print_host_webui","printhost_apikey","flashforge_serial_number",
        "printhost_port","printhost_cafile","printhost_user","printhost_password","printhost_ssl_ignore_revoke","printhost_authorization_type","timestamp","logfile"};
    return keys;
}
void omit_non_slicing_options(nlohmann::json &value)
{
    if(value.is_object() && value.size()==2 && value.contains("options") && value.value("schema",0)==1){
        // Fixed version-1 registry, never a substring/wildcard filter. These
        // options drive upload/authentication or logging, not local slicing. URLs may
        // embed credentials. Unknown geometry/override fields remain exact.
        static const auto keys=[] {
            std::set<std::string> encoded;
            for(const auto &key:non_slicing_options()){
                detail::CanonicalConfigWriter writer;writer.value(key);encoded.insert(nlohmann::json::parse(writer.take()).get<std::string>());
            }return encoded;
        }();
        auto &options=value.at("options");require(options.is_array(),"JOB_IDENTITY_CONFIG_SHAPE");
        options.erase(std::remove_if(options.begin(),options.end(),[&](const nlohmann::json &option){
            require(option.is_array() && option.size()==4 && option[0].is_string(),"JOB_IDENTITY_OPTION_SHAPE");
            return keys.count(option[0].get<std::string>())!=0;
        }),options.end());return;
    }
    if(value.is_structured())for(auto &child:value)omit_non_slicing_options(child);
}
JobIdentityView identity_view(const std::string &canonical,const std::string &source_identity={})
{
    auto value=nlohmann::json::parse(canonical);omit_non_slicing_options(value);
    if(!source_identity.empty()){
        require(value.contains("input_fingerprint"),"JOB_IDENTITY_SOURCE_BINDING");
        detail::CanonicalConfigWriter writer;writer.value(source_identity);value["input_fingerprint"]=nlohmann::json::parse(writer.take());
    }
    auto json=value.dump();auto hash=sha256_bytes(json);return {std::move(json),std::move(hash)};
}
bool working(GuardedJobPhase p)
{return p==GuardedJobPhase::Analyzing || p==GuardedJobPhase::Planning || p==GuardedJobPhase::Serializing || p==GuardedJobPhase::Verifying;}
bool native_current(const Print &print,const GuardedJobSnapshot &job)
{
    // Caller holds the owning Print's state mutex and serializes native edits.
    const auto settings=capture_print_config(print);
    const auto executed=capture_native_input(print.model(),print.full_print_config());
    return settings && settings->input_revision==job.input_revision && settings->input_fingerprint==job.input->fingerprint &&
        settings->fingerprint()==job.settings->fingerprint() && executed && executed->fingerprint==job.executed_input->fingerprint;
}
}
ResolvedConfigSnapshot guarded_slicing_config(const ResolvedConfigSnapshot &source)
{
    DynamicPrintConfig config;
    for(const auto &key:source.keys())if(!non_slicing_options().count(key))config.set_key_value(key,source.option(key)->clone());
    return ResolvedConfigSnapshot(config);
}
std::shared_ptr<const NativeInputSnapshot> guarded_slicing_input(const GuardedJobSnapshot &job)
{
    const auto &source=*job.input;auto objects=source.objects;auto materials=source.materials;
    for(auto &object:objects){
        object.config=guarded_slicing_config(object.config);
        for(auto &volume:object.volumes)volume.config=guarded_slicing_config(volume.config);
        for(auto &range:object.layer_ranges)range.config=guarded_slicing_config(range.config);
    }
    for(auto &material:materials)material.config=guarded_slicing_config(material.config);
    return std::make_shared<const NativeInputSnapshot>(NativeInputSnapshot{guarded_slicing_config(source.config),std::move(materials),std::move(objects),
        source.model_plate_index,job.input_identity.canonical_json,job.input_identity.fingerprint});
}
void GuardedJobOwner::invalidate()
{
    if(validity)validity->store(false,std::memory_order_release);
    snapshot.reset();phase=GuardedJobPhase::Stale;
    if(attempt==std::numeric_limits<uint64_t>::max())exhausted=true;else ++attempt;
}
GuardedJobOwner::~GuardedJobOwner()
{if(validity)validity->store(false,std::memory_order_release);}
bool GuardedJobOwner::accepts(const GuardedJobTask &task) const
{return !exhausted && working(phase) && task.is_current() && task.validity==validity && task.attempt==attempt && task.phase==phase && task.snapshot==snapshot && snapshot;}
std::shared_ptr<const GuardedJobTask> GuardedJobOwner::task(std::shared_ptr<const GuardedJobSnapshot> job,GuardedJobPhase step) const
{return std::shared_ptr<const GuardedJobTask>(new GuardedJobTask(std::move(job),attempt,step,std::make_shared<std::atomic<bool>>(false)));}
void GuardedJobOwner::publish(const GuardedJobTask &task)
{
    if(validity)validity->store(false,std::memory_order_release);
    validity=task.validity;validity->store(true,std::memory_order_release);
}
GuardedJobResult begin_guarded_job(Print &print,uint64_t job_id,const std::vector<JobResource> &requested,const GuardedJobLimits &requested_limits)
{
    GuardedJobResult result;GuardedJobLimits limits;const auto started=std::chrono::steady_clock::now();
    std::shared_ptr<GuardedJobOwner> owner;std::shared_ptr<const NativeInputSnapshot> input,executed;std::shared_ptr<const PrintConfigSnapshot> settings;uint64_t attempt=0;
    try {
        {
            std::scoped_lock<std::mutex> lock(print.state_mutex());
            owner=print.m_nonplanar_job;
            if(owner){owner->invalidate();attempt=owner->attempt;}
            settings=capture_print_config(print);
            if(!settings)return result;
            if(!owner){owner=std::make_shared<GuardedJobOwner>();print.m_nonplanar_job=owner;owner->invalidate();attempt=owner->attempt;}
            owner->phase=GuardedJobPhase::Editing;
            require(!owner->exhausted,"JOB_ATTEMPT_EXHAUSTED");input=print.m_nonplanar_input;
            require(input && job_id && settings->input_revision && settings->input_fingerprint==input->fingerprint,"MISSING_JOB_INPUT_BINDING");
            require(settings->plate_index>=0 && settings->plate_index==input->model_plate_index && settings->plate_origin_mm.allFinite() && settings->plate_origin_mm.cwiseAbs().maxCoeff()<=10000,"JOB_PLATE_BINDING");
            require(settings->input_conflict.empty() && !settings->model_conflict,"JOB_INPUT_POLICY_CONFLICT");
            require(resolve_policy(settings->resolved_print_config,settings->object_count,settings->instance_count).passes_config_preflight(),"JOB_CONFIG_POLICY_CONFLICT");
            for(const auto &region:settings->regions)require(region.policy.passes_config_preflight(),"JOB_REGION_POLICY_CONFLICT");
            executed=capture_native_input(print.model(),print.full_print_config());require(bool(executed),"MISSING_JOB_EXECUTED_INPUT");
        }
        limits=requested_limits;
        require(limits.timeout.count()>0 && limits.timeout.count()<=1000,"INVALID_JOB_CAPTURE_LIMIT");
        require(requested.size()<=256,"JOB_RESOURCE_COUNT_LIMIT");size_t total=0;
        for(const auto &r:requested){require(!r.name.empty() && r.name.size()<=4096,"JOB_RESOURCE_NAME_LIMIT");require(!r.bytes.empty() && r.bytes.size()<=32*1024*1024-total,"JOB_RESOURCE_BYTE_LIMIT");total+=r.bytes.size();}
        auto resources=requested; // Own all caller bytes before cancellation/freshness callbacks.
        const auto stop=[&]{require(!limits.cancelled || !limits.cancelled(),"JOB_CANCELLED");require(std::chrono::steady_clock::now()-started<limits.timeout,"JOB_CAPTURE_DEADLINE");};stop();
        auto input_view=identity_view(input->canonical_json);stop();
        auto executed_view=identity_view(executed->canonical_json);stop();
        auto settings_view=identity_view(settings->canonical_json(),input_view.fingerprint);stop();
        std::sort(resources.begin(),resources.end(),[](const JobResource &a,const JobResource &b){return std::tie(a.kind,a.name)<std::tie(b.kind,b.name);});
        std::set<std::pair<JobResourceKind,std::string>> names;std::set<JobResourceKind> singletons;std::set<std::string> sources;std::vector<JobResourceSnapshot> owned;owned.reserve(resources.size());
        detail::CanonicalConfigWriter writer;
        writer.append("{\"executed_input\":");writer.value(executed_view.fingerprint);writer.append(",\"job_id\":");writer.append(std::to_string(job_id));writer.append(",\"native_input\":");writer.value(input_view.fingerprint);
        writer.append(",\"native_revision\":");writer.append(std::to_string(settings->input_revision));writer.append(",\"print_settings\":");writer.value(settings_view.fingerprint);writer.append(",\"resources\":[");
        bool first=true;
        for(auto &r:resources){stop();require(int(r.kind)>=0 && int(r.kind)<=int(JobResourceKind::Software),"UNKNOWN_JOB_RESOURCE_KIND");require(names.emplace(r.kind,r.name).second,"DUPLICATE_JOB_RESOURCE");
            if(r.kind==JobResourceKind::SourceFile)sources.insert(r.name);else require(singletons.insert(r.kind).second,"DUPLICATE_JOB_RESOURCE_KIND");
            auto hash=sha256_bytes(r.bytes);if(!first)writer.append(",");first=false;writer.append("[");writer.value(int(r.kind));writer.append(",");writer.value(r.name);writer.append(",");writer.value(hash);writer.append(",");writer.append(std::to_string(r.bytes.size()));writer.append("]");
            owned.push_back({r.kind,std::move(r.name),std::move(r.bytes),std::move(hash)});
        }
        require(singletons.size()==7,"MISSING_JOB_RESOURCE_KIND");
        for(const auto &object:input->objects)for(const auto &volume:object.volumes)if(!volume.source_file.empty())require(sources.count(volume.source_file),"MISSING_JOB_SOURCE_FILE_BYTES");
        writer.append("],\"schema\":1}");auto json=writer.take();auto hash=sha256_bytes(json);stop();
        auto snapshot=std::shared_ptr<const GuardedJobSnapshot>(new GuardedJobSnapshot(job_id,settings->input_revision,input,executed,settings,
            std::move(input_view),std::move(executed_view),std::move(settings_view),std::move(owned),std::move(json),std::move(hash)));stop();
        {
            std::scoped_lock<std::mutex> lock(print.state_mutex());
            require(print.m_nonplanar_job==owner && owner->attempt==attempt && !owner->exhausted && native_current(print,*snapshot),"STALE_JOB_CAPTURE");
            auto token=owner->task(snapshot,GuardedJobPhase::Analyzing);
            require(std::chrono::steady_clock::now()-started<limits.timeout,"JOB_CAPTURE_DEADLINE");
            owner->snapshot=snapshot;owner->phase=GuardedJobPhase::Analyzing;owner->publish(*token);result.snapshot=std::move(snapshot);result.task=std::move(token);
        }
    }catch(const std::exception &e){result.snapshot.reset();result.task.reset();result.reason=e.what();if(owner){std::scoped_lock<std::mutex> lock(print.state_mutex());if(owner->attempt==attempt){owner->snapshot.reset();owner->phase=GuardedJobPhase::Unknown;}}}
    return result;
}
GuardedJobResult advance_guarded_job(Print &print,const GuardedJobTask &task,GuardedJobPhase next)
{
    GuardedJobResult result;std::scoped_lock<std::mutex> lock(print.state_mutex());const auto owner=print.m_nonplanar_job;
    try {
        require(owner && owner->accepts(task),"STALE_JOB_TASK");
        bool fresh=false;
        try{fresh=native_current(print,*task.snapshot);}catch(const std::exception &){owner->invalidate();throw;}
        if(!fresh){owner->invalidate();throw std::invalid_argument("STALE_JOB_NATIVE_INPUTS");}
        require((task.phase==GuardedJobPhase::Analyzing && next==GuardedJobPhase::Planning) ||
            (task.phase==GuardedJobPhase::Planning && next==GuardedJobPhase::Serializing) ||
            (task.phase==GuardedJobPhase::Serializing && next==GuardedJobPhase::Verifying),"INVALID_JOB_PHASE_TRANSITION");
        auto token=owner->task(owner->snapshot,next);owner->phase=next;owner->publish(*token);result.snapshot=owner->snapshot;result.task=std::move(token);
    }catch(const std::exception &e){result.snapshot.reset();result.task.reset();result.reason=e.what();}return result;
}
bool stop_guarded_job(Print &print,const GuardedJobTask &task,GuardedJobPhase terminal)
{
    std::scoped_lock<std::mutex> lock(print.state_mutex());const auto owner=print.m_nonplanar_job;
    if(!owner || !owner->accepts(task))return false;
    try{if(!native_current(print,*task.snapshot)){owner->invalidate();return false;}}catch(const std::exception &){owner->invalidate();return false;}
    if(terminal!=GuardedJobPhase::Failed && terminal!=GuardedJobPhase::Unknown && terminal!=GuardedJobPhase::Cancelled)return false;
    owner->invalidate();owner->phase=terminal;return true;
}
GuardedJobStatus guarded_job_status(Print &print)
{
    std::scoped_lock<std::mutex> lock(print.state_mutex());const auto owner=print.m_nonplanar_job;if(!owner)return {};
    try{if(owner->snapshot && !native_current(print,*owner->snapshot))owner->invalidate();}catch(const std::exception &){owner->invalidate();}
    return {owner->phase,owner->attempt,owner->snapshot};
}
}
