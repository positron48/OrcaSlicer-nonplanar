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
}
