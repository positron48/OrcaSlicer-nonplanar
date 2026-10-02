#include "GCodeAdapter.hpp"
#include "../GCodeWriter.hpp"
#include "StlImport.hpp"
#include "Canonical.hpp"
#include <cfenv>
#include <set>

namespace Slic3r::nptop {
std::string serialize_candidate(const std::vector<MotionEvent> &events,
                                Length diameter, FlowCompensation flow)
{
    require(!events.empty() && events.size() <= 10000, "candidate event limit");
    (void) filament_feed(Volume(1), diameter, flow);
    GCodeWriter writer;
    writer.config.use_relative_e_distances.value = true;
    writer.set_extruders({0});
    writer.set_extruder(0);
    // IR is already in machine physical coordinates. Never subtract plate
    // placement a second time. Identity firmware mapping is the spike domain.
    writer.set_xy_offset(0, 0);
    const auto xyz = [](const PhysicalPosition &p) { return Vec3d(p.x(), p.y(), p.z()); };
    writer.set_position(xyz(events.front().start));
    std::set<uint64_t> ids;
    std::string bytes = "G90\nM83\n";
    for (size_t i = 0; i < events.size(); ++i) {
        const auto &event = events[i];
        validate_event(event);
        require(event.sequence_index == i && ids.insert(event.event_id).second, "invalid event order or identity");
        require(xyz(event.start) == writer.get_position(), "discontinuous candidate");
        require(xyz(event.start).cwiseAbs().maxCoeff() <= 10000 &&
                xyz(event.end).cwiseAbs().maxCoeff() <= 10000, "candidate coordinate limit");
        require(!std::holds_alternative<Retraction>(event.payload), "retraction not implemented in spike");
        require(xyz(event.start) != xyz(event.end), "stationary candidate move");
        bool quantized_motion = false;
        for (int axis = 0; axis < 3; ++axis)
            quantized_motion |= GCodeG1Formatter::quantize_xyzf(xyz(event.start)[axis]) !=
                                GCodeG1Formatter::quantize_xyzf(xyz(event.end)[axis]);
        require(quantized_motion, "candidate movement collapses at export precision");
        const double feed = event.speed_limit.value() * 60;
        require(feed >= 0.001 && feed < 100000, "candidate feed outside writer domain");
        // Domain checks must also hold after native decimal formatting.
        // For example, 99999.9996 rounds to 100000, outside replay's F domain.
        require(GCodeG1Formatter::quantize_xyzf(feed) < 100000,
                "rounded candidate feed outside replay domain");
        double e = 0;
        if (const auto *bead = std::get_if<Deposition>(&event.payload)) {
            e = filament_feed(bead->volume, diameter, flow).value();
            require(e >= 0.00001 && e <= 10000, "candidate extrusion outside writer domain");
        }
        bytes += writer.set_speed(feed);
        // This low-level formatter accepts absolute XYZ without the GCode ZAA
        // branch. force_no_extrusion emits a straight G1 travel at our feed,
        // avoiding travel_to_xyz's implicit hop and alternate speed policy.
        bytes += writer.extrude_to_xyz(xyz(event.end), e, "", e == 0);
    }
    return bytes;
}
std::string LinearCandidatePolicy::fingerprint() const
{
    detail::CanonicalConfigWriter w;w.append("nptop-linear-candidate-policy-v1[");
    for (uint64_t id : {uint64_t(linear_candidate_version),profile_id,revision}) {w.append(std::to_string(id));w.append(",");}
    w.value(initial_acceleration.value());
    for (unsigned digits : {xyz_digits,e_digits,feed_digits,acceleration_digits,dwell_digits}) {w.append(",");w.append(std::to_string(digits));}
    w.append("]");return sha256_bytes(w.take());
}
LinearCandidateResult serialize_linear_candidate(const LinearMotionPlanResult &requested,const LinearCandidatePolicy &requested_policy,
    const LinearCandidateLimits &requested_limits)
{
    const auto plan=requested.snapshot;const auto policy=requested_policy;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();LinearCandidateResult result;result.plan=plan;
    try {
        require(plan && policy.profile_id && policy.revision && policy.initial_acceleration.value()>0 && policy.initial_acceleration.value()<=1000000,"INVALID_LINEAR_CANDIDATE");
        require(limits.max_records && limits.max_records<=200000 && limits.max_bytes && limits.max_bytes<=32*1024*1024 &&
                limits.max_evaluations && limits.max_evaluations<=2000000 && limits.timeout.count()>0 && limits.timeout<=std::chrono::seconds(30),"INVALID_CANDIDATE_LIMITS");
        for (unsigned digits : {policy.xyz_digits,policy.e_digits,policy.feed_digits,policy.acceleration_digits,policy.dwell_digits})
            require(digits>=1 && digits<=9,"UNSUPPORTED_CANDIDATE_PRECISION");
        const auto stop=[&] {
            require(!limits.cancelled || !limits.cancelled(),"CANCELLED");
            require(!limits.is_current || limits.is_current(plan->planned->material->ledger->revision),"STALE_MATERIAL_REVISION");
            require(!limits.is_scene_current || limits.is_scene_current(plan->planned->scene.profile_id,plan->planned->scene.revision),"STALE_SCENE_REVISION");
            require(!limits.is_policy_current || (limits.is_policy_current(plan->policy.profile_id,plan->policy.revision) &&
                limits.is_policy_current(policy.profile_id,policy.revision)),"STALE_CANDIDATE_POLICY");
            require(std::fegetround()==FE_TONEAREST,"UNSUPPORTED_CANDIDATE_ROUNDING");
            require(std::chrono::steady_clock::now()-started<limits.timeout,"CANDIDATE_DEADLINE");
        };
        result.evaluations=plan->evaluations;
        const auto work=[&] {require(++result.evaluations<=limits.max_evaluations,"CANDIDATE_WORK_LIMIT");stop();};work();
        const auto &rows=plan->planned->material->ledger->records;
        require(!rows.empty() && rows.size()==plan->steps.size() && rows.size()<=limits.max_records,"CANDIDATE_RECORD_LIMIT");
        double acceleration=policy.initial_acceleration.value();
        for (const auto &step : plan->steps) {work();if (step.coordinate!=LinearStepCoordinate::Dwell) acceleration=std::min(acceleration,step.acceleration_mm_s2);}
        const auto lower=[&](double value,unsigned digits) {
            const double scale=GCodeFormatter::pow_10[digits];
            return std::floor(std::nextafter(value*scale,0.))/scale;
        };
        acceleration=lower(acceleration,policy.acceleration_digits);
        require(std::isfinite(acceleration) && acceleration>0,"CANDIDATE_ACCELERATION_COLLAPSES");
        const auto quantize=[&](double value) {return GCodeFormatter::quantize(value,policy.xyz_digits);};
        const auto &first=rows.front().motion.start;
        require(std::max({std::abs(first.x()),std::abs(first.y()),std::abs(first.z())})<=10000,"CANDIDATE_XYZ_DOMAIN");
        const PhysicalPosition initial{quantize(first.x()),quantize(first.y()),quantize(first.z())};
        const double coordinate_error=std::nextafter(std::sqrt(3.)*(GCodeFormatter::pow_10_inv[policy.xyz_digits]/2 + 16*std::numeric_limits<double>::epsilon()*10000),std::numeric_limits<double>::infinity());
        plan->planned->policy.numeric.require_conversion(coordinate_error);
        std::string bytes;
        const auto append=[&](const std::string &part) {work();require(part.size()<=limits.max_bytes-bytes.size(),"CANDIDATE_BYTE_LIMIT");bytes+=part;};
        append("G90\nM83\nM400\n");
        GCodeFormatter accel;accel.emit_string("M204");accel.emit_axis('S',acceleration,policy.acceleration_digits);append(accel.string());
        std::vector<CandidateEventBytes> ranges;double pressure_debt=0;
        for (size_t i=0;i<rows.size();++i) {
            work();const auto begin=bytes.size();const auto &event=rows[i].motion;const auto &step=plan->steps[i];
            require(std::max({std::abs(event.end.x()),std::abs(event.end.y()),std::abs(event.end.z())})<=10000,"CANDIDATE_XYZ_DOMAIN");
            if (step.coordinate==LinearStepCoordinate::Dwell) {
                require(std::holds_alternative<Travel>(event.payload),"INVALID_DWELL_EVENT");
                const double scale=GCodeFormatter::pow_10[policy.dwell_digits];
                const double milliseconds=std::ceil(std::nextafter(step.duration_s.upper*1000*scale,std::numeric_limits<double>::infinity()))/scale;
                require(std::isfinite(milliseconds) && milliseconds>0 && milliseconds<=1000000,"CANDIDATE_DWELL_DOMAIN");
                GCodeFormatter dwell;dwell.emit_string("G4");dwell.emit_axis('P',milliseconds,policy.dwell_digits);append(dwell.string());
            } else {
                const double feed=lower(step.peak_speed_mm_s*60,policy.feed_digits);
                require(std::isfinite(feed) && feed>0 && feed<100000,"CANDIDATE_FEED_COLLAPSES_OR_OUTSIDE_DOMAIN");
                GCodeG1Formatter move;double extrusion=0;
                if (step.coordinate==LinearStepCoordinate::XYZ) {
                    require(quantize(event.start.x())!=quantize(event.end.x()) || quantize(event.start.y())!=quantize(event.end.y()) ||
                            quantize(event.start.z())!=quantize(event.end.z()),"CANDIDATE_XYZ_COLLAPSES");
                    move.emit_axis('X',event.end.x(),policy.xyz_digits);move.emit_axis('Y',event.end.y(),policy.xyz_digits);move.emit_axis('Z',event.end.z(),policy.xyz_digits);
                    if (const auto *deposit=std::get_if<Deposition>(&event.payload)) {
                        require(pressure_debt==0,"DEPOSITION_WHILE_RETRACTED");
                        extrusion=filament_feed(deposit->volume,plan->policy.filament_diameter,plan->policy.flow).value();
                    }
                } else {
                    const auto &pressure=std::get<Retraction>(event.payload);
                    if (pressure.after==RetractionState::Retracted) {require(pressure_debt==0,"NESTED_CANDIDATE_RETRACTION");pressure_debt=pressure.amount.value();extrusion=-pressure_debt;}
                    else {require(pressure_debt==pressure.amount.value(),"UNBALANCED_CANDIDATE_PRESSURE");extrusion=pressure_debt;pressure_debt=0;}
                }
                if (extrusion!=0) {
                    require(std::isfinite(extrusion) && std::abs(extrusion)<=10000 && GCodeFormatter::quantize(extrusion,policy.e_digits)!=0,"CANDIDATE_E_COLLAPSES_OR_OUTSIDE_DOMAIN");
                    move.emit_axis('E',extrusion,policy.e_digits);
                }
                move.emit_axis('F',feed,policy.feed_digits);append(move.string());
            }
            append("M400\n");ranges.push_back({begin,bytes.size()});
        }
        work();const auto hash=sha256_bytes(bytes);const auto identity=policy.fingerprint();stop();
        result.snapshot=std::shared_ptr<const LinearCandidateSnapshot>(new LinearCandidateSnapshot(plan,policy,initial,std::move(bytes),hash,identity,
            std::move(ranges),acceleration,coordinate_error,result.evaluations));stop();
        result.reason="OWNED_NATIVE_FULL_STOP_CANDIDATE_REQUIRES_FINAL_REPLAY";
    } catch (const std::exception &e) {result.snapshot.reset();result.reason=e.what();}
    return result;
}
namespace {
bool current_candidate(const LinearCandidateSnapshot &candidate,const LinearCandidateLimits &limits,
 std::chrono::steady_clock::time_point started,std::string &stopped)
{
    if(!stopped.empty())return false;
    try {
        if(limits.cancelled && limits.cancelled())stopped="CANCELLED";
        else if(limits.is_current && !limits.is_current(candidate.plan->planned->material->ledger->revision))stopped="STALE_MATERIAL_REVISION";
        else if(limits.is_scene_current && !limits.is_scene_current(candidate.plan->planned->scene.profile_id,candidate.plan->planned->scene.revision))stopped="STALE_SCENE_REVISION";
        else if(limits.is_policy_current && (!limits.is_policy_current(candidate.plan->policy.profile_id,candidate.plan->policy.revision) ||
            !limits.is_policy_current(candidate.policy.profile_id,candidate.policy.revision)))stopped="STALE_CANDIDATE_RATE_POLICY";
        else if(std::chrono::steady_clock::now()-started>=limits.timeout)stopped="CANDIDATE_RATE_DEADLINE";
        else if(std::fegetround()!=FE_TONEAREST)stopped="UNSUPPORTED_RATE_ROUNDING";
        else {
            volatile double normal=std::numeric_limits<double>::min(),subnormal=std::numeric_limits<double>::denorm_min();
            if(normal/2==0 || subnormal+subnormal==0)stopped="UNSUPPORTED_RATE_UNDERFLOW";
            else return true;
        }
    } catch(const std::exception &){stopped="SOURCE_RATE_CALLBACK_ERROR";}
    return false;
}
}
nptop_verify::LinearRateResult verify_linear_candidate_rates(const LinearCandidateResult &requested,const LinearCandidateLimits &requested_limits)
{
    const auto candidate=requested.snapshot;const auto limits=requested_limits;const auto started=std::chrono::steady_clock::now();
    nptop_verify::LinearRateResult result;
    if(!candidate){result.reason="MISSING_LINEAR_CANDIDATE";return result;}
    std::string stopped;
    const auto current=[&] {return current_candidate(*candidate,limits,started,stopped);};
    if(!current()){result.reason=stopped;return result;}
    const auto &source=candidate->plan->policy;nptop_verify::LinearRatePolicy policy;
    policy.version=source.version;policy.profile_id=source.profile_id;policy.revision=source.revision;
    policy.synthetic=source.origin==ProfileOrigin::Synthetic;policy.operator_confirmed_claim=source.operator_confirmed_claim;
    policy.model=source.model==LinearPlannerModel::FullStop ? nptop_verify::RateModel::FullStop : nptop_verify::RateModel::FirmwareLookahead;
    policy.kinematics=source.kinematics==LinearKinematics::CoreXY ? nptop_verify::RateKinematics::CoreXY : nptop_verify::RateKinematics::Cartesian;
    policy.position_min={source.commanded_domain.min.x(),source.commanded_domain.min.y(),source.commanded_domain.min.z()};
    policy.position_max={source.commanded_domain.max.x(),source.commanded_domain.max.y(),source.commanded_domain.max.z()};
    policy.axis_speed=source.axis_speed_mm_s;policy.axis_acceleration=source.axis_acceleration_mm_s2;
    policy.drive_speed=source.drive_speed_mm_s;policy.drive_acceleration=source.drive_acceleration_mm_s2;
    policy.initial_acceleration=candidate->policy.initial_acceleration.value();policy.filament_diameter=source.filament_diameter.value();policy.flow=source.flow.value();
    policy.filament_speed=source.filament_speed.value();policy.filament_acceleration=source.filament_acceleration.value();policy.max_retraction=source.max_retraction.value();
    policy.max_volume_rate=source.max_volume_mm3_s;policy.max_cross_section=source.max_extrude_cross_section_mm2;policy.max_event_rate=source.max_events_per_second;
    nptop_verify::LinearRateLimits query;query.max_bytes=limits.max_bytes;query.max_events=limits.max_records;query.max_evaluations=limits.max_evaluations;
    query.initial_evaluations=candidate->evaluations;query.timeout=limits.timeout;query.cancelled=[&] {return !current();};
    result=nptop_verify::verify_linear_rates(candidate->bytes,{candidate->initial_position.x(),candidate->initial_position.y(),candidate->initial_position.z()},policy,query);
    if(result.snapshot && result.snapshot->moves.size()!=candidate->events.size()){result.snapshot.reset();result.status=nptop_verify::RateStatus::Fail;result.reason="CANDIDATE_RATE_RECORD_MISMATCH";}
    if(!stopped.empty() || !current()){result.snapshot.reset();result.status=nptop_verify::RateStatus::Unknown;result.reason=stopped;}
    return result;
}

nptop_verify::LinearMaterialResult verify_linear_candidate_material(const LinearCandidateResult &requested,const LinearMaterialOptions &requested_options,
 const LinearCandidateLimits &requested_limits)
{
 const auto candidate=requested.snapshot;const auto options=requested_options;const auto limits=requested_limits;
 const auto started=std::chrono::steady_clock::now();nptop_verify::LinearMaterialResult result;std::string stopped;
 try {
  if(!candidate){result.reason="MISSING_LINEAR_CANDIDATE";return result;}
  const auto current=[&] {
   if(!current_candidate(*candidate,limits,started,stopped))return false;
   if(limits.is_policy_current && !limits.is_policy_current(options.policy_id,options.revision)){stopped="STALE_MATERIAL_REPLAY_POLICY";return false;}
   // A callback may change the arithmetic environment at publication.
   return current_candidate(*candidate,limits,started,stopped);
  };
  if(!current()){result.reason=stopped;return result;}
  LinearCandidateResult owned;owned.snapshot=candidate;
  const auto rates=verify_linear_candidate_rates(owned,limits);result.evaluations=rates.evaluations;
  if(!rates.snapshot){result.reason=rates.reason;result.status=rates.status;return result;}
  const auto &ledger=*candidate->plan->planned->material->ledger;const auto &model=ledger.model;
  nptop_verify::LinearMaterialPolicy p;p.model_id=model.model_id;p.policy_id=options.policy_id;p.revision=options.revision;
  p.source_revision=ledger.revision;p.source_fingerprint=ledger.source_fingerprint;
  p.outer_xy_growth_mm=model.outer_xy_growth.value();p.outer_z_growth_mm=model.outer_z_growth.value();
  p.inner_xy_loss_mm=model.inner_xy_loss.value();p.inner_z_loss_mm=model.inner_z_loss.value();p.numerical_coordinate_error_mm=model.numerical_coordinate_error.value();
  p.max_coordinate_delta_mm=candidate->coordinate_rounding_error_mm;p.max_nominal_delta_mm3=options.max_nominal_delta_mm3;
  p.max_total_nominal_delta_mm3=options.max_total_nominal_delta_mm3;p.max_filament_delta_mm=options.max_filament_delta_mm;
  p.relative_dose_error=options.relative_dose_error;p.absolute_dose_error_mm3=options.absolute_dose_error_mm3;
  std::vector<nptop_verify::MaterialDeclaration> rows;rows.reserve(ledger.records.size());
  for(size_t i=0;i<ledger.records.size();++i){
   if(++result.evaluations>limits.max_evaluations || !current()){result.reason=stopped.empty() ? "FINAL_MATERIAL_WORK_LIMIT" : stopped;return result;}
   const auto &r=ledger.records[i];const auto &m=r.motion;nptop_verify::MaterialDeclaration d{m.event_id,i,nptop_verify::MaterialEventKind::Travel,
    {m.start.x(),m.start.y(),m.start.z()},{m.end.x(),m.end.y(),m.end.z()},0,0,{}};
   if(const auto *deposit=std::get_if<Deposition>(&m.payload)){
    d.kind=nptop_verify::MaterialEventKind::Deposit;d.expected_nominal_volume_mm3=deposit->volume.value();
    d.section=nptop_verify::MaterialSection{r.bead->kind==BeadSectionKind::Rectangle ? nptop_verify::MaterialSectionKind::Rectangle : nptop_verify::MaterialSectionKind::RoundedRectangle,r.bead->gap_begin_mm,r.bead->gap_end_mm};
   }else if(const auto *pressure=std::get_if<Retraction>(&m.payload)){
    d.kind=pressure->after==RetractionState::Retracted ? nptop_verify::MaterialEventKind::Retraction : nptop_verify::MaterialEventKind::Restore;d.expected_filament_mm=pressure->amount.value();
   }else if(m.start.x()==m.end.x() && m.start.y()==m.end.y() && m.start.z()==m.end.z())d.kind=nptop_verify::MaterialEventKind::Dwell;
   rows.push_back(std::move(d));
  }
  nptop_verify::LinearMaterialLimits query;query.max_bytes=limits.max_bytes;query.max_events=limits.max_records;
  query.max_evaluations=limits.max_evaluations;query.initial_evaluations=result.evaluations;query.timeout=limits.timeout;query.cancelled=[&]{return !current();};
  result=nptop_verify::reconstruct_linear_material(rates.snapshot,rows,p,query);
  if(!stopped.empty() || !current()){result.snapshot.reset();result.status=nptop_verify::RateStatus::Unknown;result.reason=stopped;}
 }catch(const std::exception &e){result.snapshot.reset();result.status=nptop_verify::RateStatus::Unknown;result.reason=e.what();}
 return result;
}
}
