#include "NativeJobInputs.hpp"
#include "Canonical.hpp"

namespace Slic3r::nptop {
namespace {
using Writer=detail::CanonicalConfigWriter;
void number(Writer &w,double value)
{require(std::isfinite(value),"NONFINITE_NATIVE_JOB_INPUT");w.value(value);}
template<class Position> void point(Writer &w,Position p)
{w.value(Vec3d(p.x(),p.y(),p.z()));}
template<class Box> void box(Writer &w,const Box &b)
{w.append("[");point(w,b.min);w.append(",");point(w,b.max);w.append("]");}
std::string model_json(const MaterialModel &p)
{
    Writer w;w.append("[");w.append(std::to_string(p.model_id));
    for(auto v:{p.outer_xy_growth,p.outer_z_growth,p.inner_xy_loss,p.inner_z_loss,p.numerical_coordinate_error}){w.append(",");number(w,v.value());}
    w.append("]");return w.take();
}
std::string body_json(const BodyMaterialParameters &p)
{
    Writer w;w.append("{\"material\":[");w.append(model_json(p.model));
    for(auto id:{p.material.nominal.value(),p.material.upper.value(),p.material.lower.value(),p.support_reference_id,p.contact_reference_id}){w.append(",");w.append(std::to_string(id));}
    for(double v:{p.deposition_speed.value(),p.travel_speed.value(),p.acceleration.value()}){w.append(",");number(w,v);}
    w.append("],\"plate_origin\":");point(w,p.plate_origin);w.append("}");return w.take();
}
std::string replay_json(const LinearMaterialOptions &p)
{
    Writer w;w.append("[");w.append(std::to_string(p.policy_id));w.append(",");w.append(std::to_string(p.revision));
    for(double v:{p.max_nominal_delta_mm3,p.max_total_nominal_delta_mm3,p.max_filament_delta_mm,p.relative_dose_error,p.absolute_dose_error_mm3}){w.append(",");number(w,v);}
    w.append("]");return w.take();
}
std::string clearance_json(const ClearancePolicy &p)
{
    Writer w;w.append("{\"clearance\":[");bool first=true;
    for(double v:{p.required.value(),p.numeric.import_mm,p.numeric.chord_mm,p.numeric.distance_mm,p.numeric.conversion_mm,
        p.tool_measurement.value(),p.positioning.value(),p.material.value(),p.scene_geometry.value()}){
        if(!first)w.append(",");first=false;number(w,v);
    }w.append("],\"schema\":1}");return w.take();
}
std::string tool_json(const SimulationScene &s,const std::function<void()> &poll={})
{
    Writer w;w.append("{\"head\":[");bool first=true;
    for(const auto &part:s.head){if(poll)poll();if(!first)w.append(",");first=false;
        w.append("[");w.append(std::to_string(part.id));w.append(",");w.value(int(part.part));w.append(",");box(w,part.outer);
        w.append(",");w.value(part.moving);w.append(",");w.value(part.all_configurations_enclosed);w.append("]");
    }
    w.append("],\"schema\":1,\"tip\":[");point(w,s.tip.center);w.append(",");number(w,s.tip.opening_radius.value());
    w.append(",");number(w,s.tip.outer_radius.value());w.append("]}");return w.take();
}
std::string scene_json(const SimulationScene &s,const std::function<void()> &poll={})
{
    Writer w;w.append("{\"coverage\":[");box(w,s.nozzle_domain);w.append(",");box(w,s.scene_domain);
    w.append(",");number(w,s.unmodelled_parts_min_local_z.value());w.append(",");w.value(s.obstacle_inventory_complete);w.append(",");number(w,s.uncertainty.value());
    w.append("],\"identity\":[");w.append(std::to_string(s.version));w.append(",");w.append(std::to_string(s.profile_id));w.append(",");w.append(std::to_string(s.revision));
    w.append(",");w.value(int(s.origin));w.append(",");w.value(s.operator_confirmed_claim);w.append("],\"obstacles\":[");bool first=true;
    for(const auto &obstacle:s.obstacles){if(poll)poll();if(!first)w.append(",");first=false;box(w,obstacle);}
    w.append("],\"schema\":1}");return w.take();
}
std::string hash_resource(const char *field,const std::string &hash)
{
    Writer w;w.append("{\"");w.append(field);w.append("\":");w.value(hash);w.append(",\"schema\":1}");return w.take();
}
}
std::shared_ptr<const NativeJobInputsSnapshot> capture_native_job_inputs(NativeJobInputsRequest inputs,const std::function<void()> &poll)
{
    require(inputs.scene.head.size()<=64 && inputs.scene.obstacles.size()<=10000,"NATIVE_JOB_INPUT_SIZE_LIMIT");
    // Fingerprints also encode unsupported values exactly, so reject nonfinite
    // plain doubles before admitting a request as a typed identity input.
    Writer validation;
    for(auto array:{inputs.motion.axis_speed_mm_s,inputs.motion.axis_acceleration_mm_s2,inputs.motion.drive_speed_mm_s,inputs.motion.drive_acceleration_mm_s2})
        for(double v:array)number(validation,v);
    for(double v:{inputs.motion.max_volume_mm3_s,inputs.motion.max_extrude_cross_section_mm2,inputs.motion.max_events_per_second})number(validation,v);
    std::vector<JobResource> resources;
    resources.push_back({JobResourceKind::Toolhead,"native-toolhead-v1",tool_json(inputs.scene,poll)});poll();
    resources.push_back({JobResourceKind::Scene,"native-scene-v1",scene_json(inputs.scene,poll)});poll();
    resources.push_back({JobResourceKind::Material,"native-material-v1","{\"body\":"+body_json(inputs.body)+",\"replay\":"+replay_json(inputs.replay)+",\"schema\":1}"});poll();
    resources.push_back({JobResourceKind::Numeric,"native-clearance-v1",clearance_json(inputs.clearance)});poll();
    resources.push_back({JobResourceKind::Firmware,"native-motion-v1",hash_resource("motion_policy",inputs.motion.fingerprint())});poll();
    Writer serializer;serializer.append("{\"schema\":1,\"serializer_policy\":");serializer.value(inputs.serializer.fingerprint());serializer.append("}");
    resources.push_back({JobResourceKind::Algorithms,"native-serializer-v1",serializer.take()});poll();
    return std::shared_ptr<const NativeJobInputsSnapshot>(new NativeJobInputsSnapshot(std::move(inputs),std::move(resources)));
}
bool NativeJobInputsSnapshot::matches_body(const BodyMaterialParameters &p) const
{return body_json(values.body)==body_json(p);}
bool NativeJobInputsSnapshot::matches_candidate(const LinearCandidateSnapshot &candidate,const std::function<void()> &poll) const
{
    const auto &plan=*candidate.plan;
    for(const auto &source:{plan.source,plan.planned}){
        auto base_model=source->material->ledger->model;
        // Protected cap producers add their calculated coordinate error. The
        // native lineage separately checks the exact derived journal, so that
        // allowance cannot be supplied by a caller in place of a laid model.
        if(base_model.numerical_coordinate_error.value()<values.body.model.numerical_coordinate_error.value())return false;
        base_model.numerical_coordinate_error=values.body.model.numerical_coordinate_error;
        if(source->scene.head.size()>64 || source->scene.obstacles.size()>10000 ||
            tool_json(source->scene,poll)!=resources[0].bytes || scene_json(source->scene,poll)!=resources[1].bytes ||
            clearance_json(source->policy)!=resources[3].bytes || model_json(base_model)!=model_json(values.body.model))return false;
    }
    return plan.policy_fingerprint==values.motion.fingerprint() && candidate.policy_fingerprint==values.serializer.fingerprint();
}
bool NativeJobInputsSnapshot::matches_replay(const LinearMaterialOptions &p) const
{return replay_json(values.replay)==replay_json(p);}
}
