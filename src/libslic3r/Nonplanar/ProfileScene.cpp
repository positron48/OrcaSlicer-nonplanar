#include "ProfileScene.hpp"
#include "Interval.hpp"
#include <array>
#include <set>
#include <limits>
#include <exception>

namespace Slic3r::nptop {
namespace {
template<Frame F> std::array<double,3> coordinates(const Position<F> &p) { return {p.x(),p.y(),p.z()}; }
template<class Box> bool valid_box(const Box &box)
{
    const auto lo=coordinates(box.min), hi=coordinates(box.max);
    for (size_t i=0;i<3;++i)
        if (lo[i] >= hi[i] || std::abs(lo[i]) > 10000 || std::abs(hi[i]) > 10000) return false;
    return true;
}
bool contains(const SceneBox &box, const PhysicalPosition &p)
{
    const auto lo=coordinates(box.min), hi=coordinates(box.max), point=coordinates(p);
    for (size_t i=0;i<3;++i) if (point[i] < lo[i] || point[i] > hi[i]) return false;
    return true;
}
// Minkowski sum of the entire convex nozzle domain and each fixed-axis outer
// envelope, inflated by declared uncertainty with outward-rounded arithmetic.
bool swept_box_covered(const SimulationScene &s, const ToolBox &box)
{
    const auto a=coordinates(s.nozzle_domain.min), b=coordinates(s.nozzle_domain.max);
    const auto lo=coordinates(box.min), hi=coordinates(box.max);
    const auto scene_lo=coordinates(s.scene_domain.min), scene_hi=coordinates(s.scene_domain.max);
    for (size_t i=0;i<3;++i) {
        const auto swept=detail::Interval(a[i],b[i])+detail::Interval(lo[i],hi[i])+
                         detail::Interval(-s.uncertainty.value(),s.uncertainty.value());
        if (swept.lo < scene_lo[i] || swept.hi > scene_hi[i]) return false;
    }
    return true;
}
}
namespace {
SceneApplicability scene_geometry(const SimulationScene &s)
{
    using Result=SceneApplicability;
    try {
        detail::require_interval_environment();
        if (s.version != simulation_scene_version) return Result::UnsupportedVersion;
        // No operator qualification implementation exists. Neither relabelling
        // provenance nor toggling confirmation upgrades a synthetic fixture.
        if (s.origin != ProfileOrigin::Synthetic || s.operator_confirmed_claim)
            return Result::UnsupportedQualification;
        if (!s.profile_id || !s.revision || !valid_box(s.nozzle_domain) || !valid_box(s.scene_domain) ||
            s.head.size()>64 || s.obstacles.size()>10000 ||
            s.uncertainty.value()>10000 || s.unmodelled_parts_min_local_z.value()>10000)
            return Result::InvalidInput;
        if (!s.obstacle_inventory_complete || s.tip.center.x()!=0 || s.tip.center.y()!=0 || s.tip.center.z()!=0 ||
            s.tip.opening_radius.value()<=0 || s.tip.outer_radius.value()<=s.tip.opening_radius.value())
            return Result::IncompleteGeometry;
        if (s.tip.outer_radius.value()>10000) return Result::InvalidInput;
        std::array<bool,6> parts{};
        std::set<uint64_t> ids;
        for (const auto &head : s.head) {
            const auto part=static_cast<unsigned>(head.part);
            if (!head.id || !ids.insert(head.id).second || part>=parts.size() || !valid_box(head.outer) ||
                head.outer.min.z()<0) return Result::InvalidInput;
            parts[part]=true;
            if (head.moving && !head.all_configurations_enclosed) return Result::IncompleteGeometry;
        }
        for (bool present : parts) if (!present) return Result::IncompleteGeometry;
        for (const auto &head : s.head)
            if (!swept_box_covered(s,head.outer)) return Result::OutsideCoverage;
        const double radius=s.tip.outer_radius.value();
        // Conservative square hull of the finite annulus, not the orifice.
        if (!swept_box_covered(s,{{-radius,-radius,0},{radius,radius,0}})) return Result::OutsideCoverage;
        const auto lowest_omitted=detail::Interval(s.nozzle_domain.min.z())+
                                  detail::Interval(s.unmodelled_parts_min_local_z.value())-
                                  detail::Interval(s.uncertainty.value());
        if (lowest_omitted.lo <= s.scene_domain.max.z()) return Result::OutsideCoverage;
        for (const auto &obstacle : s.obstacles) {
            if (!valid_box(obstacle)) return Result::InvalidInput;
            const auto lo=coordinates(obstacle.min), hi=coordinates(obstacle.max);
            const auto scene_lo=coordinates(s.scene_domain.min), scene_hi=coordinates(s.scene_domain.max);
            for (size_t i=0;i<3;++i) {
                const auto inflated=detail::Interval(lo[i],hi[i])+
                                    detail::Interval(-s.uncertainty.value(),s.uncertainty.value());
                if (inflated.lo<scene_lo[i] || inflated.hi>scene_hi[i]) return Result::OutsideCoverage;
            }
        }
        return Result::SimulationOnly;
    } catch (const std::invalid_argument &) { return Result::InvalidInput; }
      catch (const std::overflow_error &) { return Result::NumericalFailure; }
}
}
SceneApplicability assess_simulation_scene(const SimulationScene &s, const std::vector<MotionEvent> &events)
{
    using Result=SceneApplicability;
    try {
        const auto geometry=scene_geometry(s);if (geometry!=Result::SimulationOnly) return geometry;
        if (events.empty() || events.size()>10000) return Result::InvalidInput;
        std::set<uint64_t> ids;
        for (size_t i=0;i<events.size();++i) {
            const auto &event=events[i];
            validate_event(event);
            if (event.sequence_index!=i || !ids.insert(event.event_id).second ||
                (i && coordinates(events[i-1].end)!=coordinates(event.start))) return Result::InvalidInput;
            // Straight segments remain inside a convex box when both endpoints
            // do. This is coverage only; no endpoint-only collision claim.
            if (!contains(s.nozzle_domain,event.start) || !contains(s.nozzle_domain,event.end))
                return Result::OutsideCoverage;
        }
        return Result::SimulationOnly;
    } catch (const std::invalid_argument &) { return Result::InvalidInput; }
      catch (const std::overflow_error &) { return Result::NumericalFailure; }
}
SimulationTravelResult check_simulation_travel(const SimulationScene &source, const std::vector<MotionEvent> &source_events,
    const ClearancePolicy &source_policy, const SimulationTravelLimits &requested_limits)
{
    SimulationTravelResult result;
    const auto started=std::chrono::steady_clock::now();
    try {
        // Bound allocation before making the owned copies. The caller keeps
        // source state stable for capture; no callback has run at this point.
        if (source.head.size()>64 || source.obstacles.size()>10000 || source_events.size()>10000) {
            result.reason="INPUT_SIZE_LIMIT"; return result;
        }
        SimulationScene scene=source;
        const auto events=source_events;
        const auto limits=requested_limits;
        auto policy=source_policy;
        result.profile_id=scene.profile_id; result.revision=scene.revision;
        const auto stop=[&] {
            if (limits.cancelled && limits.cancelled()) { result.reason="CANCELLED"; return true; }
            if (limits.is_current && !limits.is_current(scene.profile_id,scene.revision)) {
                result.reason="STALE_REVISION"; return true;
            }
            detail::require_interval_environment();
            if (std::chrono::steady_clock::now()-started>=limits.timeout) { result.reason="DEADLINE"; return true; }
            return false;
        };
        if (limits.max_pairs>1000000 || limits.max_evaluations_per_pair>65535 ||
            limits.timeout.count()<=0 || limits.timeout.count()>60000) {
            result.reason="INVALID_LIMITS"; return result;
        }
        if (stop()) return result;
        const double scene_error=scene.uncertainty.value();
        detail::Interval coverage_error(scene_error);
        // A caller's full relative budget on each covered envelope is
        // conservative. Also keep the required-clearance neighborhood inside
        // the declared inventory; a positive pair gap alone cannot prove this.
        for (double term : {policy.numeric.total_mm(),policy.tool_measurement.value(),policy.positioning.value(),
                            policy.material.value(),policy.scene_geometry.value(),policy.required.value()})
            if (term!=0) coverage_error=coverage_error+detail::Interval(term);
        scene.uncertainty=Length(coverage_error.hi);
        result.applicability=assess_simulation_scene(scene,events);
        if (stop()) return result;
        if (result.applicability!=SceneApplicability::SimulationOnly) {
            result.reason="SCENE_NOT_APPLICABLE"; return result;
        }
        for (const auto &event : events)
            if (!std::holds_alternative<Travel>(event.payload)) {
                result.reason="UNSUPPORTED_MATERIAL_STATE"; return result;
            }
        const size_t component_count=scene.head.size()+1;
        if (scene.obstacles.size()>limits.max_pairs/component_count) { result.reason="PAIR_LIMIT"; return result; }
        const size_t pairs_per_event=scene.obstacles.size()*component_count;
        if (pairs_per_event && events.size()>limits.max_pairs/pairs_per_event) {
            result.reason="PAIR_LIMIT"; return result;
        }
        result.required_pairs=events.size()*pairs_per_event;
        std::set<uint64_t> ids;
        for (const auto &part : scene.head) ids.insert(part.id);
        result.tip_component_id=1;
        while (ids.count(result.tip_component_id)) ++result.tip_component_id;
        std::vector<ToolComponent> components{{result.tip_component_id,scene.tip}};
        for (const auto &part : scene.head) components.push_back({part.id,part.outer});
        // Coverage inflated both objects separately. Charge their combined
        // relative geometry error here without losing any caller budget term.
        if (scene_error!=0) {
            const detail::Interval error(scene_error);
            policy.scene_geometry=Length((detail::Interval(policy.scene_geometry.value())+error+error).hi);
        }
        const QueryLimits query_limits{limits.max_evaluations_per_pair,started+limits.timeout};
        for (const auto &event : events)
            for (const auto &component : components)
                for (size_t index=0; index<scene.obstacles.size(); ++index) {
                    if (stop()) return result;
                    auto check=query_clearance(event,component,{uint64_t(index)+1,scene.obstacles[index]},policy,query_limits);
                    ++result.checked_pairs;
                    if (stop()) return result;
                    if (check.status!=ClearanceStatus::Pass) {
                        result.limiting_check=std::move(check);
                        result.status=result.limiting_check->status;
                        result.reason="SCENE_CLEARANCE_BLOCKED";
                        return result;
                    }
                    if (!check.bounds) { result.reason="MISSING_PAIR_BOUND"; return result; }
                    if (!result.limiting_check || check.bounds->lower_mm<result.limiting_check->bounds->lower_mm)
                        result.limiting_check=std::move(check);
                }
        if (stop()) return result;
        result.status=ClearanceStatus::Pass;
        result.reason="SIMULATION_TRAVEL_ONLY";
    } catch (...) {
        result.status=ClearanceStatus::Unknown;
        result.reason="TRAVEL_CHECK_EXCEPTION";
    }
    return result;
}
namespace {
struct MotionRefusal : std::runtime_error {using std::runtime_error::runtime_error;};
void motion_refuse(const char *reason) {throw MotionRefusal(reason);}
template<class Limits> void motion_stop(const Limits &limits,const SimulationMotionSourceSnapshot &source,std::chrono::steady_clock::time_point started)
{
    if (limits.cancelled && limits.cancelled()) motion_refuse("CANCELLED");
    if (limits.is_current && !limits.is_current(source.material->ledger->revision)) motion_refuse("STALE_MATERIAL_REVISION");
    if (limits.is_scene_current && !limits.is_scene_current(source.scene.profile_id,source.scene.revision)) motion_refuse("STALE_SCENE_REVISION");
    detail::require_interval_environment();
    if (std::chrono::steady_clock::now()-started>=limits.timeout) motion_refuse("MOTION_DEADLINE");
}
}
SimulationMotionSourceResult prepare_simulation_motion(const SimulationScene &requested,const MaterialMotionSourceResult &requested_material,
    const ClearancePolicy &requested_policy,const SimulationMotionPreparationLimits &requested_limits)
{
    SimulationMotionSourceResult result;const auto started=std::chrono::steady_clock::now();
    try {
        if (requested.head.size()>64 || requested.obstacles.size()>10000) motion_refuse("MOTION_SOURCE_SIZE_LIMIT");
        const auto scene=requested;const auto material=requested_material.snapshot;const auto policy=requested_policy;const auto limits=requested_limits;
        if (!material || !material->ledger || !limits.max_evaluations || limits.max_evaluations>2000000 || limits.timeout.count()<=0 ||
            limits.timeout>std::chrono::seconds(30)) motion_refuse("INVALID_SIMULATION_MOTION_SOURCE");
        std::vector<ToolComponent> components;uint64_t tip=1;std::set<uint64_t> ids;
        for (const auto &head : scene.head) ids.insert(head.id);
        while (ids.count(tip)) ++tip;
        components.push_back({tip,scene.tip});for (const auto &head : scene.head) components.push_back({head.id,head.outer});
        auto static_policy=policy,material_policy=policy;
        const detail::Interval error(scene.uncertainty.value());
        static_policy.scene_geometry=Length((detail::Interval(policy.scene_geometry.value())+error+error).hi);
        material_policy.scene_geometry=Length((detail::Interval(policy.scene_geometry.value())+error).hi);
        auto owned=std::shared_ptr<const SimulationMotionSourceSnapshot>(new SimulationMotionSourceSnapshot(scene,material,policy,
            static_policy,material_policy,std::move(components),tip));
        const auto work=[&] {
            if (++result.evaluations>limits.max_evaluations) motion_refuse("MOTION_PREPARATION_WORK_LIMIT");
            motion_stop(limits,*owned,started);
        };
        work();
        if ((detail::Interval(policy.numeric.total_mm())+detail::Interval(material->ledger->model.numerical_coordinate_error.value())).hi>.05)
            motion_refuse("MOTION_NUMERIC_BUDGET");
        auto covered=scene;detail::Interval inflation(scene.uncertainty.value());
        for (double term : {policy.numeric.total_mm(),policy.tool_measurement.value(),policy.positioning.value(),policy.material.value(),
                           policy.scene_geometry.value(),policy.required.value()}) inflation=inflation+detail::Interval(term);
        covered.uncertainty=Length(inflation.hi);
        // Charge the bounded complete geometry/inventory walk before invoking
        // the shared coverage predicate. It cannot silently skip a head part.
        result.evaluations+=scene.head.size()+scene.obstacles.size();
        if (result.evaluations>limits.max_evaluations) motion_refuse("MOTION_PREPARATION_WORK_LIMIT");
        result.applicability=scene_geometry(covered);work();
        if (result.applicability!=SceneApplicability::SimulationOnly) motion_refuse("SCENE_NOT_APPLICABLE");
        if (material->full_upper_z_mm && material->full_upper_z_mm->upper>=scene.scene_domain.max.z()) {
            result.applicability=SceneApplicability::OutsideCoverage;motion_refuse("UPPER_MATERIAL_OUTSIDE_OMITTED_HEAD_COVERAGE");
        }
        for (const auto &row : material->ledger->records) {
            work();
            if (!contains(scene.nozzle_domain,row.motion.start) || !contains(scene.nozzle_domain,row.motion.end)) {
                result.applicability=SceneApplicability::OutsideCoverage;motion_refuse("ORIGINAL_LEDGER_OUTSIDE_NOZZLE_DOMAIN");
            }
        }
        work();result.snapshot=std::move(owned);result.reason="OWNED_SIMULATION_HEAD_SCENE_MATERIAL_ONLY";
    } catch (const MotionRefusal &e) {result.snapshot.reset();result.reason=e.what();}
      catch (const std::exception &e) {result.snapshot.reset();result.reason="SIMULATION_MOTION_CAPTURE_FAILURE: "+std::string(e.what());}
    return result;
}
SimulationMotionResult check_simulation_motion(const SimulationMotionSourceResult &requested,size_t index,const SimulationMotionLimits &requested_limits)
{
    const auto source=requested.snapshot;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();SimulationMotionResult result;result.source=source;result.event_index=index;
    try {
        if (!source || index>=source->material->ledger->records.size() || !limits.max_evaluations || limits.max_evaluations>2000000 ||
            limits.max_scene_pairs>1000000 || !limits.max_evaluations_per_scene_pair || limits.max_evaluations_per_scene_pair>65535 ||
            limits.timeout.count()<=0 || limits.timeout>std::chrono::seconds(30)) motion_refuse("INVALID_SIMULATION_MOTION_QUERY");
        const auto stop=[&] {motion_stop(limits,*source,started);};
        const auto work=[&] {
            if (++result.evaluations>limits.max_evaluations) motion_refuse("MOTION_WORK_LIMIT");stop();
        };
        work();const auto &scene=source->scene;
        if (scene.obstacles.size()>limits.max_scene_pairs/source->components.size()) motion_refuse("MOTION_SCENE_PAIR_LIMIT");
        const auto &event=source->material->ledger->records[index].motion;
        std::vector<ClearanceResult> checks;
        for (const auto &component : source->components) for (size_t obstacle=0;obstacle<scene.obstacles.size();++obstacle) {
            work();const size_t remaining=limits.max_evaluations-result.evaluations;
            if (!remaining) motion_refuse("MOTION_WORK_LIMIT");
            const QueryLimits query{std::min(remaining,limits.max_evaluations_per_scene_pair),started+limits.timeout};
            auto check=query_clearance(event,component,{uint64_t(obstacle)+1,scene.obstacles[obstacle]},source->static_policy,query);
            result.evaluations+=check.evaluations;++result.checked_scene_pairs;stop();
            if (check.status!=ClearanceStatus::Pass) {
                result.scene_check=std::move(check);result.status=result.scene_check->status;result.reason="STATIC_SCENE_CLEARANCE_BLOCKED";return result;
            }
            if (!check.bounds) motion_refuse("MISSING_STATIC_PAIR_BOUND");
            checks.push_back(std::move(check));
        }
        work();const size_t remaining=limits.max_evaluations-result.evaluations;
        if (!remaining) motion_refuse("MOTION_WORK_LIMIT");
        MaterialMotionLimits material_limits=limits;material_limits.max_evaluations=remaining;
        material_limits.cancelled=[&] {stop();return false;};material_limits.is_current={};
        auto material=verify_material_motion({"",source->material},index,source->components,source->material_policy,material_limits);
        result.evaluations+=material.evaluations;stop();
        if (material.status!=ClearanceStatus::Pass || !material.snapshot) {
            result.status=material.status==ClearanceStatus::Pass ? ClearanceStatus::Unknown : material.status;
            result.material_check=std::move(material);result.reason="DEPOSITED_MATERIAL_CLEARANCE_BLOCKED";return result;
        }
        stop();result.snapshot=std::shared_ptr<const SimulationMotionSnapshot>(new SimulationMotionSnapshot(source,material.snapshot,std::move(checks),result.evaluations));
        result.status=ClearanceStatus::Pass;result.reason="COMPLETE_SIMULATION_HEAD_SCENE_MATERIAL_ONLY";
    } catch (const MotionRefusal &e) {result.status=ClearanceStatus::Unknown;result.snapshot.reset();result.reason=e.what();}
      catch (const std::exception &e) {result.status=ClearanceStatus::Unknown;result.snapshot.reset();result.reason="SIMULATION_MOTION_NUMERIC_FAILURE: "+std::string(e.what());}
    return result;
}

SimulationLiftRouteResult plan_simulation_lifted_travel(const SimulationMotionSourceResult &requested,size_t index,double lift_z,
    const SimulationLiftRouteLimits &requested_limits)
{
    const auto source=requested.snapshot;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();SimulationLiftRouteResult result;result.source=source;
    try {
        if (!source || !limits.max_records || limits.max_records>200000 || !limits.max_evaluations || limits.max_evaluations>2000000 ||
            !limits.max_cells || limits.max_cells>1000000 || !limits.max_depth || limits.max_depth>64 || limits.max_scene_pairs>1000000 ||
            !limits.max_evaluations_per_scene_pair || limits.max_evaluations_per_scene_pair>65535 ||
            limits.timeout.count()<=0 || limits.timeout>std::chrono::seconds(30) || !std::isfinite(lift_z) || std::abs(lift_z)>10000)
            motion_refuse("INVALID_SIMULATION_LIFT_ROUTE");
        const auto &old=*source->material->ledger;
        if (index>=old.records.size() || old.records.size()>limits.max_records ||
            !std::holds_alternative<Travel>(old.records[index].motion.payload)) motion_refuse("LIFT_ROUTE_REQUIRES_ORIGINAL_TRAVEL");
        const auto stop=[&] {motion_stop(limits,*source,started);};
        const auto charge=[&] {if (++result.evaluations>limits.max_evaluations) motion_refuse("LIFT_ROUTE_WORK_LIMIT");stop();};charge();
        const auto remaining_time=[&] {
            stop();const auto time=limits.timeout-std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
            if (time.count()<=0) motion_refuse("MOTION_DEADLINE");return time;
        };
        const auto remaining_work=[&] {
            stop();if (result.evaluations>=limits.max_evaluations) motion_refuse("LIFT_ROUTE_WORK_LIMIT");
            return limits.max_evaluations-result.evaluations;
        };
        const auto &original=old.records[index].motion;
        if (lift_z<std::max(original.start.z(),original.end.z())) motion_refuse("LIFT_ROUTE_HEIGHT_BELOW_ENDPOINT");
        std::vector<PhysicalPosition> points{original.start};
        for (auto p : {PhysicalPosition{original.start.x(),original.start.y(),lift_z},
                       PhysicalPosition{original.end.x(),original.end.y(),lift_z},original.end}) {
            const auto &last=points.back();if (p.x()!=last.x() || p.y()!=last.y() || p.z()!=last.z()) points.push_back(p);
        }
        // Preserve an original zero-length Travel as an instant clearance
        // obligation rather than accepting an empty set of unchecked legs.
        if (points.size()==1) points.push_back(original.end);
        const size_t extra=points.size()-2;
        if (extra>limits.max_records-old.records.size()) motion_refuse("LIFT_ROUTE_RECORD_LIMIT");
        uint64_t id=0;for (const auto &row : old.records) {charge();id=std::max(id,row.motion.event_id);}
        if (id>std::numeric_limits<uint64_t>::max()-extra) motion_refuse("LIFT_ROUTE_EVENT_ID_LIMIT");
        std::vector<MaterialRecord> rows;std::vector<size_t> origins;
        const auto append=[&](MaterialRecord row,size_t origin) {
            charge();row.motion.sequence_index=rows.size();rows.push_back(std::move(row));origins.push_back(origin);
        };
        for (size_t i=0;i<old.records.size();++i) {
            if (i!=index) append(old.records[i],i);
            else for (size_t leg=0;leg+1<points.size();++leg) {
                auto row=old.records[i];row.motion.start=points[leg];row.motion.end=points[leg+1];
                if (leg) row.motion.event_id=++id;append(std::move(row),i);
            }
        }
        // The public aggregate is only owned raw input. The existing protected
        // preparation factory revalidates every record and rebuilds geometry;
        // no caller-supplied derived geometry is trusted or carried forward.
        auto raw=std::make_shared<const MaterialSequenceSnapshot>(MaterialSequenceSnapshot{old.revision,old.source_fingerprint,old.model,std::move(rows),{}});
        MaterialMotionPreparationLimits material_limits;material_limits.max_records=limits.max_records;
        material_limits.max_evaluations=remaining_work();material_limits.timeout=remaining_time();material_limits.cancelled=[&] {stop();return false;};
        const auto material=prepare_material_motion({"",raw},material_limits);result.evaluations+=material.evaluations;stop();
        if (!material.snapshot) throw MotionRefusal(material.reason);
        SimulationMotionPreparationLimits scene_limits;scene_limits.max_evaluations=remaining_work();scene_limits.timeout=remaining_time();
        scene_limits.cancelled=[&] {stop();return false;};
        const auto planned=prepare_simulation_motion(source->scene,material,source->policy,scene_limits);
        result.evaluations+=planned.evaluations;stop();if (!planned.snapshot) throw MotionRefusal(planned.reason);
        std::vector<std::shared_ptr<const SimulationMotionSnapshot>> checks;
        for (size_t leg=0;leg+1<points.size();++leg) {
            if (result.cells>=limits.max_cells) motion_refuse("LIFT_ROUTE_CELL_LIMIT");
            SimulationMotionLimits next=limits;next.max_evaluations=remaining_work();next.max_cells-=result.cells;
            next.max_scene_pairs-=result.checked_scene_pairs;next.timeout=remaining_time();next.cancelled=[&] {stop();return false;};
            next.is_current={};next.is_scene_current={};
            auto check=check_simulation_motion(planned,index+leg,next);result.evaluations+=check.evaluations;
            result.checked_scene_pairs+=check.checked_scene_pairs;
            if (check.snapshot) result.cells+=check.snapshot->material->cells;
            else if (check.material_check) result.cells+=check.material_check->cells;
            stop();
            if (check.status!=ClearanceStatus::Pass || !check.snapshot) {
                result.status=check.status==ClearanceStatus::Pass ? ClearanceStatus::Unknown : check.status;
                result.blocked_leg=leg;result.reason=check.reason;result.motion_check=std::move(check);return result;
            }
            checks.push_back(std::move(check.snapshot));
        }
        stop();result.snapshot=std::shared_ptr<const SimulationLiftRouteSnapshot>(new SimulationLiftRouteSnapshot(source,planned.snapshot,index,lift_z,
            std::move(origins),std::move(checks),result.cells,result.checked_scene_pairs,result.evaluations));stop();
        result.status=ClearanceStatus::Pass;result.reason="COMPLETE_SIMULATION_LIFT_TRANSFER_DESCENT_WITH_ORIGINAL_DEPOSITS_ONLY";
    } catch (const MotionRefusal &e) {result.status=ClearanceStatus::Unknown;result.snapshot.reset();result.reason=e.what();}
      catch (const std::exception &e) {result.status=ClearanceStatus::Unknown;result.snapshot.reset();result.reason="LIFT_ROUTE_NUMERIC_FAILURE: "+std::string(e.what());}
    return result;
}

SimulationCapDepartureResult plan_simulation_cap_departure(const FirstCapMaterialResult &requested_before,const NextCapBeadResult &requested_bead,
    const SimulationScene &requested_scene,const ClearancePolicy &requested_policy,const SimulationCapDepartureRequest &requested,
    const SimulationCapDepartureLimits &requested_limits)
{
    SimulationCapDepartureResult result;const auto started=std::chrono::steady_clock::now();
    try {
        if (requested_scene.head.size()>64 || requested_scene.obstacles.size()>10000) motion_refuse("DEPARTURE_SCENE_SIZE_LIMIT");
        const auto before=requested_before.snapshot;const auto bead=requested_bead.snapshot;
        const auto scene=requested_scene;const auto policy=requested_policy;const auto request=requested;const auto limits=requested_limits;
        result.before=before;result.bead=bead;
        if (!before || !bead || !bead->source || bead->source->source!=before || !bead->normal_spacing ||
            bead->normal_spacing->source!=bead->source || !limits.max_records || limits.max_records>200000 ||
            !limits.max_evaluations || limits.max_evaluations>2000000 || limits.timeout.count()<=0 || limits.timeout>std::chrono::seconds(30))
            motion_refuse("INVALID_CAP_DEPARTURE_SOURCE_OR_LIMITS");
        const auto revision=before->material->sequence->revision;
        std::exception_ptr stopped;
        const auto poll=[&] {
            if (stopped) std::rethrow_exception(stopped);
            try {
                if ((limits.cancelled && limits.cancelled()) || (limits.material.cancelled && limits.material.cancelled())) motion_refuse("CANCELLED");
                if ((limits.is_current && !limits.is_current(revision)) ||
                    (limits.material.is_current && !limits.material.is_current(revision))) motion_refuse("STALE_MATERIAL_REVISION");
                if (limits.is_scene_current && !limits.is_scene_current(scene.profile_id,scene.revision)) motion_refuse("STALE_SCENE_REVISION");
                detail::require_interval_environment();
                if (std::chrono::steady_clock::now()-started>=limits.timeout) motion_refuse("CAP_DEPARTURE_DEADLINE");
            } catch (...) {stopped=std::current_exception();throw;}
        };
        const auto time_left=[&](std::chrono::milliseconds original) {
            poll();if (original.count()<=0 || original>std::chrono::seconds(30)) motion_refuse("INVALID_CAP_DEPARTURE_NESTED_TIMEOUT");
            const auto left=limits.timeout-std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
            if (left.count()<=0) motion_refuse("CAP_DEPARTURE_DEADLINE");return std::min(original,left);
        };
        const auto work_left=[&] {
            poll();if (result.evaluations>=limits.max_evaluations) motion_refuse("CAP_DEPARTURE_WORK_LIMIT");
            return limits.max_evaluations-result.evaluations;
        };
        const auto charge=[&](size_t n) {if (n>work_left()) motion_refuse("CAP_DEPARTURE_WORK_LIMIT");result.evaluations+=n;poll();};
        poll();
        auto material_limits=limits.material;material_limits.max_records=std::min(material_limits.max_records,limits.max_records);
        material_limits.max_evaluations=std::min(material_limits.max_evaluations,work_left());material_limits.timeout=time_left(material_limits.timeout);
        material_limits.cancelled=[&] {poll();return false;};material_limits.is_current={};
        NextCapMaterialLimits append_limits;static_cast<FirstCapMaterialLimits &>(append_limits)=material_limits;
        const auto material=append_next_cap_material({"",before},{{"",bead}},{},0,append_limits);charge(material.evaluations);
        if (!material.snapshot) throw MotionRefusal(material.reason);
        const auto &old=*material.snapshot->material->sequence;
        if (old.records.size()>=material_limits.max_records) motion_refuse("CAP_DEPARTURE_RECORD_LIMIT");
        uint64_t id=0;std::vector<MaterialRecord> rows;rows.reserve(old.records.size()+1);
        for (const auto &row : old.records) {charge(1);id=std::max(id,row.motion.event_id);rows.push_back(row);}
        if (id==std::numeric_limits<uint64_t>::max()) motion_refuse("CAP_DEPARTURE_EVENT_ID_LIMIT");
        charge(1);const size_t index=rows.size();
        rows.push_back({{id+1,index,0,rows.back().motion.end,request.destination,request.travel_speed,request.travel_acceleration,Travel{}},{}});
        auto raw=std::make_shared<const MaterialSequenceSnapshot>(MaterialSequenceSnapshot{old.revision,old.source_fingerprint,old.model,std::move(rows),{}});
        MaterialMotionPreparationLimits preparation;preparation.max_records=material_limits.max_records;
        preparation.max_evaluations=work_left();preparation.timeout=time_left(material_limits.timeout);preparation.cancelled=[&] {poll();return false;};
        const auto motion=prepare_material_motion({"",raw},preparation);charge(motion.evaluations);
        if (!motion.snapshot) throw MotionRefusal(motion.reason);
        SimulationMotionPreparationLimits scene_limits;scene_limits.max_evaluations=work_left();scene_limits.timeout=time_left(scene_limits.timeout);
        scene_limits.cancelled=[&] {poll();return false;};
        const auto source=prepare_simulation_motion(scene,motion,policy,scene_limits);charge(source.evaluations);
        if (!source.snapshot) throw MotionRefusal(source.reason);
        auto route_limits=static_cast<const SimulationLiftRouteLimits &>(limits);route_limits.max_records=material_limits.max_records;
        route_limits.max_evaluations=work_left();route_limits.timeout=time_left(route_limits.timeout);
        route_limits.cancelled=[&] {poll();return false;};route_limits.is_current={};route_limits.is_scene_current={};
        auto route=plan_simulation_lifted_travel(source,index,request.lift_z_mm,route_limits);charge(route.evaluations);
        if (!route.snapshot || route.status!=ClearanceStatus::Pass) {
            result.status=route.status==ClearanceStatus::Pass ? ClearanceStatus::Unknown : route.status;
            result.reason=route.reason;result.route_check=std::move(route);return result;
        }
        poll();result.snapshot=std::shared_ptr<const SimulationCapDepartureSnapshot>(new SimulationCapDepartureSnapshot(before,material.snapshot,bead,
            route.snapshot,request,result.evaluations));poll();
        result.status=ClearanceStatus::Pass;result.reason="OWNED_NEXT_CAP_AND_COMPLETE_SIMULATION_DEPARTURE_ONLY";
    } catch (const MotionRefusal &e) {result.status=ClearanceStatus::Unknown;result.snapshot.reset();result.reason=e.what();}
      catch (const std::exception &e) {result.status=ClearanceStatus::Unknown;result.snapshot.reset();result.reason="CAP_DEPARTURE_FAILURE: "+std::string(e.what());}
      catch (...) {result.status=ClearanceStatus::Unknown;result.snapshot.reset();result.reason="CAP_DEPARTURE_CALLBACK_FAILURE";}
    return result;
}

}
