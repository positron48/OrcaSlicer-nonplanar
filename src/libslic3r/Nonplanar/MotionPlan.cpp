#include "MotionPlan.hpp"
#include "Canonical.hpp"
#include "Interval.hpp"
#include "StlImport.hpp"

namespace Slic3r::nptop {
namespace {
using detail::Interval;
struct Refusal : std::runtime_error { using std::runtime_error::runtime_error; };
void refuse(const char *reason) {throw Refusal(reason);}
ScalarBounds bounds(Interval value) {return {value.lo,value.hi};}
Interval absolute(Interval value) {
    return {value.lo<=0 && value.hi>=0 ? 0 : std::min(std::abs(value.lo),std::abs(value.hi)),std::max(std::abs(value.lo),std::abs(value.hi))};
}
bool contains(const SceneBox &box,PhysicalPosition p) {
    return p.x()>=box.min.x() && p.x()<=box.max.x() && p.y()>=box.min.y() && p.y()<=box.max.y() && p.z()>=box.min.z() && p.z()<=box.max.z();
}
}
std::string LinearMotionPolicy::fingerprint() const
{
    detail::CanonicalConfigWriter w;w.append("nptop-linear-motion-policy-v1[");
    for (uint64_t id : {version,profile_id,revision}) {w.append(std::to_string(id));w.append(",");}
    w.value(int(origin));w.append(",");w.value(operator_confirmed_claim);w.append(",");w.value(int(model));w.append(",");w.value(int(kinematics));w.append(",");
    w.value(Vec3d(commanded_domain.min.x(),commanded_domain.min.y(),commanded_domain.min.z()));w.append(",");
    w.value(Vec3d(commanded_domain.max.x(),commanded_domain.max.y(),commanded_domain.max.z()));
    for (auto array : {axis_speed_mm_s,axis_acceleration_mm_s2,drive_speed_mm_s,drive_acceleration_mm_s2})
        for (double v : array) {w.append(",");w.value(v);}
    for (double v : {filament_diameter.value(),flow.value(),filament_speed.value(),filament_acceleration.value(),max_retraction.value(),
                     max_volume_mm3_s,max_extrude_cross_section_mm2,max_events_per_second}) {w.append(",");w.value(v);}
    w.append("]");return sha256_bytes(w.take());
}
LinearMotionPlanResult plan_linear_motion(const SimulationMotionSourceResult &requested,const LinearMotionPolicy &requested_policy,
    const LinearMotionPlanLimits &requested_limits)
{
    const auto source=requested.snapshot;const auto policy=requested_policy;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();LinearMotionPlanResult result;result.source=source;
    try {
        if (!source || !limits.max_records || limits.max_records>200000 || !limits.max_evaluations || limits.max_evaluations>2000000 ||
            limits.timeout.count()<=0 || limits.timeout>std::chrono::seconds(30)) refuse("INVALID_LINEAR_MOTION_INPUT");
        const auto stop=[&] {
            if (limits.cancelled && limits.cancelled()) refuse("CANCELLED");
            if (limits.is_current && !limits.is_current(source->material->ledger->revision)) refuse("STALE_MATERIAL_REVISION");
            if (limits.is_scene_current && !limits.is_scene_current(source->scene.profile_id,source->scene.revision)) refuse("STALE_SCENE_REVISION");
            if (limits.is_policy_current && !limits.is_policy_current(policy.profile_id,policy.revision)) refuse("STALE_MOTION_POLICY");
            detail::require_interval_environment();
            if (std::chrono::steady_clock::now()-started>=limits.timeout) refuse("LINEAR_MOTION_DEADLINE");
        };
        const auto work=[&] {if (++result.evaluations>limits.max_evaluations) refuse("LINEAR_MOTION_WORK_LIMIT");stop();};work();
        if (policy.version!=1 || !policy.profile_id || !policy.revision || policy.origin!=ProfileOrigin::Synthetic ||
            policy.operator_confirmed_claim || policy.model!=LinearPlannerModel::FullStop ||
            (policy.kinematics!=LinearKinematics::Cartesian && policy.kinematics!=LinearKinematics::CoreXY)) refuse("UNSUPPORTED_LINEAR_MOTION_POLICY");
        for (auto array : {policy.axis_speed_mm_s,policy.axis_acceleration_mm_s2,policy.drive_speed_mm_s,policy.drive_acceleration_mm_s2}) for (double value : array) {
            work();if (!std::isfinite(value) || value<=0 || value>1000000) refuse("INVALID_AXIS_LIMIT");
        }
        for (double value : {policy.filament_diameter.value(),policy.flow.value(),policy.filament_speed.value(),policy.filament_acceleration.value(),
                             policy.max_retraction.value(),policy.max_volume_mm3_s,policy.max_extrude_cross_section_mm2,policy.max_events_per_second}) {
            work();if (!std::isfinite(value) || value<=0 || value>1000000) refuse("INVALID_EXTRUSION_LIMIT");
        }
        const auto &domain=policy.commanded_domain;
        if (domain.min.x()>=domain.max.x() || domain.min.y()>=domain.max.y() || domain.min.z()>=domain.max.z()) refuse("INVALID_AXIS_DOMAIN");
        const auto &old=*source->material->ledger;if (old.records.size()>limits.max_records) refuse("LINEAR_MOTION_RECORD_LIMIT");
        const Interval diameter(policy.filament_diameter.value());
        const auto area=Interval(3.141592653589793,3.1415926535897936)*detail::square(diameter/Interval(2));
        const Interval period=Interval(1)/Interval(policy.max_events_per_second);
        std::vector<MaterialRecord> rows;rows.reserve(old.records.size());std::vector<LinearMotionStep> steps;steps.reserve(old.records.size());
        Interval total_volume(0),total_filament(0),total_time(0);
        for (size_t i=0;i<old.records.size();++i) {
            work();result.blocked_record=i;auto row=old.records[i];const auto &event=row.motion;
            if (!contains(domain,event.start) || !contains(domain,event.end)) refuse("MOTION_OUTSIDE_AXIS_DOMAIN");
            Interval volume(0),filament(0),command_volume(0);
            if (const auto *deposition=std::get_if<Deposition>(&event.payload)) {
                volume=Interval(deposition->volume.value());command_volume=volume*Interval(policy.flow.value());filament=command_volume/area;
                const double feed=filament_feed(deposition->volume,policy.filament_diameter,policy.flow).value();
                if (feed<filament.lo || feed>filament.hi) refuse("FILAMENT_COMMAND_OUTSIDE_INTERVAL");
                total_volume=total_volume+volume;total_filament=total_filament+filament;
            } else if (const auto *pressure=std::get_if<Retraction>(&event.payload)) {
                if (pressure->amount.value()>policy.max_retraction.value()) refuse("RETRACTION_LENGTH_LIMIT");
                const double sign=pressure->after==RetractionState::Retracted ? -1 : 1;filament=Interval(sign*pressure->amount.value());
            }
            const bool pressure=std::holds_alternative<Retraction>(event.payload);
            const std::array<Interval,3> delta{Interval(event.end.x())-Interval(event.start.x()),Interval(event.end.y())-Interval(event.start.y()),
                                             Interval(event.end.z())-Interval(event.start.z())};
            const bool stationary=event.start.x()==event.end.x() && event.start.y()==event.end.y() && event.start.z()==event.end.z();
            if (stationary && !pressure) {
                steps.push_back({LinearStepCoordinate::Dwell,{0,0},bounds(filament),{period.hi,period.hi},0,0});
                total_time=total_time+Interval(period.hi);rows.push_back(std::move(row));continue;
            }
            const auto length=pressure ? absolute(filament) : detail::root(detail::square(delta[0])+detail::square(delta[1])+detail::square(delta[2]));
            if (length.lo<=0) refuse("UNCERTAIN_MOTION_LENGTH");
            double speed=pressure ? policy.filament_speed.value() : event.speed_limit.value();
            double acceleration=pressure ? policy.filament_acceleration.value() : event.acceleration_limit.value();
            const auto limit=[&](double maximum,double ratio,double &value) {
                if (ratio>0 && (Interval(value)*Interval(ratio)).hi>maximum)
                    value=std::min(value,(Interval(maximum)/Interval(ratio)).lo);
            };
            if (!pressure) for (size_t axis=0;axis<3;++axis) {
                work();const double ratio=(absolute(delta[axis])/length).hi;
                limit(policy.axis_speed_mm_s[axis],ratio,speed);limit(policy.axis_acceleration_mm_s2[axis],ratio,acceleration);
            }
            const std::array<Interval,3> drive=policy.kinematics==LinearKinematics::CoreXY ?
                std::array<Interval,3>{delta[0]+delta[1],delta[0]-delta[1],delta[2]} : delta;
            if (!pressure) for (size_t axis=0;axis<3;++axis) {
                work();const double ratio=(absolute(drive[axis])/length).hi;
                limit(policy.drive_speed_mm_s[axis],ratio,speed);limit(policy.drive_acceleration_mm_s2[axis],ratio,acceleration);
            }
            const double e_ratio=(absolute(filament)/length).hi;
            limit(policy.filament_speed.value(),e_ratio,speed);limit(policy.filament_acceleration.value(),e_ratio,acceleration);
            if (volume.lo>0) {
                const auto section=command_volume/length;
                if (section.hi>policy.max_extrude_cross_section_mm2) refuse("EXTRUDE_CROSS_SECTION_LIMIT");
                limit(policy.max_volume_mm3_s,section.hi,speed);
            }
            speed=std::min(speed,(length*Interval(policy.max_events_per_second)).lo);
            speed=std::min(speed,detail::root(Interval(acceleration)*length).lo);
            speed=detail::down(speed);acceleration=detail::down(acceleration);
            // Reapply the triangular bound after rounding acceleration downward.
            speed=std::min(speed,detail::root(Interval(acceleration)*length).lo);
            if (!std::isfinite(speed) || !std::isfinite(acceleration) || speed<=0 || acceleration<=0) refuse("UNREPRESENTABLE_MOTION_LIMIT");
            const auto time=length/Interval(speed)+Interval(speed)/Interval(acceleration);
            if (time.lo<period.lo) refuse("MOTION_EVENT_RATE_LIMIT");
            if (!pressure) {row.motion.speed_limit=Speed(speed);row.motion.acceleration_limit=Acceleration(acceleration);}
            steps.push_back({pressure ? LinearStepCoordinate::Filament : LinearStepCoordinate::XYZ,bounds(length),bounds(filament),bounds(time),speed,acceleration});
            total_time=total_time+time;rows.push_back(std::move(row));
        }
        const auto remaining=[&] {
            stop();if (result.evaluations>=limits.max_evaluations) refuse("LINEAR_MOTION_WORK_LIMIT");
            return limits.max_evaluations-result.evaluations;
        };
        const auto time_left=[&] {stop();return limits.timeout-std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);};
        auto raw=std::make_shared<const MaterialSequenceSnapshot>(MaterialSequenceSnapshot{old.revision,old.source_fingerprint,old.model,std::move(rows),{}});
        MaterialMotionPreparationLimits prep;prep.max_records=limits.max_records;prep.max_evaluations=remaining();prep.timeout=time_left();
        prep.cancelled=[&] {stop();return false;};const auto material=prepare_material_motion({"",raw},prep);result.evaluations+=material.evaluations;stop();
        if (!material.snapshot) throw Refusal(material.reason);
        SimulationMotionPreparationLimits capture;capture.max_evaluations=remaining();capture.timeout=time_left();capture.cancelled=[&] {stop();return false;};
        const auto planned=prepare_simulation_motion(source->scene,material,source->policy,capture);result.evaluations+=planned.evaluations;stop();
        if (!planned.snapshot) throw Refusal(planned.reason);work();const auto identity=policy.fingerprint();stop();
        result.snapshot=std::shared_ptr<const LinearMotionPlanSnapshot>(new LinearMotionPlanSnapshot(source,planned.snapshot,policy,identity,std::move(steps),
            bounds(total_volume),bounds(total_filament),bounds(total_time),result.evaluations));stop();
        result.blocked_record.reset();result.status=ClearanceStatus::Pass;result.reason="COMPLETE_FULL_STOP_LINEAR_SIMULATION_MOTION_LIMITS_ONLY";
    } catch (const Refusal &e) {result.snapshot.reset();result.reason=e.what();}
      catch (const std::exception &e) {result.snapshot.reset();result.reason="LINEAR_MOTION_NUMERIC_FAILURE: "+std::string(e.what());}
    return result;
}
}
