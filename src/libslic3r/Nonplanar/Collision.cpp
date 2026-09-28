#include "Collision.hpp"
#include "Interval.hpp"
#include <algorithm>
#include <array>
#include <cfenv>
#include <queue>
#include <vector>

namespace Slic3r::nptop {
namespace {

using namespace detail;
struct QueryFailure { ClearanceReason reason; };

template<Frame F> std::array<double,3> xyz(const Position<F> &p) { return {p.x(),p.y(),p.z()}; }
template<Frame F> void validate_position(const Position<F> &p)
{
    for (double coordinate : xyz(p))
        require(std::abs(coordinate) <= NativeScale::max_coordinate_mm, "collision coordinate outside domain");
}
template<class Box> void validate_box(const Box &box)
{
    validate_position(box.min);
    validate_position(box.max);
    const auto low = xyz(box.min), high = xyz(box.max);
    for (size_t i = 0; i < 3; ++i) require(low[i] < high[i], "empty or inverted box");
}
std::array<Interval,3> position(const MotionEvent &motion, Interval t)
{
    const auto start = xyz(motion.start), end = xyz(motion.end);
    std::array<Interval,3> out{Interval(0),Interval(0),Interval(0)};
    for (size_t i = 0; i < 3; ++i)
        out[i] = Interval(start[i]) + t * (Interval(end[i]) - Interval(start[i]));
    return out;
}
Interval tip_plane(const MotionEvent &motion, const FiniteTip &tip, const PlaneObstacle &plane, double t)
{
    auto p = position(motion, Interval(t));
    const auto offset = xyz(tip.center);
    for (size_t i = 0; i < 3; ++i) p[i] = p[i] + Interval(offset[i]);
    const Interval a(plane.gradient_x), b(plane.gradient_y);
    const Interval slope_squared = square(a) + square(b);
    return (p[2] - a*p[0] - b*p[1] - Interval(plane.intercept_mm) -
            Interval(tip.outer_radius.value()) * root(slope_squared)) / root(Interval(1) + slope_squared);
}
Interval box_box(const MotionEvent &motion, const ToolBox &tool, const SceneBox &scene, Interval t)
{
    const auto p = position(motion,t);
    const auto tool_min = xyz(tool.min), tool_max = xyz(tool.max);
    const auto scene_min = xyz(scene.min), scene_max = xyz(scene.max);
    Interval positive_squared(0), largest_gap(-4*NativeScale::max_coordinate_mm);
    for (size_t i = 0; i < 3; ++i) {
        const Interval gap = maximum(p[i] + Interval(tool_min[i]) - Interval(scene_max[i]),
                                     Interval(scene_min[i]) - p[i] - Interval(tool_max[i]));
        largest_gap = maximum(largest_gap,gap);
        positive_squared = positive_squared + square(maximum(gap,Interval(0)));
    }
    // Euclidean separation outside, negative minimum separating translation
    // inside. Interval evaluation encloses every t, including between samples.
    return root(positive_squared) + minimum(largest_gap,Interval(0));
}

} // namespace

ClearanceResult query_clearance(const MotionEvent &motion, const ToolComponent &tool, const SceneObstacle &scene,
                                const ClearancePolicy &policy, const QueryLimits &limits)
{
    ClearanceResult result{ClearanceStatus::Unknown, ClearanceReason::InvalidInput,
                           motion.event_id, motion.sequence_index, tool.id, scene.id,
                           tool.interaction, policy.required.value()};
    try {
        require_interval_environment();
        validate_event(motion);
        validate_position(motion.start);
        validate_position(motion.end);
        require(tool.id != 0 && scene.id != 0, "missing geometry identity");
        require(limits.max_evaluations <= 65535, "query work limit outside domain");
        const double uncertainty = (Interval(policy.numeric.total_mm()) + Interval(policy.tool_measurement.value()) +
                                    Interval(policy.positioning.value()) + Interval(policy.material.value())).hi;
        const auto *tip = std::get_if<FiniteTip>(&tool.geometry);
        const auto *box = std::get_if<ToolBox>(&tool.geometry);
        const auto *plane = std::get_if<PlaneObstacle>(&scene.geometry);
        const auto *wall = std::get_if<SceneBox>(&scene.geometry);
        if (tip) {
            validate_position(tip->center);
            require(tip->outer_radius.value() > tip->opening_radius.value() &&
                    tip->outer_radius.value() <= NativeScale::max_coordinate_mm, "invalid annular tip");
        }
        if (box) validate_box(*box);
        if (wall) validate_box(*wall);
        if (plane) require(std::isfinite(plane->gradient_x) && std::isfinite(plane->gradient_y) &&
                           std::isfinite(plane->intercept_mm), "nonfinite plane");
        if (tool.interaction != InteractionClass::RigidForbidden)
            throw QueryFailure{ClearanceReason::UnsupportedContact};
        if (!(tip && plane) && !(box && wall)) throw QueryFailure{ClearanceReason::UnsupportedPair};
        auto check_deadline = [&] {
            if (std::chrono::steady_clock::now() >= limits.deadline)
                throw QueryFailure{ClearanceReason::Timeout};
        };
        auto evaluation = [&] {
            check_deadline();
            if (result.evaluations >= limits.max_evaluations) throw QueryFailure{ClearanceReason::WorkLimit};
            ++result.evaluations;
        };
        auto bounds = [&](double lower, double upper) {
            const Interval expanded = Interval(lower,upper) + Interval(-uncertainty,uncertainty);
            result.bounds = ClearanceBounds{expanded.lo,expanded.hi,uncertainty};
        };
        auto witness = [&](double t) {
            // Display position only. All decisions use interval-valued position().
            const auto a = xyz(motion.start), b = xyz(motion.end);
            result.witness = ClearanceWitness{t, PhysicalPosition((1-t)*a[0]+t*b[0],
                                                                 (1-t)*a[1]+t*b[1], (1-t)*a[2]+t*b[2])};
        };
        if (tip && plane) {
            evaluation();
            const Interval first = tip_plane(motion,*tip,*plane,0);
            evaluation();
            const Interval last = tip_plane(motion,*tip,*plane,1);
            const Interval all = minimum(first,last);
            bounds(all.lo,all.hi);
            witness(first.hi <= last.hi ? 0 : 1);
            check_deadline();
            if (result.bounds->lower_mm > policy.required.value()) {
                result.status = ClearanceStatus::Pass;
                result.reason = ClearanceReason::Separated;
            } else if (result.bounds->upper_mm < policy.required.value()) {
                result.status = ClearanceStatus::Fail;
                result.reason = ClearanceReason::ClearanceViolation;
            } else result.reason = ClearanceReason::UncertainBoundary;
            return result;
        }

        evaluation();
        const Interval whole = box_box(motion,*box,*wall,Interval(0,1));
        bounds(whole.lo,whole.hi);
        struct Node { double begin, end, lower; };
        // Visit the least-clear bound first: a tangent subinterval must not
        // starve an already queued interval containing a definite collision.
        auto lower_first = [](const Node &a, const Node &b) { return a.lower > b.lower; };
        std::priority_queue<Node,std::vector<Node>,decltype(lower_first)> pending(lower_first);
        pending.push({0,1,whole.lo});
        double proven_lower = infinity, best_upper = infinity;
        const bool stationary = xyz(motion.start) == xyz(motion.end);
        while (!pending.empty()) {
            check_deadline();
            const Node node = pending.top();
            pending.pop();
            if (down(node.lower-uncertainty) > policy.required.value()) {
                proven_lower = std::min(proven_lower,node.lower);
                continue;
            }
            const double middle = node.begin + (node.end-node.begin)/2;
            evaluation();
            const Interval sample = box_box(motion,*box,*wall,Interval(middle));
            if (sample.hi <= best_upper) {
                best_upper = sample.hi;
                witness(middle);
            }
            bounds(whole.lo,best_upper);
            if (result.bounds->upper_mm < policy.required.value()) {
                check_deadline();
                result.status = ClearanceStatus::Fail;
                result.reason = ClearanceReason::ClearanceViolation;
                return result;
            }
            if (stationary || middle == node.begin || middle == node.end)
                throw QueryFailure{ClearanceReason::UncertainBoundary};
            evaluation();
            const Interval left = box_box(motion,*box,*wall,Interval(node.begin,middle));
            evaluation();
            const Interval right = box_box(motion,*box,*wall,Interval(middle,node.end));
            pending.push({middle,node.end,right.lo});
            pending.push({node.begin,middle,left.lo});
        }
        bounds(proven_lower,std::min(best_upper,whole.hi));
        check_deadline();
        result.status = ClearanceStatus::Pass;
        result.reason = ClearanceReason::Separated;
    } catch (const std::overflow_error &) {
        result.reason = ClearanceReason::NumericalFailure;
    } catch (const std::invalid_argument &) {
        result.reason = ClearanceReason::InvalidInput;
    } catch (const QueryFailure &failure) {
        result.reason = failure.reason;
    }
    return result;
}

} // namespace Slic3r::nptop
