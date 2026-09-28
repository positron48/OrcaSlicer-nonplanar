#include "Transition.hpp"
#include "Interval.hpp"
#include "../Flow.hpp"
#include "../Layer.hpp"

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

PlanarRegionResult capture_planar_region(const LayerRegion &region, Position<Frame::BuildPlate> origin,
                                        uint64_t revision, const PlanarRegionLimits &requested_limits)
{
    const PlanarRegionLimits limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    struct Rejection { const char *reason; };
    const auto reject=[](const char *reason) { throw Rejection{reason}; };
    const auto deadline=[&] {
        if (std::chrono::steady_clock::now()-started>=limits.timeout) reject("PLANAR_REGION_DEADLINE");
    };
    const auto stop=[&] {
        if (limits.cancelled && limits.cancelled()) reject("CANCELLED");
        if (limits.is_current && !limits.is_current(revision)) reject("STALE_REVISION");
        deadline(); require_interval_environment();
    };
    try {
        if (revision==0 || limits.max_entities<2 || limits.max_entities>20000 || limits.max_points<2 ||
            limits.max_points>200000 || limits.max_depth==0 || limits.max_depth>32 ||
            limits.timeout.count()<=0 || limits.timeout>std::chrono::seconds(30)) reject("INVALID_PLANAR_REGION_LIMITS");
        require_interval_environment();
        const auto scale=NativeScale::capture();
        coordinate(origin.x()); coordinate(origin.y()); coordinate(origin.z());
        const auto *layer=region.layer();
        if (!layer || layer->slicing_errors) reject("INVALID_NATIVE_LAYER");
        const size_t layer_id=layer->id();
        const double layer_z=layer->print_z, layer_height=layer->height;
        coordinate(layer_z); coordinate(layer_height);
        if (layer_height<=0) reject("INVALID_NATIVE_LAYER");
        std::vector<PlanarEntityRecord> entities{
            {0,PlanarEntityKind::Collection,region.perimeters.can_reverse(),region.perimeters.can_sort(),region.perimeters.inset_idx,{}},
            {0,PlanarEntityKind::Collection,region.fills.can_reverse(),region.fills.can_sort(),region.fills.inset_idx,{}}};
        std::vector<PlanarPathRecord> paths;
        size_t points=0;
        std::function<void(const ExtrusionEntity &,size_t,size_t)> capture;
        capture=[&](const ExtrusionEntity &entity,size_t parent,size_t depth) {
            deadline();
            if (entities.size()>=limits.max_entities || depth>limits.max_depth) reject("PLANAR_ENTITY_LIMIT");
            PlanarEntityKind kind;
            const auto &type=typeid(entity);
            if (type==typeid(ExtrusionEntityCollection)) kind=PlanarEntityKind::Collection;
            else if (type==typeid(ExtrusionLoop)) kind=PlanarEntityKind::Loop;
            else if (type==typeid(ExtrusionMultiPath)) kind=PlanarEntityKind::MultiPath;
            else if (type==typeid(ExtrusionPath) || type==typeid(ExtrusionPathOriented)) kind=PlanarEntityKind::Path;
            else throw Rejection{"UNSUPPORTED_PLANAR_ENTITY"};
            entities.push_back({parent,kind,entity.can_reverse(),entity.can_sort(),entity.inset_idx,{}});
            if (kind==PlanarEntityKind::Loop) {
                const auto role=static_cast<const ExtrusionLoop &>(entity).loop_role();
                if (unsigned(role)&~unsigned(elrHole|elrInternal)) reject("UNSUPPORTED_PLANAR_LOOP_ROLE");
                entities.back().loop_role=role;
            }
            const size_t id=entities.size();
            if (kind==PlanarEntityKind::Collection) {
                for (const auto *child : static_cast<const ExtrusionEntityCollection &>(entity).entities) {
                    if (!child) reject("INVALID_PLANAR_ENTITY");
                    capture(*child,id,depth+1);
                }
            } else if (kind==PlanarEntityKind::Loop || kind==PlanarEntityKind::MultiPath) {
                const auto &group=kind==PlanarEntityKind::Loop ? static_cast<const ExtrusionLoop &>(entity).paths :
                                                                static_cast<const ExtrusionMultiPath &>(entity).paths;
                if (group.empty()) reject("EMPTY_PLANAR_GROUP");
                for (size_t i=0; i<group.size(); ++i) {
                    capture(group[i],id,depth+1);
                    if (group[i].width!=group.front().width || group[i].height!=group.front().height ||
                        group[i].mm3_per_mm!=group.front().mm3_per_mm) reject("UNSUPPORTED_VARIABLE_WIDTH_GROUP");
                    if (i && group[i-1].polyline.points.back()!=group[i].polyline.points.front()) reject("DISCONNECTED_PLANAR_GROUP");
                }
                if (kind==PlanarEntityKind::Loop && group.back().polyline.points.back()!=group.front().polyline.points.front())
                    reject("OPEN_PLANAR_LOOP");
            } else {
                const auto &path=static_cast<const ExtrusionPath &>(entity);
                const auto role=path.role();
                if (role!=erPerimeter && role!=erExternalPerimeter && role!=erInternalInfill &&
                    role!=erSolidInfill && role!=erTopSolidInfill && role!=erBottomSurface) reject("UNSUPPORTED_PLANAR_ROLE");
                if (path.z_contoured || path.is_force_no_extrusion() || !path.polyline.fitting_result.empty())
                    reject("UNSUPPORTED_PLANAR_PATH");
                if (!std::isfinite(path.width) || !std::isfinite(path.height) || !std::isfinite(path.mm3_per_mm) ||
                    path.height<=0 || path.width<=path.height || path.width>10000 || path.mm3_per_mm<=0 ||
                    path.polyline.points.size()<2) reject("INVALID_PLANAR_PATH");
                if (path.polyline.points.size()>limits.max_points-points) reject("PLANAR_POINT_LIMIT");
                points+=path.polyline.points.size();
                paths.push_back({id,role,path.width,path.height,path.mm3_per_mm,path.polyline.points,{}});
            }
        };
        for (const auto *entity : region.perimeters.entities) {
            if (!entity) reject("INVALID_PLANAR_ENTITY");
            capture(*entity,1,1);
        }
        for (const auto *entity : region.fills.entities) {
            if (!entity) reject("INVALID_PLANAR_ENTITY");
            capture(*entity,2,1);
        }
        // The native region is no longer read. Callbacks may now invalidate or
        // destroy it; only bounded owned values enter arithmetic/publication.
        stop();
        if (paths.empty()) reject("EMPTY_PLANAR_REGION");
        Interval total_volume(0);
        for (auto &path : paths) {
            stop();
            Interval length(0);
            path.points.reserve(path.native_points.size());
            for (size_t i=0; i<path.native_points.size(); ++i) {
                if (i%128==0) stop();
                const auto &point=path.native_points[i];
                if (point.z()!=0) reject("NONPLANAR_NATIVE_POINT");
                const auto decoded=from_native_scaled<Frame::ModelLocal>(point,scale);
                const std::array<double,3> center{decoded.position.x()+origin.x(),decoded.position.y()+origin.y(),layer_z+origin.z()};
                const Interval decode_error(-decoded.error_mm,decoded.error_mm);
                const std::array<Interval,3> exact{
                    Interval(decoded.position.x())+Interval(origin.x())+decode_error,
                    Interval(decoded.position.y())+Interval(origin.y())+decode_error,
                    Interval(layer_z)+Interval(origin.z())};
                Interval error(0);
                for (size_t axis=0; axis<3; ++axis) {
                    coordinate(center[axis]); coordinate(exact[axis].lo); coordinate(exact[axis].hi);
                    const auto delta=exact[axis]-Interval(center[axis]);
                    error=error+Interval(std::max(std::abs(delta.lo),std::abs(delta.hi)));
                }
                path.coordinate_error_upper_mm=std::max(path.coordinate_error_upper_mm,error.hi);
                path.points.emplace_back(center[0],center[1],center[2]);
                if (i) {
                    const auto &previous=path.native_points[i-1];
                    if (point==previous) reject("ZERO_LENGTH_PLANAR_SEGMENT");
                    const Interval dx=Interval(double(point.x()-previous.x()))*Interval(scale.mm_per_unit());
                    const Interval dy=Interval(double(point.y()-previous.y()))*Interval(scale.mm_per_unit());
                    length=length+root(square(dx)+square(dy));
                }
            }
            const auto volume=length*Interval(path.mm3_per_mm);
            if (length.lo<=0 || volume.lo<=0) reject("UNRESOLVED_PLANAR_VOLUME");
            path.length_mm=bounds(length); path.native_volume_mm3=bounds(volume);
            total_volume=total_volume+volume;
        }
        stop();
        return {"NOMINAL_NATIVE_REGION_ONLY",std::make_shared<const PlanarRegionSnapshot>(PlanarRegionSnapshot{
            revision,layer_id,layer_height,layer_z,origin,scale,std::move(entities),std::move(paths),bounds(total_volume)})};
    } catch (const Rejection &rejected) { return {rejected.reason,{}}; }
    catch (const std::exception &) { return {"PLANAR_REGION_NUMERIC_OR_CAPTURE_FAILURE",{}}; }
}

} // namespace Slic3r::nptop
