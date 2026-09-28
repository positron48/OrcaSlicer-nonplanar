#include "Transition.hpp"
#include "Interval.hpp"
#include "../Flow.hpp"

namespace Slic3r::nptop {
namespace {
using namespace detail;
void coordinate(double value)
{
    require(std::isfinite(value) && std::abs(value) <= NativeScale::max_coordinate_mm,
            "transition coordinate outside domain");
}
void validate(const RectangleXY &rect)
{
    for (double value : {rect.min_x,rect.min_y,rect.max_x,rect.max_y}) coordinate(value);
    require(rect.min_x < rect.max_x && rect.min_y < rect.max_y, "empty transition footprint");
}
ScalarBounds bounds(Interval value) { return {value.lo,value.hi}; }
Interval interval(ScalarBounds value) { return {value.lower,value.upper}; }
}

SupportCoreResult reconstruct_planar_core(const ExtrusionPath &path, size_t segment, double layer_print_z,
    const PhysicalPosition &origin, const NativeScale &scale, uint64_t source_path_id,
    Length nozzle, Length xy_uncertainty, Length vertical_uncertainty)
{
    SupportCoreResult result;
    try {
        require_interval_environment();
        require(source_path_id != 0 && segment < path.polyline.size() &&
                path.polyline.size()-segment >= 2, "missing planar path identity or segment");
        require(nozzle.value() <= NativeScale::max_coordinate_mm && float(nozzle.value()) > 0,
                "nozzle outside native flow domain");
        coordinate(layer_print_z);
        coordinate(origin.x()); coordinate(origin.y()); coordinate(origin.z());
        require(std::isfinite(path.width) && std::isfinite(path.height) && std::isfinite(path.mm3_per_mm) &&
                path.height > 0 && path.width > path.height && path.width <= 10000 && path.mm3_per_mm > 0,
                "invalid planar bead dimensions");
        const Point3 &a = path.polyline.points[segment], &b = path.polyline.points[segment+1];
        const bool along_x = a.x() != b.x() && a.y() == b.y();
        const bool along_y = a.y() != b.y() && a.x() == b.x();
        if (path.z_contoured || path.is_force_no_extrusion() || !is_solid_infill(path.role()) ||
            is_bridge(path.role()) || a.z() != 0 || b.z() != 0 || (!along_x && !along_y) ||
            dynamic_cast<const ExtrusionPathSloped *>(&path) != nullptr ||
            dynamic_cast<const ExtrusionPathContoured *>(&path) != nullptr ||
            path.mm3_per_mm != Flow(path.width,path.height,float(nozzle.value())).mm3_per_mm()) {
            result.reason = TransitionReason::UnsupportedPath;
            return result;
        }
        const auto pa = from_native_scaled<Frame::ModelLocal>(a,scale);
        const auto pb = from_native_scaled<Frame::ModelLocal>(b,scale);
        const double error = (Interval(std::max(pa.error_mm,pb.error_mm)) + Interval(xy_uncertainty.value())).hi;
        const Interval x0 = Interval(pa.position.x()) + Interval(origin.x());
        const Interval x1 = Interval(pb.position.x()) + Interval(origin.x());
        const Interval y0 = Interval(pa.position.y()) + Interval(origin.y());
        const Interval y1 = Interval(pb.position.y()) + Interval(origin.y());
        const Interval half_core = (Interval(path.width)-Interval(path.height))/Interval(2);
        const Interval zero(0), erosion(error);
        const Interval extend_x = along_y ? half_core : zero;
        const Interval extend_y = along_x ? half_core : zero;
        RectangleXY footprint{
            (minimum(x0,x1)-extend_x+erosion).hi, (minimum(y0,y1)-extend_y+erosion).hi,
            (maximum(x0,x1)+extend_x-erosion).lo, (maximum(y0,y1)+extend_y-erosion).lo};
        if (footprint.min_x >= footprint.max_x || footprint.min_y >= footprint.max_y) {
            result.reason = TransitionReason::UnsupportedFootprint;
            return result;
        }
        validate(footprint);
        const Interval top = Interval(origin.z()) + Interval(layer_print_z);
        coordinate(top.lo); coordinate(top.hi);
        const Interval possible = top + Interval(-vertical_uncertainty.value(),vertical_uncertainty.value());
        coordinate(possible.lo); coordinate(possible.hi);
        result.core = PlanarSupportCore{footprint,bounds(top),bounds(possible),source_path_id};
        result.reason = TransitionReason::SupportedCell;
    } catch (const FlowError &) {
        result.reason = TransitionReason::NumericalFailure;
    } catch (const std::overflow_error &) {
        result.reason = TransitionReason::NumericalFailure;
    } catch (const std::invalid_argument &) {
        result.reason = TransitionReason::InvalidInput;
    }
    return result;
}

TransitionResult assess_first_pass(const AffineCapCell &cell, const PlanarSupportCore &support, const TransitionPolicy &policy)
{
    TransitionResult result;
    result.source_path_id = support.source_path_id;
    try {
        require_interval_environment();
        validate(cell.footprint); validate(support.lower_footprint);
        coordinate(cell.z00); coordinate(cell.z10); coordinate(cell.z01);
        require(support.source_path_id != 0 && policy.minimum.value() > 0 &&
                policy.minimum.value() < policy.maximum.value(), "invalid transition limits or support provenance");
        const Interval nominal_support = interval(support.nominal_top_mm);
        const Interval possible_support = interval(support.possible_top_mm);
        coordinate(possible_support.lo); coordinate(possible_support.hi);
        require(possible_support.lo <= nominal_support.lo && possible_support.hi >= nominal_support.hi,
                "support uncertainty does not contain nominal top");
        const auto &r = cell.footprint, &s = support.lower_footprint;
        if (r.min_x < s.min_x || r.min_y < s.min_y || r.max_x > s.max_x || r.max_y > s.max_y) {
            result.reason = TransitionReason::UnsupportedFootprint;
            return result;
        }
        const Interval error(-policy.corner_height_error.value(),policy.corner_height_error.value());
        const Interval h00 = Interval(cell.z00)+error, h10 = Interval(cell.z10)+error, h01 = Interval(cell.z01)+error;
        const std::array<Interval,4> corners{h00,h10,h01,h10+h01-h00};
        for (const Interval corner : corners) { coordinate(corner.lo); coordinate(corner.hi); }
        Interval all_gap = corners[0]-possible_support;
        bool too_small = false, too_large = false;
        for (const Interval corner : corners) {
            const Interval gap = corner-possible_support;
            all_gap = Interval(std::min(all_gap.lo,gap.lo),std::max(all_gap.hi,gap.hi));
            too_small |= gap.hi < policy.minimum.value();
            too_large |= gap.lo > policy.maximum.value();
        }
        result.gap_mm = bounds(all_gap);
        const Interval area = (Interval(r.max_x)-Interval(r.min_x))*(Interval(r.max_y)-Interval(r.min_y));
        const Interval nominal_volume = area*((Interval(cell.z10)+Interval(cell.z01))/Interval(2)-nominal_support);
        const Interval possible_volume = area*((h10+h01)/Interval(2)-possible_support);
        result.nominal_volume_mm3 = bounds(nominal_volume);
        result.possible_volume_mm3 = bounds(possible_volume);
        if (too_small || too_large) {
            result.status = TransitionStatus::Rejected;
            result.reason = too_small ? TransitionReason::GapTooSmall : TransitionReason::GapTooLarge;
        } else if (all_gap.lo > policy.minimum.value() && all_gap.hi < policy.maximum.value()) {
            result.status = TransitionStatus::Compatible;
            result.reason = TransitionReason::SupportedCell;
        } else result.reason = TransitionReason::UncertainGap;
    } catch (const std::overflow_error &) {
        result.reason = TransitionReason::NumericalFailure;
    } catch (const std::invalid_argument &) {
        result.reason = TransitionReason::InvalidInput;
    }
    return result;
}

} // namespace Slic3r::nptop
