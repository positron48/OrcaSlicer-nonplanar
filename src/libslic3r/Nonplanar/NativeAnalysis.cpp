#include "NativeAnalysis.hpp"
#include "Canonical.hpp"
#include "Interval.hpp"
#include "../Print.hpp"
#include <exception>
#include <mutex>

namespace Slic3r::nptop {
std::shared_ptr<const NativeAnalysisRequestSnapshot> capture_native_analysis_request(const NativeAnalysisRequest &requested)
{
    require(requested.reservation.its.vertices.size()<=15000 && requested.reservation.its.indices.size()<=5000 &&
        requested.reservation.its.properties.size()<=requested.reservation.its.indices.size() &&
        requested.inputs.scene.head.size()<=64 && requested.inputs.scene.obstacles.size()<=10000,"NATIVE_ANALYSIS_REQUEST_SIZE_LIMIT");
    const auto request=requested;
    const auto inputs=capture_native_job_inputs(request.inputs,[]{});
    detail::CanonicalConfigWriter w;
    const auto number=[&](double v){require(std::isfinite(v),"NONFINITE_NATIVE_ANALYSIS_REQUEST");w.value(v);};
    const auto point=[&](auto p){w.value(Vec3d(p.x(),p.y(),p.z()));};
    w.append("{\"contour\":[");number(request.contour.width.value());w.append(",");w.append(std::to_string(request.contour.seam_corner));
    w.append(",");w.value(request.contour.clockwise);w.append(",");number(request.contour.maximum_outside_target.value());
    w.append("],\"fill_region\":[");point(request.fill_region.min);w.append(",");point(request.fill_region.max);
    w.append("],\"footprint\":[");bool first=true;
    for(double v:{request.passes.footprint.min_x,request.passes.footprint.min_y,request.passes.footprint.max_x,request.passes.footprint.max_y}){if(!first)w.append(",");first=false;number(v);}
    w.append("],\"hatches\":[");first=true;
    for(double v:{request.hatches.width.value(),request.hatches.maximum_pitch.value(),request.hatches.boundary_band.value()}){if(!first)w.append(",");first=false;number(v);}
    w.append(",");w.value(int(request.hatches.first_direction));
    w.append("],\"inputs\":[");first=true;
    for(const auto &r:inputs->resources){if(!first)w.append(",");first=false;w.append("[");w.value(int(r.kind));w.append(",");w.value(r.name);w.append(",");w.value(sha256_bytes(r.bytes));w.append("]");}
    w.append("],\"millimeters_declared\":");w.value(request.millimeters_declared);
    w.append(",\"passes\":[");const auto &p=request.passes.policy;w.append(std::to_string(p.passes));
    for(double v:{p.first_gap.minimum.value(),p.first_gap.maximum.value(),p.first_gap.corner_height_error.value(),p.later_vertical_minimum.value(),
        p.later_vertical_maximum.value(),p.later_normal_minimum.value(),p.later_normal_maximum.value(),p.total_volume_error.value()}){w.append(",");number(v);}
    w.append("],\"patch\":");w.append(std::to_string(request.passes.patch));w.append(",\"reservation\":");w.value(native_mesh_fingerprint(request.reservation.its));
    w.append(",\"schema\":1,\"support_plane\":");number(request.passes.support_plane_z_mm);w.append("}");
    auto json=w.take();auto hash=sha256_bytes(json);
    return std::shared_ptr<const NativeAnalysisRequestSnapshot>(new NativeAnalysisRequestSnapshot(request,std::move(json),std::move(hash)));
}
namespace {
class AnalysisGuard {
    const GuardedNativeLimits limits;
    const std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
    std::mutex mutex;
    std::exception_ptr stopped;
    std::shared_ptr<const GuardedJobTask> task;
public:
    explicit AnalysisGuard(const GuardedNativeLimits &options):limits(options){}
    void set_task(std::shared_ptr<const GuardedJobTask> next){std::lock_guard<std::mutex> lock(mutex);task=std::move(next);}
    void poll(bool check_task=true,const std::function<bool()> &extra={})
    {
        std::lock_guard<std::mutex> lock(mutex);if(stopped)std::rethrow_exception(stopped);
        try{
            require(limits.timeout.count()>0 && limits.timeout<=std::chrono::seconds(30),"NATIVE_ANALYSIS_TIMEOUT_DOMAIN");
            require(!limits.cancelled || !limits.cancelled(),"NATIVE_ANALYSIS_CANCELLED");
            require(!extra || !extra(),"NATIVE_ANALYSIS_NESTED_REFUSAL");
            require(!check_task || (task && task->is_current()),"STALE_NATIVE_ANALYSIS_TASK");
            require(std::chrono::steady_clock::now()-started<limits.timeout,"NATIVE_ANALYSIS_DEADLINE");detail::require_interval_environment();
        }catch(...){stopped=std::current_exception();throw;}
    }
    std::chrono::milliseconds remaining(std::chrono::milliseconds ceiling,bool check_task=true)
    {
        poll(check_task);require(ceiling.count()>0 && ceiling<=std::chrono::seconds(30),"NATIVE_ANALYSIS_NESTED_TIMEOUT_DOMAIN");
        const auto left=limits.timeout-std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        require(left.count()>0,"NATIVE_ANALYSIS_DEADLINE");return std::min(ceiling,left);
    }
    template<class Limits> void wrap(Limits &nested,bool check_task=true)
    {
        const auto cancel=nested.cancelled;nested.timeout=remaining(nested.timeout,check_task);
        nested.cancelled=[this,cancel,check_task]{poll(check_task,cancel);return false;};
    }
    void finish()
    {
        std::lock_guard<std::mutex> lock(mutex);if(stopped)std::rethrow_exception(stopped);
        require(std::chrono::steady_clock::now()-started<limits.timeout,"NATIVE_ANALYSIS_DEADLINE");
    }
};
}
NativeAnalysisResult run_native_analysis(Print &print,uint64_t id,std::shared_ptr<const NativeAnalysisRequestSnapshot> request,
    const std::vector<JobResource> &source_files,const NativeAnalysisLimits &requested_limits)
{
    NativeAnalysisResult result;const auto limits=requested_limits;AnalysisGuard guard(limits);
    std::shared_ptr<const GuardedJobTask> task;
    try{
        // Invalid run requests also invalidate the previous host attempt. Do
        // not perform an unbounded caller-resource copy before that boundary.
        bool bounded=source_files.size()<=248;size_t total=0;
        for(const auto &r:source_files){if(r.name.empty() || r.name.size()>4096 || r.bytes.size()>32*1024*1024-total){bounded=false;break;}total+=r.bytes.size();}
        GuardedJobLimits capture;capture.cancelled=[&]{guard.poll(false);return false;};
        if(!request || !bounded){begin_guarded_job(print,id,{},capture);require(false,request ? "NATIVE_ANALYSIS_SOURCE_SIZE_LIMIT" : "MISSING_NATIVE_ANALYSIS_REQUEST");}
        auto files=source_files;files.push_back({JobResourceKind::SourceFile,"native-analysis-request-v1",request->canonical_json});
        const auto job=begin_guarded_job(print,id,files,capture,JobSoftwareMode::CompiledInputs,&request->values.inputs);
        result.job=job.snapshot;task=job.task;require(bool(task),job.reason.empty() ? "NATIVE_ANALYSIS_REQUIRES_GUARDED_MODE" : job.reason.c_str());guard.set_task(task);
        const auto stage=[&](NativeAnalysisStage next){result.stage=next;guard.poll();if(limits.progress)limits.progress(next);guard.poll();};
        const auto advance=[&](GuardedJobPhase next){const auto value=advance_guarded_job(print,*task,next);require(bool(value.task),value.reason.c_str());task=value.task;guard.set_task(task);guard.poll();};
        stage(NativeAnalysisStage::Capture);
        const auto &r=request->values;
        stage(NativeAnalysisStage::Body);auto body_limits=limits.body;guard.wrap(body_limits);
        const auto body=analyze_guarded_native_body(*task,{r.millimeters_declared,r.reservation,r.inputs.body},body_limits);guard.poll();require(bool(body.snapshot),body.reason.c_str());
        advance(GuardedJobPhase::Planning);stage(NativeAnalysisStage::Hatches);auto hatch_limits=limits.hatches;guard.wrap(hatch_limits);
        const auto hatches=plan_guarded_native_hatches(*task,body.snapshot,r.passes,r.hatches,hatch_limits);guard.poll();require(bool(hatches.snapshot),hatches.reason.c_str());
        stage(NativeAnalysisStage::Cap);auto cap_limits=limits.cap;guard.wrap(cap_limits);
        const auto cap=plan_first_cap({"",hatches.snapshot->native->hatches},r.contour,r.fill_region,cap_limits);guard.poll();require(bool(cap.snapshot),cap.reason.c_str());
        stage(NativeAnalysisStage::Material);auto material_limits=limits.material;guard.wrap(material_limits);
        const auto assembly=reconstruct_first_cap_material(cap,{},0,material_limits);guard.poll();require(bool(assembly.snapshot),assembly.reason.c_str());
        stage(NativeAnalysisStage::Motion);
        MaterialMotionPreparationLimits preparation;guard.wrap(preparation);
        const auto material=prepare_material_motion({"",assembly.snapshot->material->sequence},preparation);guard.poll();require(bool(material.snapshot),material.reason.c_str());
        SimulationMotionPreparationLimits scene_limits;guard.wrap(scene_limits);
        const auto source=prepare_simulation_motion(r.inputs.scene,material,r.inputs.clearance,scene_limits);guard.poll();require(bool(source.snapshot),source.reason.c_str());
        auto motion_limits=limits.motion;guard.wrap(motion_limits);
        const auto motion=plan_linear_motion(source,r.inputs.motion,motion_limits);guard.poll();require(bool(motion.snapshot),motion.reason.c_str());
        advance(GuardedJobPhase::Serializing);stage(NativeAnalysisStage::Serialize);auto candidate_limits=limits.candidate;guard.wrap(candidate_limits);
        const auto candidate=serialize_linear_candidate(motion,r.inputs.serializer,candidate_limits);guard.poll();require(bool(candidate.snapshot),candidate.reason.c_str());
        stage(NativeAnalysisStage::Lineage);GuardedJobLimits binding_limits;guard.wrap(binding_limits);
        const auto native=capture_guarded_native_plan(*task,hatches.snapshot,assembly,candidate,binding_limits);guard.poll();require(bool(native.snapshot),native.reason.c_str());
        // The binder advances its token internally. Its own before/after owner
        // checks remain authoritative while the shared guard enforces time/cancel.
        binding_limits={};guard.wrap(binding_limits,false);
        const auto binding=bind_guarded_candidate(print,*task,candidate,binding_limits,native.snapshot);require(bool(binding.task),binding.reason.c_str());task=binding.task;guard.set_task(task);guard.poll();
        stage(NativeAnalysisStage::Replay);auto replay_limits=limits.candidate;replay_limits.timeout=std::min(replay_limits.timeout,std::chrono::milliseconds(1000));guard.wrap(replay_limits);
        const auto report=verify_guarded_candidate_report(binding,r.inputs.replay,replay_limits);guard.poll();require(bool(report.snapshot),report.reason.c_str());
        stage(NativeAnalysisStage::Admission);GuardedJobLimits admission;guard.wrap(admission);
        require(accept_guarded_candidate_report(print,report.snapshot,admission),"NATIVE_ANALYSIS_REPORT_ADMISSION_REFUSED");
        // Admission deliberately terminates the working token. No callback may
        // run afterwards, and a blocked report never becomes an export credential.
        guard.finish();result.snapshot=std::shared_ptr<const NativeAnalysisSnapshot>(new NativeAnalysisSnapshot(request,native.snapshot,report.snapshot,report.evaluations));guard.finish();
        result.reason="NATIVE_ANALYSIS_COMPLETE_EXPORT_BLOCKED";
    }catch(const std::exception &e){result.snapshot.reset();result.reason=*e.what() ? e.what() : "NATIVE_ANALYSIS_EXCEPTION_WITHOUT_REASON";if(task)stop_guarded_job(print,*task,GuardedJobPhase::Unknown);}
    catch(...){result.snapshot.reset();result.reason="NATIVE_ANALYSIS_UNKNOWN_EXCEPTION";if(task)stop_guarded_job(print,*task,GuardedJobPhase::Unknown);}
    return result;
}
}
