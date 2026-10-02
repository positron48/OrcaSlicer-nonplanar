#include "JobNative.hpp"
#include "Canonical.hpp"
#include "Interval.hpp"
#include "StlImport.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <exception>
#include <mutex>

namespace Slic3r::nptop {
namespace {
using Json=nlohmann::json;
// Native body processing may poll from TBB workers. Latch the first refusal:
// nested diagnostic helpers can catch exceptions, but cannot restore this call.
class NativeGuard {
    const GuardedJobTask task;
    const GuardedNativeLimits limits;
    const std::chrono::steady_clock::time_point started;
    std::mutex mutex;
    std::exception_ptr stopped;
public:
    NativeGuard(const GuardedJobTask &token,const GuardedNativeLimits &options,GuardedJobPhase phase,std::chrono::steady_clock::time_point begin)
        :task(token),limits(options),started(begin)
    {
        require(task.snapshot && task.phase==phase,"NATIVE_JOB_PHASE");
        require(limits.timeout.count()>0 && limits.timeout<=std::chrono::seconds(30),"NATIVE_JOB_TIMEOUT_DOMAIN");
    }
    void poll(const std::function<bool()> &extra={})
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(stopped)std::rethrow_exception(stopped);
        try {
            require(!limits.cancelled || !limits.cancelled(),"NATIVE_JOB_CANCELLED");
            require(!extra || !extra(),"NATIVE_JOB_NESTED_REFUSAL");
            require(task.is_current(),"STALE_NATIVE_JOB_TASK");
            require(std::chrono::steady_clock::now()-started<limits.timeout,"NATIVE_JOB_DEADLINE");
            detail::require_interval_environment();
        }catch(...){stopped=std::current_exception();throw;}
    }
    std::chrono::milliseconds remaining(std::chrono::milliseconds original)
    {
        poll();require(original.count()>0 && original<=std::chrono::seconds(30),"NATIVE_JOB_NESTED_TIMEOUT_DOMAIN");
        const auto left=limits.timeout-std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        require(left.count()>0,"NATIVE_JOB_DEADLINE");return std::min(original,left);
    }
    template<class Limits> void wrap(Limits &nested)
    {
        const auto cancel=nested.cancelled;nested.timeout=remaining(nested.timeout);
        nested.cancelled=[this,cancel]{poll(cancel);return false;};
    }
    template<class Limits> void revision(Limits &nested)
    {
        const auto current=nested.is_current;
        nested.is_current=[this,current](uint64_t value){poll([&]{return current && !current(value);});return true;};
    }
};
void mesh_bound(const TriangleMesh &mesh,const MeshAuditLimits &limits)
{
    require(limits.valid() && mesh.its.indices.size()<=limits.max_faces && mesh.its.vertices.size()<=limits.max_vertices &&
        mesh.its.properties.size()<=mesh.its.indices.size(),"NATIVE_JOB_MESH_CAPTURE_LIMIT");
}
std::string body_request(const GuardedNativeBodyRequest &request)
{
    const auto &p=request.material;detail::CanonicalConfigWriter w;
    w.append("{\"material\":[");w.append(std::to_string(p.model.model_id));
    for(auto value:{p.model.outer_xy_growth,p.model.outer_z_growth,p.model.inner_xy_loss,p.model.inner_z_loss,p.model.numerical_coordinate_error}){w.append(",");w.value(value.value());}
    for(auto value:{p.material.nominal.value(),p.material.upper.value(),p.material.lower.value(),p.support_reference_id,p.contact_reference_id}){w.append(",");w.append(std::to_string(value));}
    for(double value:{p.deposition_speed.value(),p.travel_speed.value(),p.acceleration.value()}){w.append(",");w.value(value);}
    w.append("],\"millimeters_declared\":");w.value(request.millimeters_declared);
    w.append(",\"plate_origin\":");w.value(Vec3d(p.plate_origin.x(),p.plate_origin.y(),p.plate_origin.z()));
    w.append(",\"reservation\":");w.value(native_mesh_fingerprint(request.reservation.its));w.append(",\"schema\":1}");return w.take();
}
std::string hatch_request(const NativeAffinePassRequest &request,const AffineHatchPolicy &hatch)
{
    const auto &p=request.policy;detail::CanonicalConfigWriter w;
    w.append("{\"footprint\":[");bool first=true;
    for(double value:{request.footprint.min_x,request.footprint.min_y,request.footprint.max_x,request.footprint.max_y}){if(!first)w.append(",");first=false;w.value(value);}
    w.append("],\"hatch\":[");w.value(hatch.width.value());w.append(",");w.value(hatch.maximum_pitch.value());w.append(",");w.value(hatch.boundary_band.value());w.append(",");w.value(int(hatch.first_direction));
    w.append("],\"passes\":[");w.append(std::to_string(p.passes));
    for(double value:{p.first_gap.minimum.value(),p.first_gap.maximum.value(),p.first_gap.corner_height_error.value(),p.later_vertical_minimum.value(),
        p.later_vertical_maximum.value(),p.later_normal_minimum.value(),p.later_normal_maximum.value(),p.total_volume_error.value()}){w.append(",");w.value(value);}
    w.append("],\"patch\":");w.append(std::to_string(request.patch));w.append(",\"schema\":1,\"support_plane\":");w.value(request.support_plane_z_mm);w.append("}");return w.take();
}
// Retain actual generated line alternatives and quotas, rather than hashing
// just their request. This is prospective geometry, never a laid material proof.
std::string hatch_geometry(const AffineHatchSnapshot &snapshot)
{
    detail::CanonicalConfigWriter w;const auto range=[&](ScalarBounds b){w.append("[");w.value(b.lower);w.append(",");w.value(b.upper);w.append("]");};
    const auto rectangle=[&](RectangleXY r){w.append("[");w.value(r.min_x);w.append(",");w.value(r.min_y);w.append(",");w.value(r.max_x);w.append(",");w.value(r.max_y);w.append("]");};
    const auto position=[&](PhysicalPosition p){w.value(Vec3d(p.x(),p.y(),p.z()));};
    w.append("{\"extent\":");w.value(int(snapshot.first_pass_extent));w.append(",\"lines\":");w.append(std::to_string(snapshot.line_count));
    w.append(",\"numeric_error\":");w.value(snapshot.numerical_error_upper_mm);w.append(",\"passes\":[");bool first=true;
    for(const auto &pass:snapshot.passes){if(!first)w.append(",");first=false;w.append("[");w.value(int(pass.direction));w.append(",");range(pass.pitch_mm);w.append(",");range(pass.prospective_volume_mm3);
        w.append(",[");bool line_first=true;for(const auto &line:pass.lines){if(!line_first)w.append(",");line_first=false;w.append("[");position(line.start);w.append(",");position(line.end);w.append(",");position(line.reverse_start);w.append(",");position(line.reverse_end);
            w.append(",");w.value(line.width.value());w.append(",");rectangle(line.volume_cell);w.append(",");range(line.prospective_cell_volume_mm3);w.append(",");range(line.projected_length_mm);w.append(",");w.value(line.coordinate_error_upper_mm);w.append("]");}
        w.append("],[");line_first=true;for(const auto &band:pass.boundary_regions){if(!line_first)w.append(",");line_first=false;rectangle(band);}w.append("]]");}
    w.append("],\"schema\":1,\"total\":");range(snapshot.total_prospective_volume_mm3);w.append("}");return w.take();
}
void body_config_binding(const GuardedJobSnapshot &job,const PlanarBodySnapshot &body)
{
    require(guarded_slicing_config(job.settings->resolved_print_config).fingerprint()==
        guarded_slicing_config(body.guarded_settings->resolved_print_config).fingerprint(),"NATIVE_JOB_EFFECTIVE_PRINT_CONFIG");
    require(job.settings->regions.size()==1 && body.guarded_settings->regions.size()==1 &&
        guarded_slicing_config(job.settings->regions.front().config).fingerprint()==
        guarded_slicing_config(body.guarded_settings->regions.front().config).fingerprint(),"NATIVE_JOB_EFFECTIVE_REGION_CONFIG");
}
}
GuardedNativeBodyResult analyze_guarded_native_body(const GuardedJobTask &requested_task,const GuardedNativeBodyRequest &requested,
    const GuardedNativeBodyLimits &requested_limits)
{
    GuardedNativeBodyResult result;const auto started=std::chrono::steady_clock::now();
    try {
        const auto task=requested_task;auto limits=requested_limits;
        mesh_bound(requested.reservation,limits.partition.geometry);
        require(task.snapshot && task.snapshot->input->objects.size()==1 && task.snapshot->input->objects.front().volumes.size()==1,"NATIVE_JOB_SINGLE_SOURCE");
        const auto &volume=task.snapshot->input->objects.front().volumes.front();
        require(volume.mesh.indices.size()<=limits.placement.geometry.max_faces && volume.mesh.vertices.size()<=limits.placement.geometry.max_vertices,"NATIVE_JOB_SOURCE_MESH_LIMIT");
        const auto request=requested; // Copy caller mesh/parameters before every callback.
        NativeGuard guard(task,limits,GuardedJobPhase::Analyzing,started);guard.poll();
        require(request.millimeters_declared,"NATIVE_JOB_SOURCE_UNITS_UNKNOWN");
        require(Vec3d(request.material.plate_origin.x(),request.material.plate_origin.y(),request.material.plate_origin.z())==task.snapshot->settings->plate_origin_mm,"NATIVE_JOB_PHYSICAL_ORIGIN_BINDING");
        const auto source=std::find_if(task.snapshot->resources.begin(),task.snapshot->resources.end(),[&](const auto &resource){return resource.kind==JobResourceKind::SourceFile && resource.name==volume.source_file;});
        require(source!=task.snapshot->resources.end(),"NATIVE_JOB_SOURCE_BYTES_MISSING");
        const auto input=guarded_slicing_input(*task.snapshot);guard.poll();
        guard.wrap(limits.import);const auto imported=import_stl_snapshot(source->bytes,true,limits.import);guard.poll();
        require(imported.geometry.status==MeshAuditStatus::ValidGeometry && imported.source && imported.source->sha256==source->sha256,"NATIVE_JOB_SOURCE_IMPORT");
        guard.wrap(limits.placement.geometry);guard.revision(limits.placement);
        const auto placed=capture_input_placement(imported,{task.snapshot->input_revision,input},
            {size_t(task.snapshot->settings->plate_index),task.snapshot->settings->plate_origin_mm},limits.placement);guard.poll();
        require(placed.snapshot && placed.geometry.status==MeshAuditStatus::ValidGeometry,"NATIVE_JOB_SOURCE_PLACEMENT");
        guard.wrap(limits.partition.geometry);guard.revision(limits.partition);
        const auto partition=partition_cap(placed,request.reservation,limits.partition);guard.poll();
        require(partition.status==VolumePartitionStatus::Partitioned && partition.snapshot,"NATIVE_JOB_PARTITION");
        guard.wrap(limits.body.paths);guard.revision(limits.body.paths);
        const auto body=generate_planar_body(partition,limits.body);guard.poll();require(bool(body.snapshot),body.reason.c_str());
        body_config_binding(*task.snapshot,*body.snapshot);guard.poll();
        guard.wrap(limits.material);guard.revision(limits.material);
        const auto material=reconstruct_planar_body_material(body,request.material,limits.material);guard.poll();require(bool(material.snapshot),material.reason.c_str());
        auto parameters=body_request(request);const Json document={{"schema",1},{"scope","owned_native_body_dependency_lineage_only"},
            {"job_fingerprint",task.snapshot->fingerprint},{"attempt",task.attempt},{"source_sha256",source->sha256},
            {"slicing_input",input->fingerprint},{"request",Json::parse(parameters)},{"body",material.snapshot->body_fingerprint},
            {"body_material",material.snapshot->fingerprint()},{"body_journal",material.snapshot->material_fingerprint}};
        auto json=document.dump();auto hash=sha256_bytes(json);guard.poll();
        result.snapshot=std::shared_ptr<const GuardedNativeBodySnapshot>(new GuardedNativeBodySnapshot(task.snapshot,task.attempt,input,material.snapshot,
            std::move(parameters),std::move(json),std::move(hash)));guard.poll();result.reason="OWNED_NATIVE_BODY_LINEAGE_ONLY";
    }catch(const std::exception &e){result.snapshot.reset();result.reason=*e.what() ? e.what() : "NATIVE_JOB_EXCEPTION_WITHOUT_REASON";}
    catch(...){result.snapshot.reset();result.reason="NATIVE_JOB_UNKNOWN_EXCEPTION";}return result;
}
GuardedNativeHatchResult plan_guarded_native_hatches(const GuardedJobTask &requested_task,std::shared_ptr<const GuardedNativeBodySnapshot> body,
    const NativeAffinePassRequest &requested,const AffineHatchPolicy &requested_policy,const GuardedNativeHatchLimits &requested_limits)
{
    GuardedNativeHatchResult result;const auto started=std::chrono::steady_clock::now();
    try {
        const auto task=requested_task;const auto request=requested;const auto policy=requested_policy;auto limits=requested_limits;
        NativeGuard guard(task,limits,GuardedJobPhase::Planning,started);guard.poll();
        require(body && body->job==task.snapshot && body->attempt==task.attempt && sha256_bytes(body->canonical_json)==body->sha256,"NATIVE_JOB_BODY_OWNER");
        guard.wrap(limits.passes.material);guard.revision(limits.passes.material);
        guard.wrap(limits.passes.projection.geometry);guard.revision(limits.passes.projection);
        guard.wrap(limits.hatches);guard.revision(limits.hatches);guard.wrap(limits.hatches.volumes);guard.revision(limits.hatches.volumes);
        const auto native=plan_native_affine_hatches({"",body->body},request,policy,limits.passes,limits.hatches);guard.poll();require(bool(native.snapshot),native.reason.c_str());
        require(native.snapshot->passes->body_material==body->body && native.snapshot->hatches->source==native.snapshot->passes->stack,"NATIVE_JOB_HATCH_PARENT");
        auto parameters=hatch_request(request,policy);const Json document={{"schema",1},{"scope","owned_native_affine_hatch_dependency_lineage_only"},
            {"body_lineage",body->sha256},{"request",Json::parse(parameters)},{"geometry",Json::parse(hatch_geometry(*native.snapshot->hatches))}};
        auto json=document.dump();auto hash=sha256_bytes(json);guard.poll();
        result.snapshot=std::shared_ptr<const GuardedNativeHatchSnapshot>(new GuardedNativeHatchSnapshot(body,native.snapshot,std::move(parameters),std::move(json),std::move(hash)));
        guard.poll();result.reason="OWNED_NATIVE_AFFINE_HATCH_LINEAGE_ONLY";
    }catch(const std::exception &e){result.snapshot.reset();result.reason=*e.what() ? e.what() : "NATIVE_JOB_EXCEPTION_WITHOUT_REASON";}
    catch(...){result.snapshot.reset();result.reason="NATIVE_JOB_UNKNOWN_EXCEPTION";}return result;
}
GuardedNativePlanResult capture_guarded_native_plan(const GuardedJobTask &requested_task,std::shared_ptr<const GuardedNativeHatchSnapshot> hatches,
    const FirstCapMaterialResult &requested_assembly,const LinearCandidateResult &requested_candidate,const GuardedJobLimits &requested_limits)
{
    GuardedNativePlanResult result;const auto started=std::chrono::steady_clock::now();
    try {
        const auto task=requested_task;const auto assembly=requested_assembly.snapshot;const auto candidate=requested_candidate.snapshot;
        const auto plan=requested_candidate.plan;const auto limits=requested_limits;
        require(limits.timeout.count()>0 && limits.timeout.count()<=1000,"NATIVE_JOB_PLAN_TIMEOUT_DOMAIN");
        NativeGuard guard(task,{limits.timeout,limits.cancelled},GuardedJobPhase::Serializing,started);guard.poll();
        require(hatches && hatches->body->job==task.snapshot && hatches->body->attempt==task.attempt &&
            sha256_bytes(hatches->canonical_json)==hatches->sha256,"NATIVE_JOB_HATCH_OWNER");
        require(assembly && assembly->source->source==hatches->native->hatches,"NATIVE_JOB_CAP_PARENT");
        const auto body=hatches->body->body->material;
        require(assembly->body->sequence==body && assembly->body_records==body->records.size() &&
            assembly->body->completed_records==body->records.size() && assembly->body->current_progress==0,"NATIVE_JOB_COMPLETE_BODY_REQUIRED");
        require(assembly->material->completed_records==assembly->material->sequence->records.size() && assembly->material->current_progress==0,"NATIVE_JOB_COMPLETE_ASSEMBLY_REQUIRED");
        require(candidate && plan && candidate->plan==plan && sha256_bytes(candidate->bytes)==candidate->sha256,"NATIVE_JOB_CANDIDATE_PARENT");
        const auto &original=*assembly->material->sequence;const auto &source=*plan->source->material->ledger;const auto &planned=*plan->planned->material->ledger;
        require(original.fingerprint()==source.fingerprint(),"NATIVE_JOB_MOTION_INPUT_JOURNAL");guard.poll();
        require(original.canonical_context()==planned.canonical_context() && original.records.size()==planned.records.size(),"NATIVE_JOB_MOTION_OUTPUT_CONTEXT");
        auto rows=original.records;
        for(size_t i=0;i<rows.size();++i){if(i%128==0)guard.poll();const auto &limited=planned.records[i].motion;
            require(limited.speed_limit.value()<=rows[i].motion.speed_limit.value() && limited.acceleration_limit.value()<=rows[i].motion.acceleration_limit.value(),"NATIVE_JOB_MOTION_LIMIT_INCREASE");
            rows[i].motion.speed_limit=limited.speed_limit;rows[i].motion.acceleration_limit=limited.acceleration_limit;}
        const MaterialSequenceSnapshot expected{original.revision,original.source_fingerprint,original.model,std::move(rows),original.geometry};
        require(expected.fingerprint()==planned.fingerprint(),"NATIVE_JOB_MOTION_OUTPUT_JOURNAL");guard.poll();
        const Json document={{"schema",1},{"scope","owned_native_body_cap_linear_candidate_lineage_only"},{"job_fingerprint",task.snapshot->fingerprint},
            {"attempt",task.attempt},{"hatch_lineage",hatches->sha256},{"assembled_journal",original.fingerprint()},
            {"planned_journal",planned.fingerprint()},{"candidate_sha256",candidate->sha256}};
        auto json=document.dump();auto hash=sha256_bytes(json);guard.poll();
        result.snapshot=std::shared_ptr<const GuardedNativePlanSnapshot>(new GuardedNativePlanSnapshot(hatches,assembly,candidate,std::move(json),std::move(hash)));
        guard.poll();result.reason="OWNED_NATIVE_PLAN_LINEAGE_FULL_JOB_QUALIFICATION_PENDING";
    }catch(const std::exception &e){result.snapshot.reset();result.reason=*e.what() ? e.what() : "NATIVE_JOB_EXCEPTION_WITHOUT_REASON";}
    catch(...){result.snapshot.reset();result.reason="NATIVE_JOB_UNKNOWN_EXCEPTION";}return result;
}
}
