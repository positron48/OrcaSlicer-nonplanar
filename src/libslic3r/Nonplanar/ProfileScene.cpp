#include "ProfileScene.hpp"
#include "Interval.hpp"
#include <array>
#include <set>

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
SceneApplicability assess_simulation_scene(const SimulationScene &s, const std::vector<MotionEvent> &events)
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
            events.empty() || events.size()>10000 || s.head.size()>64 || s.obstacles.size()>10000 ||
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
        ids.clear();
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
}
