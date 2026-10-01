#include "DepositionModel.hpp"
#include "Canonical.hpp"
#include "Interval.hpp"
#include "StlImport.hpp"
#include <set>
#include <map>
#include <tuple>
#include <CGAL/Gmpq.h>
#include <CGAL/number_utils.h>

namespace Slic3r::nptop {
namespace {
using detail::Interval;
using detail::square;
struct Rejection : std::runtime_error { using std::runtime_error::runtime_error; };
void reject(const char *reason) { throw Rejection(reason); }
ScalarBounds bounds(Interval v) { return {v.lo,v.hi}; }
Interval interval(ScalarBounds v) { return {v.lower,v.upper}; }
Interval absolute(Interval v) { return {v.lo<=0 && v.hi>=0 ? 0 : std::min(std::abs(v.lo),std::abs(v.hi)),std::max(std::abs(v.lo),std::abs(v.hi))}; }
Interval pi() { return {3.141592653589793,3.1415926535897936}; }
Interval length_squared(PhysicalPosition start, PhysicalPosition end)
{
    return square(Interval(end.x())-Interval(start.x()))+square(Interval(end.y())-Interval(start.y()));
}
Interval length_squared(const MaterialRecord &row) { return length_squared(row.motion.start,row.motion.end); }
Interval section_width(Interval area, Interval height, BeadSectionKind kind)
{
    return kind==BeadSectionKind::RoundedRectangle ? area/height+(Interval(1)-pi()/Interval(4))*height : area/height;
}
void coordinate(double v) { if (!std::isfinite(v) || std::abs(v)>NativeScale::max_coordinate_mm) reject("MATERIAL_COORDINATE_DOMAIN"); }
template<class Limits> void stop(const Limits &limits, uint64_t revision, std::chrono::steady_clock::time_point started)
{
    if (limits.cancelled && limits.cancelled()) reject("CANCELLED");
    if (limits.is_current && !limits.is_current(revision)) reject("STALE_REVISION");
    if (std::chrono::steady_clock::now()-started>=limits.timeout) reject("MATERIAL_DEADLINE");
    detail::require_interval_environment();
}
bool valid_timeout(std::chrono::milliseconds timeout) { return timeout.count()>0 && timeout<=std::chrono::seconds(30); }
std::string sequence_hash(const MaterialSequenceSnapshot &sequence, const std::function<void()> &poll)
{
    std::string hash=sha256_bytes(std::string("nptop-material-ledger-v1\0",25)+sequence.canonical_context());
    for (size_t i=0; i<sequence.records.size(); ++i) {
        if (i%128==0) poll();
        hash=sha256_bytes(std::string("nptop-material-record-v1\0",25)+hash+sequence.canonical_record(i));
    }
    return hash;
}
}
std::string MaterialSequenceSnapshot::canonical_context() const
{
    detail::CanonicalConfigWriter w;
    w.append("{\"model\":["); w.append(std::to_string(model.model_id));
    for (auto v : {model.outer_xy_growth,model.outer_z_growth,model.inner_xy_loss,model.inner_z_loss,model.numerical_coordinate_error}) {
        w.append(","); w.value(v.value());
    }
    w.append("],\"record_count\":"); w.append(std::to_string(records.size()));
    w.append(",\"revision\":"); w.append(std::to_string(revision));
    w.append(",\"schema\":1,\"source\":"); w.value(source_fingerprint); w.append("}"); return w.take();
}
std::string MaterialSequenceSnapshot::canonical_record(size_t index) const
{
    const auto &row=records.at(index); const auto &event=row.motion;
    detail::CanonicalConfigWriter w;
    const auto integer=[&](auto v) { w.append(std::to_string(v)); };
    const auto range=[&](ScalarBounds v) { w.append("["); w.value(v.lower); w.append(",");w.value(v.upper);w.append("]"); };
    const auto position=[&](PhysicalPosition p) { w.value(Vec3d(p.x(),p.y(),p.z())); };
    w.append("{\"bead\":");
    if (row.bead) {
        w.append("["); integer(int(row.bead->kind)); w.append(","); w.value(row.bead->gap_begin_mm);
        w.append(","); w.value(row.bead->gap_end_mm); w.append(","); range(row.bead->width_mm); w.append("]");
    } else w.append("null");
    w.append(",\"geometry\":");
    if (geometry.at(index)) {
        w.append("["); range(geometry[index]->xy_length_mm); w.append(",");range(geometry[index]->volume_mm3);w.append("]");
    } else w.append("null");
    w.append(",\"motion\":["); integer(event.event_id); w.append(","); integer(event.sequence_index);
    w.append(","); integer(event.source_patch_id); w.append(","); position(event.start); w.append(","); position(event.end);
    w.append(","); w.value(event.speed_limit.value()); w.append(","); w.value(event.acceleration_limit.value()); w.append(",[");
    if (const auto *bead=std::get_if<Deposition>(&event.payload)) {
        w.append("1");
        for (double v : {bead->volume.value(),bead->width.value(),bead->gap_min.value(),bead->gap_max.value()}) { w.append(",");w.value(v); }
        for (auto v : {bead->material.nominal.value(),bead->material.upper.value(),bead->material.lower.value(),
                        bead->support_provenance_id,bead->contact_model_id}) { w.append(",");integer(v); }
    } else if (const auto *pressure=std::get_if<Retraction>(&event.payload)) {
        w.append("2,"); w.value(pressure->amount.value()); w.append(",");integer(int(pressure->before)); w.append(",");integer(int(pressure->after));
    } else w.append("0");
    w.append("],"); integer(event.nominal_layer_label); w.append("]}"); return w.take();
}
std::string MaterialSequenceSnapshot::fingerprint() const { return sequence_hash(*this,[]{}); }

MaterialSequenceResult capture_material_sequence(const std::vector<MaterialRecord> &requested, const MaterialModel &requested_model,
    uint64_t revision, const std::string &requested_source, const MaterialLimits &requested_limits)
{
    const auto model=requested_model; const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!revision || !model.model_id || !limits.max_records || limits.max_records>200000 || !valid_timeout(limits.timeout) ||
            requested_source.size()!=64)
            reject("INVALID_MATERIAL_CONTEXT");
        const auto source=requested_source;
        if (!std::all_of(source.begin(),source.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}))
            reject("INVALID_MATERIAL_CONTEXT");
        for (auto v : {model.outer_xy_growth,model.outer_z_growth,model.inner_xy_loss,model.inner_z_loss})
            if (v.value()>NativeScale::max_coordinate_mm) reject("MATERIAL_MODEL_DOMAIN");
        if (model.numerical_coordinate_error.value()>0.05) reject("MATERIAL_NUMERIC_BUDGET");
        if (requested.empty() || requested.size()>limits.max_records) reject("MATERIAL_RECORD_LIMIT");
        auto records=requested; // No callback can change the captured sequence.
        std::vector<std::optional<DepositedBeadGeometry>> geometry; geometry.reserve(records.size());
        std::set<uint64_t> identities; RetractionState pressure_state=RetractionState::Ready; double retracted=0;
        for (size_t i=0; i<records.size(); ++i) {
            stop(limits,revision,started);
            const auto &row=records[i]; const auto &event=row.motion; validate_event(event);
            if (event.sequence_index!=i || !identities.insert(event.event_id).second) reject("MATERIAL_EVENT_ORDER");
            for (auto p : {event.start,event.end}) { coordinate(p.x());coordinate(p.y());coordinate(p.z()); }
            if (i) {
                const auto &p=records[i-1].motion.end;
                if (p.x()!=event.start.x() || p.y()!=event.start.y() || p.z()!=event.start.z()) reject("MATERIAL_POSITION_DISCONTINUITY");
            }
            if (const auto *bead=std::get_if<Deposition>(&event.payload)) {
                if (pressure_state!=RetractionState::Ready || !row.bead) reject("MATERIAL_DEPOSITION_STATE");
                const auto &b=*row.bead;
                if ((b.kind!=BeadSectionKind::Rectangle && b.kind!=BeadSectionKind::RoundedRectangle) ||
                    !std::isfinite(b.gap_begin_mm) || !std::isfinite(b.gap_end_mm) || b.gap_begin_mm<=0 || b.gap_end_mm<=0 ||
                    b.gap_begin_mm<bead->gap_min.value() || b.gap_end_mm<bead->gap_min.value() ||
                    b.gap_begin_mm>bead->gap_max.value() || b.gap_end_mm>bead->gap_max.value() ||
                    b.width_mm.lower<=0 || b.width_mm.lower>bead->width.value() || b.width_mm.upper<bead->width.value() ||
                    b.width_mm.upper>NativeScale::max_coordinate_mm || std::max(b.gap_begin_mm,b.gap_end_mm)>NativeScale::max_coordinate_mm)
                    reject("INVALID_MATERIAL_SECTION");
                const auto width=interval(b.width_mm);
                const auto length=detail::root(length_squared(row));
                if (length.lo<=0) reject("MATERIAL_REQUIRES_XY_LENGTH");
                const Interval amount(bead->volume.value());
                const auto area=amount/length;
                const Interval hmin(std::min(b.gap_begin_mm,b.gap_end_mm)), hmax(std::max(b.gap_begin_mm,b.gap_end_mm));
                if (b.kind==BeadSectionKind::RoundedRectangle && area.lo<=(pi()/Interval(4)*square(hmax)).hi)
                    reject("UNSUPPORTED_ROUNDED_SECTION");
                // G1 interpolates E linearly: area is constant, width changes
                // with the prescribed gap. In the supported rounded domain
                // width decreases monotonically with height.
                const auto minimum=section_width(area,hmax,b.kind), maximum=section_width(area,hmin,b.kind);
                if (minimum.lo<width.lo || maximum.hi>width.hi) reject("MATERIAL_WIDTH_RANGE");
                const auto midpoint=section_width(area,(Interval(b.gap_begin_mm)+Interval(b.gap_end_mm))/Interval(2),b.kind);
                if (bead->width.value()<midpoint.lo || bead->width.value()>midpoint.hi) reject("MATERIAL_VOLUME_MISMATCH");
                geometry.push_back(DepositedBeadGeometry{bounds(length),bounds(amount)});
            } else {
                if (row.bead) reject("MATERIAL_NONDEPOSITION_SECTION");
                if (const auto *pressure=std::get_if<Retraction>(&event.payload)) {
                    if (pressure->before!=pressure_state) reject("MATERIAL_PRESSURE_STATE");
                    if (pressure->after==RetractionState::Retracted) retracted=pressure->amount.value();
                    else if (retracted!=pressure->amount.value()) reject("MATERIAL_PRESSURE_AMOUNT");
                    pressure_state=pressure->after;
                }
                geometry.push_back({});
            }
        }
        auto snapshot=std::make_shared<const MaterialSequenceSnapshot>(MaterialSequenceSnapshot{revision,source,model,std::move(records),std::move(geometry)});
        sequence_hash(*snapshot,[&]{stop(limits,revision,started);});
        stop(limits,revision,started); return {"DECLARED_MATERIAL_MODEL_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) { return {e.what(),{}}; }
    catch (const std::exception &e) { return {"MATERIAL_CAPTURE_OR_NUMERIC_FAILURE: "+std::string(e.what()),{}}; }
}
std::string MaterialPrefixSnapshot::canonical() const
{
    detail::CanonicalConfigWriter w;
    w.append("{\"completed_records\":"); w.append(std::to_string(completed_records));
    w.append(",\"current_progress\":"); w.value(current_progress);
    w.append(",\"nominal_deposited_volume\":["); w.value(nominal_deposited_volume_mm3.lower);
    w.append(","); w.value(nominal_deposited_volume_mm3.upper);
    w.append("],\"schema\":1,\"sequence\":"); w.value(sequence->fingerprint()); w.append("}"); return w.take();
}
std::string MaterialPrefixSnapshot::fingerprint() const
{ return sha256_bytes(std::string("nptop-material-prefix-v1\0",25)+canonical()); }
MaterialAt material_at(std::shared_ptr<const MaterialSequenceSnapshot> sequence, size_t completed, double progress, const MaterialLimits &requested_limits)
{
    const auto limits=requested_limits; const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!sequence || !valid_timeout(limits.timeout) || !limits.max_records || limits.max_records>200000 ||
            sequence->records.size()>limits.max_records || sequence->geometry.size()!=sequence->records.size() ||
            completed>sequence->records.size() || !std::isfinite(progress) || progress<0 || progress>1 ||
            (completed==sequence->records.size() && progress!=0)) reject("INVALID_MATERIAL_PREFIX");
        stop(limits,sequence->revision,started); Interval total(0);
        for (size_t i=0; i<completed; ++i) {
            if (i%128==0) stop(limits,sequence->revision,started);
            if (sequence->geometry[i]) total=total+interval(sequence->geometry[i]->volume_mm3);
        }
        if (completed<sequence->records.size() && progress>0 && sequence->geometry[completed])
            total=total+interval(sequence->geometry[completed]->volume_mm3)*Interval(progress);
        auto snapshot=std::make_shared<const MaterialPrefixSnapshot>(MaterialPrefixSnapshot{sequence,completed,progress,{std::max(0.,total.lo),total.hi}});
        stop(limits,sequence->revision,started); return {"DECLARED_MATERIAL_PREFIX_ONLY",{snapshot},{snapshot},{snapshot}};
    } catch (const Rejection &e) { return {e.what(),{},{},{}}; }
    catch (const std::exception &e) { return {"MATERIAL_PREFIX_NUMERIC_FAILURE: "+std::string(e.what()),{},{},{}}; }
}
namespace {
using Representation=MaterialRepresentation;
struct Projection { Interval t, normal, z; };
Projection project(const MaterialRecord &row, Interval x, Interval y, Interval z)
{
    const auto &m=row.motion;
    const auto dx=Interval(m.end.x())-Interval(m.start.x()), dy=Interval(m.end.y())-Interval(m.start.y());
    const auto length2=square(dx)+square(dy);
    x=x-Interval(m.start.x()); y=y-Interval(m.start.y());
    return {(dx*x+dy*y)/length2,(dx*y-dy*x)/detail::root(length2),z};
}
MaterialMembership piece(const MaterialRecord &row, const MaterialModel &model, Projection projection, double progress, Representation rep,bool closed_nominal=false)
{
    const auto &b=*row.bead; const auto &m=row.motion;
    const auto length=detail::root(length_squared(row));
    const auto t=projection.t, n=absolute(projection.normal), point_z=projection.z;
    Interval xy(0), z(0), begin(0), end(progress);
    if (rep!=Representation::Nominal) {
        xy=Interval(rep==Representation::Upper ? model.outer_xy_growth.value() : model.inner_xy_loss.value())+Interval(model.numerical_coordinate_error.value());
        z=Interval(rep==Representation::Upper ? model.outer_z_growth.value() : model.inner_z_loss.value())+Interval(model.numerical_coordinate_error.value());
        const auto erosion=xy/length;
        if (rep==Representation::Upper) { begin=Interval(-erosion.hi);end=Interval((end+erosion).hi); }
        else { begin=Interval(erosion.hi);end=Interval((end-erosion).lo); }
    }
    if (begin.lo>=end.hi || t.hi<begin.lo || t.lo>end.hi) return MaterialMembership::Outside;
    const bool along_inside=closed_nominal && rep==Representation::Nominal ? t.lo>=begin.hi && t.hi<=end.lo : t.lo>begin.hi && t.hi<end.lo;
    const auto local=detail::maximum(Interval(0),detail::minimum(t,Interval(progress)));
    const auto dh=Interval(b.gap_end_mm)-Interval(b.gap_begin_mm), dz=Interval(m.end.z())-Interval(m.start.z());
    const auto h=Interval(b.gap_begin_mm)+dh*local, top=Interval(m.start.z())+dz*local;
    const auto shift=detail::minimum(xy/length,Interval(progress));
    const auto area=Interval(std::get<Deposition>(m.payload).volume.value())/length;
    const auto w=section_width(area,h,b.kind);
    const auto prefix_h=Interval(b.gap_begin_mm)+dh*Interval(progress);
    const auto hmin=detail::minimum(Interval(b.gap_begin_mm),prefix_h), hmax=detail::maximum(Interval(b.gap_begin_mm),prefix_h);
    const auto width_derivative=area/square(hmin)*absolute(dh);
    if (b.kind==BeadSectionKind::Rectangle) {
        const auto width_error=xy+width_derivative*shift/Interval(2);
        const auto half=rep==Representation::Upper ? w/Interval(2)+width_error : rep==Representation::Lower ? w/Interval(2)-width_error : w/Interval(2);
        const auto top_error=z+absolute(dz)*shift, bottom_error=z+absolute(dz-dh)*shift;
        const auto upper=rep==Representation::Upper ? top+top_error : rep==Representation::Lower ? top-top_error : top;
        const auto lower=rep==Representation::Upper ? top-h-bottom_error : rep==Representation::Lower ? top-h+bottom_error : top-h;
        if (half.hi<=0 || n.lo>half.hi || point_z.hi<lower.lo || point_z.lo>upper.hi) return MaterialMembership::Outside;
        if (along_inside && half.lo>0 && n.hi<half.lo && point_z.lo>lower.hi && point_z.hi<upper.lo) return MaterialMembership::Inside;
        return MaterialMembership::Unknown;
    }
    const auto center=top-h/Interval(2);
    const auto core_error=(area/square(hmin)+pi()/Interval(4))*absolute(dh)/Interval(2)*shift;
    const auto radius_error=xy+z+(absolute(dz-dh/Interval(2))+absolute(dh)/Interval(2))*shift;
    auto core=(w-h)/Interval(2), radius=h/Interval(2);
    if (rep==Representation::Upper) { core=core+core_error;radius=radius+radius_error; }
    else if (rep==Representation::Lower) {
        // If the eroded core cannot stay positive over the whole prefix, a
        // smaller disk still gives a guaranteed inner section.
        const auto min_core=(section_width(area,hmax,b.kind)-hmax)/Interval(2)-core_error;
        core=min_core.lo<=0 ? Interval(0) : core-core_error;
        const auto min_radius=hmin/Interval(2)-radius_error;
        if (min_radius.lo<=0) return MaterialMembership::Outside;
        radius=radius-radius_error;
    }
    if (core.lo<0 || radius.lo<=0) return MaterialMembership::Unknown;
    const auto transverse=detail::maximum(n-core,Interval(0));
    const auto distance2=square(transverse)+square(point_z-center), radius2=square(radius);
    if (distance2.lo>radius2.hi) return MaterialMembership::Outside;
    if (along_inside && distance2.hi<radius2.lo) return MaterialMembership::Inside;
    return MaterialMembership::Unknown;
}
MaterialQueryResult query(std::shared_ptr<const MaterialPrefixSnapshot> cursor, PhysicalPosition point, Representation rep,
                          const MaterialQueryLimits &requested_limits)
{
    const auto limits=requested_limits; const auto started=std::chrono::steady_clock::now(); MaterialQueryResult result;
    try {
        detail::require_interval_environment();
        if (!cursor || !cursor->sequence || !limits.max_evaluations || limits.max_evaluations>200000 || !valid_timeout(limits.timeout) ||
            cursor->completed_records>cursor->sequence->records.size() || !std::isfinite(cursor->current_progress) ||
            cursor->current_progress<0 || cursor->current_progress>1 ||
            (cursor->completed_records==cursor->sequence->records.size() && cursor->current_progress!=0)) reject("INVALID_MATERIAL_QUERY");
        coordinate(point.x());coordinate(point.y());coordinate(point.z());
        const auto sequence=cursor->sequence; stop(limits,sequence->revision,started); bool uncertain=false;
        const size_t end=cursor->completed_records+(cursor->current_progress>0 && cursor->completed_records<sequence->records.size());
        for (size_t i=0; i<end; ++i) {
            if (i%128==0) stop(limits,sequence->revision,started);
            const auto &row=sequence->records[i]; if (!row.bead) continue;
            if (result.evaluations>=limits.max_evaluations) reject("MATERIAL_QUERY_WORK_LIMIT");
            ++result.evaluations;
            const auto membership=piece(row,sequence->model,project(row,Interval(point.x()),Interval(point.y()),Interval(point.z())),i<cursor->completed_records ? 1 : cursor->current_progress,rep);
            if (membership==MaterialMembership::Inside) {
                stop(limits,sequence->revision,started); result.membership=membership;result.reason="INSIDE_DECLARED_MATERIAL";
                result.source_event_id=row.motion.event_id;return result;
            }
            uncertain|=membership==MaterialMembership::Unknown;
        }
        stop(limits,sequence->revision,started);
        result.membership=uncertain ? MaterialMembership::Unknown : MaterialMembership::Outside;
        result.reason=uncertain ? "MATERIAL_BOUNDARY_UNCERTAIN" : "OUTSIDE_DECLARED_MATERIAL";
    } catch (const Rejection &e) { result.membership=MaterialMembership::Unknown;result.reason=e.what(); }
    catch (const std::exception &e) { result.membership=MaterialMembership::Unknown;result.reason="MATERIAL_QUERY_NUMERIC_FAILURE: "+std::string(e.what()); }
    return result;
}
}
MaterialQueryResult classify_material(const NominalMaterialView &v, const PhysicalPosition &p, const MaterialQueryLimits &l)
{ return query(v.snapshot,p,Representation::Nominal,l); }
MaterialQueryResult classify_material(const UpperMaterialView &v, const PhysicalPosition &p, const MaterialQueryLimits &l)
{ return query(v.snapshot,p,Representation::Upper,l); }
MaterialQueryResult classify_material(const LowerMaterialView &v, const PhysicalPosition &p, const MaterialQueryLimits &l)
{ return query(v.snapshot,p,Representation::Lower,l); }

namespace {
// Eager rationals keep each bounded split independent of a deferred expression
// history and its later recursive evaluation. Binary64 inputs remain exact.
using Exact=CGAL::Gmpq;
using Vertex=std::array<Exact,2>;
using Polygon=std::vector<Vertex>;
Interval exact_interval(const Exact &value)
{
    const auto range=CGAL::to_interval(value); detail::require_interval_environment();
    return {range.first,range.second};
}
std::pair<double,double> stored_exact(const Exact &value)
{
    const auto range=exact_interval(value);const double rounded=(range.lo+range.hi)/2;coordinate(rounded);
    const auto error=range-Interval(rounded);
    return {rounded,std::max(std::abs(error.lo),std::abs(error.hi))};
}
Projection project_polygon(const MaterialRecord &row, const Polygon &polygon, Interval z)
{
    auto projection=project(row,exact_interval(polygon.front()[0]),exact_interval(polygon.front()[1]),z);
    for (size_t i=1; i<polygon.size(); ++i) {
        const auto next=project(row,exact_interval(polygon[i][0]),exact_interval(polygon[i][1]),z);
        projection.t={std::min(projection.t.lo,next.t.lo),std::max(projection.t.hi,next.t.hi)};
        projection.normal={std::min(projection.normal.lo,next.normal.lo),std::max(projection.normal.hi,next.normal.hi)};
    }
    return projection;
}
Polygon clip(const Polygon &polygon, const Vertex &normal, const Exact &offset, bool positive)
{
    Polygon result;
    const auto distance=[&](const Vertex &p) { return normal[0]*p[0]+normal[1]*p[1]-offset; };
    const auto append=[&](const Vertex &p) { if (result.empty() || result.back()!=p) result.push_back(p); };
    auto previous=polygon.back(); auto previous_distance=distance(previous);
    bool previous_inside=positive ? previous_distance>=0 : previous_distance<=0;
    for (const auto &current : polygon) {
        const auto current_distance=distance(current);
        const bool current_inside=positive ? current_distance>=0 : current_distance<=0;
        if (current_inside!=previous_inside) {
            const auto fraction=previous_distance/(previous_distance-current_distance);
            append({previous[0]+fraction*(current[0]-previous[0]),previous[1]+fraction*(current[1]-previous[1])});
        }
        if (current_inside) append(current);
        previous=current; previous_distance=current_distance; previous_inside=current_inside;
    }
    if (result.size()>1 && result.front()==result.back()) result.pop_back();
    if (result.size()>64) reject("MATERIAL_COVERAGE_VERTEX_LIMIT");
    return result;
}
struct RoofProjection { Projection projected; Interval top_growth; Polygon polygon; };
std::optional<RoofProjection> roof_projection(const MaterialRecord &row, const MaterialModel &model,
                                             Polygon polygon, double progress, Representation rep)
{
    // Enclose precisely the section model used by piece(). Exact clipping to
    // its oriented XY enclosure removes irrelevant neighboring walls before
    // evaluating height, retaining longitudinal/transverse correlation.
    const auto &m=row.motion; const auto &b=*row.bead;
    const auto length=detail::root(length_squared(row));
    const auto area=Interval(std::get<Deposition>(m.payload).volume.value())/length;
    const auto dh=Interval(b.gap_end_mm)-Interval(b.gap_begin_mm), dz=Interval(m.end.z())-Interval(m.start.z());
    const auto prefix_h=Interval(b.gap_begin_mm)+dh*Interval(progress);
    const auto hmin=detail::minimum(Interval(b.gap_begin_mm),prefix_h);
    Interval begin(0), end(progress), half=section_width(area,hmin,b.kind)/Interval(2), top_growth(0);
    if (rep==Representation::Upper) {
        const auto xy=Interval(model.outer_xy_growth.value())+Interval(model.numerical_coordinate_error.value());
        const auto z=Interval(model.outer_z_growth.value())+Interval(model.numerical_coordinate_error.value());
        const auto erosion=xy/length, shift=detail::minimum(erosion,Interval(progress));
        begin=Interval(-erosion.hi); end=Interval((end+erosion).hi);
        if (b.kind==BeadSectionKind::Rectangle) {
            half=half+xy+area/square(hmin)*absolute(dh)*shift/Interval(2);
            top_growth=z+absolute(dz)*shift;
        } else {
            const auto core_error=(area/square(hmin)+pi()/Interval(4))*absolute(dh)/Interval(2)*shift;
            const auto radius_error=xy+z+(absolute(dz-dh/Interval(2))+absolute(dh)/Interval(2))*shift;
            half=half+core_error+radius_error;
            top_growth=radius_error;
        }
    }
    const Vertex delta{Exact(m.end.x())-Exact(m.start.x()),Exact(m.end.y())-Exact(m.start.y())};
    const Exact length2=delta[0]*delta[0]+delta[1]*delta[1];
    const Exact start=delta[0]*Exact(m.start.x())+delta[1]*Exact(m.start.y());
    polygon=clip(polygon,delta,start+Exact(begin.lo)*length2,true);
    if (polygon.empty()) return {};
    polygon=clip(polygon,delta,start+Exact(end.hi)*length2,false);
    if (polygon.empty()) return {};
    const Vertex normal{-delta[1],delta[0]};
    const Exact center=normal[0]*Exact(m.start.x())+normal[1]*Exact(m.start.y());
    const Exact transverse=Exact(half.hi)*Exact(length.hi);
    polygon=clip(polygon,normal,center-transverse,true);
    if (polygon.empty()) return {};
    polygon=clip(polygon,normal,center+transverse,false);
    if (polygon.empty()) return {};
    const auto projected=project_polygon(row,polygon,Interval(0));
    return RoofProjection{projected,top_growth,std::move(polygon)};
}
std::optional<double> roof_ceiling(const MaterialRecord &row, const MaterialModel &model,
                                   Polygon polygon, double progress, Representation rep)
{
    const auto projected=roof_projection(row,model,std::move(polygon),progress,rep);
    if (!projected) return {};
    const auto local=detail::maximum(Interval(0),detail::minimum(projected->projected.t,Interval(progress)));
    const auto top=Interval(row.motion.start.z())+(Interval(row.motion.end.z())-Interval(row.motion.start.z()))*local+projected->top_growth;
    coordinate(top.lo); coordinate(top.hi);
    return top.hi;
}
struct NominalRoof { Interval height, depth_below_top; bool whole_footprint, whole_transverse; };
std::optional<NominalRoof> nominal_roof(const MaterialRecord &row, Projection projection, double progress,
                                        std::optional<Interval> cached_area={})
{
    const auto &b=*row.bead; const auto &m=row.motion;
    const auto local=detail::maximum(Interval(0),detail::minimum(projection.t,Interval(progress)));
    const auto height=Interval(b.gap_begin_mm)+(Interval(b.gap_end_mm)-Interval(b.gap_begin_mm))*local;
    const auto top=Interval(m.start.z())+(Interval(m.end.z())-Interval(m.start.z()))*local;
    const auto area=cached_area ? *cached_area : Interval(std::get<Deposition>(m.payload).volume.value())/detail::root(length_squared(row));
    const auto width=section_width(area,height,b.kind), normal=absolute(projection.normal);
    const bool transverse_whole=normal.hi<(width/Interval(2)).lo;
    const bool whole=projection.t.lo>0 && projection.t.hi<progress && transverse_whole;
    if (b.kind==BeadSectionKind::Rectangle) return NominalRoof{top,Interval(0),whole,transverse_whole};
    const auto core=(width-height)/Interval(2), radius=height/Interval(2);
    const auto transverse=detail::maximum(normal-core,Interval(0));
    const auto radicand=square(radius)-square(transverse);
    if (radicand.hi<0) return {};
    // The nonnegative root encloses every real cross-section in the projected
    // cell. A lower roof is usable only if that bead covers the entire XY cell.
    const auto shoulder=detail::root(radicand);
    auto depth=height/Interval(2)-shoulder;
    if (transverse.hi==0) depth=Interval(0);
    else if (transverse_whole && area.lo>(pi()/Interval(4)*square(height)).hi) {
        // In the admitted rounded-section domain, depth below the flat top
        // increases with |normal| and h at fixed area: u'=A/(2h²)+pi/8>1/2.
        // Evaluate correlated endpoint heights, retaining outward A/pi bounds.
        const auto at=[&](double h,double n) {
            const auto radius=Interval(h)/Interval(2),core=(section_width(area,Interval(h),b.kind)-Interval(h))/Interval(2);
            return radius-detail::root(square(radius)-square(detail::maximum(Interval(n)-core,Interval(0))));
        };
        depth={std::max(0.,at(height.lo,normal.lo).lo),at(height.hi,normal.hi).hi};
    }
    // A stadium shoulder has r - sqrt(r*r - u*u) >= 0. Uncoupled
    // height/radicand intervals must not invent material above its axis top.
    depth=detail::maximum(Interval(0),depth);
    return NominalRoof{top-depth,depth,whole,transverse_whole};
}
// Shared whole-cell nominal roof enclosure. The floor must already be a
// certified lower bound for the target domain. Only a wholly covering bead may
// raise that lower bound; clipped possible footprints supply upper bounds.
struct RoofCellBounds {Interval height;std::vector<size_t> active;size_t splitter;};
template<class Evaluate>
RoofCellBounds nominal_roof_bounds(const MaterialPrefixSnapshot &cursor,const Polygon &polygon,
    const std::vector<size_t> &candidates,double floor,const Evaluate &evaluate)
{
    const auto &sequence=*cursor.sequence;double lower=floor,upper=floor;
    size_t splitter=sequence.records.size();std::vector<size_t> active;
    for (size_t i : candidates) {
        evaluate();const auto &row=sequence.records[i];const double progress=i<cursor.completed_records ? 1 : cursor.current_progress;
        const auto end=Interval(row.motion.start.z())+(Interval(row.motion.end.z())-Interval(row.motion.start.z()))*Interval(progress);
        if (std::max(row.motion.start.z(),end.hi)<floor) continue;
        const auto projected=project_polygon(row,polygon,Interval(0));
        const bool interior_flat=row.motion.start.z()==row.motion.end.z() && row.bead->gap_begin_mm==row.bead->gap_end_mm &&
            projected.t.lo>0 && projected.t.hi<progress;
        // With constant Z/h and the whole cell between the finite butts, the
        // roof depends only on normal distance. Its full polygon projection
        // supplies the same possible height without exact XY clipping.
        std::optional<NominalRoof> possible;
        if (interior_flat) {
            const auto area=Interval(std::get<Deposition>(row.motion.payload).volume.value())/detail::root(length_squared(row));
            if (absolute(projected.normal).lo>(section_width(area,Interval(row.bead->gap_begin_mm),row.bead->kind)/Interval(2)).hi) continue;
            possible=nominal_roof(row,projected,progress,area);
        }
        else {
            const auto clipped=roof_projection(row,sequence.model,polygon,progress,Representation::Nominal);
            if (clipped) possible=nominal_roof(row,clipped->projected,progress);
        }
        if (!possible || possible->height.hi<floor) continue;
        active.push_back(i);
        if (splitter==sequence.records.size() || possible->height.hi>upper) splitter=i;
        upper=std::max(upper,possible->height.hi);
        const auto guaranteed=interior_flat ? possible : nominal_roof(row,projected,progress);
        if (guaranteed && guaranteed->whole_footprint) lower=std::max(lower,guaranteed->height.lo);
    }
    return {Interval(lower,upper),std::move(active),splitter};
}
std::pair<Interval,Interval> affine_integral(const Polygon &polygon, const AffineCapCell &cell, Exact *exact_area=nullptr)
{
    const bool flat=cell.z00==cell.z10 && cell.z00==cell.z01;
    Exact twice_area(0), x_moment(0), y_moment(0);
    for (size_t i=0; i<polygon.size(); ++i) {
        const auto &a=polygon[i], &b=polygon[(i+1)%polygon.size()];
        const Exact cross=a[0]*b[1]-b[0]*a[1];
        twice_area+=cross;
        if (!flat) {x_moment+=(a[0]+b[0])*cross;y_moment+=(a[1]+b[1])*cross;}
    }
    if (twice_area==0 && exact_area) { *exact_area=Exact(0);return {Interval(0),Interval(0)}; }
    if (twice_area<=0) reject("MATERIAL_INTEGRAL_DEGENERATE_CELL");
    if (flat) {
        const Exact area=twice_area/Exact(2);if (exact_area) *exact_area=area;
        return {exact_interval(area),exact_interval(area*Exact(cell.z00))};
    }
    const Exact x=x_moment/(Exact(3)*twice_area), y=y_moment/(Exact(3)*twice_area);
    const auto &r=cell.footprint;
    const Exact mean=Exact(cell.z00)+(Exact(cell.z10)-Exact(cell.z00))*(x-Exact(r.min_x))/(Exact(r.max_x)-Exact(r.min_x))+
        (Exact(cell.z01)-Exact(cell.z00))*(y-Exact(r.min_y))/(Exact(r.max_y)-Exact(r.min_y));
    const Exact area=twice_area/Exact(2);
    if (exact_area) *exact_area=area;
    return {exact_interval(area),exact_interval(area*mean)};
}
MaterialCoverageResult coverage(std::shared_ptr<const MaterialPrefixSnapshot> cursor, const SceneBox &requested,
                               Representation rep, const MaterialCoverageLimits &requested_limits)
{
    const auto domain=requested; const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now(); MaterialCoverageResult result; result.source=cursor;
    result.domain=domain; result.representation=rep;
    try {
        detail::require_interval_environment();
        if (!cursor || !cursor->sequence || cursor->sequence->records.size()>200000 ||
            cursor->sequence->geometry.size()!=cursor->sequence->records.size() ||
            cursor->completed_records>cursor->sequence->records.size() || !std::isfinite(cursor->current_progress) ||
            cursor->current_progress<0 || cursor->current_progress>1 ||
            (cursor->completed_records==cursor->sequence->records.size() && cursor->current_progress!=0) ||
            !limits.max_evaluations || limits.max_evaluations>200000 || !limits.max_cells || limits.max_cells>65535 ||
            !limits.max_depth || limits.max_depth>32 || !valid_timeout(limits.timeout) ||
            domain.min.x()>domain.max.x() || domain.min.y()>domain.max.y() || domain.min.z()>domain.max.z())
            reject("INVALID_MATERIAL_COVERAGE");
        for (const auto &p : {domain.min,domain.max}) { coordinate(p.x());coordinate(p.y());coordinate(p.z()); }
        const auto sequence=cursor->sequence;
        const auto poll=[&] { stop(limits,sequence->revision,started); };
        poll();
        const size_t end=cursor->completed_records+(cursor->current_progress>0 && cursor->completed_records<sequence->records.size());
        std::vector<size_t> active;
        for (size_t i=0; i<end; ++i) if (sequence->records[i].bead) {
            if (!sequence->geometry[i] || !std::holds_alternative<Deposition>(sequence->records[i].motion.payload))
                reject("INVALID_MATERIAL_COVERAGE_GEOMETRY");
            active.push_back(i);
        }
        const auto fraction=[&](size_t i) { return i<cursor->completed_records ? 1 : cursor->current_progress; };
        const auto evaluate=[&](size_t i, Projection projection) {
            if (result.evaluations>=limits.max_evaluations) reject("MATERIAL_COVERAGE_WORK_LIMIT");
            if (result.evaluations%128==0) poll(); ++result.evaluations;
            return piece(sequence->records[i],sequence->model,projection,fraction(i),rep);
        };
        Polygon initial{{Exact(domain.min.x()),Exact(domain.min.y())},{Exact(domain.max.x()),Exact(domain.min.y())},
                        {Exact(domain.max.x()),Exact(domain.max.y())},{Exact(domain.min.x()),Exact(domain.max.y())}};
        struct Node { Polygon polygon; Interval z; std::vector<size_t> candidates; size_t depth; };
        std::vector<Node> stack; stack.push_back({std::move(initial),Interval(domain.min.z(),domain.max.z()),active,0});
        bool unresolved=false;
        while (!stack.empty()) {
            poll(); if (result.cells>=limits.max_cells) reject("MATERIAL_COVERAGE_CELL_LIMIT"); ++result.cells;
            auto node=std::move(stack.back()); stack.pop_back();
            bool covered=false; std::vector<size_t> uncertain;
            for (size_t i : node.candidates) {
                const auto membership=evaluate(i,project_polygon(sequence->records[i],node.polygon,node.z));
                if (membership==MaterialMembership::Inside) { covered=true;break; }
                if (membership==MaterialMembership::Unknown) uncertain.push_back(i);
            }
            if (covered) continue;
            Exact x(0), y(0);
            for (const auto &p : node.polygon) { x+=p[0];y+=p[1]; }
            const auto bx=exact_interval(x/Exact(node.polygon.size())), by=exact_interval(y/Exact(node.polygon.size()));
            const PhysicalPosition witness((bx.lo+bx.hi)/2,(by.lo+by.hi)/2,(node.z.lo+node.z.hi)/2);
            // A certified interior counterexample may reject immediately;
            // samples are never used to accept continuous coverage.
            bool interior=true;
            const Vertex exact_witness{Exact(witness.x()),Exact(witness.y())};
            auto xmin=node.polygon.front()[0], xmax=xmin, ymin=node.polygon.front()[1], ymax=ymin;
            for (size_t j=0; j<node.polygon.size(); ++j) {
                const auto &a=node.polygon[j], &b=node.polygon[(j+1)%node.polygon.size()];
                if ((b[0]-a[0])*(exact_witness[1]-a[1])-(b[1]-a[1])*(exact_witness[0]-a[0])<0) interior=false;
                xmin=std::min(xmin,a[0]); xmax=std::max(xmax,a[0]); ymin=std::min(ymin,a[1]); ymax=std::max(ymax,a[1]);
            }
            interior=interior && exact_witness[0]>=xmin && exact_witness[0]<=xmax && exact_witness[1]>=ymin && exact_witness[1]<=ymax;
            bool outside=interior;
            if (interior) for (size_t i : uncertain)
                if (evaluate(i,project(sequence->records[i],Interval(witness.x()),Interval(witness.y()),Interval(witness.z())))!=MaterialMembership::Outside) {
                    outside=false;break;
                }
            if (outside) {
                poll(); result.status=MaterialCoverageStatus::Uncovered;result.reason="UNCOVERED_DECLARED_MATERIAL";
                result.witness=witness; return result;
            }
            if (uncertain.empty()) reject("MATERIAL_COVERAGE_WITNESS_ROUNDING");
            if (node.depth>=limits.max_depth) { unresolved=true;continue; }
            // Split parallel to a long relevant bead; aligned strips avoid a
            // tiny axis-aligned grid at every diagonal overlap. This changes
            // proof cells only, never the motion order or material geometry.
            size_t splitter=uncertain.front();
            for (size_t i : uncertain)
                if (sequence->geometry[i]->xy_length_mm.upper>sequence->geometry[splitter]->xy_length_mm.upper) splitter=i;
            const auto &motion=sequence->records[splitter].motion;
            const Exact dx=Exact(motion.end.x())-Exact(motion.start.x()), dy=Exact(motion.end.y())-Exact(motion.start.y());
            Vertex normal{-dy,dx};
            const auto projection=project_polygon(sequence->records[splitter],node.polygon,node.z);
            if ((projection.t.lo<=0 || projection.t.hi>=fraction(splitter)) &&
                projection.normal.hi-projection.normal.lo<sequence->records[splitter].bead->width_mm.lower/2)
                normal={dx,dy};
            auto minimum=normal[0]*node.polygon.front()[0]+normal[1]*node.polygon.front()[1], maximum=minimum;
            for (const auto &p : node.polygon) {
                const auto value=normal[0]*p[0]+normal[1]*p[1];
                if (value<minimum) minimum=value; if (value>maximum) maximum=value;
            }
            if (minimum==maximum) {
                if (node.z.lo==node.z.hi) { unresolved=true;continue; }
                const double middle=(node.z.lo+node.z.hi)/2;
                if (middle<=node.z.lo || middle>=node.z.hi) { unresolved=true;continue; }
                stack.push_back({node.polygon,Interval(middle,node.z.hi),uncertain,node.depth+1});
                stack.push_back({std::move(node.polygon),Interval(node.z.lo,middle),std::move(uncertain),node.depth+1});
            } else {
                const Exact middle=(minimum+maximum)/Exact(2);
                auto first=clip(node.polygon,normal,middle,false), second=clip(node.polygon,normal,middle,true);
                if (first.empty() || second.empty()) reject("MATERIAL_COVERAGE_INVALID_SPLIT");
                stack.push_back({std::move(second),node.z,uncertain,node.depth+1});
                stack.push_back({std::move(first),node.z,std::move(uncertain),node.depth+1});
            }
        }
        poll();
        if (unresolved) result.reason="MATERIAL_COVERAGE_UNCERTAIN_BOUNDARY";
        else { result.status=MaterialCoverageStatus::Covered;result.reason="CONTINUOUS_DECLARED_MATERIAL_COVERAGE"; }
    } catch (const Rejection &e) { result.reason=e.what(); }
    catch (const std::exception &e) { result.reason="MATERIAL_COVERAGE_NUMERIC_FAILURE: "+std::string(e.what()); }
    return result;
}
}
MaterialCoverageResult cover_material(const NominalMaterialView &v, const SceneBox &d, const MaterialCoverageLimits &l)
{ return coverage(v.snapshot,d,Representation::Nominal,l); }
MaterialCoverageResult cover_material(const UpperMaterialView &v, const SceneBox &d, const MaterialCoverageLimits &l)
{ return coverage(v.snapshot,d,Representation::Upper,l); }
MaterialCoverageResult cover_material(const LowerMaterialView &v, const SceneBox &d, const MaterialCoverageLimits &l)
{ return coverage(v.snapshot,d,Representation::Lower,l); }

// The proof is constructible only by the continuous integrator. Its exact
// closed leaf partition and nominal roof bounds remain owned and immutable;
// caller-edited result fields cannot replace the source or geometry.
class MaterialIntegralProof {
    friend FirstCapInterfaceResult assess_first_cap_interface(const FirstCapResult &,const FirstCapInterfacePolicy &,const FirstCapInterfaceLimits &);
    struct Cell { Polygon polygon; Interval roof; };
    const std::shared_ptr<const MaterialPrefixSnapshot> source;
    const AffineCapCell target;
    const ScalarBounds total_volume;
    const std::vector<Cell> cells;
    MaterialIntegralProof(std::shared_ptr<const MaterialPrefixSnapshot> s, AffineCapCell t, ScalarBounds v, std::vector<Cell> c)
        : source(std::move(s)), target(t), total_volume(v), cells(std::move(c)) {}
    friend MaterialIntegralResult integrate_material_first_pass(const LowerMaterialView &, const AffineCapCell &,
        double, const TransitionPolicy &, const MaterialIntegralLimits &);
    friend IntegralStripsResult split_material_integral(const MaterialIntegralResult &, IntegralSplitAxis,
        const std::vector<double> &, const MaterialIntegralLimits &);
    friend AffineHatchCellsResult allocate_affine_hatch_cells(const AffineHatchResult &, const MaterialIntegralLimits &);
    friend MaterialFillResult reconcile_material_fill(const MaterialIntegralResult &, const MaterialUnionResult &, const MaterialFillLimits &);
    friend MaterialVoidResult classify_material_voids(const MaterialFillResult &,const MaterialFillLimits &);
    friend MaterialDeficitResult locate_material_deficit(const MaterialFillResult &,const std::vector<double> &,
        const std::vector<double> &,const MaterialDeficitLimits &);
    friend RemainingHatchResult plan_remaining_first_hatch(const AffineHatchResult &,size_t,const MaterialFillResult &,
        const RemainingHatchPolicy &,const RemainingHatchLimits &);
    ScalarBounds rectangle_volume(const RectangleXY &r, const MaterialIntegralLimits &limits,
        size_t &fragments, size_t &evaluations, const std::function<void()> &poll, const char *reason_prefix) const
    {
        const auto fail=[&](const char *suffix) { throw Rejection(std::string(reason_prefix)+suffix); };
        Exact covered_area(0),lower(0),upper(0);
        for (const auto &leaf : cells) {
            poll(); if (evaluations>=limits.max_evaluations) fail("_WORK_LIMIT"); ++evaluations;
            auto polygon=leaf.polygon;
            for (const auto &plane : {std::pair<Vertex,Exact>{{Exact(1),Exact(0)},Exact(r.min_x)},
                    {{Exact(-1),Exact(0)},-Exact(r.max_x)},{{Exact(0),Exact(1)},Exact(r.min_y)},
                    {{Exact(0),Exact(-1)},-Exact(r.max_y)}}) {
                polygon=clip(polygon,plane.first,plane.second,true);
                if (polygon.size()<3) break;
            }
            if (polygon.size()<3) continue;
            Exact area(0); const auto integral=affine_integral(polygon,target,&area);
            if (area==0) continue;
            if (fragments>=limits.max_cells) fail("_CELL_LIMIT"); ++fragments;
            covered_area+=area;
            const auto volume=integral.second-integral.first*leaf.roof;
            lower+=Exact(volume.lo); upper+=Exact(volume.hi);
        }
        const Exact area=(Exact(r.max_x)-Exact(r.min_x))*(Exact(r.max_y)-Exact(r.min_y));
        if (covered_area!=area) fail("_DOMAIN_MISMATCH");
        const auto volume=Interval(exact_interval(lower).lo,exact_interval(upper).hi);
        if (volume.lo<=0) fail("_UNCERTAIN_POSITIVE_VOLUME");
        return bounds(volume);
    }
};

MaterialTransitionResult assess_material_first_pass(const LowerMaterialView &view, const AffineCapCell &requested_cell,
    double plane, const TransitionPolicy &requested_policy, const MaterialCoverageLimits &requested_limits)
{
    // All caller-owned values/handles are captured before any callback.
    const auto cursor=view.snapshot; const auto cell=requested_cell; const auto policy=requested_policy;
    const auto limits=requested_limits; const auto started=std::chrono::steady_clock::now();
    MaterialTransitionResult result; result.source=cursor; result.cell=cell; result.policy=policy; result.support_plane_z_mm=plane;
    try {
        detail::require_interval_environment();
        if (!cursor || !cursor->sequence || cursor->sequence->records.size()>200000 ||
            cursor->sequence->geometry.size()!=cursor->sequence->records.size() ||
            cursor->completed_records>cursor->sequence->records.size() || !std::isfinite(cursor->current_progress) ||
            cursor->current_progress<0 || cursor->current_progress>1 ||
            (cursor->completed_records==cursor->sequence->records.size() && cursor->current_progress!=0) ||
            !limits.max_evaluations || limits.max_evaluations>200000 || !limits.max_cells || limits.max_cells>65535 ||
            !limits.max_depth || limits.max_depth>32 || !valid_timeout(limits.timeout) ||
            cell.footprint.min_x>=cell.footprint.max_x || cell.footprint.min_y>=cell.footprint.max_y ||
            policy.minimum.value()<=0 || policy.minimum.value()>=policy.maximum.value()) reject("INVALID_MATERIAL_TRANSITION");
        const auto &r=cell.footprint;
        for (double v : {r.min_x,r.min_y,r.max_x,r.max_y,cell.z00,cell.z10,cell.z01,plane}) coordinate(v);
        const auto sequence=cursor->sequence; const auto poll=[&] { stop(limits,sequence->revision,started); };
        poll();
        const Polygon footprint{{Exact(r.min_x),Exact(r.min_y)},{Exact(r.max_x),Exact(r.min_y)},
                                {Exact(r.max_x),Exact(r.max_y)},{Exact(r.min_x),Exact(r.max_y)}};
        const size_t end=cursor->completed_records+(cursor->current_progress>0 && cursor->completed_records<sequence->records.size());
        for (size_t i=0; i<end; ++i) {
            if (i%128==0) poll();
            const auto &row=sequence->records[i]; if (!row.bead) continue;
            if (!sequence->geometry[i] || !std::holds_alternative<Deposition>(row.motion.payload))
                reject("INVALID_MATERIAL_TRANSITION_GEOMETRY");
            const double progress=i<cursor->completed_records ? 1 : cursor->current_progress;
            for (auto rep : {Representation::Nominal,Representation::Upper}) {
                if (result.roof_evaluations>=limits.max_evaluations) reject("MATERIAL_TRANSITION_WORK_LIMIT");
                if (result.roof_evaluations%128==0) poll(); ++result.roof_evaluations;
                const auto ceiling=roof_ceiling(row,sequence->model,footprint,progress,rep);
                auto &target=rep==Representation::Nominal ? result.nominal_roof_ceiling_mm : result.upper_roof_ceiling_mm;
                if (ceiling) target=target ? std::max(*target,*ceiling) : *ceiling;
            }
        }
        poll();
        if (result.roof_evaluations>=limits.max_evaluations) reject("MATERIAL_TRANSITION_WORK_LIMIT");
        auto remaining=limits; remaining.max_evaluations-=result.roof_evaluations;
        remaining.timeout-=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        if (!valid_timeout(remaining.timeout)) reject("MATERIAL_DEADLINE");
        result.support=coverage(cursor,{{r.min_x,r.min_y,plane},{r.max_x,r.max_y,plane}},Representation::Lower,remaining);
        poll();
        if (result.support.status==MaterialCoverageStatus::Uncovered) {
            result.status=TransitionStatus::Rejected; result.reason="UNSUPPORTED_MATERIAL_TRANSITION_FOOTPRINT"; return result;
        }
        if (result.support.status!=MaterialCoverageStatus::Covered) { result.reason=result.support.reason; return result; }
        if (!result.nominal_roof_ceiling_mm || !result.upper_roof_ceiling_mm ||
            *result.nominal_roof_ceiling_mm<plane || *result.upper_roof_ceiling_mm<*result.nominal_roof_ceiling_mm)
            reject("MATERIAL_TRANSITION_INCONSISTENT_ROOF");
        const Interval error(-policy.corner_height_error.value(),policy.corner_height_error.value());
        const auto h00=Interval(cell.z00)+error, h10=Interval(cell.z10)+error, h01=Interval(cell.z01)+error;
        const std::array<Interval,4> corners{h00,h10,h01,h10+h01-h00};
        auto cap=corners[0]; bool too_small=false,too_large=false;
        for (auto corner : corners) {
            coordinate(corner.lo); coordinate(corner.hi);
            cap=Interval(std::min(cap.lo,corner.lo),std::max(cap.hi,corner.hi));
            too_small|=(corner-Interval(plane)).hi<policy.minimum.value();
            too_large|=(corner-Interval(*result.upper_roof_ceiling_mm)).lo>policy.maximum.value();
        }
        const auto gap=cap-Interval(plane,*result.upper_roof_ceiling_mm); result.gap_mm=bounds(gap);
        const auto area=(Interval(r.max_x)-Interval(r.min_x))*(Interval(r.max_y)-Interval(r.min_y));
        const auto mean=(Interval(cell.z10)+Interval(cell.z01))/Interval(2);
        result.nominal_volume_mm3=bounds(area*(mean-Interval(plane,*result.nominal_roof_ceiling_mm)));
        poll();
        if (too_small || too_large) {
            result.status=TransitionStatus::Rejected;result.reason=too_small ? "MATERIAL_GAP_TOO_SMALL" : "MATERIAL_GAP_TOO_LARGE";
        } else if (gap.lo>policy.minimum.value() && gap.hi<policy.maximum.value() && result.nominal_volume_mm3->lower>0) {
            result.status=TransitionStatus::Compatible;result.reason="CONTINUOUS_MATERIAL_FIRST_PASS_FEASIBLE";
        } else result.reason="MATERIAL_TRANSITION_UNCERTAIN_GAP";
    } catch (const Rejection &e) { result.status=TransitionStatus::Unknown;result.reason=e.what(); }
    catch (const std::exception &e) { result.status=TransitionStatus::Unknown;result.reason="MATERIAL_TRANSITION_NUMERIC_FAILURE: "+std::string(e.what()); }
    return result;
}

MaterialIntegralResult integrate_material_first_pass(const LowerMaterialView &view, const AffineCapCell &requested_cell,
    double plane, const TransitionPolicy &requested_policy, const MaterialIntegralLimits &requested_limits)
{
    const auto cursor=view.snapshot; const auto cell=requested_cell; const auto policy=requested_policy; const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now(); MaterialIntegralResult result;
    result.maximum_interval_width_mm3=limits.maximum_interval_width.value();
    try {
        detail::require_interval_environment();
        if (!cursor || !cursor->sequence || limits.maximum_interval_width.value()<=0) reject("INVALID_MATERIAL_INTEGRAL");
        const auto sequence=cursor->sequence; const auto poll=[&] { stop(limits,sequence->revision,started); };
        result.first_pass=assess_material_first_pass({cursor},cell,plane,policy,limits);
        poll();
        if (result.first_pass.status!=TransitionStatus::Compatible) {
            result.reason=result.first_pass.reason;
            if (result.first_pass.status==TransitionStatus::Rejected) result.status=MaterialIntegralStatus::Rejected;
            return result;
        }
        const size_t previous_work=result.first_pass.roof_evaluations+result.first_pass.support.evaluations;
        const auto evaluate=[&] {
            if (result.evaluations+previous_work>=limits.max_evaluations) reject("MATERIAL_INTEGRAL_WORK_LIMIT");
            if (result.evaluations%128==0) poll(); ++result.evaluations;
        };
        const auto fraction=[&](size_t i) { return i<cursor->completed_records ? 1 : cursor->current_progress; };
        struct Node { Polygon polygon; std::vector<size_t> candidates; Interval volume, roof; size_t depth, id, splitter; double uncertainty; };
        const auto make_node=[&](Polygon polygon, const std::vector<size_t> &candidates, size_t depth) {
            poll();
            if (result.cells+result.first_pass.support.cells>=limits.max_cells) reject("MATERIAL_INTEGRAL_CELL_LIMIT");
            const size_t id=result.cells++;
            auto roof=nominal_roof_bounds(*cursor,polygon,candidates,plane,evaluate);
            if (roof.active.empty()) reject("MATERIAL_INTEGRAL_INCONSISTENT_ROOF");
            const auto integral=affine_integral(polygon,cell);
            const auto volume=integral.second-integral.first*roof.height;
            return Node{std::move(polygon),std::move(roof.active),volume,roof.height,depth,id,roof.splitter,(Interval(volume.hi)-Interval(volume.lo)).hi};
        };
        std::vector<size_t> active;
        const size_t end=cursor->completed_records+(cursor->current_progress>0 && cursor->completed_records<sequence->records.size());
        for (size_t i=0; i<end; ++i) if (sequence->records[i].bead) active.push_back(i);
        const auto &r=cell.footprint;
        auto root=make_node({{Exact(r.min_x),Exact(r.min_y)},{Exact(r.max_x),Exact(r.min_y)},
                            {Exact(r.max_x),Exact(r.max_y)},{Exact(r.min_x),Exact(r.max_y)}},active,0);
        Exact total_lower(root.volume.lo),total_upper(root.volume.hi);
        const auto total=[&] { return Interval(exact_interval(total_lower).lo,exact_interval(total_upper).hi); };
        const auto compare=[](const Node &a, const Node &b) { return a.uncertainty==b.uncertainty ? a.id>b.id : a.uncertainty<b.uncertainty; };
        std::vector<Node> heap; heap.push_back(std::move(root));
        std::vector<MaterialIntegralProof::Cell> terminal;
        while (true) {
            poll(); const auto amount=total(); result.nominal_volume_mm3=bounds(amount);
            if ((Interval(amount.hi)-Interval(amount.lo)).hi<=limits.maximum_interval_width.value()) break;
            if (heap.empty()) reject("MATERIAL_INTEGRAL_DEPTH_LIMIT");
            std::pop_heap(heap.begin(),heap.end(),compare); auto node=std::move(heap.back()); heap.pop_back();
            // Keep terminal cells in the exact total, even when other cells can
            // still narrow enough to satisfy the requested global tolerance.
            if (node.depth>=limits.max_depth) { terminal.push_back({std::move(node.polygon),node.roof}); continue; }
            const auto &row=sequence->records[node.splitter]; const auto &m=row.motion;
            const Exact dx=Exact(m.end.x())-Exact(m.start.x()),dy=Exact(m.end.y())-Exact(m.start.y());
            Vertex normal{-dy,dx};
            const auto projected=project_polygon(row,node.polygon,Interval(0));
            const double span=projected.normal.hi-projected.normal.lo;
            if (((projected.t.lo<=0 || projected.t.hi>=fraction(node.splitter)) && span<row.bead->width_mm.lower/2) ||
                std::max(std::abs(m.end.z()-m.start.z()),std::abs(row.bead->gap_end_mm-row.bead->gap_begin_mm))*(projected.t.hi-projected.t.lo)>span)
                normal={dx,dy};
            auto minimum=normal[0]*node.polygon.front()[0]+normal[1]*node.polygon.front()[1], maximum=minimum;
            for (const auto &p : node.polygon) {
                const Exact value=normal[0]*p[0]+normal[1]*p[1]; minimum=std::min(minimum,value);maximum=std::max(maximum,value);
            }
            if (minimum==maximum) reject("MATERIAL_INTEGRAL_INVALID_SPLIT");
            const Exact middle=(minimum+maximum)/Exact(2);
            auto first=make_node(clip(node.polygon,normal,middle,false),node.candidates,node.depth+1);
            auto second=make_node(clip(node.polygon,normal,middle,true),node.candidates,node.depth+1);
            // Sum lower and upper endpoints separately in eager exact arithmetic.
            // Interval subtraction would incorrectly retain the replaced parent
            // uncertainty, and ordinary running sums could lose the error budget.
            total_lower+=Exact(first.volume.lo)+Exact(second.volume.lo)-Exact(node.volume.lo);
            total_upper+=Exact(first.volume.hi)+Exact(second.volume.hi)-Exact(node.volume.hi);
            heap.push_back(std::move(first)); std::push_heap(heap.begin(),heap.end(),compare);
            heap.push_back(std::move(second)); std::push_heap(heap.begin(),heap.end(),compare);
        }
        poll();
        if (result.nominal_volume_mm3->lower<=0) reject("MATERIAL_INTEGRAL_UNCERTAIN_POSITIVE_VOLUME");
        for (auto &node : heap) { poll(); terminal.push_back({std::move(node.polygon),node.roof}); }
        auto proof=std::shared_ptr<const MaterialIntegralProof>(new MaterialIntegralProof(cursor,cell,*result.nominal_volume_mm3,std::move(terminal)));
        poll(); result.proof=std::move(proof);
        result.status=MaterialIntegralStatus::Bounded;result.reason="BOUNDED_NOMINAL_VERTICAL_CELL_INTEGRAL";
    } catch (const Rejection &e) { result.proof.reset();result.status=MaterialIntegralStatus::Unknown;result.reason=e.what(); }
    catch (const std::exception &e) { result.proof.reset();result.status=MaterialIntegralStatus::Unknown;result.reason="MATERIAL_INTEGRAL_NUMERIC_FAILURE: "+std::string(e.what()); }
    return result;
}

IntegralStripsResult split_material_integral(const MaterialIntegralResult &requested, IntegralSplitAxis axis,
    const std::vector<double> &requested_cuts, const MaterialIntegralLimits &requested_limits)
{
    const auto proof=requested.proof; const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        // Check the size before copying caller data, as in the other bounded
        // captures. No callback runs until all inputs are owned.
        if (!proof || requested.status!=MaterialIntegralStatus::Bounded || requested_cuts.size()<2 || requested_cuts.size()>4097 ||
            (axis!=IntegralSplitAxis::X && axis!=IntegralSplitAxis::Y) || !limits.max_evaluations || limits.max_evaluations>200000 ||
            !limits.max_cells || limits.max_cells>65535 || !valid_timeout(limits.timeout) || limits.maximum_interval_width.value()<=0)
            reject("INVALID_INTEGRAL_STRIP_PARTITION");
        const auto cuts=requested_cuts; const auto poll=[&] { stop(limits,proof->source->sequence->revision,started); };
        poll(); const auto &r=proof->target.footprint;
        const bool x_axis=axis==IntegralSplitAxis::X;
        if (cuts.front()!=(x_axis ? r.min_x : r.min_y) || cuts.back()!=(x_axis ? r.max_x : r.max_y))
            reject("INCOMPLETE_INTEGRAL_STRIP_PARTITION");
        for (size_t i=0; i<cuts.size(); ++i) {
            coordinate(cuts[i]); if (i && cuts[i]<=cuts[i-1]) reject("UNORDERED_INTEGRAL_STRIP_PARTITION");
        }
        std::vector<IntegralStripVolume> strips; Exact all_lower(0),all_upper(0); size_t cells=0,evaluations=0;
        for (size_t i=1; i<cuts.size(); ++i) {
            poll(); RectangleXY strip=r;
            if (x_axis) { strip.min_x=cuts[i-1];strip.max_x=cuts[i]; }
            else { strip.min_y=cuts[i-1];strip.max_y=cuts[i]; }
            const auto amount=proof->rectangle_volume(strip,limits,cells,evaluations,poll,"INTEGRAL_STRIP");
            strips.push_back({strip,amount});
            all_lower+=Exact(amount.lower);all_upper+=Exact(amount.upper);
        }
        const auto total=Interval(exact_interval(all_lower).lo,exact_interval(all_upper).hi);
        if (total.hi<proof->total_volume.lower || total.lo>proof->total_volume.upper) reject("INTEGRAL_STRIP_INCONSISTENT_VOLUME");
        if ((Interval(total.hi)-Interval(total.lo)).hi>limits.maximum_interval_width.value())
            reject("INTEGRAL_STRIP_GLOBAL_PRECISION");
        poll();
        auto snapshot=std::make_shared<const IntegralStripsSnapshot>(IntegralStripsSnapshot{proof,axis,cuts,std::move(strips),bounds(total),cells,evaluations});
        poll(); return {"BOUNDED_COMPLETE_NOMINAL_STRIP_VOLUMES_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) { return {e.what(),{}}; }
    catch (const std::exception &e) { return {"INTEGRAL_STRIP_NUMERIC_FAILURE: "+std::string(e.what()),{}}; }
}

AffinePassStackResult plan_affine_pass_stack(const LowerMaterialView &view, const AffineCapCell &requested_target,
    double plane, const AffinePassPolicy &requested_policy, const MaterialIntegralLimits &requested_limits)
{
    const auto cursor=view.snapshot; const auto target=requested_target; const auto policy=requested_policy; const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!cursor || !cursor->sequence || policy.passes<2 || policy.passes>16 || policy.total_volume_error.value()<=0 ||
            policy.later_vertical_minimum.value()<=0 || policy.later_vertical_minimum.value()>=policy.later_vertical_maximum.value() ||
            policy.later_normal_minimum.value()<=0 || policy.later_normal_minimum.value()>=policy.later_normal_maximum.value() ||
            limits.maximum_interval_width.value()<=0) reject("INVALID_AFFINE_PASS_POLICY");
        const auto sequence=cursor->sequence; const auto poll=[&] { stop(limits,sequence->revision,started); };
        const auto numeric_error=[&](double shift_error) {
            const auto total=Interval(sequence->model.numerical_coordinate_error.value())+
                Interval(3)*(Interval(policy.first_gap.corner_height_error.value())+Interval(shift_error));
            if (total.hi>.05) reject("AFFINE_PASS_NUMERICAL_BUDGET"); return total.hi;
        };
        numeric_error(0);
        // This is a geometry probe at the final target, not a printed first
        // candidate. Its valid support/roof bounds may show that target too high
        // or low for a first pass. No failed/unknown material proof is accepted.
        const auto probe=assess_material_first_pass({cursor},target,plane,policy.first_gap,limits);
        poll();
        if (probe.support.status!=MaterialCoverageStatus::Covered || !probe.gap_mm || !probe.upper_roof_ceiling_mm ||
            (probe.reason!="CONTINUOUS_MATERIAL_FIRST_PASS_FEASIBLE" && probe.reason!="MATERIAL_GAP_TOO_SMALL" &&
             probe.reason!="MATERIAL_GAP_TOO_LARGE" && probe.reason!="MATERIAL_TRANSITION_UNCERTAIN_GAP"))
            return {probe.reason,{}};
        const auto &r=target.footprint;
        const Interval error(-policy.first_gap.corner_height_error.value(),policy.first_gap.corner_height_error.value());
        const auto gx=(Interval(target.z10)+error-Interval(target.z00)-error)/(Interval(r.max_x)-Interval(r.min_x));
        const auto gy=(Interval(target.z01)+error-Interval(target.z00)-error)/(Interval(r.max_y)-Interval(r.min_y));
        const auto normalizer=detail::root(Interval(1)+square(gx)+square(gy));
        const Interval remaining_passes(double(policy.passes-1));
        double low=(Interval(probe.gap_mm->upper)-Interval(policy.first_gap.maximum.value())).hi;
        double high=(Interval(probe.gap_mm->lower)-Interval(policy.first_gap.minimum.value())).lo;
        low=std::max({low,(remaining_passes*Interval(policy.later_vertical_minimum.value())).hi,
            (remaining_passes*Interval(policy.later_normal_minimum.value())*normalizer).hi});
        high=std::min({high,(remaining_passes*Interval(policy.later_vertical_maximum.value())).lo,
            (remaining_passes*Interval(policy.later_normal_maximum.value())*normalizer).lo});
        if (low>=high) return {"PARALLEL_AFFINE_STACK_INFEASIBLE",{}};
        const double offset=(low+high)/2;
        if (!(offset>low && offset<high)) reject("AFFINE_PASS_OFFSET_ROUNDING");
        std::vector<AffineCapCell> cells; double shift_error=0;
        for (size_t pass=1; pass<=policy.passes; ++pass) {
            poll(); auto cell=target;
            if (pass!=policy.passes) {
                const double fraction=double(policy.passes-pass)/double(policy.passes-1);
                const auto exact_offset=Interval(offset)*Interval(double(policy.passes-pass))/remaining_passes;
                for (auto pair : {std::pair<double *,double>{&cell.z00,target.z00},{&cell.z10,target.z10},{&cell.z01,target.z01}}) {
                    *pair.first=pair.second-offset*fraction; coordinate(*pair.first);
                    const auto delta=Interval(pair.second)-exact_offset-Interval(*pair.first);
                    shift_error=std::max(shift_error,std::max(std::abs(delta.lo),std::abs(delta.hi)));
                }
            }
            cells.push_back(cell);
        }
        const double total_numeric=numeric_error(shift_error);
        const size_t previous_work=probe.roof_evaluations+probe.support.evaluations, previous_cells=probe.support.cells;
        if (previous_work>=limits.max_evaluations || previous_cells>=limits.max_cells) reject("AFFINE_PASS_WORK_LIMIT");
        auto remaining=limits; remaining.max_evaluations-=previous_work; remaining.max_cells-=previous_cells;
        remaining.timeout-=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        remaining.cancelled=[&] { poll();return false; }; remaining.is_current={};
        remaining.maximum_interval_width=Volume(std::min(limits.maximum_interval_width.value(),policy.total_volume_error.value()));
        auto first_policy=policy.first_gap;
        first_policy.corner_height_error=Length((Interval(first_policy.corner_height_error.value())+Interval(shift_error)).hi);
        const auto first=integrate_material_first_pass({cursor},cells.front(),plane,first_policy,remaining);
        poll(); if (first.status!=MaterialIntegralStatus::Bounded || !first.nominal_volume_mm3) return {first.reason,{}};
        const auto area=(Interval(r.max_x)-Interval(r.min_x))*(Interval(r.max_y)-Interval(r.min_y));
        const auto corners=[](const AffineCapCell &c) {
            return std::array<Interval,4>{Interval(c.z00),Interval(c.z10),Interval(c.z01),Interval(c.z10)+Interval(c.z01)-Interval(c.z00)};
        };
        const auto gradient=[&](const AffineCapCell &c) {
            return std::array<Interval,2>{
                (Interval(c.z10)+error-Interval(c.z00)-error)/(Interval(r.max_x)-Interval(r.min_x)),
                (Interval(c.z01)+error-Interval(c.z00)-error)/(Interval(r.max_y)-Interval(r.min_y))};
        };
        std::vector<AffinePassSurface> surfaces; Interval total_volume(0); double allocated=0;
        for (size_t i=0; i<cells.size(); ++i) {
            poll(); Interval volume=interval(*first.nominal_volume_mm3);
            std::optional<ScalarBounds> vertical,normal;
            if (i) {
                const auto before=corners(cells[i-1]), after=corners(cells[i]); auto spacing=after[0]-before[0];
                for (size_t j=1; j<4; ++j) {
                    const auto gap=after[j]-before[j]; spacing=Interval(std::min(spacing.lo,gap.lo),std::max(spacing.hi,gap.hi));
                }
                // Normal separation is measured along the lower affine plane's
                // normal. Account for the tiny nonparallelism of rounded stored
                // vertices instead of silently using the ideal target gradient.
                const auto g0=gradient(cells[i-1]),g1=gradient(cells[i]);
                const auto normal_spacing=spacing*detail::root(Interval(1)+square(g0[0])+square(g0[1]))/
                    (Interval(1)+g0[0]*g1[0]+g0[1]*g1[1]);
                if (spacing.lo<=policy.later_vertical_minimum.value() || spacing.hi>=policy.later_vertical_maximum.value() ||
                    normal_spacing.lo<=policy.later_normal_minimum.value() || normal_spacing.hi>=policy.later_normal_maximum.value())
                    reject("AFFINE_PASS_SPACING_UNCERTAIN");
                vertical=bounds(spacing); normal=bounds(normal_spacing);
                volume=area*((after[1]+after[2]-before[1]-before[2])/Interval(2));
            }
            if (volume.lo<=0) reject("AFFINE_PASS_VOLUME_UNCERTAIN");
            const double quota=(volume.lo+volume.hi)/2;
            const auto quota_error=volume-Interval(quota);
            surfaces.push_back({cells[i],vertical,normal,bounds(volume),Volume(quota),std::max(std::abs(quota_error.lo),std::abs(quota_error.hi))});
            total_volume=total_volume+volume; allocated+=quota;
        }
        const auto allocation_error=total_volume-Interval(allocated);
        const double total_error=std::max(std::abs(allocation_error.lo),std::abs(allocation_error.hi));
        if (total_error>policy.total_volume_error.value()) reject("AFFINE_PASS_TOTAL_VOLUME_ERROR");
        poll();
        auto snapshot=std::shared_ptr<const AffinePassStackSnapshot>(new AffinePassStackSnapshot(cursor,target,policy,plane,offset,{low,high},first,
            std::move(surfaces),bounds(total_volume),Volume(allocated),total_error,total_numeric));
        poll(); return {"PROSPECTIVE_AFFINE_SURFACES_AND_CELL_QUOTAS_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) { return {e.what(),{}}; }
    catch (const std::exception &e) { return {"AFFINE_PASS_STACK_NUMERIC_FAILURE: "+std::string(e.what()),{}}; }
}

AffineHatchResult plan_affine_hatches(const AffinePassStackResult &requested, const AffineHatchPolicy &requested_policy,
    const AffineHatchLimits &requested_limits)
{
    const auto source=requested.snapshot; const auto policy=requested_policy; const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!source || !source->source || !source->first_pass.proof || !valid_timeout(limits.timeout) ||
            !limits.max_lines || limits.max_lines>200000 || policy.width.value()<=0 || policy.maximum_pitch.value()<=0 ||
            policy.boundary_band.value()<=0 || (policy.first_direction!=HatchDirection::AlongX && policy.first_direction!=HatchDirection::AlongY))
            reject("INVALID_AFFINE_HATCH_REQUEST");
        const auto poll=[&] {
            stop(limits,source->source->sequence->revision,started);
            if (limits.volumes.cancelled && limits.volumes.cancelled()) reject("CANCELLED");
            if (limits.volumes.is_current && !limits.volumes.is_current(source->source->sequence->revision)) reject("STALE_REVISION");
        };
        poll(); const auto &r=source->final_surface.footprint;
        const auto radius=Interval(policy.width.value())/Interval(2)+Interval(source->numerical_error_upper_mm);
        const auto inset=radius+Interval(policy.boundary_band.value());
        const RectangleXY center{(Interval(r.min_x)+inset).hi,(Interval(r.min_y)+inset).hi,
            (Interval(r.max_x)-inset).lo,(Interval(r.max_y)-inset).lo};
        if (center.min_x>=center.max_x || center.min_y>=center.max_y) reject("AFFINE_HATCH_ROI_TOO_THIN");
        if (policy.maximum_pitch.value()>(Interval(policy.width.value())-Interval(2)*Interval(source->numerical_error_upper_mm)).lo)
            reject("AFFINE_HATCH_SPARSE_PROJECTED_PITCH");
        if (!source->first_pass.first_pass.gap_mm || policy.width.value()<=source->first_pass.first_pass.gap_mm->upper ||
            policy.width.value()<=source->policy.later_vertical_maximum.value()) reject("AFFINE_HATCH_WIDTH_HEIGHT_DOMAIN");
        const auto point=[&](double x,double y,const AffineCapCell &c) {
            const Exact z=Exact(c.z00)+(Exact(c.z10)-Exact(c.z00))*(Exact(x)-Exact(r.min_x))/(Exact(r.max_x)-Exact(r.min_x))+
                (Exact(c.z01)-Exact(c.z00))*(Exact(y)-Exact(r.min_y))/(Exact(r.max_y)-Exact(r.min_y));
            const auto value=stored_exact(z); return std::pair<PhysicalPosition,double>{{x,y,value.first},value.second};
        };
        std::vector<AffineHatchPass> passes; size_t line_count=0; Exact total_lower(0),total_upper(0); double numeric=source->numerical_error_upper_mm;
        for (size_t p=0; p<source->surfaces.size(); ++p) {
            poll(); const bool x_axis=(policy.first_direction==HatchDirection::AlongX)==(p%2==0);
            const auto direction=x_axis ? HatchDirection::AlongX : HatchDirection::AlongY;
            const double low=x_axis ? center.min_y : center.min_x, high=x_axis ? center.max_y : center.max_x;
            const auto ratio=exact_interval((Exact(high)-Exact(low))/Exact(policy.maximum_pitch.value()));
            if (ratio.hi>double(limits.max_lines)) reject("AFFINE_HATCH_LINE_LIMIT");
            const size_t intervals=std::max(size_t(1),size_t(std::ceil(ratio.hi))), count=intervals+1;
            if (count>limits.max_lines-line_count || count>4096) reject("AFFINE_HATCH_LINE_LIMIT");
            auto pitch=exact_interval((Exact(high)-Exact(low))/Exact(int(intervals)));
            if (pitch.hi>policy.maximum_pitch.value()) reject("AFFINE_HATCH_PITCH_ROUNDING");
            std::vector<double> centers,cuts; std::vector<double> errors;
            for (size_t i=0; i<count; ++i) {
                poll(); const auto value=stored_exact(Exact(low)+(Exact(high)-Exact(low))*Exact(int(i))/Exact(int(intervals)));
                centers.push_back(value.first); errors.push_back(value.second);
                if (i) cuts.push_back(stored_exact((Exact(centers[i-1])+Exact(centers[i]))/Exact(2)).first);
            }
            for (size_t i=1; i<centers.size(); ++i) {
                const auto gap=exact_interval(Exact(centers[i])-Exact(centers[i-1]));
                if (gap.lo<=0 || gap.hi>policy.maximum_pitch.value()) reject("AFFINE_HATCH_PITCH_ROUNDING");
                pitch=Interval(std::min(pitch.lo,gap.lo),std::max(pitch.hi,gap.hi));
            }
            cuts.insert(cuts.begin(),x_axis ? r.min_y : r.min_x); cuts.push_back(x_axis ? r.max_y : r.max_x);
            std::vector<IntegralStripVolume> volumes;
            if (!p) {
                auto remaining=limits.volumes;
                remaining.timeout=std::min(remaining.timeout,limits.timeout-
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started));
                remaining.cancelled=[&] { poll();return false; };remaining.is_current={};
                const auto split=split_material_integral(source->first_pass,x_axis ? IntegralSplitAxis::Y : IntegralSplitAxis::X,cuts,remaining);
                if (!split.snapshot) return {split.reason,{}}; volumes=split.snapshot->strips;
            } else {
                for (size_t i=1; i<cuts.size(); ++i) {
                    poll(); RectangleXY cell=r;
                    if (x_axis) { cell.min_y=cuts[i-1];cell.max_y=cuts[i]; }
                    else { cell.min_x=cuts[i-1];cell.max_x=cuts[i]; }
                    const Polygon polygon{{Exact(cell.min_x),Exact(cell.min_y)},{Exact(cell.max_x),Exact(cell.min_y)},
                        {Exact(cell.max_x),Exact(cell.max_y)},{Exact(cell.min_x),Exact(cell.max_y)}};
                    const auto above=affine_integral(polygon,source->surfaces[p].cell),below=affine_integral(polygon,source->surfaces[p-1].cell);
                    volumes.push_back({cell,bounds(above.second-below.second)});
                }
            }
            AffineHatchPass pass{direction,bounds(pitch),{0,0},{},{}}; Exact lower(0),upper(0);
            for (size_t i=0; i<count; ++i) {
                poll(); const auto &cell=source->surfaces[p].cell;
                const auto a=point(x_axis ? center.min_x : centers[i],x_axis ? centers[i] : center.min_y,cell);
                const auto b=point(x_axis ? center.max_x : centers[i],x_axis ? centers[i] : center.max_y,cell);
                const auto error=Interval(errors[i])+Interval(std::max(a.second,b.second));
                numeric=std::max(numeric,(Interval(source->numerical_error_upper_mm)+error).hi);
                if (numeric>.05) reject("AFFINE_HATCH_NUMERICAL_BUDGET");
                const auto footprint_radius=radius+Interval(errors[i]);
                for (const auto &v : {a.first,b.first}) {
                    if ((Interval(v.x())-footprint_radius).lo<r.min_x || (Interval(v.x())+footprint_radius).hi>r.max_x ||
                        (Interval(v.y())-footprint_radius).lo<r.min_y || (Interval(v.y())+footprint_radius).hi>r.max_y)
                        reject("AFFINE_HATCH_FOOTPRINT_OUTSIDE_ROI");
                }
                const auto length=detail::root(square(Interval(b.first.x())-Interval(a.first.x()))+square(Interval(b.first.y())-Interval(a.first.y())));
                if (length.lo<=0 || volumes[i].volume_mm3.lower<=0) reject("AFFINE_HATCH_DEGENERATE_LINE_OR_VOLUME");
                pass.lines.push_back({a.first,b.first,b.first,a.first,policy.width,volumes[i].footprint,volumes[i].volume_mm3,bounds(length),error.hi});
                lower+=Exact(volumes[i].volume_mm3.lower);upper+=Exact(volumes[i].volume_mm3.upper);
            }
            pass.prospective_volume_mm3={exact_interval(lower).lo,exact_interval(upper).hi};
            const auto band=policy.boundary_band.value();
            pass.boundary_regions={{r.min_x,r.min_y,r.min_x+band,r.max_y},{r.max_x-band,r.min_y,r.max_x,r.max_y},
                {r.min_x+band,r.min_y,r.max_x-band,r.min_y+band},{r.min_x+band,r.max_y-band,r.max_x-band,r.max_y}};
            total_lower+=Exact(pass.prospective_volume_mm3.lower);total_upper+=Exact(pass.prospective_volume_mm3.upper);
            line_count+=count; passes.push_back(std::move(pass));
        }
        const auto total=Interval(exact_interval(total_lower).lo,exact_interval(total_upper).hi);
        if (total.hi<source->total_volume_mm3.lower || total.lo>source->total_volume_mm3.upper ||
            (Interval(total.hi)-Interval(total.lo)).hi>std::min(limits.volumes.maximum_interval_width.value(),source->policy.total_volume_error.value()))
            reject("AFFINE_HATCH_TOTAL_VOLUME_PRECISION");
        poll(); auto snapshot=std::shared_ptr<const AffineHatchSnapshot>(new AffineHatchSnapshot(source,policy,std::move(passes),bounds(total),line_count,numeric));
        poll(); return {"OWNED_FIXED_WIDTH_AFFINE_HATCH_CANDIDATES_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) { return {e.what(),{}}; }
    catch (const std::exception &e) { return {"AFFINE_HATCH_NUMERIC_FAILURE: "+std::string(e.what()),{}}; }
}

AffineHatchCellsResult allocate_affine_hatch_cells(const AffineHatchResult &requested, const MaterialIntegralLimits &requested_limits)
{
    const auto source=requested.snapshot; const auto limits=requested_limits; const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!source || !limits.max_evaluations || limits.max_evaluations>200000 || !limits.max_cells || limits.max_cells>65535 ||
            !valid_timeout(limits.timeout) || limits.maximum_interval_width.value()<=0) reject("INVALID_HATCH_CELL_PARTITION");
        const auto stack=source->source; const auto proof=stack->first_pass.proof;
        const auto poll=[&] { stop(limits,stack->source->sequence->revision,started); };
        poll(); const auto &r=stack->final_surface.footprint;
        const auto area=[](const RectangleXY &v) { return (Exact(v.max_x)-Exact(v.min_x))*(Exact(v.max_y)-Exact(v.min_y)); };
        const auto sum=[](const std::vector<IntegralStripVolume> &cells) {
            Exact lower(0),upper(0); for (const auto &cell : cells) { lower+=Exact(cell.volume_mm3.lower);upper+=Exact(cell.volume_mm3.upper); }
            return ScalarBounds{exact_interval(lower).lo,exact_interval(upper).hi};
        };
        std::vector<AffineHatchCellPass> passes; size_t fragments=0,evaluations=0;
        Exact all_finite_lower(0),all_finite_upper(0),all_remainder_lower(0),all_remainder_upper(0);
        for (size_t p=0; p<source->passes.size(); ++p) {
            poll(); const auto &hatch=source->passes[p]; const bool x_axis=hatch.direction==HatchDirection::AlongX;
            const auto &first=hatch.lines.front(), &last=hatch.lines.back();
            const auto first_radius=Interval(first.width.value())/Interval(2),last_radius=Interval(last.width.value())/Interval(2);
            // Inward rounding keeps every owner inside the finite nominal
            // flat-ended path footprint. Rounded transverse voids are not filled.
            const RectangleXY core=x_axis ? RectangleXY{first.start.x(),(Interval(first.start.y())-first_radius).hi,
                    first.end.x(),(Interval(last.start.y())+last_radius).lo} :
                RectangleXY{(Interval(first.start.x())-first_radius).hi,first.start.y(),
                    (Interval(last.start.x())+last_radius).lo,first.end.y()};
            if (core.min_x<=r.min_x || core.max_x>=r.max_x || core.min_y<=r.min_y || core.max_y>=r.max_y ||
                core.min_x>=core.max_x || core.min_y>=core.max_y) reject("HATCH_CELL_FINITE_DOMAIN");
            AffineHatchCellPass pass{core,{},{},{0,0},{0,0},{0,0}}; Exact covered(0);
            const auto append=[&](const RectangleXY &cell, std::vector<IntegralStripVolume> &destination) {
                poll(); if (cell.min_x>=cell.max_x || cell.min_y>=cell.max_y || cell.min_x<r.min_x || cell.max_x>r.max_x ||
                    cell.min_y<r.min_y || cell.max_y>r.max_y) reject("HATCH_CELL_DOMAIN_MISMATCH");
                ScalarBounds amount{0,0};
                if (!p) amount=proof->rectangle_volume(cell,limits,fragments,evaluations,poll,"HATCH_CELL");
                else {
                    if (evaluations>=limits.max_evaluations) reject("HATCH_CELL_WORK_LIMIT"); ++evaluations;
                    if (fragments>=limits.max_cells) reject("HATCH_CELL_CELL_LIMIT"); ++fragments;
                    const Polygon polygon{{Exact(cell.min_x),Exact(cell.min_y)},{Exact(cell.max_x),Exact(cell.min_y)},
                        {Exact(cell.max_x),Exact(cell.max_y)},{Exact(cell.min_x),Exact(cell.max_y)}};
                    amount=bounds(affine_integral(polygon,stack->surfaces[p].cell).second-
                        affine_integral(polygon,stack->surfaces[p-1].cell).second);
                    if (amount.lower<=0) reject("HATCH_CELL_UNCERTAIN_POSITIVE_VOLUME");
                }
                covered+=area(cell);destination.push_back({cell,amount});
            };
            double previous=x_axis ? core.min_y : core.min_x;
            for (const auto &line : hatch.lines) {
                poll(); RectangleXY cell=core;
                if (x_axis) { cell.min_y=std::max(core.min_y,line.volume_cell.min_y);cell.max_y=std::min(core.max_y,line.volume_cell.max_y); }
                else { cell.min_x=std::max(core.min_x,line.volume_cell.min_x);cell.max_x=std::min(core.max_x,line.volume_cell.max_x); }
                const Exact half=Exact(line.width.value())/Exact(2);
                const bool inside=x_axis ? cell.min_x>=line.start.x() && cell.max_x<=line.end.x() &&
                    Exact(cell.min_y)>=Exact(line.start.y())-half && Exact(cell.max_y)<=Exact(line.start.y())+half :
                    cell.min_y>=line.start.y() && cell.max_y<=line.end.y() &&
                    Exact(cell.min_x)>=Exact(line.start.x())-half && Exact(cell.max_x)<=Exact(line.start.x())+half;
                if (!inside) reject("HATCH_CELL_OUTSIDE_FINITE_PATH");
                if ((x_axis ? cell.min_y : cell.min_x)!=previous) reject("HATCH_CELL_NONCONTIGUOUS_PARTITION");
                previous=x_axis ? cell.max_y : cell.max_x;append(cell,pass.finite_cells);
            }
            if (previous!=(x_axis ? core.max_y : core.max_x)) reject("HATCH_CELL_INCOMPLETE_PARTITION");
            for (const auto &cell : {RectangleXY{r.min_x,r.min_y,core.min_x,r.max_y},
                    {core.max_x,r.min_y,r.max_x,r.max_y},{core.min_x,r.min_y,core.max_x,core.min_y},
                    {core.min_x,core.max_y,core.max_x,r.max_y}}) append(cell,pass.remainder_cells);
            if (covered!=area(r)) reject("HATCH_CELL_INCOMPLETE_PARTITION");
            pass.finite_volume_mm3=sum(pass.finite_cells);pass.remainder_volume_mm3=sum(pass.remainder_cells);
            pass.total_volume_mm3={exact_interval(Exact(pass.finite_volume_mm3.lower)+Exact(pass.remainder_volume_mm3.lower)).lo,
                exact_interval(Exact(pass.finite_volume_mm3.upper)+Exact(pass.remainder_volume_mm3.upper)).hi};
            const auto &parent=stack->surfaces[p].volume_mm3;
            if (pass.total_volume_mm3.upper<parent.lower || pass.total_volume_mm3.lower>parent.upper) reject("HATCH_CELL_INCONSISTENT_VOLUME");
            all_finite_lower+=Exact(pass.finite_volume_mm3.lower);all_finite_upper+=Exact(pass.finite_volume_mm3.upper);
            all_remainder_lower+=Exact(pass.remainder_volume_mm3.lower);all_remainder_upper+=Exact(pass.remainder_volume_mm3.upper);
            passes.push_back(std::move(pass));
        }
        const ScalarBounds finite{exact_interval(all_finite_lower).lo,exact_interval(all_finite_upper).hi};
        const ScalarBounds remainder{exact_interval(all_remainder_lower).lo,exact_interval(all_remainder_upper).hi};
        const ScalarBounds total{exact_interval(all_finite_lower+all_remainder_lower).lo,exact_interval(all_finite_upper+all_remainder_upper).hi};
        if (total.upper<stack->total_volume_mm3.lower || total.lower>stack->total_volume_mm3.upper ||
            (Interval(total.upper)-Interval(total.lower)).hi>std::min(limits.maximum_interval_width.value(),stack->policy.total_volume_error.value()))
            reject("HATCH_CELL_GLOBAL_PRECISION");
        poll(); auto snapshot=std::shared_ptr<const AffineHatchCellsSnapshot>(new AffineHatchCellsSnapshot(source,
            std::move(passes),finite,remainder,total,fragments,evaluations));
        poll();return {"BOUNDED_FINITE_HATCH_CELLS_AND_EXPLICIT_REMAINDER_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) { return {e.what(),{}}; }
    catch (const std::exception &e) { return {"HATCH_CELL_NUMERIC_FAILURE: "+std::string(e.what()),{}}; }
}

FixedWidthBeadResult plan_fixed_width_bead(const FixedWidthBeadRequest &requested, const FixedWidthBeadLimits &requested_limits)
{
    const auto limits=requested_limits; const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (requested.source_fingerprint.size()!=64) reject("INVALID_FIXED_WIDTH_BEAD_CONTEXT");
        const auto request=requested;
        if (!request.revision || !std::all_of(request.source_fingerprint.begin(),request.source_fingerprint.end(),
                [](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}) ||
            (request.kind!=BeadSectionKind::Rectangle && request.kind!=BeadSectionKind::RoundedRectangle) ||
            request.width.value()<=0 || request.gap_begin.value()<=0 || request.gap_end.value()<=0 ||
            limits.maximum_width_error.value()<=0 || limits.maximum_width_error.value()>.05 || limits.maximum_volume_error.value()<=0 ||
            !limits.max_segments || limits.max_segments>65535 || !limits.max_depth || limits.max_depth>32 || !valid_timeout(limits.timeout))
            reject("INVALID_FIXED_WIDTH_BEAD_REQUEST");
        for (auto p : {request.start,request.end}) { coordinate(p.x());coordinate(p.y());coordinate(p.z()); }
        for (double v : {request.width.value(),request.gap_begin.value(),request.gap_end.value()}) coordinate(v);
        if (request.kind==BeadSectionKind::RoundedRectangle && request.width.value()<=std::max(request.gap_begin.value(),request.gap_end.value()))
            reject("UNSUPPORTED_FIXED_WIDTH_ROUNDED_DOMAIN");
        const auto poll=[&] { stop(limits,request.revision,started); };poll();
        const auto original_length=detail::root(length_squared(request.start,request.end));
        if (original_length.lo<=0) reject("FIXED_WIDTH_REQUIRES_XY_LENGTH");
        const auto correction=request.kind==BeadSectionKind::RoundedRectangle ? Interval(1)-pi()/Interval(4) : Interval(0);
        const auto ideal=[&](Interval begin,Interval end,Interval fraction) {
            return original_length*fraction*(Interval(request.width.value())*(begin+end)/Interval(2)-
                correction*(square(begin)+begin*end+square(end))/Interval(3));
        };
        const auto target=ideal(Interval(request.gap_begin.value()),Interval(request.gap_end.value()),Interval(1));
        if (target.lo<=0) reject("FIXED_WIDTH_TARGET_VOLUME_UNCERTAIN");
        struct Endpoint { PhysicalPosition point; double gap, error; };
        const auto endpoint=[&](const Exact &t) {
            const auto x=stored_exact(Exact(request.start.x())+(Exact(request.end.x())-Exact(request.start.x()))*t);
            const auto y=stored_exact(Exact(request.start.y())+(Exact(request.end.y())-Exact(request.start.y()))*t);
            const auto z=stored_exact(Exact(request.start.z())+(Exact(request.end.z())-Exact(request.start.z()))*t);
            const auto h=stored_exact(Exact(request.gap_begin.value())+(Exact(request.gap_end.value())-Exact(request.gap_begin.value()))*t);
            const double error=(Interval(x.second)+Interval(y.second)+Interval(z.second)+Interval(h.second)).hi;
            if (h.first<=0 || error>.05) reject("FIXED_WIDTH_COORDINATE_BUDGET");
            return Endpoint{{x.first,y.first,z.first},h.first,error};
        };
        struct Node { Exact begin,end; Endpoint a,b; size_t depth; };
        std::vector<Node> pending{{Exact(0),Exact(1),endpoint(Exact(0)),endpoint(Exact(1)),0}};
        std::vector<FixedWidthBeadPiece> pieces; Exact deposited(0);double maximum_width_error=0,numeric=0;
        while (!pending.empty()) {
            poll();auto node=std::move(pending.back());pending.pop_back();
            const auto length=detail::root(length_squared(node.a.point,node.b.point));
            if (length.lo<=0) reject("FIXED_WIDTH_PACKET_XY_ROUNDING");
            const Interval h0(node.a.gap),h1(node.b.gap),middle=(h0+h1)/Interval(2);
            const auto nominal_amount=length*middle*(Interval(request.width.value())-correction*middle);
            const double amount=(nominal_amount.lo+nominal_amount.hi)/2;
            if (!std::isfinite(amount) || amount<=0) reject("FIXED_WIDTH_PACKET_AMOUNT_ROUNDING");
            const auto area=Interval(amount)/length;
            const Interval hmin(std::min(node.a.gap,node.b.gap)),hmax(std::max(node.a.gap,node.b.gap));
            const auto wmin=section_width(area,hmax,request.kind),wmax=section_width(area,hmin,request.kind);
            const double width_error=std::max({0.,(Interval(request.width.value())-wmin).hi,(wmax-Interval(request.width.value())).hi});
            const auto anchor=section_width(area,middle,request.kind);
            const Exact original_h0=Exact(request.gap_begin.value())+(Exact(request.gap_end.value())-Exact(request.gap_begin.value()))*node.begin;
            const Exact original_h1=Exact(request.gap_begin.value())+(Exact(request.gap_end.value())-Exact(request.gap_begin.value()))*node.end;
            const auto fraction=exact_interval(node.end-node.begin);
            const auto volume_error=Interval(amount)-ideal(exact_interval(original_h0),exact_interval(original_h1),fraction);
            const double error=std::max(std::abs(volume_error.lo),std::abs(volume_error.hi));
            const bool rounded_domain=request.kind!=BeadSectionKind::RoundedRectangle || area.lo>(pi()/Interval(4)*square(hmax)).hi;
            const bool accepted=rounded_domain && width_error<=limits.maximum_width_error.value() &&
                error<=(Interval(limits.maximum_volume_error.value())*fraction).lo &&
                request.width.value()>=anchor.lo && request.width.value()<=anchor.hi;
            if (!accepted) {
                if (node.depth>=limits.max_depth) reject("FIXED_WIDTH_PACKET_DEPTH_LIMIT");
                if (pieces.size()+pending.size()+2>limits.max_segments) reject("FIXED_WIDTH_PACKET_COUNT_LIMIT");
                const Exact t=(node.begin+node.end)/Exact(2);const auto point=endpoint(t);
                pending.push_back({t,node.end,point,node.b,node.depth+1});pending.push_back({node.begin,t,node.a,point,node.depth+1});
                continue;
            }
            if (pieces.size()>=limits.max_segments) reject("FIXED_WIDTH_PACKET_COUNT_LIMIT");
            maximum_width_error=std::max(maximum_width_error,width_error);numeric=std::max({numeric,node.a.error,node.b.error});
            deposited+=Exact(amount);
            pieces.push_back({node.a.point,node.b.point,request.width,Volume(amount),
                {request.kind,node.a.gap,node.b.gap,{wmin.lo,wmax.hi}},width_error,std::max(node.a.error,node.b.error)});
        }
        const auto delivered=exact_interval(deposited),difference=delivered-target;
        const double error=std::max(std::abs(difference.lo),std::abs(difference.hi));
        if (error>limits.maximum_volume_error.value()) reject("FIXED_WIDTH_GLOBAL_VOLUME_ERROR");
        poll();auto snapshot=std::shared_ptr<const FixedWidthBeadSnapshot>(new FixedWidthBeadSnapshot(request,std::move(pieces),
            bounds(target),bounds(delivered),error,maximum_width_error,numeric));
        poll();return {"BOUNDED_CONSTANT_FLUX_FIXED_NOMINAL_WIDTH_CANDIDATE_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) { return {e.what(),{}}; }
    catch (const std::exception &e) { return {"FIXED_WIDTH_BEAD_NUMERIC_FAILURE: "+std::string(e.what()),{}}; }
}

FirstHatchBeadResult plan_first_hatch_bead(const AffineHatchResult &requested, size_t line_index,
                                         const FirstHatchBeadLimits &requested_limits)
{ return FirstHatchBeadSnapshot::plan(requested,line_index,requested_limits,FirstHatchRoofDomain::Centerline); }

FirstHatchBeadResult plan_first_hatch_footprint_bead(const AffineHatchResult &requested, size_t line_index,
                                                   const FirstHatchBeadLimits &requested_limits)
{ return FirstHatchBeadSnapshot::plan(requested,line_index,requested_limits,FirstHatchRoofDomain::FiniteWidth); }

FirstHatchBeadResult FirstHatchBeadSnapshot::plan(const AffineHatchResult &requested, std::optional<size_t> line_index,
    const FirstHatchBeadLimits &requested_limits, FirstHatchRoofDomain domain,const AffineHatchLine *slice,double slice_error)
{
    const auto source=requested.snapshot; const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!source || !source->source || source->passes.empty() || (line_index && *line_index>=source->passes.front().lines.size()) ||
            (!line_index && (!slice || domain!=FirstHatchRoofDomain::FiniteWidth)) ||
            !limits.max_evaluations || limits.max_evaluations>200000 || !valid_timeout(limits.timeout) ||
            !valid_timeout(limits.packets.timeout) || limits.maximum_gap_error.value()<=0 || limits.maximum_gap_error.value()>.05 ||
            !limits.max_roof_segments || limits.max_roof_segments>65535 || !limits.max_depth || limits.max_depth>32 ||
            !limits.packets.max_segments || limits.packets.max_segments>65535 || !limits.packets.max_depth || limits.packets.max_depth>32 ||
            limits.packets.maximum_width_error.value()<=0 || limits.packets.maximum_width_error.value()>.05 ||
            limits.packets.maximum_volume_error.value()<=0)
            reject("INVALID_FIRST_HATCH_BEAD");
        const auto stack=source->source;const auto cursor=stack->source;
        if (!cursor || !cursor->sequence || !stack->first_pass.proof || stack->first_pass.status!=MaterialIntegralStatus::Bounded)
            reject("FIRST_HATCH_MISSING_OWNED_ROOF_PROOF");
        const auto sequence=cursor->sequence; const auto line=slice ? *slice : source->passes.front().lines[*line_index];
        const bool x_axis=line.start.y()==line.end.y(), y_axis=line.start.x()==line.end.x();
        if (x_axis==y_axis) reject("FIRST_HATCH_REQUIRES_AXIS_ALIGNED_CENTERLINE");
        // Any admitted actual-gap width lies in nominal +/- this requested
        // error. Query its exact outer strip before deriving the gap/amount;
        // shrinking the query to the nominal centerline would be circular.
        const Exact half=(Exact(line.width.value())+Exact(limits.packets.maximum_width_error.value()))/Exact(2);
        const auto footprint=[&](PhysicalPosition a,PhysicalPosition b) {
            if (domain==FirstHatchRoofDomain::Centerline)
                return Polygon{{Exact(a.x()),Exact(a.y())},{Exact(b.x()),Exact(b.y())}};
            const Exact x0=Exact(std::min(a.x(),b.x()))-(y_axis ? half : Exact(0));
            const Exact x1=Exact(std::max(a.x(),b.x()))+(y_axis ? half : Exact(0));
            const Exact y0=Exact(std::min(a.y(),b.y()))-(x_axis ? half : Exact(0));
            const Exact y1=Exact(std::max(a.y(),b.y()))+(x_axis ? half : Exact(0));
            const auto &roi=stack->surfaces.front().cell.footprint;
            if (x0<Exact(roi.min_x) || x1>Exact(roi.max_x) || y0<Exact(roi.min_y) || y1>Exact(roi.max_y))
                reject("FIRST_HATCH_FINITE_FOOTPRINT_OUTSIDE_SUPPORTED_ROI");
            return Polygon{{x0,y0},{x1,y0},{x1,y1},{x0,y1}};
        };
        const auto poll=[&] { stop(limits,sequence->revision,started);stop(limits.packets,sequence->revision,started); };poll();
        const auto context=sequence_hash(*sequence,poll);
        size_t evaluations=0, segments=0;
        const auto evaluate=[&] {
            if (evaluations>=limits.max_evaluations) reject("FIRST_HATCH_ROOF_WORK_LIMIT");
            if (evaluations%128==0) poll();++evaluations;
        };
        struct Endpoint { PhysicalPosition point;double error; };
        const auto endpoint=[&](const Exact &t) {
            const auto x=stored_exact(Exact(line.start.x())+(Exact(line.end.x())-Exact(line.start.x()))*t);
            const auto y=stored_exact(Exact(line.start.y())+(Exact(line.end.y())-Exact(line.start.y()))*t);
            const auto z=stored_exact(Exact(line.start.z())+(Exact(line.end.z())-Exact(line.start.z()))*t);
            return Endpoint{{x.first,y.first,z.first},(Interval(x.second)+Interval(y.second)+Interval(z.second)).hi};
        };
        struct Node { Exact begin,end;Endpoint a,b;size_t depth;std::vector<size_t> candidates; };
        std::vector<size_t> active;
        const size_t end=cursor->completed_records+(cursor->current_progress>0 && cursor->completed_records<sequence->records.size());
        for (size_t i=0;i<end;++i) { if (i%128==0) poll();if (sequence->records[i].bead) active.push_back(i); }
        std::vector<Node> pending{{Exact(0),Exact(1),endpoint(Exact(0)),endpoint(Exact(1)),0,std::move(active)}};
        std::vector<FixedWidthBeadPiece> pieces;
        Exact target_lower(0),target_upper(0),deposited(0);double maximum_gap=0,maximum_width=0,coordinate_error=slice_error;
        const auto correction=Interval(1)-pi()/Interval(4);
        while (!pending.empty()) {
            poll();auto node=std::move(pending.back());pending.pop_back();
            auto roof_bound=nominal_roof_bounds(*cursor,footprint(node.a.point,node.b.point),node.candidates,stack->support_plane_z_mm,evaluate);
            const double lower=roof_bound.height.lo,upper=roof_bound.height.hi;
            auto candidates=std::move(roof_bound.active);
            // The owned parent proves D_lower covers the whole ROI at plane.
            // It is a lower bound on the nominal roof, never its actual value.
            if (candidates.empty() || lower>upper) reject("FIRST_HATCH_INCONSISTENT_NOMINAL_ROOF");
            const Interval roof(lower,upper);const double middle=(lower+upper)/2;
            const double h0=node.a.point.z()-middle,h1=node.b.point.z()-middle;
            const auto bottom0=Interval(node.a.point.z())-Interval(h0),bottom1=Interval(node.b.point.z())-Interval(h1);
            const Interval bottom(std::min(bottom0.lo,bottom1.lo),std::max(bottom0.hi,bottom1.hi));
            const auto gap_difference=bottom-roof;
            double gap_error=std::max(std::abs(gap_difference.lo),std::abs(gap_difference.hi));
            const auto fraction=exact_interval(node.end-node.begin);
            const double budget=(Interval(limits.packets.maximum_volume_error.value())*fraction).lo;
            bool accepted=false;FixedWidthBeadResult packets;Interval target(0);double width_error=0;
            if (h0>0 && h1>0 && std::max(h0,h1)<line.width.value() && gap_error<=limits.maximum_gap_error.value()) {
                auto packet_limits=limits.packets;
                if (pieces.size()>=packet_limits.max_segments) reject("FIRST_HATCH_PACKET_COUNT_LIMIT");
                packet_limits.max_segments-=pieces.size();packet_limits.maximum_volume_error=Volume(budget/4);
                packet_limits.timeout=std::min(limits.timeout,limits.packets.timeout)-
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
                packet_limits.cancelled=[&] { poll();return false; };packet_limits.is_current={};
                packets=plan_fixed_width_bead({node.a.point,node.b.point,line.width,VerticalGap(h0),VerticalGap(h1),
                    BeadSectionKind::RoundedRectangle,sequence->revision,context},packet_limits);
                poll();
                if (packets.snapshot) {
                    // Axis-aligned stored endpoints stay on the queried segment.
                    // Charge its Z/gap interpolation rounding to the roof model.
                    gap_error=(Interval(gap_error)+Interval(packets.snapshot->numerical_error_upper_mm)).hi;
                    bool domain=true;
                    for (const auto &piece : packets.snapshot->pieces) {
                        const auto within=[&](PhysicalPosition p) {
                            return p.x()>=std::min(node.a.point.x(),node.b.point.x()) && p.x()<=std::max(node.a.point.x(),node.b.point.x()) &&
                                p.y()>=std::min(node.a.point.y(),node.b.point.y()) && p.y()<=std::max(node.a.point.y(),node.b.point.y());
                        };
                        if (!within(piece.start) || !within(piece.end)) reject("FIRST_HATCH_PACKET_OUTSIDE_QUERIED_SEGMENT");
                        const auto hmin=Interval(std::min(piece.section.gap_begin_mm,piece.section.gap_end_mm))-Interval(gap_error);
                        const auto hmax=Interval(std::max(piece.section.gap_begin_mm,piece.section.gap_end_mm))+Interval(gap_error);
                        if (hmin.lo<=0) { domain=false;break; }
                        const auto area=Interval(piece.volume.value())/detail::root(length_squared(piece.start,piece.end));
                        if (area.lo<=(pi()/Interval(4)*square(hmax)).hi) { domain=false;break; }
                        const auto wmin=section_width(area,hmax,BeadSectionKind::RoundedRectangle),wmax=section_width(area,hmin,BeadSectionKind::RoundedRectangle);
                        width_error=std::max({width_error,(Interval(line.width.value())-wmin).hi,(wmax-Interval(line.width.value())).hi});
                    }
                    const auto length=detail::root(length_squared(node.a.point,node.b.point));
                    const auto maximum_h=Interval(std::max(h0,h1))+Interval(gap_error);
                    const auto uncertainty=length*Interval(gap_error)*(Interval(line.width.value())+Interval(2)*correction*maximum_h);
                    target=interval(packets.snapshot->target_volume_mm3)+Interval(-uncertainty.hi,uncertainty.hi);
                    const auto difference=interval(packets.snapshot->deposited_volume_mm3)-target;
                    accepted=domain && target.lo>0 && gap_error<=limits.maximum_gap_error.value() &&
                        width_error<=limits.packets.maximum_width_error.value() && std::max(std::abs(difference.lo),std::abs(difference.hi))<=budget;
                } else if (packets.reason!="FIXED_WIDTH_PACKET_COUNT_LIMIT" && packets.reason!="FIXED_WIDTH_PACKET_DEPTH_LIMIT")
                    throw Rejection(packets.reason);
            }
            if (!accepted) {
                if (node.depth>=limits.max_depth) reject("FIRST_HATCH_ROOF_DEPTH_LIMIT");
                if (segments+pending.size()+2>limits.max_roof_segments) reject("FIRST_HATCH_ROOF_SEGMENT_LIMIT");
                const Exact t=(node.begin+node.end)/Exact(2);const auto point=endpoint(t);
                pending.push_back({t,node.end,point,node.b,node.depth+1,candidates});
                pending.push_back({node.begin,t,node.a,point,node.depth+1,std::move(candidates)});continue;
            }
            ++segments;maximum_gap=std::max(maximum_gap,gap_error);maximum_width=std::max(maximum_width,width_error);
            coordinate_error=std::max({coordinate_error,node.a.error,node.b.error,packets.snapshot->numerical_error_upper_mm});
            target_lower+=Exact(target.lo);target_upper+=Exact(target.hi);
            for (auto piece : packets.snapshot->pieces) {
                deposited+=Exact(piece.volume.value());pieces.push_back(std::move(piece));
            }
        }
        const Interval target(exact_interval(target_lower).lo,exact_interval(target_upper).hi),delivered=exact_interval(deposited);
        const auto difference=delivered-target;const double error=std::max(std::abs(difference.lo),std::abs(difference.hi));
        // Both actual and affine-model widths lie within nominal +/- maximum_width.
        // Their edge difference is therefore at most maximum_width, not half it.
        const double numeric=(Interval(source->numerical_error_upper_mm)+Interval(coordinate_error)+Interval(maximum_gap)+Interval(maximum_width)).hi;
        if (error>limits.packets.maximum_volume_error.value()) reject("FIRST_HATCH_GLOBAL_VOLUME_ERROR");
        if (numeric>.05) reject("FIRST_HATCH_NUMERICAL_BUDGET");
        poll();auto snapshot=std::shared_ptr<const FirstHatchBeadSnapshot>(new FirstHatchBeadSnapshot(source,line_index,domain,line.start,line.end,std::move(pieces),
            bounds(target),bounds(delivered),maximum_gap,maximum_width,error,numeric,segments,evaluations));
        poll();return {domain==FirstHatchRoofDomain::FiniteWidth ? "BOUNDED_FIRST_FINITE_FOOTPRINT_NOMINAL_GAP_AND_AMOUNTS_ONLY" :
            "BOUNDED_FIRST_CENTERLINE_NOMINAL_ROOF_GAP_AND_AMOUNTS_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) { return {e.what(),{}}; }
    catch (const std::exception &e) { return {"FIRST_HATCH_BEAD_NUMERIC_FAILURE: "+std::string(e.what()),{}}; }
}

namespace {
enum class UnionClip { Box, BelowRoof, AboveSurface, TargetShadow };
struct UnionRoof {std::shared_ptr<const MaterialPrefixSnapshot> source;double minimum;};
struct UnionAmountsResult {
    std::string reason;
    std::optional<std::array<ScalarBounds,3>> amounts;
    size_t cells=0,evaluations=0;
    std::optional<ScalarBounds> provisional_union,provisional_excess;
};
UnionAmountsResult union_integral(const std::shared_ptr<const MaterialPrefixSnapshot> &cursor, const SceneBox &domain,
    const MaterialUnionLimits &limits, std::chrono::steady_clock::time_point started,
    UnionClip clip_kind=UnionClip::Box, const UnionRoof *roof_source=nullptr, const AffineCapCell *surface=nullptr)
{
    size_t evaluations=0,cells=0;
    std::optional<ScalarBounds> provisional_union,provisional_excess;
    try {
        detail::require_interval_environment();
        if (!cursor || !cursor->sequence || cursor->completed_records>cursor->sequence->records.size() ||
            cursor->sequence->records.size()>200000 || cursor->sequence->geometry.size()!=cursor->sequence->records.size() ||
            !std::isfinite(cursor->current_progress) || cursor->current_progress<0 || cursor->current_progress>1 ||
            (cursor->completed_records==cursor->sequence->records.size() && cursor->current_progress!=0) ||
            domain.min.x()>=domain.max.x() || domain.min.y()>=domain.max.y() || domain.min.z()>=domain.max.z() ||
            limits.maximum_interval_width.value()<=0 || !limits.max_evaluations || limits.max_evaluations>2000000 ||
            !limits.max_cells || limits.max_cells>65535 || !limits.max_depth || limits.max_depth>32 || !valid_timeout(limits.timeout))
            reject("INVALID_MATERIAL_UNION_INTEGRAL");
        for (auto p : {domain.min,domain.max}) {coordinate(p.x());coordinate(p.y());coordinate(p.z());}
        const auto sequence=cursor->sequence;const auto poll=[&] {stop(limits,sequence->revision,started);};poll();
        const auto evaluate=[&] {if (evaluations>=limits.max_evaluations) reject("MATERIAL_UNION_WORK_LIMIT");if (evaluations%128==0) poll();++evaluations;};
        const auto progress=[&](size_t i) {return i<cursor->completed_records ? 1 : cursor->current_progress;};
        Exact shear_x(0),shear_y(0);
        struct Coefficients {Interval area{0},start{0},along{0},normal{0},floor_start{0},floor_along{0};};
        std::vector<Coefficients> coefficients(sequence->records.size());
        const auto vertical=[&](size_t i,Projection projection,double fraction) {
            const auto &row=sequence->records[i];const auto &cached=coefficients[i];
            const auto roof=nominal_roof(row,projection,fraction,cached.area);
            if (!roof) return std::optional<std::pair<Interval,NominalRoof>>{};
            const auto t=detail::maximum(Interval(0),detail::minimum(projection.t,Interval(fraction)));
            const auto top=cached.start+cached.along*t-cached.normal*projection.normal;
            auto relative=*roof;relative.height=top-roof->depth_below_top;
            const auto bottom=cached.floor_start+cached.floor_along*t-cached.normal*projection.normal+roof->depth_below_top;
            return std::optional<std::pair<Interval,NominalRoof>>{{bottom,relative}};
        };
        const auto longitudinal=[&](const MaterialRecord &row,const Polygon &polygon,double fraction) {
            const Vertex d{Exact(row.motion.end.x())-Exact(row.motion.start.x()),Exact(row.motion.end.y())-Exact(row.motion.start.y())};
            const Exact start=d[0]*Exact(row.motion.start.x())+d[1]*Exact(row.motion.start.y()),last=start+Exact(fraction)*(d[0]*d[0]+d[1]*d[1]);
            return std::all_of(polygon.begin(),polygon.end(),[&](const Vertex &p) {const Exact t=d[0]*p[0]+d[1]*p[1];return t>=start && t<=last;});
        };
        using Span=std::pair<double,double>;
        const auto measure=[](std::vector<Span> spans) {
            std::sort(spans.begin(),spans.end());Exact total(0);bool have=false;double begin=0,end=0;
            for (auto s : spans) {
                if (s.first>=s.second) continue;
                if (!have) {begin=s.first;end=s.second;have=true;}
                else if (s.first<=end) end=std::max(end,s.second);
                else {total+=Exact(end)-Exact(begin);begin=s.first;end=s.second;}
            }
            if (have) total+=Exact(end)-Exact(begin);return total;
        };
        std::optional<Exact> full_individual=clip_kind==UnionClip::Box ? std::optional<Exact>{Exact(0)} : std::nullopt;std::vector<size_t> active;
        std::vector<std::array<double,4>> xy_bounds(sequence->records.size());
        std::vector<bool> whole_vertical(sequence->records.size(),false);
        const size_t end=cursor->completed_records+(cursor->current_progress>0 && cursor->completed_records<sequence->records.size());
        for (size_t i=0;i<end;++i) {
            if (i%128==0) poll();const auto &row=sequence->records[i];if (!row.bead) continue;evaluate();
            const double fraction=progress(i);const auto &m=row.motion;const auto &b=*row.bead;
            const auto length=detail::root(length_squared(row));
            coefficients[i].area=Interval(std::get<Deposition>(m.payload).volume.value())/length;
            const auto h=Interval(b.gap_begin_mm)+(Interval(b.gap_end_mm)-Interval(b.gap_begin_mm))*Interval(fraction);
            const auto half=section_width(Interval(std::get<Deposition>(m.payload).volume.value())/length,
                detail::minimum(Interval(b.gap_begin_mm),h),b.kind)/Interval(2);
            const auto x=exact_interval(Exact(m.start.x())+(Exact(m.end.x())-Exact(m.start.x()))*Exact(fraction));
            const auto y=exact_interval(Exact(m.start.y())+(Exact(m.end.y())-Exact(m.start.y()))*Exact(fraction));
            const auto z=exact_interval(Exact(m.start.z())+(Exact(m.end.z())-Exact(m.start.z()))*Exact(fraction));
            const double x_growth=m.start.y()==m.end.y() ? 0 : (absolute(Interval(m.end.y())-Interval(m.start.y()))/length*half).hi;
            const double y_growth=m.start.x()==m.end.x() ? 0 : (absolute(Interval(m.end.x())-Interval(m.start.x()))/length*half).hi;
            const auto xmin=exact_interval(Exact(std::min(m.start.x(),x.lo))-Exact(x_growth)),xmax=exact_interval(Exact(std::max(m.start.x(),x.hi))+Exact(x_growth));
            const auto ymin=exact_interval(Exact(std::min(m.start.y(),y.lo))-Exact(y_growth)),ymax=exact_interval(Exact(std::max(m.start.y(),y.hi))+Exact(y_growth));
            const double zmin=(Interval(std::min(m.start.z(),z.lo))-detail::maximum(Interval(b.gap_begin_mm),h)).lo,zmax=std::max(m.start.z(),z.hi);
            if (xmax.hi<domain.min.x() || xmin.lo>domain.max.x() || ymax.hi<domain.min.y() || ymin.lo>domain.max.y() || zmax<domain.min.z() || zmin>domain.max.z()) continue;
            active.push_back(i);
            xy_bounds[i]={xmin.lo,xmax.hi,ymin.lo,ymax.hi};
            whole_vertical[i]=zmin>=domain.min.z() && zmax<=domain.max.z();
            if (full_individual) {
                if (xmin.lo>=domain.min.x() && xmax.hi<=domain.max.x() && ymin.lo>=domain.min.y() && ymax.hi<=domain.max.y() && zmin>=domain.min.z() && zmax<=domain.max.z())
                    *full_individual+=Exact(std::get<Deposition>(m.payload).volume.value())*Exact(fraction);
                else full_individual.reset();
            }
        }
        if (clip_kind==UnionClip::AboveSurface || clip_kind==UnionClip::TargetShadow) {
            const auto &r=surface->footprint;
            shear_x=(Exact(surface->z10)-Exact(surface->z00))/(Exact(r.max_x)-Exact(r.min_x));
            shear_y=(Exact(surface->z01)-Exact(surface->z00))/(Exact(r.max_y)-Exact(r.min_y));
        } else if (clip_kind==UnionClip::Box && !active.empty()) {
            const auto &m=sequence->records[active.front()].motion;
            const Exact dx=Exact(m.end.x())-Exact(m.start.x()),dy=Exact(m.end.y())-Exact(m.start.y()),dz=Exact(m.end.z())-Exact(m.start.z());
            shear_x=dz*dx/(dx*dx+dy*dy);shear_y=dz*dy/(dx*dx+dy*dy);
        }
        for (size_t i : active) {
            evaluate();const auto &m=sequence->records[i].motion;auto &cached=coefficients[i];
            const Exact dx=Exact(m.end.x())-Exact(m.start.x()),dy=Exact(m.end.y())-Exact(m.start.y());
            cached.start=exact_interval(Exact(m.start.z())-shear_x*Exact(m.start.x())-shear_y*Exact(m.start.y()));
            cached.along=exact_interval(Exact(m.end.z())-Exact(m.start.z())-shear_x*dx-shear_y*dy);
            cached.normal=exact_interval(-shear_x*dy+shear_y*dx)/detail::root(length_squared(sequence->records[i]));
            const auto &b=*sequence->records[i].bead;
            cached.floor_start=exact_interval(Exact(m.start.z())-Exact(b.gap_begin_mm)-shear_x*Exact(m.start.x())-shear_y*Exact(m.start.y()));
            cached.floor_along=exact_interval(Exact(m.end.z())-Exact(m.start.z())-Exact(b.gap_end_mm)+Exact(b.gap_begin_mm)-shear_x*dx-shear_y*dy);
        }
        const auto project_bounds=[&](const MaterialRecord &row,const std::array<double,4> &box) {
            const auto &m=row.motion;const bool x_axis=m.start.y()==m.end.y();
            const auto delta=x_axis ? Interval(m.end.x())-Interval(m.start.x()) : Interval(m.end.y())-Interval(m.start.y());
            const size_t axis=x_axis ? 0 : 1;
            auto along=Interval(box[axis*2],box[axis*2+1])-Interval(x_axis ? m.start.x() : m.start.y());
            auto normal=Interval(box[(1-axis)*2],box[(1-axis)*2+1])-Interval(x_axis ? m.start.y() : m.start.x());
            if (delta.hi<0) along=Interval(0)-along;
            if (x_axis ? delta.hi<0 : delta.lo>0) normal=Interval(0)-normal;
            return Projection{along/absolute(delta),normal,Interval(0)};
        };
        if (clip_kind==UnionClip::Box && full_individual && !active.empty()) {
            // A closed axis-aligned constant-height stadium loop with merged
            // long-side flat cores is one full central prism plus one long
            // section and the two outer half short-side sections. Opposite
            // short sides must remain disjoint. Preserve actual binary amounts.
            if (active.size()==4) {
                std::vector<size_t> xs,ys;bool loop=true;
                const auto &first=sequence->records[active.front()];const double z=first.motion.start.z(),h=first.bead->gap_begin_mm;
                for (size_t i : active) {
                    evaluate();const auto &row=sequence->records[i];const auto &m=row.motion;const auto &b=*row.bead;
                    const bool x=m.start.y()==m.end.y(),y=m.start.x()==m.end.x();
                    if (x==y || progress(i)!=1 || b.kind!=BeadSectionKind::RoundedRectangle || b.gap_begin_mm!=h || b.gap_end_mm!=h ||
                        m.start.z()!=z || m.end.z()!=z) {loop=false;break;}
                    (x ? xs : ys).push_back(i);
                }
                if (loop && xs.size()==2 && ys.size()==2) {
                    const auto &rows=sequence->records;
                    if (rows[xs[0]].motion.start.y()>rows[xs[1]].motion.start.y()) std::swap(xs[0],xs[1]);
                    if (rows[ys[0]].motion.start.x()>rows[ys[1]].motion.start.x()) std::swap(ys[0],ys[1]);
                    const auto &x0=rows[xs[0]].motion,&x1=rows[xs[1]].motion,&y0=rows[ys[0]].motion,&y1=rows[ys[1]].motion;
                    const double xmin=std::min(x0.start.x(),x0.end.x()),xmax=std::max(x0.start.x(),x0.end.x());
                    const double ymin=std::min(y0.start.y(),y0.end.y()),ymax=std::max(y0.start.y(),y0.end.y());
                    const auto amount=[](const MotionEvent &m) {return std::get<Deposition>(m.payload).volume.value();};
                    loop=std::min(x1.start.x(),x1.end.x())==xmin && std::max(x1.start.x(),x1.end.x())==xmax &&
                        std::min(y1.start.y(),y1.end.y())==ymin && std::max(y1.start.y(),y1.end.y())==ymax &&
                        x0.start.y()==ymin && x1.start.y()==ymax && y0.start.x()==xmin && y1.start.x()==xmax &&
                        amount(x0)==amount(x1) && amount(y0)==amount(y1);
                    const Exact dx=Exact(xmax)-Exact(xmin),dy=Exact(ymax)-Exact(ymin);
                    if (loop) {
                        const bool long_x=dx>=dy;const Exact a=long_x ? dx : dy,b=long_x ? dy : dx;
                        const Exact long_amount(amount(long_x ? x0 : y0)),short_amount(amount(long_x ? y0 : x0));
                        const auto core=section_width(exact_interval(long_amount/a),Interval(h),BeadSectionKind::RoundedRectangle)-Interval(h);
                        const auto short_width=section_width(exact_interval(short_amount/b),Interval(h),BeadSectionKind::RoundedRectangle);
                        if (exact_interval(b).hi<=core.lo && exact_interval(a).lo>=short_width.hi) {
                            if (4>limits.max_cells-cells) reject("MATERIAL_UNION_CELL_LIMIT");cells+=4;
                            const Exact volume=long_amount+short_amount+a*b*Exact(h);
                            const auto occupied=exact_interval(volume),individual=exact_interval(*full_individual),repeated=exact_interval(*full_individual-volume);
                            for (auto v : {occupied,individual,repeated}) if (v.lo<0 || (Interval(v.hi)-Interval(v.lo)).hi>limits.maximum_interval_width.value())
                                reject("MATERIAL_UNION_CLOSED_LOOP_PRECISION");
                            provisional_union=bounds(occupied);provisional_excess=bounds(repeated);poll();
                            return {"BOUNDED_CLIPPED_NOMINAL_UNION_AND_MULTIPLICITY_EXCESS_ONLY",
                                std::array<ScalarBounds,3>{bounds(occupied),bounds(individual),bounds(repeated)},cells,evaluations,provisional_union,provisional_excess};
                        }
                        // Otherwise integrate exact 2D rectangle-union areas
                        // over section height. g_long=c_long+t, g_short=c_short+t,
                        // t=sqrt(d*(h-d)). This area is concave on half-height
                        // only under the two certified derivative conditions.
                        const auto aa=exact_interval(a),bb=exact_interval(b),c=core/Interval(2),e=(short_width-Interval(h))/Interval(2);
                        const auto derivative=Interval(4)*(aa+bb-c-e),jump=aa+Interval(2)*c-Interval(2)*e-bb;
                        if (c.lo>=0 && e.lo>=0 && aa.lo>=short_width.hi && derivative.lo>=(Interval(4)*Interval(h)).hi && jump.lo>=0) {
                            const auto area=[&](const Exact &d) {
                                evaluate();const auto t=detail::root(exact_interval(d*(Exact(h)-d)));
                                const auto g=c+t,q=e+t;
                                return aa*detail::minimum(Interval(4)*g,bb+Interval(2)*g)+Interval(4)*q*bb-
                                    Interval(2)*q*detail::minimum(bb,Interval(2)*g);
                            };
                            struct HeightCell {Exact begin,end;Interval volume;size_t depth;};
                            const auto cell=[&](Exact lo,Exact hi,size_t depth) {
                                if (cells>=limits.max_cells) reject("MATERIAL_UNION_CELL_LIMIT");++cells;
                                const auto left=area(lo),right=area(hi),middle=area((lo+hi)/Exact(2)),length=exact_interval(hi-lo);
                                const auto lower=length*(left+right),upper=Interval(2)*length*middle; // Both halves.
                                return HeightCell{lo,hi,{lower.lo,upper.hi},depth};
                            };
                            const auto less=[](const HeightCell &l,const HeightCell &r) {return l.volume.hi-l.volume.lo<r.volume.hi-r.volume.lo;};
                            std::vector<HeightCell> heap{cell(Exact(0),Exact(h)/Exact(2),0)};
                            Exact low(heap[0].volume.lo),high(heap[0].volume.hi);
                            for (;;) {
                                poll();const auto individual=exact_interval(*full_individual);
                                const auto raw=Interval(exact_interval(low).lo,exact_interval(high).hi);
                                if (raw.lo>individual.hi) reject("MATERIAL_UNION_CLOSED_LOOP_INCONSISTENT");
                                const Interval united(std::max(0.,raw.lo),std::min(raw.hi,individual.hi));
                                const auto excess=individual-united;
                                const Interval repeated(std::max(0.,excess.lo),std::max(0.,excess.hi));
                                provisional_union=bounds(united);provisional_excess=bounds(repeated);
                                if ((Interval(united.hi)-Interval(united.lo)).hi<=limits.maximum_interval_width.value() &&
                                    (Interval(repeated.hi)-Interval(repeated.lo)).hi<=limits.maximum_interval_width.value()) {
                                    poll();return {"BOUNDED_CLIPPED_NOMINAL_UNION_AND_MULTIPLICITY_EXCESS_ONLY",
                                        std::array<ScalarBounds,3>{bounds(united),bounds(individual),bounds(repeated)},cells,evaluations,provisional_union,provisional_excess};
                                }
                                std::pop_heap(heap.begin(),heap.end(),less);auto old=std::move(heap.back());heap.pop_back();
                                if (old.depth>=limits.max_depth) reject("MATERIAL_UNION_DEPTH_LIMIT");
                                const Exact middle=(old.begin+old.end)/Exact(2);
                                const auto left=cell(old.begin,middle,old.depth+1),right=cell(middle,old.end,old.depth+1);
                                low+=Exact(left.volume.lo)+Exact(right.volume.lo)-Exact(old.volume.lo);
                                high+=Exact(left.volume.hi)+Exact(right.volume.hi)-Exact(old.volume.hi);
                                for (auto next : {left,right}) {heap.push_back(std::move(next));std::push_heap(heap.begin(),heap.end(),less);}
                            }
                        }
                    }
                }
            }
            // Two congruent stadia shifted by d within their flat core have
            // union area A+h*d. Integrate the affine gap exactly over aligned,
            // interior-disjoint packets. No nominal geometry is approximated.
            std::map<std::pair<bool,double>,std::vector<size_t>> groups;bool supported=true;
            for (size_t i : active) {
                evaluate();const auto &row=sequence->records[i];const auto &m=row.motion;
                const bool x=m.start.y()==m.end.y(),y=m.start.x()==m.end.x();
                if (x==y || progress(i)!=1 || row.bead->kind!=BeadSectionKind::RoundedRectangle) {supported=false;break;}
                groups[{x,x ? m.start.y() : m.start.x()}].push_back(i);
            }
            if (supported && groups.size()==2 && groups.begin()->first.first==groups.rbegin()->first.first &&
                groups.begin()->second.size()==groups.rbegin()->second.size()) {
                const bool x=groups.begin()->first.first;
                const auto coordinate=[&](PhysicalPosition p) {return x ? p.x() : p.y();};
                for (auto &group : groups) std::sort(group.second.begin(),group.second.end(),[&](size_t a,size_t b) {
                    evaluate();const auto &ma=sequence->records[a].motion,&mb=sequence->records[b].motion;
                    return std::min(coordinate(ma.start),coordinate(ma.end))<std::min(coordinate(mb.start),coordinate(mb.end));
                });
                const Exact separation=Exact(groups.rbegin()->first.second)-Exact(groups.begin()->first.second);
                Exact occupied(0);std::optional<Exact> previous;
                for (size_t n=0;n<groups.begin()->second.size();++n) {
                    evaluate();const auto &a=sequence->records[groups.begin()->second[n]],&b=sequence->records[groups.rbegin()->second[n]];
                    const auto &ma=a.motion,&mb=b.motion;const auto &sa=*a.bead,&sb=*b.bead;
                    const Exact begin(std::min(coordinate(ma.start),coordinate(ma.end))),end(std::max(coordinate(ma.start),coordinate(ma.end)));
                    const double amount=std::get<Deposition>(ma.payload).volume.value();
                    if ((previous && begin<*previous) || coordinate(ma.start)!=coordinate(mb.start) || coordinate(ma.end)!=coordinate(mb.end) ||
                        ma.start.z()!=mb.start.z() || ma.end.z()!=mb.end.z() || sa.gap_begin_mm!=sb.gap_begin_mm || sa.gap_end_mm!=sb.gap_end_mm ||
                        amount!=std::get<Deposition>(mb.payload).volume.value()) {supported=false;break;}
                    const auto maximum_gap=Interval(std::max(sa.gap_begin_mm,sa.gap_end_mm));
                    const auto core=section_width(exact_interval(Exact(amount)/(end-begin)),maximum_gap,sa.kind)-maximum_gap;
                    if (exact_interval(separation).hi>core.lo) {supported=false;break;}
                    occupied+=Exact(amount)+separation*(end-begin)*(Exact(sa.gap_begin_mm)+Exact(sa.gap_end_mm))/Exact(2);previous=end;
                }
                if (supported) {
                    const size_t count=groups.begin()->second.size();if (count>limits.max_cells-cells) reject("MATERIAL_UNION_CELL_LIMIT");cells+=count;
                    const auto united=exact_interval(occupied),individual=exact_interval(*full_individual),repeated=exact_interval(*full_individual-occupied);
                    for (auto v : {united,individual,repeated}) if (v.lo<0 || (Interval(v.hi)-Interval(v.lo)).hi>limits.maximum_interval_width.value())
                        reject("MATERIAL_UNION_CONGRUENT_PRECISION");
                    provisional_union=bounds(united);provisional_excess=bounds(repeated);poll();
                    return {"BOUNDED_CLIPPED_NOMINAL_UNION_AND_MULTIPLICITY_EXCESS_ONLY",
                        std::array<ScalarBounds,3>{bounds(united),bounds(individual),bounds(repeated)},cells,evaluations,provisional_union,provisional_excess};
                }
            }
            // Parallel stadia sharing affine top/gap graphs fill the span
            // between their centres when adjacent flat cores merge. Unequal
            // actual doses are allowed only if every outer shoulder retains
            // its centre order throughout the whole positive gap interval.
            if (supported && groups.size()>=3 && groups.begin()->first.first==groups.rbegin()->first.first) {
                const bool x=groups.begin()->first.first;bool qualified=true;
                struct Profile {double lo,hi,z0,z1,h0,h1,amount;};
                const auto profile=[&](size_t i) {
                    const auto &m=sequence->records[i].motion;const auto &b=*sequence->records[i].bead;
                    const double a=x ? m.start.x() : m.start.y(),z=x ? m.end.x() : m.end.y();const bool forward=a<z;
                    return Profile{std::min(a,z),std::max(a,z),forward ? m.start.z() : m.end.z(),forward ? m.end.z() : m.start.z(),
                        forward ? b.gap_begin_mm : b.gap_end_mm,forward ? b.gap_end_mm : b.gap_begin_mm,std::get<Deposition>(m.payload).volume.value()};
                };
                for (auto &g : groups) {
                    evaluate();if (g.second.size()!=groups.begin()->second.size()) {qualified=false;break;}
                    std::sort(g.second.begin(),g.second.end(),[&](size_t a,size_t b) {evaluate();return profile(a).lo<profile(b).lo;});
                }
                Exact occupied(0);std::optional<double> previous;
                const Exact span=Exact(groups.rbegin()->first.second)-Exact(groups.begin()->first.second);
                for (size_t n=0;qualified && n<groups.begin()->second.size();++n) {
                    evaluate();const auto first=profile(groups.begin()->second[n]),last=profile(groups.rbegin()->second[n]);
                    if (previous && first.lo<*previous) {qualified=false;break;}
                    const Exact length=Exact(first.hi)-Exact(first.lo),minimum_gap(std::min(first.h0,first.h1));
                    const Interval maximum_gap(std::max(first.h0,first.h1));
                    std::optional<std::pair<double,Profile>> prior;
                    for (const auto &g : groups) {
                        evaluate();const auto p=profile(g.second[n]);
                        if (std::tie(p.lo,p.hi,p.z0,p.z1,p.h0,p.h1)!=std::tie(first.lo,first.hi,first.z0,first.z1,first.h0,first.h1)) {qualified=false;break;}
                        if (prior) {
                            const Exact separation=Exact(g.first.second)-Exact(prior->first);
                            const auto a=section_width(exact_interval(Exact(p.amount)/length),maximum_gap,BeadSectionKind::RoundedRectangle)-maximum_gap;
                            const auto b=section_width(exact_interval(Exact(prior->second.amount)/length),maximum_gap,BeadSectionKind::RoundedRectangle)-maximum_gap;
                            if (a.lo<0 || b.lo<0 || exact_interval(separation).hi>((a+b)/Interval(2)).lo ||
                                separation<abs(Exact(p.amount)-Exact(prior->second.amount))/(Exact(2)*length*minimum_gap)) {qualified=false;break;}
                        }
                        prior=std::pair<double,Profile>{g.first.second,p};
                    }
                    if (!qualified) break;
                    occupied+=(Exact(first.amount)+Exact(last.amount))/Exact(2)+span*length*(Exact(first.h0)+Exact(first.h1))/Exact(2);previous=first.hi;
                }
                if (qualified) {
                    const size_t count=active.size();if (count>limits.max_cells-cells) reject("MATERIAL_UNION_CELL_LIMIT");cells+=count;
                    const auto united=exact_interval(occupied),individual=exact_interval(*full_individual),repeated=exact_interval(*full_individual-occupied);
                    for (auto v : {united,individual,repeated}) if (v.lo<0 || (Interval(v.hi)-Interval(v.lo)).hi>limits.maximum_interval_width.value())
                        reject("MATERIAL_UNION_PARALLEL_PRECISION");
                    provisional_union=bounds(united);provisional_excess=bounds(repeated);poll();
                    return {"BOUNDED_CLIPPED_NOMINAL_UNION_AND_MULTIPLICITY_EXCESS_ONLY",
                        std::array<ScalarBounds,3>{bounds(united),bounds(individual),bounds(repeated)},cells,evaluations,provisional_union,provisional_excess};
                }
            }
            // A loop with congruent primary packets and two constant
            // transverse ends. Adjacent flat cores must merge, so the primary
            // union fills its whole centre span even with multiple infill rows
            // between affine top/bottom graphs. Each end intersection therefore
            // reduces to a bounded convex XZ section, extruded over that span.
            if (supported && groups.size()>=4) {
                using Group=decltype(groups)::value_type;
                std::vector<const Group *> xx,yy;
                for (const auto &g : groups) (g.first.first ? xx : yy).push_back(&g);
                const auto extent=[&](const Group &g,bool x) {
                    double lo=std::numeric_limits<double>::max(),hi=-lo;
                    for (size_t i : g.second) {evaluate();const auto &m=sequence->records[i].motion;
                        lo=std::min({lo,x ? m.start.x() : m.start.y(),x ? m.end.x() : m.end.y()});
                        hi=std::max({hi,x ? m.start.x() : m.start.y(),x ? m.end.x() : m.end.y()});}
                    return std::pair<double,double>{lo,hi};
                };
                if ((xx.size()>=2 && yy.size()==2) || (yy.size()>=2 && xx.size()==2)) {
                    const auto xspan=extent(*xx[0],true),yspan=extent(*yy[0],false);
                    const bool x=xx.size()>2 || (yy.size()==2 &&
                        (Exact(xspan.second)-Exact(xspan.first))>=(Exact(yspan.second)-Exact(yspan.first)));
                    const auto &main=x ? xx : yy,&ends=x ? yy : xx;
                    if (ends.size()==2 && ends[0]->second.size()==1 && ends[1]->second.size()==1) {
                        struct LoopPacket {double lo,hi,z0,z1,h0,h1,amount;};
                        const auto packet=[&](size_t i) {
                            const auto &m=sequence->records[i].motion;const auto &b=*sequence->records[i].bead;
                            const double a=x ? m.start.x() : m.start.y(),z=x ? m.end.x() : m.end.y();const bool forward=a<z;
                            return LoopPacket{std::min(a,z),std::max(a,z),forward ? m.start.z() : m.end.z(),forward ? m.end.z() : m.start.z(),
                                forward ? b.gap_begin_mm : b.gap_end_mm,forward ? b.gap_end_mm : b.gap_begin_mm,std::get<Deposition>(m.payload).volume.value()};
                        };
                        const double begin=ends[0]->first.second,last=ends[1]->first.second;
                        std::vector<std::vector<size_t>> lists;bool loop=begin<last,extended=false;
                        for (const auto *group : main) {
                            evaluate();lists.emplace_back();
                            for (size_t i : group->second) {
                                evaluate();const auto p=packet(i);
                                if (p.lo>=begin && p.hi<=last) lists.back().push_back(i);
                                else if ((p.hi<=begin || p.lo>=last) && group!=main.front() && group!=main.back()) extended=true;
                                else {loop=false;break;}
                            }
                            if (!loop || lists.back().empty() || lists.back().size()!=lists.front().size()) {loop=false;break;}
                            std::sort(lists.back().begin(),lists.back().end(),[&](size_t a,size_t b) {evaluate();return packet(a).lo<packet(b).lo;});
                        }
                        const Exact span=Exact(main.back()->first.second)-Exact(main.front()->first.second);std::vector<LoopPacket> lofts;Exact volume(0);
                        for (size_t n=0;loop && n<lists.front().size();++n) {
                            evaluate();const auto a=packet(lists.front()[n]);
                            if (!lofts.empty() && (a.lo!=lofts.back().hi || a.z0!=lofts.back().z1 || a.h0!=lofts.back().h1)) {loop=false;break;}
                            const auto hmax=Interval(std::max(a.h0,a.h1));
                            const auto core=section_width(exact_interval(Exact(a.amount)/(Exact(a.hi)-Exact(a.lo))),hmax,BeadSectionKind::RoundedRectangle)-hmax;
                            for (size_t g=1;g<lists.size();++g) {
                                evaluate();const auto b=packet(lists[g][n]);
                                if (std::tie(a.lo,a.hi,a.z0,a.z1,a.h0,a.h1,a.amount)!=std::tie(b.lo,b.hi,b.z0,b.z1,b.h0,b.h1,b.amount) ||
                                    exact_interval(Exact(main[g]->first.second)-Exact(main[g-1]->first.second)).hi>core.lo) {loop=false;break;}
                            }
                            if (!loop) break;
                            volume+=Exact(a.amount)+span*(Exact(a.hi)-Exact(a.lo))*(Exact(a.h0)+Exact(a.h1))/Exact(2);lofts.push_back(a);
                        }
                        struct LoopEnd {Exact centre,zmid;Interval core,radius,half;};std::vector<LoopEnd> sides;Exact end_amount(0);
                        for (size_t n=0;loop && n<2;++n) {
                            evaluate();const auto &row=sequence->records[ends[n]->second[0]];const auto &m=row.motion;const auto &b=*row.bead;
                            const double a=x ? m.start.y() : m.start.x(),z=x ? m.end.y() : m.end.x();
                            if (std::min(a,z)!=main.front()->first.second || std::max(a,z)!=main.back()->first.second || m.start.z()!=m.end.z() ||
                                b.gap_begin_mm!=b.gap_end_mm || ends[n]->first.second!=(n ? lofts.back().hi : lofts.front().lo) ||
                                m.start.z()!=(n ? lofts.back().z1 : lofts.front().z0)) {loop=false;break;}
                            const Exact amount(std::get<Deposition>(m.payload).volume.value());volume+=amount;end_amount+=amount;
                            const auto h=Interval(b.gap_begin_mm),r=exact_interval(Exact(b.gap_begin_mm)/Exact(2));
                            const auto c=(section_width(exact_interval(amount/span),h,b.kind)-h)/Interval(2);
                            if (c.lo<0) {loop=false;break;}
                            sides.push_back({Exact(ends[n]->first.second),Exact(m.start.z())-Exact(b.gap_begin_mm)/Exact(2),c,r,c+r});
                        }
                        if (loop && exact_interval(Exact(lofts.back().hi)-Exact(lofts.front().lo)).lo>=(sides[0].half+sides[1].half).hi) {
                            if (lofts.size()>limits.max_cells-cells) reject("MATERIAL_UNION_CELL_LIMIT");cells+=lofts.size();
                            const auto fibre=[&](size_t p,size_t e,const Exact &lo,const Exact &hi) {
                                evaluate();const auto &a=lofts[p];const auto &b=sides[e];
                                const auto graph=[&](const Exact &v,bool bottom) {
                                    const Exact t=(v-Exact(a.lo))/(Exact(a.hi)-Exact(a.lo));
                                    const Exact z=Exact(a.z0)+(Exact(a.z1)-Exact(a.z0))*t;
                                    return z-(bottom ? Exact(a.h0)+(Exact(a.h1)-Exact(a.h0))*t : Exact(0));
                                };
                                const auto tl=graph(lo,false),tr=graph(hi,false),bl=graph(lo,true),br=graph(hi,true);
                                const Interval top(exact_interval(std::min(tl,tr)).lo,exact_interval(std::max(tl,tr)).hi);
                                const Interval bottom(exact_interval(std::min(bl,br)).lo,exact_interval(std::max(bl,br)).hi);
                                const auto normal=absolute(Interval(exact_interval(lo-b.centre).lo,exact_interval(hi-b.centre).hi));
                                const auto offset=detail::maximum(Interval(0),normal-b.core),radicand=square(b.radius)-square(offset);
                                const auto depth=radicand.hi<0 ? Interval(0) : detail::root({std::max(0.,radicand.lo),radicand.hi});
                                const auto centre=exact_interval(b.zmid),ytop=centre+depth,ybottom=centre-depth;
                                return Interval(std::max(0.,(detail::minimum(top,ytop)-detail::maximum(bottom,ybottom)).lo),
                                    std::max(0.,(detail::minimum(top,ytop)-detail::maximum(bottom,ybottom)).hi));
                            };
                            struct IntersectionCell {Exact begin,end;size_t packet,side,depth;Interval volume;};
                            const auto cell=[&](Exact lo,Exact hi,size_t p,size_t e,size_t depth) {
                                if (cells>=limits.max_cells) reject("MATERIAL_UNION_CELL_LIMIT");++cells;
                                const auto length=exact_interval((hi-lo)*span),whole=length*fibre(p,e,lo,hi);
                                const auto a=fibre(p,e,lo,lo),b=fibre(p,e,hi,hi);Interval amount=whole;
                                // Convex-set section length is concave wherever
                                // both ends have certified nonempty intersection.
                                if (a.lo>0 && b.lo>0) {
                                    const auto lower=length*(a+b)/Interval(2),upper=length*fibre(p,e,(lo+hi)/Exact(2),(lo+hi)/Exact(2));
                                    amount={std::max(whole.lo,lower.lo),std::min(whole.hi,upper.hi)};
                                }
                                return IntersectionCell{lo,hi,p,e,depth,amount};
                            };
                            const auto less=[](const IntersectionCell &a,const IntersectionCell &b) {return a.volume.hi-a.volume.lo<b.volume.hi-b.volume.lo;};
                            std::vector<IntersectionCell> heap;Exact lower(0),upper(0);
                            for (size_t p=0;p<lofts.size();++p) for (size_t e=0;e<2;++e) {
                                evaluate();const auto &end=sides[e];const Exact lo=std::max(Exact(lofts[p].lo),end.centre-Exact(end.half.hi));
                                const Exact hi=std::min(Exact(lofts[p].hi),end.centre+Exact(end.half.hi));
                                if (lo>=hi) continue;auto next=cell(lo,hi,p,e,0);lower+=Exact(next.volume.lo);upper+=Exact(next.volume.hi);
                                heap.push_back(std::move(next));std::push_heap(heap.begin(),heap.end(),less);
                            }
                            for (;;) {
                                poll();const auto individual=exact_interval(*full_individual),raw=Interval(exact_interval(volume-upper).lo,exact_interval(volume-lower).hi);
                                if (raw.lo>individual.hi) reject("MATERIAL_UNION_LOFT_LOOP_INCONSISTENT");
                                const Interval united(std::max(0.,raw.lo),std::min(raw.hi,individual.hi));const auto excess=individual-united;
                                const Interval repeated(std::max(0.,excess.lo),std::max(0.,excess.hi));
                                if (!extended) {provisional_union=bounds(united);provisional_excess=bounds(repeated);}
                                const double precision=extended ? (Interval(limits.maximum_interval_width.value())/Interval(8)).lo : limits.maximum_interval_width.value();
                                if ((Interval(united.hi)-Interval(united.lo)).hi<=precision &&
                                    (Interval(repeated.hi)-Interval(repeated.lo)).hi<=precision) {
                                    if (extended) {
                                        // Disjoint spatial partition at the original end centrelines.
                                        // The old loop formula includes each outer half end section;
                                        // remove those halves before adding the two clipped new unions.
                                        // All children share the original deadline and remaining work.
                                        Interval total=united-exact_interval(end_amount/Exact(2));
                                        // Reserve the measured central width and one percent for
                                        // outward arithmetic; share only the remaining width.
                                        const double end_precision=((Interval(limits.maximum_interval_width.value())-
                                            (Interval(united.hi)-Interval(united.lo)))*Interval(.99)/Interval(2)).lo;
                                        for (size_t e=0;e<2;++e) {
                                            poll();if (cells>=limits.max_cells || evaluations>=limits.max_evaluations) reject("MATERIAL_UNION_WORK_LIMIT");
                                            auto remaining=limits;remaining.max_cells-=cells;remaining.max_evaluations-=evaluations;
                                            remaining.maximum_interval_width=Volume(end_precision);
                                            const double cut=e ? last : begin;
                                            const SceneBox part{
                                                PhysicalPosition{e && x ? cut : domain.min.x(),e && !x ? cut : domain.min.y(),domain.min.z()},
                                                PhysicalPosition{!e && x ? cut : domain.max.x(),!e && !x ? cut : domain.max.y(),domain.max.z()}};
                                            const auto child=union_integral(cursor,part,remaining,started);
                                            cells+=child.cells;evaluations+=child.evaluations;
                                            if (!child.amounts) throw Rejection(child.reason+" end="+std::to_string(e)+
                                                (child.provisional_union ? " width="+std::to_string(child.provisional_union->upper-child.provisional_union->lower) : ""));
                                            poll();
                                            total=total+interval((*child.amounts)[0]);
                                        }
                                        const auto excess=individual-total;
                                        const Interval repeated_total(std::max(0.,excess.lo),std::max(0.,excess.hi));
                                        if (total.lo<0 || total.lo>individual.hi ||
                                            (Interval(total.hi)-Interval(total.lo)).hi>limits.maximum_interval_width.value() ||
                                            (Interval(repeated_total.hi)-Interval(repeated_total.lo)).hi>limits.maximum_interval_width.value())
                                            reject("MATERIAL_UNION_EXTENDED_LOOP_PRECISION");
                                        provisional_union=bounds(total);provisional_excess=bounds(repeated_total);poll();
                                        return {"BOUNDED_CLIPPED_NOMINAL_UNION_AND_MULTIPLICITY_EXCESS_ONLY",
                                            std::array<ScalarBounds,3>{bounds(total),bounds(individual),bounds(repeated_total)},cells,evaluations,provisional_union,provisional_excess};
                                    }
                                    poll();return {"BOUNDED_CLIPPED_NOMINAL_UNION_AND_MULTIPLICITY_EXCESS_ONLY",
                                        std::array<ScalarBounds,3>{bounds(united),bounds(individual),bounds(repeated)},cells,evaluations,provisional_union,provisional_excess};
                                }
                                if (heap.empty()) reject("MATERIAL_UNION_LOFT_LOOP_PRECISION");
                                std::pop_heap(heap.begin(),heap.end(),less);auto old=std::move(heap.back());heap.pop_back();
                                if (old.depth>=limits.max_depth) reject("MATERIAL_UNION_DEPTH_LIMIT");const Exact middle=(old.begin+old.end)/Exact(2);
                                const auto a=cell(old.begin,middle,old.packet,old.side,old.depth+1),b=cell(middle,old.end,old.packet,old.side,old.depth+1);
                                lower+=Exact(a.volume.lo)+Exact(b.volume.lo)-Exact(old.volume.lo);upper+=Exact(a.volume.hi)+Exact(b.volume.hi)-Exact(old.volume.hi);
                                for (auto next : {a,b}) {heap.push_back(std::move(next));std::push_heap(heap.begin(),heap.end(),less);}
                            }
                        }
                    }
                }
            }
        }
        // A constant-gap axis-aligned rectangular loft has exact finite XY
        // bounds and affine top/bottom. Clip at the original Z planes and reuse
        // polygon moments; an interval grid would unnecessarily destroy this
        // correlation. Rounded and variable-gap sections retain the union solver.
        if (clip_kind==UnionClip::Box && active.size()==1) {
            const auto &row=sequence->records[active.front()];const auto &m=row.motion;const auto &b=*row.bead;
            const bool x_axis=m.start.y()==m.end.y(),y_axis=m.start.x()==m.end.x();
            if (b.kind==BeadSectionKind::Rectangle && b.gap_begin_mm==b.gap_end_mm && x_axis!=y_axis) {
                const Exact begin(x_axis ? m.start.x() : m.start.y()),end(x_axis ? m.end.x() : m.end.y()),gap(b.gap_begin_mm);
                const Exact last=begin+(end-begin)*Exact(progress(active.front())),length=end>begin ? end-begin : begin-end;
                const Exact center(x_axis ? m.start.y() : m.start.x()),half=Exact(std::get<Deposition>(m.payload).volume.value())/(Exact(2)*length*gap);
                const Exact a=std::min(begin,last),z=std::max(begin,last),c=center-half,d=center+half;
                Polygon polygon=x_axis ? Polygon{{a,c},{z,c},{z,d},{a,d}} : Polygon{{c,a},{d,a},{d,z},{c,z}};
                for (const auto &boundary : {std::tuple<Vertex,Exact,bool>{{Exact(1),Exact(0)},Exact(domain.min.x()),true},
                    {{Exact(1),Exact(0)},Exact(domain.max.x()),false},{{Exact(0),Exact(1)},Exact(domain.min.y()),true},
                    {{Exact(0),Exact(1)},Exact(domain.max.y()),false}}) {
                    if (polygon.empty()) break;polygon=clip(polygon,std::get<0>(boundary),std::get<1>(boundary),std::get<2>(boundary));
                }
                const Exact slope=(Exact(m.end.z())-Exact(m.start.z()))/(end-begin),intercept=Exact(m.start.z())-slope*begin;
                const Vertex normal=x_axis ? Vertex{slope,Exact(0)} : Vertex{Exact(0),slope};
                if (!polygon.empty()) polygon=clip(polygon,normal,Exact(domain.min.z())-intercept,true);
                if (!polygon.empty()) polygon=clip(polygon,normal,Exact(domain.max.z())+gap-intercept,false);
                const auto split=[&](const Polygon &p,const Exact &height) {
                    if (p.empty()) return std::array<Polygon,2>{};
                    if (slope==0) return intercept<=height ? std::array<Polygon,2>{p,Polygon{}} : std::array<Polygon,2>{Polygon{},p};
                    return std::array<Polygon,2>{clip(p,normal,height-intercept,false),clip(p,normal,height-intercept,true)};
                };
                Exact lower(0),upper(0);const auto tops=split(polygon,Exact(domain.max.z()));
                for (size_t top=0;top<2;++top) {
                    const auto bottoms=split(tops[top],Exact(domain.min.z())+gap);
                    for (size_t bottom=0;bottom<2;++bottom) {
                        poll();if (bottoms[bottom].empty()) continue;
                        if (cells>=limits.max_cells) reject("MATERIAL_UNION_CELL_LIMIT");++cells;evaluate();
                        const AffineCapCell moment{{0,0,1,1},0,x_axis ? 1. : 0.,x_axis ? 0. : 1.};Exact area(0);
                        const auto moments=affine_integral(bottoms[bottom],moment,&area);if (area==0) continue;
                        const Exact alpha=(top ? Exact(0) : slope)-(bottom ? slope : Exact(0));
                        const Exact beta=(top ? Exact(domain.max.z()) : intercept)-(bottom ? intercept-gap : Exact(domain.min.z()));
                        const auto volume=exact_interval(alpha)*moments.second+exact_interval(beta)*moments.first;
                        if (volume.hi<0) reject("MATERIAL_UNION_INCONSISTENT_RECTANGLE");
                        lower+=Exact(std::max(0.,volume.lo));upper+=Exact(std::max(0.,volume.hi));
                    }
                }
                const Interval volume(exact_interval(lower).lo,exact_interval(upper).hi);
                if ((Interval(volume.hi)-Interval(volume.lo)).hi>limits.maximum_interval_width.value()) reject("MATERIAL_UNION_RECTANGLE_PRECISION");
                provisional_union=bounds(volume);provisional_excess=ScalarBounds{0,0};poll();
                const std::array<ScalarBounds,3> amounts{bounds(volume),bounds(volume),ScalarBounds{0,0}};
                poll();return {"BOUNDED_CLIPPED_NOMINAL_UNION_AND_MULTIPLICITY_EXCESS_ONLY",amounts,cells,evaluations,provisional_union,provisional_excess};
            }
        }
        // Exact individual mass remains available when a box clips only a
        // finite longitudinal strip, or bisects a constant axis section on its
        // symmetry plane. This does not prove union occupancy or fill gaps.
        if (clip_kind==UnionClip::Box && !full_individual) {
            Exact amount(0);bool exact=true;
            for (size_t i : active) {
                evaluate();const auto &row=sequence->records[i];const auto &m=row.motion;const auto &b=*row.bead;
                const bool x=m.start.y()==m.end.y(),y=m.start.x()==m.end.x();
                if (x==y || !whole_vertical[i]) {exact=false;break;}
                const size_t axis=x ? 0 : 1,normal=1-axis;const auto &box=xy_bounds[i];
                const double low[2]{domain.min.x(),domain.min.y()},high[2]{domain.max.x(),domain.max.y()};
                const Exact start(x ? m.start.x() : m.start.y()),end(x ? m.end.x() : m.end.y());
                const Exact current=start+(end-start)*Exact(progress(i));
                const Exact lo=std::max(std::min(start,current),Exact(low[axis])),hi=std::min(std::max(start,current),Exact(high[axis]));
                if (lo>=hi) continue;
                Exact fraction(1);
                if (box[normal*2]<low[normal] || box[normal*2+1]>high[normal]) {
                    const double centre=x ? m.start.y() : m.start.x();
                    if (b.gap_begin_mm!=b.gap_end_mm ||
                        !((low[normal]==centre && box[normal*2+1]<=high[normal]) ||
                          (high[normal]==centre && box[normal*2]>=low[normal]))) {exact=false;break;}
                    fraction=Exact(1)/Exact(2);
                }
                const Exact length=end>start ? end-start : start-end;
                amount+=Exact(std::get<Deposition>(m.payload).volume.value())*(hi-lo)/length*fraction;
            }
            if (exact) full_individual=amount;
        }
        // Envelopes choose finite-end split planes only, never occupancy.
        // Keep internal packet seams out of this hint; the continuous solver
        // below still retains real holes and every packet's actual profile.
        std::map<std::pair<bool,double>,std::array<double,4>> group_envelopes;
        if (clip_kind==UnionClip::Box) for (size_t i : active) {
            evaluate();const auto &m=sequence->records[i].motion;const bool x=m.start.y()==m.end.y(),y=m.start.x()==m.end.x();
            if (x==y) continue;const auto key=std::make_pair(x,x ? m.start.y() : m.start.x());
            const auto inserted=group_envelopes.emplace(key,xy_bounds[i]);
            if (!inserted.second) for (size_t axis=0;axis<2;++axis) {
                inserted.first->second[axis*2]=std::min(inserted.first->second[axis*2],xy_bounds[i][axis*2]);
                inserted.first->second[axis*2+1]=std::max(inserted.first->second[axis*2+1],xy_bounds[i][axis*2+1]);
            }
        }
        const AffineCapCell flat{{domain.min.x(),domain.min.y(),domain.max.x(),domain.max.y()},0,0,0};
        const auto body_fraction=[&](size_t i) {return i<roof_source->source->completed_records ? 1 : roof_source->source->current_progress;};
        std::vector<size_t> body_active;
        if (clip_kind==UnionClip::BelowRoof || clip_kind==UnionClip::TargetShadow) {
            const auto &body=*roof_source->source->sequence;
            const size_t end=roof_source->source->completed_records+(roof_source->source->current_progress>0 && roof_source->source->completed_records<body.records.size());
            for (size_t i=0;i<end;++i) {evaluate();if (body.records[i].bead) body_active.push_back(i);}
        }
        const auto surface_q=(clip_kind==UnionClip::AboveSurface || clip_kind==UnionClip::TargetShadow) ? exact_interval(Exact(surface->z00)-shear_x*Exact(surface->footprint.min_x)-
            shear_y*Exact(surface->footprint.min_y)) : Interval(0);
        struct Node {Polygon polygon;std::vector<size_t> candidates;Exact union_lo,union_hi,sum_lo,sum_hi,repeated_lo,repeated_hi;size_t depth,id,splitter;double uncertainty;bool potential_chain;std::optional<Exact> packet_cut;bool packet_x,convex_excess;Interval roof;std::vector<size_t> body_candidates;size_t roof_splitter;};
        const auto make_node=[&](Polygon polygon,const std::vector<size_t> &candidates,size_t depth,const std::vector<size_t> &body_candidates,
                                 std::optional<Interval> inherited_roof={}) {
            poll();if (cells>=limits.max_cells) reject("MATERIAL_UNION_CELL_LIMIT");const size_t id=cells++;
            Exact area(0);affine_integral(polygon,flat,&area);
            std::array<double,4> cell_bounds{exact_interval(polygon.front()[0]).lo,exact_interval(polygon.front()[0]).hi,
                exact_interval(polygon.front()[1]).lo,exact_interval(polygon.front()[1]).hi};
            for (const auto &p : polygon) {
                const auto x=exact_interval(p[0]),y=exact_interval(p[1]);cell_bounds[0]=std::min(cell_bounds[0],x.lo);cell_bounds[1]=std::max(cell_bounds[1],x.hi);
                cell_bounds[2]=std::min(cell_bounds[2],y.lo);cell_bounds[3]=std::max(cell_bounds[3],y.hi);
            }
            auto reference=exact_interval(shear_x*polygon.front()[0]+shear_y*polygon.front()[1]);
            for (const auto &p : polygon) {
                const auto value=exact_interval(shear_x*p[0]+shear_y*p[1]);reference={std::min(reference.lo,value.lo),std::max(reference.hi,value.hi)};
            }
            Interval roof(0);std::vector<size_t> next_body;size_t roof_splitter=0;
            if (clip_kind==UnionClip::BelowRoof || clip_kind==UnionClip::TargetShadow) {
                // Refine the protected actual source with the target
                // integrator's same continuous whole-cell roof bounds.
                if (inherited_roof) {
                    // A parent whole-cell bound remains valid on both children.
                    // Its small retained width is still charged to the integral.
                    roof=*inherited_roof;next_body=body_candidates;roof_splitter=roof_source->source->sequence->records.size();
                } else {
                    auto bound=nominal_roof_bounds(*roof_source->source,polygon,body_candidates,roof_source->minimum,evaluate);
                    roof=bound.height;next_body=std::move(bound.active);roof_splitter=bound.splitter;
                }
            }
            const auto span=[&](double lo,double hi,bool guaranteed=false) {
                if (full_individual) return Span{lo,hi}; // Every active section is inside the original Z interval.
                double bottom=guaranteed ? (Interval(domain.min.z())-Interval(reference.lo)).hi : (Interval(domain.min.z())-Interval(reference.hi)).lo;
                double top=guaranteed ? (Interval(domain.max.z())-Interval(reference.hi)).lo : (Interval(domain.max.z())-Interval(reference.lo)).hi;
                if (clip_kind==UnionClip::BelowRoof) top=std::min(top,guaranteed ? (Interval(roof.lo)-Interval(reference.hi)).lo : (Interval(roof.hi)-Interval(reference.lo)).hi);
                if (clip_kind==UnionClip::AboveSurface) bottom=std::max(bottom,guaranteed ? surface_q.hi : surface_q.lo);
                const Span actual{std::max(bottom,lo),std::min(top,hi)};
                if (clip_kind!=UnionClip::TargetShadow || actual.first>=actual.second) return actual;
                // Shadow only material actually present inside the original
                // XYZ union box. Extend its occupied column down to the original
                // body roof, then clip to the protected target surface. This is
                // a separate measure, never a replacement for bead occupancy.
                bottom=std::max(bottom,guaranteed ? (Interval(roof.hi)-Interval(reference.lo)).hi :
                    (Interval(roof.lo)-Interval(reference.hi)).lo);
                top=std::min(actual.second,guaranteed ? surface_q.lo : surface_q.hi);
                return Span{bottom,top};
            };
            std::vector<Span> lower,upper;std::vector<size_t> next;Exact sum_lo(0),sum_hi(0),largest_lower(0);
            struct Strip {Exact begin,end;Span vertical,possible;bool guaranteed;Projection projected;size_t index;double roof_lower;};
            double shadow_top_lower=-std::numeric_limits<double>::infinity(),shadow_top_upper=shadow_top_lower;
            std::map<std::pair<bool,double>,std::vector<Strip>> strips;
            std::vector<Span> count_lower,count_upper;
            struct Refinement {size_t index;double transverse,partial;std::pair<bool,double> group;};
            std::vector<Refinement> refinement;
            size_t splitter=sequence->records.size();double worst=-1;
            for (size_t i : candidates) {
                evaluate();const auto &row=sequence->records[i];const double fraction=progress(i);
                const auto &box=xy_bounds[i];
                // A pure boundary intersection has zero volume. This does not
                // change the separate point/footprint contact boundary policy.
                if (box[1]<=cell_bounds[0] || box[0]>=cell_bounds[1] || box[3]<=cell_bounds[2] || box[2]>=cell_bounds[3]) continue;
                const bool x_axis=row.motion.start.y()==row.motion.end.y(),y_axis=row.motion.start.x()==row.motion.end.x();
                std::optional<RoofProjection> footprint;
                if (x_axis!=y_axis) {
                    auto clipped=polygon;
                    for (size_t axis=0;axis<2 && !clipped.empty();++axis) {
                        const Vertex normal=axis==0 ? Vertex{Exact(1),Exact(0)} : Vertex{Exact(0),Exact(1)};
                        if (box[axis*2]>cell_bounds[axis*2]) clipped=clip(clipped,normal,Exact(box[axis*2]),true);
                        if (!clipped.empty() && box[axis*2+1]<cell_bounds[axis*2+1]) clipped=clip(clipped,normal,Exact(box[axis*2+1]),false);
                    }
                    if (!clipped.empty() && fraction!=1) {
                        // The current endpoint need not be representable in
                        // binary64. Its AABB is an outer enclosure, so clip the
                        // exact finite butt before charging an individual lower bound.
                        const auto &m=row.motion;const double a=x_axis ? m.start.x() : m.start.y(),b=x_axis ? m.end.x() : m.end.y();
                        const Exact last=Exact(a)+(Exact(b)-Exact(a))*Exact(fraction);
                        clipped=clip(clipped,x_axis ? Vertex{Exact(1),Exact(0)} : Vertex{Exact(0),Exact(1)},last,b<a);
                    }
                    if (!clipped.empty()) {
                        const std::array<double,4> bounds{std::max(box[0],cell_bounds[0]),std::min(box[1],cell_bounds[1]),
                            std::max(box[2],cell_bounds[2]),std::min(box[3],cell_bounds[3])};
                        footprint=RoofProjection{project_bounds(row,bounds),Interval(0),std::move(clipped)};
                    }
                } else footprint=roof_projection(row,sequence->model,polygon,fraction,Representation::Nominal);
                if (!footprint) continue;
                const auto possible=vertical(i,footprint->projected,fraction);if (!possible) continue;
                const auto outside=span(possible->first.lo,possible->second.height.hi);if (outside.first>=outside.second) continue;
                next.push_back(i);upper.push_back(outside);
                shadow_top_upper=std::max(shadow_top_upper,possible->second.height.hi);
                const auto inside=vertical(i,x_axis!=y_axis ? project_bounds(row,cell_bounds) : project_polygon(row,polygon,Interval(0)),fraction);
                if (inside && inside->second.whole_transverse && longitudinal(row,polygon,fraction)) {
                    const auto complete=span(inside->first.hi,inside->second.height.lo,true);lower.push_back(complete);
                    if (complete.first<complete.second || (clip_kind==UnionClip::TargetShadow && whole_vertical[i]))
                        shadow_top_lower=std::max(shadow_top_lower,inside->second.height.lo);
                }
                // Adjacent finite packets can cover a cell together even when
                // none covers its full length. Prove that longitudinal union;
                // never replace disconnected packets by one continuous line.
                if (x_axis!=y_axis) {
                    const auto &m=row.motion;const auto coordinate=[&](PhysicalPosition p) {return x_axis ? p.x() : p.y();};
                    const Exact start(coordinate(m.start)),last=start+(Exact(coordinate(m.end))-start)*Exact(fraction);
                    const auto s=span(possible->first.hi,possible->second.height.lo,true);
                    strips[{x_axis,x_axis ? m.start.y() : m.start.x()}].push_back({std::min(start,last),std::max(start,last),s,outside,
                        inside && inside->second.whole_transverse && (s.first<s.second || (clip_kind==UnionClip::TargetShadow && whole_vertical[i])),
                        footprint->projected,i,possible->second.height.lo});
                } else {
                    count_upper.push_back(outside);
                    if (inside && inside->second.whole_footprint) count_lower.push_back(span(inside->first.hi,inside->second.height.lo,true));
                }
                double uncertainty=outside.second-outside.first;
                {
                    // Reuse the already clipped finite XY enclosure. A whole
                    // parent area must never be charged to a short packet.
                    Exact clipped_area(0);affine_integral(footprint->polygon,flat,&clipped_area);
                    const Exact individual_hi=clipped_area*(Exact(outside.second)-Exact(outside.first));sum_hi+=individual_hi;
                    Exact individual_lo(0);
                    if (possible->second.whole_transverse) {
                        const auto s=span(possible->first.hi,possible->second.height.lo,true);
                        if (s.first<s.second) individual_lo=clipped_area*(Exact(s.second)-Exact(s.first));
                    }
                    sum_lo+=individual_lo;largest_lower=std::max(largest_lower,individual_lo);
                    if (!full_individual) uncertainty=exact_interval(individual_hi-individual_lo).hi;
                }
                if (full_individual && inside && inside->second.whole_transverse) {
                    const auto s=span(inside->first.hi,inside->second.height.lo,true);
                    uncertainty-=std::max(0.,s.second-s.first);
                }
                const double partial=full_individual && x_axis!=y_axis && !longitudinal(row,polygon,fraction) ? outside.second-outside.first : 0.;
                refinement.push_back({i,uncertainty,partial,{x_axis,x_axis ? row.motion.start.y() : row.motion.start.x()}});
            }
            std::set<std::pair<bool,double>> potential_chains;
            std::vector<std::pair<bool,std::vector<Strip>>> covered_chains;
            for (auto &group : strips) {
                auto &rows=group.second;const size_t axis=group.first.first ? 0 : 1;
                Exact begin=polygon.front()[axis],end=begin;
                for (const auto &point : polygon) {begin=std::min(begin,point[axis]);end=std::max(end,point[axis]);}
                std::sort(rows.begin(),rows.end(),[](const Strip &a,const Strip &b) {return a.begin<b.begin;});
                struct Chain {Exact last;std::vector<Strip> rows;};std::vector<Chain> chains;
                for (const auto &row : rows) {
                    auto chain=std::find_if(chains.begin(),chains.end(),[&](const Chain &c) {return c.last<=row.begin;});
                    if (chain==chains.end()) chains.push_back({row.end,{row}});
                    else {chain->last=row.end;chain->rows.push_back(row);}
                }
                for (const auto &chain : chains) {
                    Exact covered=begin;double bottom=0,top=0,possible_bottom=chain.rows.front().possible.first,possible_top=chain.rows.front().possible.second;
                    double roof_lower=std::numeric_limits<double>::infinity();bool have=false;
                    Exact potential_covered=begin;
                    for (const auto &row : chain.rows) {
                        possible_bottom=std::min(possible_bottom,row.possible.first);possible_top=std::max(possible_top,row.possible.second);
                        if (row.begin<=potential_covered) potential_covered=std::max(potential_covered,row.end);
                        if (!row.guaranteed || row.end<begin || row.begin>end || row.begin>covered) continue;
                        covered=std::max(covered,row.end);roof_lower=std::min(roof_lower,row.roof_lower);
                        if (!have) {bottom=row.vertical.first;top=row.vertical.second;have=true;}
                        else {bottom=std::max(bottom,row.vertical.first);top=std::min(top,row.vertical.second);}
                    }
                    // Interior-disjoint butt-ended packets contribute at most
                    // one profile per chain at each XY point. Coincident or
                    // longitudinally overlapping packets occupy separate chains.
                    count_upper.push_back({possible_bottom,possible_top});
                    // Split-direction hint only: potential longitudinal
                    // continuity does not certify transverse/vertical coverage.
                    if (potential_covered>=end) potential_chains.insert(group.first);
                    if (have && covered>=end) shadow_top_lower=std::max(shadow_top_lower,roof_lower);
                    if (have && covered>=end && bottom<top) {
                        lower.push_back({bottom,top});count_lower.push_back({bottom,top});
                        covered_chains.push_back({group.first.first,chain.rows});
                    }
                }
            }
            for (const auto &hint : refinement) {
                // A flat transverse section can still end inside this cell.
                // Prefer that real finite discontinuity unless an actual packet
                // chain spans the whole cell. This chooses a split only.
                const double uncertainty=std::max(hint.transverse,potential_chains.count(hint.group) ? 0. : hint.partial);
                if (uncertainty>worst) {worst=uncertainty;splitter=hint.index;}
            }
            Exact union_lo=std::max(area*measure(lower),largest_lower),union_hi=std::min(area*measure(upper),sum_hi);
            if (clip_kind==UnionClip::TargetShadow) {
                // A full finite packet/chain bounds the top in the surface's
                // sheared frame. Integrate its affine column above the original
                // constant roof enclosure exactly in XY instead of charging
                // the full longitudinal height range to every point. Partial
                // footprints still require the existing union bounds.
                std::vector<std::tuple<double,double,Interval>> columns;columns.reserve(8);
                std::optional<Interval> full_moment;
                const auto column=[&](double ceiling,double floor) {
                    if (!std::isfinite(ceiling)) return Interval(0);
                    for (const auto &cached : columns)
                        if (std::get<0>(cached)==ceiling && std::get<1>(cached)==floor) return std::get<2>(cached);
                    evaluate();const auto compute=[&] {
                        if ((Interval(ceiling)+Interval(reference.hi)-Interval(floor)).hi<=0) return Interval(0);
                        if ((Interval(ceiling)+Interval(reference.lo)-Interval(floor)).lo>=0) {
                            if (!full_moment) {
                                const auto mx=affine_integral(polygon,{{0,0,1,1},0,1,0}).second;
                                const auto my=affine_integral(polygon,{{0,0,1,1},0,0,1}).second;
                                full_moment=exact_interval(shear_x)*mx+exact_interval(shear_y)*my;
                            }
                            return detail::maximum(Interval(0),exact_interval(area)*(Interval(ceiling)-Interval(floor))+*full_moment);
                        }
                        const Vertex normal{shear_x,shear_y};const Exact offset=Exact(floor)-Exact(ceiling);
                        auto clipped=polygon;
                        if (normal[0]==0 && normal[1]==0) {if (offset>=0) return Interval(0);}
                        else clipped=clip(clipped,normal,offset,true);
                        if (clipped.empty()) return Interval(0);
                        Exact a(0);const auto mx=affine_integral(clipped,{{0,0,1,1},0,1,0},&a).second;
                        const auto my=affine_integral(clipped,{{0,0,1,1},0,0,1}).second;
                        const auto v=exact_interval(a)*(Interval(ceiling)-Interval(floor))+exact_interval(shear_x)*mx+exact_interval(shear_y)*my;
                        return detail::maximum(Interval(0),v);
                    };
                    const auto volume=compute();columns.emplace_back(ceiling,floor,volume);return volume;
                };
                const auto low=column(std::min(shadow_top_lower,surface_q.lo),roof.hi);
                const auto high=column(std::min(shadow_top_upper,surface_q.hi),roof.lo);
                union_lo=std::max(union_lo,Exact(low.lo));union_hi=std::min(union_hi,Exact(high.hi));
                const Exact domain_area=(Exact(domain.max.x())-Exact(domain.min.x()))*(Exact(domain.max.y())-Exact(domain.min.y()));
                if (count_upper.size()==1 && covered_chains.size()==1 &&
                    union_hi-union_lo>Exact(limits.maximum_interval_width.value())*area/domain_area) {
                    Exact xmin=polygon.front()[0],xmax=xmin,ymin=polygon.front()[1],ymax=ymin;
                    for (const auto &p : polygon) {xmin=std::min(xmin,p[0]);xmax=std::max(xmax,p[0]);ymin=std::min(ymin,p[1]);ymax=std::max(ymax,p[1]);}
                    if (area==(xmax-xmin)*(ymax-ymin)) {
                        // At each longitudinal point exactly one packet of
                        // this complete chain supplies a concave transverse
                        // roof. Clipping by the affine target preserves that
                        // concavity. Positive columns allow Hermite-Hadamard:
                        // endpoint trapezoid below, midpoint above. Bound every
                        // original packet, without averaging its varying flux.
                        const auto &chain=covered_chains.front();const bool x_axis=chain.first;
                        const Exact first=x_axis ? ymin : xmin,last=x_axis ? ymax : xmax;
                        std::array<Interval,3> lower_columns{Interval(0),Interval(0),Interval(0)},upper_columns=lower_columns;
                        bool positive=true;
                        for (size_t sample=0;sample<3;++sample) {
                            const auto coordinate=exact_interval(sample==0 ? first : sample==1 ? last : (first+last)/Exact(2));
                            double lo=std::numeric_limits<double>::infinity(),hi=-lo;
                            for (const auto &strip : chain.second) {
                                evaluate();const auto &row=sequence->records[strip.index];const auto &m=row.motion;
                                auto projection=strip.projected;auto normal=coordinate-Interval(x_axis ? m.start.y() : m.start.x());
                                if (x_axis ? m.end.x()<m.start.x() : m.end.y()>m.start.y()) normal=Interval(0)-normal;
                                projection.normal=normal;const auto section=vertical(strip.index,projection,progress(strip.index));
                                if (!section) reject("MATERIAL_SHADOW_INCONSISTENT_CONCAVE_SECTION");
                                lo=std::min(lo,section->second.height.lo);hi=std::max(hi,section->second.height.hi);
                            }
                            lo=std::min(lo,surface_q.lo);hi=std::min(hi,surface_q.hi);
                            if ((Interval(lo)+Interval(reference.lo)-Interval(roof.hi)).lo<=0) positive=false;
                            lower_columns[sample]=column(lo,roof.hi);upper_columns[sample]=column(hi,roof.lo);
                        }
                        if (positive) {
                            union_lo=std::max(union_lo,Exact(((lower_columns[0]+lower_columns[1])/Interval(2)).lo));
                            union_hi=std::min(union_hi,Exact(upper_columns[2].hi));
                        }
                    }
                }
            }
            if (union_lo>union_hi || sum_lo>sum_hi) reject("MATERIAL_UNION_INCONSISTENT_SECTION");
            const auto excess=[&](const std::vector<Span> &spans) {
                Exact sum(0);for (auto s : spans) if (s.first<s.second) sum+=Exact(s.second)-Exact(s.first);
                return sum-measure(spans);
            };
            // Local clipped individual bounds tighten multiplicity even when
            // the complete ledger amount is already exact. Partial footprints
            // must not be charged as a whole-cell overlapping profile.
            Exact repeated_lo=std::max(area*excess(count_lower),std::max(Exact(0),sum_lo-union_hi));
            Exact repeated_hi=std::min(area*excess(count_upper),std::max(Exact(0),sum_hi-union_lo));
            std::optional<Exact> packet_cut;bool packet_x=false,convex_excess=false;
            double common_bottom=-std::numeric_limits<double>::infinity(),common_top=std::numeric_limits<double>::infinity();
            for (const auto &profile : count_lower) {common_bottom=std::max(common_bottom,profile.first);common_top=std::min(common_top,profile.second);}
            if (clip_kind==UnionClip::Box && !count_upper.empty() && covered_chains.size()==count_upper.size() && common_bottom<common_top) {
                Exact xmin=polygon.front()[0],xmax=xmin,ymin=polygon.front()[1],ymax=ymin;
                for (const auto &p : polygon) {xmin=std::min(xmin,p[0]);xmax=std::max(xmax,p[0]);ymin=std::min(ymin,p[1]);ymax=std::max(ymax,p[1]);}
                if (area==(xmax-xmin)*(ymax-ymin)) {
                    std::optional<Exact> best_distance;
                    for (const auto &chain : covered_chains) if (chain.second.size()>1) {
                        const Exact a=chain.first ? xmin : ymin,b=chain.first ? xmax : ymax,mid=(a+b)/Exact(2);
                        for (const auto &strip : chain.second) for (const auto &end : {strip.begin,strip.end}) {
                            evaluate();if (end<=a || end>=b) continue;const Exact distance=abs(end-mid);
                            if (!best_distance || distance<*best_distance) {best_distance=distance;packet_cut=end;packet_x=chain.first;}
                        }
                    }
                    if (!packet_cut) {
                        // A stadium loft with affine width, gap and axis is
                        // convex. At constant flux w(h)=A/h+(1-pi/4)*h is convex
                        // in positive h. Its tangent below and secant above
                        // define affine inner/outer lofts, enclosing actual
                        // multiplicity without declaring the actual loft convex.
                        struct WidthEnvelope {Exact slope,intercept;};
                        struct Envelope {size_t index;WidthEnvelope inner,outer;double error;};
                        std::vector<Envelope> widths;bool eligible=true;
                        const auto affine_section=[&](size_t i,Projection projection,const WidthEnvelope &envelope) -> std::optional<std::pair<Interval,Interval>> {
                            evaluate();const auto &row=sequence->records[i];const auto &b=*row.bead;const auto &c=coefficients[i];
                            const auto t=detail::maximum(Interval(0),detail::minimum(projection.t,Interval(progress(i))));
                            const auto h=Interval(b.gap_begin_mm)+(Interval(b.gap_end_mm)-Interval(b.gap_begin_mm))*t;
                            const auto width=exact_interval(envelope.slope)*h+exact_interval(envelope.intercept);
                            if (width.lo<h.hi || absolute(projection.normal).hi>=(width/Interval(2)).lo) return {};
                            const auto radius=h/Interval(2),core=(width-h)/Interval(2),offset=detail::maximum(Interval(0),absolute(projection.normal)-core);
                            const auto radicand=square(radius)-square(offset);if (radicand.hi<0) return {};
                            const auto depth=offset.hi==0 ? Interval(0) : detail::maximum(Interval(0),radius-detail::root(radicand));
                            const auto z=c.start+c.along*t-c.normal*projection.normal;
                            return std::pair<Interval,Interval>{z-h+depth,z-depth};
                        };
                        double inner_bottom=-std::numeric_limits<double>::infinity(),inner_top=std::numeric_limits<double>::infinity();
                        for (const auto &chain : covered_chains) {
                            if (chain.second.size()!=1) {eligible=false;break;}const auto &strip=chain.second.front();const auto &row=sequence->records[strip.index];
                            if (row.bead->kind!=BeadSectionKind::RoundedRectangle) {eligible=false;break;}
                            const auto &b=*row.bead;const auto p=project_polygon(row,polygon,Interval(0));
                            const auto t=detail::maximum(Interval(0),detail::minimum(p.t,Interval(progress(strip.index))));
                            const auto h=Interval(b.gap_begin_mm)+(Interval(b.gap_end_mm)-Interval(b.gap_begin_mm))*t;
                            if (h.lo<=0) {eligible=false;break;}
                            const Exact lo(h.lo),hi(h.hi),mid=(lo+hi)/Exact(2);const auto k=Interval(1)-pi()/Interval(4);
                            const auto A=coefficients[strip.index].area;
                            const WidthEnvelope lower{Exact(k.lo)-Exact(A.lo)/(mid*mid),Exact(2)*Exact(A.lo)/mid};
                            const WidthEnvelope upper{Exact(k.hi)-Exact(A.hi)/(lo*hi),Exact(A.hi)/lo+Exact(A.hi)/hi};
                            const auto inner=affine_section(strip.index,p,lower);if (!inner) {eligible=false;break;}
                            const auto profile=span(inner->first.hi,inner->second.lo,true);
                            const auto error=exact_interval((upper.slope-lower.slope)*mid+upper.intercept-lower.intercept);
                            inner_bottom=std::max(inner_bottom,profile.first);inner_top=std::min(inner_top,profile.second);
                            widths.push_back({strip.index,lower,upper,error.hi});
                        }
                        if (eligible && inner_bottom<inner_top) {
                            convex_excess=true;double widest=-1;
                            for (const auto &entry : widths) {
                                const auto &section=*sequence->records[entry.index].bead;
                                if (section.gap_begin_mm!=section.gap_end_mm && entry.error>widest) {widest=entry.error;splitter=entry.index;}
                            }
                            std::array<Exact,5> low{Exact(0),Exact(0),Exact(0),Exact(0),Exact(0)},high=low,union_low=low,union_high=low;
                            const std::array<Vertex,5> samples{{{xmin,ymin},{xmax,ymin},{xmax,ymax},{xmin,ymax},{(xmin+xmax)/Exact(2),(ymin+ymax)/Exact(2)}}};
                            for (size_t n=0;n<samples.size();++n) {
                                const std::array<double,4> point{exact_interval(samples[n][0]).lo,exact_interval(samples[n][0]).hi,
                                    exact_interval(samples[n][1]).lo,exact_interval(samples[n][1]).hi};std::vector<Span> lower_profiles,upper_profiles;
                                for (const auto &entry : widths) {
                                    const auto p=project_bounds(sequence->records[entry.index],point);
                                    const auto inner=affine_section(entry.index,p,entry.inner),outer=affine_section(entry.index,p,entry.outer);
                                    if (!inner || !outer) reject("MATERIAL_UNION_INCONSISTENT_CONVEX_ENVELOPE");
                                    lower_profiles.push_back(span(inner->first.hi,inner->second.lo,true));
                                    upper_profiles.push_back(span(outer->first.lo,outer->second.hi));
                                }
                                low[n]=excess(lower_profiles);high[n]=excess(upper_profiles);
                                if (widths.size()==1) {union_low[n]=measure(lower_profiles);union_high[n]=measure(upper_profiles);}
                            }
                            // Concave excess of commonly intersecting convex
                            // vertical profiles: corner mean <= mean <= midpoint.
                            repeated_lo=std::max(repeated_lo,area*(low[0]+low[1]+low[2]+low[3])/Exact(4));
                            repeated_hi=std::min(repeated_hi,area*high[4]);
                            if (widths.size()==1) {
                                // One complete convex profile has a concave
                                // vertical section length. Its corner mean
                                // and midpoint bound union itself; no maximum
                                // of multiple roofs is declared concave.
                                union_lo=std::max(union_lo,area*(union_low[0]+union_low[1]+union_low[2]+union_low[3])/Exact(4));
                                union_hi=std::min(union_hi,area*union_high[4]);
                            }
                        }
                    }
                }
            }
            std::optional<bool> excess_axis;
            if (clip_kind==UnionClip::Box && count_upper.size()>=2 && covered_chains.size()==count_upper.size() && common_bottom<common_top) {
                for (bool axis : {true,false}) if (std::all_of(covered_chains.begin(),covered_chains.end(),[&](const auto &chain) {
                    if (chain.first==axis) return true;
                    if (chain.second.size()!=1) return false;
                    const auto &section=*sequence->records[chain.second.front().index].bead;
                    return section.gap_begin_mm==section.gap_end_mm;
                })) {excess_axis=axis;break;}
            }
            if (excess_axis) {
                Exact xmin=polygon.front()[0],xmax=xmin,ymin=polygon.front()[1],ymax=ymin;
                for (const auto &p : polygon) {xmin=std::min(xmin,p[0]);xmax=std::max(xmax,p[0]);ymin=std::min(ymin,p[1]);ymax=std::max(ymax,p[1]);}
                if (area==(xmax-xmin)*(ymax-ymin)) {
                    // All complete profiles share a positive vertical
                    // interval throughout this rectangle. Their union is one
                    // interval, so multiplicity excess is sum(height)-span:
                    // min_i(sum_{j!=i} top_j)-max_i(sum_{j!=i} bottom_j).
                    // Tops are concave and bottoms convex transversely at each
                    // longitudinal point, hence this excess is concave for any
                    // number of profiles. Hermite-Hadamard bounds its mean;
                    // retain every finite packet and original Z clipping. A
                    // perpendicular chain is allowed only as one constant-gap
                    // packet covering this cell: its section is affine along
                    // this integration direction, without an internal seam.
                    const bool x_axis=*excess_axis;
                    const Exact first=x_axis ? ymin : xmin,last=x_axis ? ymax : xmax;
                    std::array<Exact,3> low{Exact(0),Exact(0),Exact(0)},high=low;
                    for (size_t sample=0;sample<3;++sample) {
                        const auto coordinate=exact_interval(sample==0 ? first : sample==1 ? last : (first+last)/Exact(2));
                        std::vector<Span> lower_profiles,upper_profiles;
                        for (const auto &chain : covered_chains) {
                            double bottom=0,top=0,outer_bottom=0,outer_top=0;bool have=false;
                            for (const auto &strip : chain.second) {
                                evaluate();const auto &row=sequence->records[strip.index];const auto &m=row.motion;
                                auto projection=strip.projected;
                                if (chain.first==x_axis) {
                                    auto normal=coordinate-Interval(x_axis ? m.start.y() : m.start.x());
                                    if (x_axis ? m.end.x()<m.start.x() : m.end.y()>m.start.y()) normal=Interval(0)-normal;
                                    projection.normal=normal;
                                } else {
                                    const auto delta=Interval(x_axis ? m.end.y() : m.end.x())-Interval(x_axis ? m.start.y() : m.start.x());
                                    auto along=coordinate-Interval(x_axis ? m.start.y() : m.start.x());
                                    if (delta.hi<0) along=Interval(0)-along;
                                    projection.t=along/absolute(delta);
                                }
                                const auto section=vertical(strip.index,projection,progress(strip.index));
                                if (!section) reject("MATERIAL_UNION_INCONSISTENT_CONCAVE_SECTION");
                                const auto inner=span(section->first.hi,section->second.height.lo,true),outer=span(section->first.lo,section->second.height.hi);
                                if (!have) {bottom=inner.first;top=inner.second;outer_bottom=outer.first;outer_top=outer.second;have=true;}
                                else {bottom=std::max(bottom,inner.first);top=std::min(top,inner.second);
                                    outer_bottom=std::min(outer_bottom,outer.first);outer_top=std::max(outer_top,outer.second);}
                            }
                            lower_profiles.push_back({bottom,top});upper_profiles.push_back({outer_bottom,outer_top});
                        }
                        low[sample]=excess(lower_profiles);high[sample]=excess(upper_profiles);
                    }
                    repeated_lo=std::max(repeated_lo,area*(low[0]+low[1])/Exact(2));
                    repeated_hi=std::min(repeated_hi,area*high[2]);
                }
            }
            if (union_lo>union_hi || repeated_lo>repeated_hi) reject("MATERIAL_UNION_INCONSISTENT_SECTION_EXCESS");
            const double uncertainty=exact_interval(clip_kind!=UnionClip::Box ? union_hi-union_lo : full_individual ? repeated_hi-repeated_lo : union_hi-union_lo+sum_hi-sum_lo).hi;
            bool splitter_chain=false;
            if (splitter<sequence->records.size()) {
                const auto &m=sequence->records[splitter].motion;const bool x_axis=m.start.y()==m.end.y();
                splitter_chain=potential_chains.count({x_axis,x_axis ? m.start.y() : m.start.x()})!=0;
            }
            return Node{std::move(polygon),std::move(next),union_lo,union_hi,sum_lo,sum_hi,repeated_lo,repeated_hi,depth,id,splitter,uncertainty,splitter_chain,std::move(packet_cut),packet_x,convex_excess,roof,std::move(next_body),roof_splitter};
        };
        const auto compare=[](const Node &a,const Node &b) {return a.uncertainty==b.uncertainty ? a.id>b.id : a.uncertainty<b.uncertainty;};
        std::vector<Node> heap;
        auto root=make_node({{Exact(domain.min.x()),Exact(domain.min.y())},{Exact(domain.max.x()),Exact(domain.min.y())},
            {Exact(domain.max.x()),Exact(domain.max.y())},{Exact(domain.min.x()),Exact(domain.max.y())}},active,0,body_active);
        Exact union_lo=root.union_lo,union_hi=root.union_hi,sum_lo=root.sum_lo,sum_hi=root.sum_hi,repeated_lower=root.repeated_lo,repeated_upper=root.repeated_hi;
        heap.push_back(std::move(root));
        while (true) {
            poll();const Exact individual_lo=full_individual ? *full_individual : sum_lo,individual_hi=full_individual ? *full_individual : sum_hi;
            const Exact lower=std::max(union_lo,individual_lo-repeated_upper),upper=std::min(union_hi,individual_hi-repeated_lower);
            if (lower>upper) reject("MATERIAL_UNION_INCONSISTENT_TOTAL");
            const Exact repeated_lo=std::max(repeated_lower,std::max(Exact(0),individual_lo-upper)),repeated_hi=std::min(repeated_upper,std::max(Exact(0),individual_hi-lower));
            if (repeated_lo>repeated_hi) reject("MATERIAL_UNION_INCONSISTENT_EXCESS");
            const auto occupied=Interval(exact_interval(lower).lo,exact_interval(upper).hi),individual=Interval(exact_interval(individual_lo).lo,exact_interval(individual_hi).hi),repeated=Interval(exact_interval(repeated_lo).lo,exact_interval(repeated_hi).hi);
            provisional_union=bounds(occupied);provisional_excess=bounds(repeated);
            const double width=clip_kind==UnionClip::Box ? std::max({(Interval(occupied.hi)-Interval(occupied.lo)).hi,
                (Interval(individual.hi)-Interval(individual.lo)).hi,(Interval(repeated.hi)-Interval(repeated.lo)).hi}) : (Interval(occupied.hi)-Interval(occupied.lo)).hi;
            if (width<=limits.maximum_interval_width.value()) {
                const std::array<ScalarBounds,3> amounts{bounds(occupied),bounds(individual),bounds(repeated)};
                poll();return {"BOUNDED_CLIPPED_NOMINAL_UNION_AND_MULTIPLICITY_EXCESS_ONLY",amounts,cells,evaluations,provisional_union,provisional_excess};
            }
            if (heap.empty()) reject("MATERIAL_UNION_DEPTH_LIMIT");
            std::pop_heap(heap.begin(),heap.end(),compare);auto node=std::move(heap.back());heap.pop_back();
            if (node.depth>=limits.max_depth) continue;
            if (node.splitter>=sequence->records.size()) reject("MATERIAL_UNION_INVALID_SPLIT");
            const auto &row=sequence->records[node.splitter];const auto &m=row.motion;
            const Exact dx=Exact(m.end.x())-Exact(m.start.x()),dy=Exact(m.end.y())-Exact(m.start.y());Vertex normal{-dy,dx};
            const auto p=project_polygon(row,node.polygon,Interval(0));
            const auto finite_end=(!node.potential_chain && (p.t.lo<=0 || p.t.hi>=progress(node.splitter)) &&
                p.normal.hi-p.normal.lo<row.bead->width_mm.lower/2 &&
                !longitudinal(row,node.polygon,progress(node.splitter)));
            bool along=finite_end;
            if (clip_kind==UnionClip::TargetShadow && !finite_end) {
                // The affine column integral already accounts for the target
                // slope. Choose refinement from the actual sheared roof's
                // variation, including rounded shoulders and varying gaps.
                // Midpoint probes choose a direction only, never a bound.
                auto longitudinal=p,transverse=p;
                longitudinal.normal=Interval((p.normal.lo+p.normal.hi)/2);
                transverse.t=Interval((p.t.lo+p.t.hi)/2);
                const auto uncertainty=[&](Projection projected) {
                    evaluate();const auto section=vertical(node.splitter,projected,progress(node.splitter));
                    return section ? (Interval(section->second.height.hi)-Interval(section->second.height.lo)).hi : 0.;
                };
                along=uncertainty(longitudinal)>uncertainty(transverse);
            } else if (clip_kind==UnionClip::BelowRoof && !finite_end) {
                // Split where the clipped vertical interval actually varies.
                // Top and gap can slope together while their floor stays flat;
                // their separate magnitudes would endlessly split along a line
                // whose below-roof volume only varies transversely. These two
                // midpoint probes choose a direction, never supply a bound.
                auto longitudinal=p,transverse=p;
                longitudinal.normal=Interval((p.normal.lo+p.normal.hi)/2);
                transverse.t=Interval((p.t.lo+p.t.hi)/2);
                const auto node_roof=node.roof;
                const auto uncertainty=[&](Projection projected) {
                    evaluate();const auto section=vertical(node.splitter,projected,progress(node.splitter));
                    if (!section) return 0.;
                    const double outer=std::max(0.,std::min(node_roof.hi,section->second.height.hi)-std::max(domain.min.z(),section->first.lo));
                    const double inner=std::max(0.,std::min(node_roof.lo,section->second.height.lo)-std::max(domain.min.z(),section->first.hi));
                    return outer-inner;
                };
                along=uncertainty(longitudinal)>uncertainty(transverse);
            } else if (clip_kind!=UnionClip::BelowRoof)
                along=along || std::max(std::abs(m.end.z()-m.start.z()),std::abs(row.bead->gap_end_mm-row.bead->gap_begin_mm))*(p.t.hi-p.t.lo)>p.normal.hi-p.normal.lo;
            if (node.convex_excess && !finite_end && row.bead->gap_begin_mm!=row.bead->gap_end_mm) {
                // Tight corner/midpoint quadrature leaves width-envelope error
                // as the longitudinal uncertainty. Split that axis before
                // repeatedly refining an already small transverse curvature.
                // This balance is a hint only, never a replacement for bounds.
                evaluate();const auto t=detail::maximum(Interval(0),detail::minimum(p.t,Interval(progress(node.splitter))));
                const auto h=Interval(row.bead->gap_begin_mm)+(Interval(row.bead->gap_end_mm)-Interval(row.bead->gap_begin_mm))*t;
                const Exact lo(h.lo),hi(h.hi),mid=(lo+hi)/Exact(2),A(coefficients[node.splitter].area.hi);
                const auto error=exact_interval(A/lo+A/hi-Exact(2)*A/mid);
                const double transverse=p.normal.hi-p.normal.lo;
                along=along || error.hi>transverse*transverse/h.hi/10;
            }
            if (along) normal={dx,dy};
            if ((clip_kind==UnionClip::BelowRoof || clip_kind==UnionClip::TargetShadow) && node.depth%2==0 && node.roof.hi-node.roof.lo>1e-12 &&
                node.roof_splitter<roof_source->source->sequence->records.size()) {
                const auto &support=roof_source->source->sequence->records[node.roof_splitter];const auto &m=support.motion;
                const Exact x=Exact(m.end.x())-Exact(m.start.x()),y=Exact(m.end.y())-Exact(m.start.y());normal={-y,x};
                const auto projected=project_polygon(support,node.polygon,Interval(0));const double span=projected.normal.hi-projected.normal.lo;
                if (((projected.t.lo<=0 || projected.t.hi>=body_fraction(node.roof_splitter)) && span<support.bead->width_mm.lower/2) ||
                    std::max(std::abs(m.end.z()-m.start.z()),std::abs(support.bead->gap_end_mm-support.bead->gap_begin_mm))*(projected.t.hi-projected.t.lo)>span)
                    normal={x,y};
            }
            if (node.packet_cut) normal=node.packet_x ? Vertex{Exact(1),Exact(0)} : Vertex{Exact(0),Exact(1)};
            auto lo=normal[0]*node.polygon.front()[0]+normal[1]*node.polygon.front()[1],hi=lo;
            for (auto point : node.polygon) {const Exact value=normal[0]*point[0]+normal[1]*point[1];lo=std::min(lo,value);hi=std::max(hi,value);}
            if (lo==hi) reject("MATERIAL_UNION_INVALID_SPLIT");Exact middle=(lo+hi)/Exact(2);
            if (node.packet_cut) middle=*node.packet_cut;
            else if (clip_kind==UnionClip::Box && (dx==0 || dy==0)) {
                // Place refinement on finite group envelope faces when possible.
                // A midpoint grid needlessly straddles unequal cap/contour ends.
                // These are split hints only; both children retain all original
                // continuous occupancy and multiplicity bounds and shared limits.
                const bool split_x=normal[1]==0;const size_t axis=split_x ? 0 : 1;
                const bool x_axis=dy==0;
                const auto &bounds=group_envelopes.at({x_axis,x_axis ? m.start.y() : m.start.x()});
                std::optional<Exact> cut;
                for (size_t face=0;face<2;++face) {
                    evaluate();const Exact value=normal[axis]*Exact(bounds[axis*2+face]);
                    if (value>lo && value<hi && (!cut || abs(value-middle)<abs(*cut-middle))) cut=value;
                }
                if (cut) middle=*cut;
            }
            std::optional<Interval> inherited;
            const auto domain_area=(Interval(domain.max.x())-Interval(domain.min.x()))*(Interval(domain.max.y())-Interval(domain.min.y()));
            if (clip_kind==UnionClip::TargetShadow && ((Interval(node.roof.hi)-Interval(node.roof.lo))*domain_area).hi<=limits.maximum_interval_width.value()/32)
                inherited=node.roof;
            auto first=make_node(clip(node.polygon,normal,middle,false),node.candidates,node.depth+1,node.body_candidates,inherited);
            auto second=make_node(clip(node.polygon,normal,middle,true),node.candidates,node.depth+1,node.body_candidates,inherited);
            if (clip_kind==UnionClip::Box && node.convex_excess && !node.packet_cut &&
                row.bead->gap_begin_mm!=row.bead->gap_end_mm && (dx==0 || dy==0) &&
                first.uncertainty+second.uncertainty>node.uncertainty*.75 &&
                limits.max_cells>=2 && cells<=limits.max_cells-2) {
                // Coupled affine gap and rounded shoulders can make a geometric
                // split hint ineffective. Compare actual bounds in the other
                // axis; discarded queries consume the same root budget.
                const Vertex alternative=normal[0]==0 ? Vertex{Exact(1),Exact(0)} : Vertex{Exact(0),Exact(1)};
                Exact a=alternative[0]*node.polygon.front()[0]+alternative[1]*node.polygon.front()[1],b=a;
                for (const auto &point : node.polygon) {const Exact v=alternative[0]*point[0]+alternative[1]*point[1];a=std::min(a,v);b=std::max(b,v);}
                if (a<b) {
                    const Exact cut=(a+b)/Exact(2);
                    auto left=make_node(clip(node.polygon,alternative,cut,false),node.candidates,node.depth+1,node.body_candidates);
                    auto right=make_node(clip(node.polygon,alternative,cut,true),node.candidates,node.depth+1,node.body_candidates);
                    if (left.uncertainty+right.uncertainty<first.uncertainty+second.uncertainty) {first=std::move(left);second=std::move(right);}
                }
            }
            union_lo+=first.union_lo+second.union_lo-node.union_lo;union_hi+=first.union_hi+second.union_hi-node.union_hi;
            sum_lo+=first.sum_lo+second.sum_lo-node.sum_lo;sum_hi+=first.sum_hi+second.sum_hi-node.sum_hi;
            repeated_lower+=first.repeated_lo+second.repeated_lo-node.repeated_lo;repeated_upper+=first.repeated_hi+second.repeated_hi-node.repeated_hi;
            heap.push_back(std::move(first));std::push_heap(heap.begin(),heap.end(),compare);
            heap.push_back(std::move(second));std::push_heap(heap.begin(),heap.end(),compare);
        }
    } catch (const Rejection &e) {return {e.what(),{},cells,evaluations,provisional_union,provisional_excess};}
    catch (const std::exception &e) {return {"MATERIAL_UNION_NUMERIC_FAILURE: "+std::string(e.what()),{},cells,evaluations,provisional_union,provisional_excess};}
}
}

MaterialUnionResult integrate_material_union(const NominalMaterialView &view, const SceneBox &requested_domain,
                                              const MaterialUnionLimits &requested_limits)
{
    const auto cursor=view.snapshot;const auto domain=requested_domain;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();const auto result=union_integral(cursor,domain,limits,started);
    MaterialUnionResult output{result.reason,{},result.cells,result.evaluations,result.provisional_union,result.provisional_excess};
    if (!result.amounts) return output;
    try {
        stop(limits,cursor->sequence->revision,started);const auto &a=*result.amounts;
        auto snapshot=std::shared_ptr<const MaterialUnionSnapshot>(new MaterialUnionSnapshot(cursor,domain,a[0],a[1],a[2],result.cells,result.evaluations));
        stop(limits,cursor->sequence->revision,started);output.snapshot=std::move(snapshot);
    } catch (const Rejection &e) {output.reason=e.what();}
    catch (const std::exception &e) {output.reason="MATERIAL_UNION_NUMERIC_FAILURE: "+std::string(e.what());}
    return output;
}

MaterialFillResult reconcile_material_fill(const MaterialIntegralResult &requested_target, const MaterialUnionResult &requested_occupied,
                                           const MaterialFillLimits &requested_limits)
{
    const auto target=requested_target.proof;const auto occupied=requested_occupied.snapshot;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();MaterialFillResult result;
    try {
        detail::require_interval_environment();
        if (!target || !occupied || !target->source || !target->source->sequence || !occupied->source || !occupied->source->sequence ||
            !limits.max_cells || limits.max_cells>65535 || !limits.max_evaluations || limits.max_evaluations>2000000 ||
            !limits.max_depth || limits.max_depth>32 || !valid_timeout(limits.timeout) || limits.maximum_interval_width.value()<=0)
            reject("INVALID_MATERIAL_FILL_INPUT");
        const auto &body=*target->source->sequence,&cap=*occupied->source->sequence;
        const auto poll=[&] {stop(limits,body.revision,started);stop(limits,cap.revision,started);};poll();
        if (body.revision!=cap.revision || body.model.model_id!=cap.model.model_id) reject("MATERIAL_FILL_SOURCE_MISMATCH");
        if (body.source_fingerprint!=cap.source_fingerprint) {
            // Native cap ledgers name their complete laid-body ledger as parent,
            // rather than its earlier geometry source. Verify that exact parent;
            // a partial body prefix cannot claim a complete-sequence derivation.
            if (target->source->completed_records!=body.records.size() || target->source->current_progress!=0)
                reject("MATERIAL_FILL_SOURCE_MISMATCH");
            if (body.records.size()>limits.max_evaluations) reject("MATERIAL_FILL_WORK_LIMIT");
            result.evaluations+=body.records.size();
            if (sequence_hash(body,poll)!=cap.source_fingerprint) reject("MATERIAL_FILL_SOURCE_MISMATCH");
        }
        const auto &r=target->target.footprint;const auto &box=occupied->domain;
        if (box.min.x()!=r.min_x || box.max.x()!=r.max_x || box.min.y()!=r.min_y || box.max.y()!=r.max_y)
            reject("MATERIAL_FILL_XY_DOMAIN_MISMATCH");
        UnionRoof roof{target->source,target->cells.front().roof.lo};
        const auto &surface=target->target;
        const auto z11=Interval(surface.z10)+Interval(surface.z01)-Interval(surface.z00);
        const double maximum_top=std::max({surface.z00,surface.z10,surface.z01,z11.hi});
        if (box.max.z()<maximum_top) reject("MATERIAL_FILL_INCOMPLETE_Z_DOMAIN");
        for (const auto &leaf : target->cells) {
            poll();if (box.min.z()>leaf.roof.lo) reject("MATERIAL_FILL_INCOMPLETE_Z_DOMAIN");
            if (result.evaluations>=limits.max_evaluations) reject("MATERIAL_FILL_WORK_LIMIT");++result.evaluations;
            roof.minimum=std::min(roof.minimum,leaf.roof.lo);
        }
        const auto target_volume=interval(target->total_volume),total=interval(occupied->union_volume_mm3);
        const auto available=Interval(limits.maximum_interval_width.value())-(Interval(target_volume.hi)-Interval(target_volume.lo))-(Interval(total.hi)-Interval(total.lo));
        if (available.lo<=0) reject("MATERIAL_FILL_INPUT_PRECISION_TOO_COARSE");
        // Reserve half the remaining width for final outward sums/subtractions.
        const double component_precision=available.lo/4;
        std::array<ScalarBounds,2> components;
        for (size_t i=0;i<components.size();++i) {
            poll();if (result.cells>=limits.max_cells || result.evaluations>=limits.max_evaluations) reject("MATERIAL_FILL_WORK_LIMIT");
            MaterialUnionLimits remaining=limits;remaining.max_cells-=result.cells;remaining.max_evaluations-=result.evaluations;
            remaining.maximum_interval_width=Volume(component_precision);
            const auto clipped=union_integral(occupied->source,box,remaining,started,
                i==0 ? UnionClip::BelowRoof : UnionClip::AboveSurface,&roof,&surface);
            result.cells+=clipped.cells;result.evaluations+=clipped.evaluations;
            (i==0 ? result.provisional_below_roof_mm3 : result.provisional_above_surface_mm3)=clipped.provisional_union;poll();
            if (!clipped.amounts) throw Rejection(clipped.reason);
            components[i]=(*clipped.amounts)[0];
        }
        auto outside=interval(components[0])+interval(components[1]);
        if (outside.lo>total.hi) reject("MATERIAL_FILL_INCONSISTENT_SPILL");
        outside={std::max(0.,outside.lo),std::min(total.hi,outside.hi)};
        const auto remainder=total-outside;
        const Interval covered(std::max(0.,remainder.lo),std::min({total.hi,target_volume.hi,remainder.hi}));
        const auto deficit=target_volume-covered;
        const Interval missing(std::max(0.,deficit.lo),std::min(target_volume.hi,std::max(0.,deficit.hi)));
        for (auto v : {target_volume,total,covered,missing,outside,interval(components[0]),interval(components[1])})
            if ((Interval(v.hi)-Interval(v.lo)).hi>limits.maximum_interval_width.value()) reject("MATERIAL_FILL_PRECISION_LIMIT");
        poll();auto snapshot=std::shared_ptr<const MaterialFillSnapshot>(new MaterialFillSnapshot(target,occupied,bounds(target_volume),bounds(covered),
            bounds(missing),bounds(outside),components[0],components[1],result.cells,result.evaluations));
        poll();result.snapshot=std::move(snapshot);result.reason="BOUNDED_NOMINAL_TARGET_DEFICIT_AND_Z_SPILL_ONLY";
    } catch (const Rejection &e) {result.reason=e.what();}
    catch (const std::exception &e) {result.reason="MATERIAL_FILL_NUMERIC_FAILURE: "+std::string(e.what());}
    return result;
}
MaterialVoidResult classify_material_voids(const MaterialFillResult &requested,const MaterialFillLimits &requested_limits)
{
    const auto source=requested.snapshot;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();MaterialVoidResult result;
    try {
        detail::require_interval_environment();
        if (!source || !source->target || !source->occupied || !limits.max_cells || limits.max_cells>65535 ||
            !limits.max_evaluations || limits.max_evaluations>2000000 || !limits.max_depth || limits.max_depth>32 ||
            !valid_timeout(limits.timeout) || limits.maximum_interval_width.value()<=0) reject("INVALID_MATERIAL_VOID_INPUT");
        const auto target=source->target;const auto occupied=source->occupied;
        const auto poll=[&] {stop(limits,target->source->sequence->revision,started);stop(limits,occupied->source->sequence->revision,started);};
        poll();UnionRoof roof{target->source,target->cells.front().roof.lo};
        for (const auto &leaf : target->cells) {
            poll();if (result.evaluations>=limits.max_evaluations) reject("MATERIAL_VOID_WORK_LIMIT");++result.evaluations;
            roof.minimum=std::min(roof.minimum,leaf.roof.lo);
        }
        const auto total=interval(source->target_volume_mm3),covered=interval(source->covered_target_mm3),missing=interval(source->missing_target_mm3);
        const auto available=Interval(limits.maximum_interval_width.value())-
            detail::maximum(Interval(total.hi)-Interval(total.lo),Interval(covered.hi)-Interval(covered.lo));
        if (available.lo<=0) reject("MATERIAL_VOID_INPUT_PRECISION_TOO_COARSE");
        if (result.evaluations>=limits.max_evaluations) reject("MATERIAL_VOID_WORK_LIMIT");
        MaterialUnionLimits remaining=limits;remaining.max_evaluations-=result.evaluations;
        // Allocate the unused caller width to this one integral. Every final
        // outward difference is checked again against the original hard limit;
        // rounding can still cause a conservative refusal.
        remaining.maximum_interval_width=Volume(std::nextafter(available.lo,0.));
        const auto shadow=union_integral(occupied->source,occupied->domain,remaining,started,UnionClip::TargetShadow,&roof,&target->target);
        result.cells+=shadow.cells;result.evaluations+=shadow.evaluations;result.provisional_shadow_mm3=shadow.provisional_union;
        poll();if (!shadow.amounts) throw Rejection(shadow.reason);
        const auto raw=interval((*shadow.amounts)[0]);
        if (raw.hi<covered.lo || raw.lo>total.hi) reject("MATERIAL_VOID_INCONSISTENT_SHADOW");
        const Interval shade(std::max(covered.lo,raw.lo),std::min(total.hi,raw.hi));
        const auto under=shade-covered,clear=total-shade;
        const Interval under_missing(std::max(0.,under.lo),std::min(missing.hi,std::max(0.,under.hi)));
        const Interval clear_missing(std::max(0.,clear.lo),std::min(missing.hi,std::max(0.,clear.hi)));
        for (auto volume : {shade,under_missing,clear_missing})
            if ((Interval(volume.hi)-Interval(volume.lo)).hi>limits.maximum_interval_width.value()) reject("MATERIAL_VOID_PRECISION_LIMIT");
        poll();auto snapshot=std::shared_ptr<const MaterialVoidSnapshot>(new MaterialVoidSnapshot(source,bounds(shade),
            bounds(under_missing),bounds(clear_missing),result.cells,result.evaluations));
        poll();result.snapshot=std::move(snapshot);result.reason="BOUNDED_NOMINAL_UNDER_MATERIAL_AND_VERTICAL_CLEAR_DEFICIT_ONLY";
    } catch (const Rejection &e) {result.reason=e.what();}
    catch (const std::exception &e) {result.reason="MATERIAL_VOID_NUMERIC_FAILURE: "+std::string(e.what());}
    return result;
}

MaterialDeficitResult locate_material_deficit(const MaterialFillResult &requested,const std::vector<double> &requested_x,
    const std::vector<double> &requested_y,const MaterialDeficitLimits &requested_limits)
{
    const auto source=requested.snapshot;const auto limits=requested_limits;const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!source || !source->target || !source->occupied || requested_x.size()<2 || requested_y.size()<2 ||
            requested_x.size()>257 || requested_y.size()>257 || !limits.max_regions || limits.max_regions>256 ||
            (requested_x.size()-1)*(requested_y.size()-1)>limits.max_regions ||
            !limits.max_cells || limits.max_cells>65535 || !limits.max_evaluations || limits.max_evaluations>200000 ||
            !valid_timeout(limits.timeout) || limits.maximum_interval_width.value()<=0) reject("INVALID_MATERIAL_DEFICIT_GRID");
        const auto x=requested_x,y=requested_y;const auto target=source->target;const auto cursor=source->occupied->source;
        const auto &sequence=*cursor->sequence;const auto &r=target->target.footprint;
        const auto poll=[&] {stop(limits,target->source->sequence->revision,started);stop(limits,sequence.revision,started);};poll();
        if (x.front()!=r.min_x || x.back()!=r.max_x || y.front()!=r.min_y || y.back()!=r.max_y)
            reject("INCOMPLETE_MATERIAL_DEFICIT_GRID");
        for (const auto &cuts : {x,y}) for (size_t i=0;i<cuts.size();++i) {
            coordinate(cuts[i]);if (i && cuts[i]<=cuts[i-1]) reject("UNORDERED_MATERIAL_DEFICIT_GRID");
        }
        const size_t end=cursor->completed_records+(cursor->current_progress>0 && cursor->completed_records<sequence.records.size());
        std::vector<MaterialDeficitCell> cells;size_t fragments=0,evaluations=0;
        Exact total_lower(0),total_upper(0),missing_lower(0);
        const AffineCapCell flat{r,0,0,0};
        for (size_t yi=1;yi<y.size();++yi) for (size_t xi=1;xi<x.size();++xi) {
            poll();const RectangleXY region{x[xi-1],y[yi-1],x[xi],y[yi]};
            const auto volume=target->rectangle_volume(region,limits,fragments,evaluations,poll,"MATERIAL_DEFICIT");
            const Polygon polygon{{Exact(region.min_x),Exact(region.min_y)},{Exact(region.max_x),Exact(region.min_y)},
                {Exact(region.max_x),Exact(region.max_y)},{Exact(region.min_x),Exact(region.max_y)}};
            Exact possible_amount(0);
            for (size_t i=0;i<end;++i) {
                if (evaluations>=limits.max_evaluations) reject("MATERIAL_DEFICIT_WORK_LIMIT");
                if (evaluations%128==0) poll();++evaluations;const auto &row=sequence.records[i];if (!row.bead) continue;
                const double fraction=i<cursor->completed_records ? 1 : cursor->current_progress;
                const auto footprint=roof_projection(row,sequence.model,polygon,fraction,Representation::Nominal);
                if (!footprint) continue;Exact area(0);affine_integral(footprint->polygon,flat,&area);if (area==0) continue;
                // Every nominal point is inside this exact-clipped outer XY
                // footprint. Count the complete laid fraction, deliberately
                // overestimating a partial intersection and all overlaps. This
                // can only weaken a missing-volume lower bound, never fill it.
                possible_amount+=Exact(std::get<Deposition>(row.motion.payload).volume.value())*Exact(fraction);
            }
            const double amount_upper=exact_interval(possible_amount).hi;
            const double covered_upper=std::min({amount_upper,volume.upper,source->covered_target_mm3.upper});
            const double missing=std::max(0.,(Interval(volume.lower)-Interval(covered_upper)).lo);
            cells.push_back({region,volume,amount_upper,covered_upper,missing});
            total_lower+=Exact(volume.lower);total_upper+=Exact(volume.upper);missing_lower+=Exact(missing);
        }
        const Interval total(exact_interval(total_lower).lo,exact_interval(total_upper).hi);
        if (total.hi<source->target_volume_mm3.lower || total.lo>source->target_volume_mm3.upper)
            reject("MATERIAL_DEFICIT_INCONSISTENT_TARGET");
        if ((Interval(total.hi)-Interval(total.lo)).hi>limits.maximum_interval_width.value()) reject("MATERIAL_DEFICIT_GLOBAL_PRECISION");
        const double missing=exact_interval(missing_lower).lo;
        if (missing>source->missing_target_mm3.upper) reject("MATERIAL_DEFICIT_INCONSISTENT_LOWER_BOUND");
        poll();auto snapshot=std::shared_ptr<const MaterialDeficitSnapshot>(new MaterialDeficitSnapshot(source,std::move(cells),bounds(total),missing,fragments,evaluations));
        poll();return {"BOUNDED_COMPLETE_GRID_MISSING_VOLUME_LOWER_WITNESSES_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) {return {e.what(),{}};}
    catch (const std::exception &e) {return {"MATERIAL_DEFICIT_NUMERIC_FAILURE: "+std::string(e.what()),{}};}
}

RemainingHatchResult plan_remaining_first_hatch(const AffineHatchResult &requested_hatches,size_t line_index,
    const MaterialFillResult &requested_fill,const RemainingHatchPolicy &requested_policy,const RemainingHatchLimits &requested_limits)
{
    const auto hatches=requested_hatches.snapshot;const auto before=requested_fill.snapshot;
    const auto policy=requested_policy;const auto limits=requested_limits;const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!hatches || !hatches->source || hatches->passes.empty() || line_index>=hatches->passes.front().lines.size() ||
            !before || !before->target || !before->occupied || !limits.max_paths || limits.max_paths>256 ||
            !limits.max_cells || limits.max_cells>65535 || !limits.max_evaluations || limits.max_evaluations>2000000 ||
            !valid_timeout(limits.timeout) || !valid_timeout(limits.beads.timeout) || !valid_timeout(limits.beads.packets.timeout) ||
            !valid_timeout(limits.volumes.timeout) || limits.maximum_result_width.value()<=0 ||
            !limits.beads.max_evaluations || limits.beads.max_evaluations>200000 ||
            !limits.beads.max_roof_segments || limits.beads.max_roof_segments>65535 || !limits.beads.max_depth || limits.beads.max_depth>32 ||
            !limits.beads.packets.max_segments || limits.beads.packets.max_segments>65535 || !limits.beads.packets.max_depth || limits.beads.packets.max_depth>32 ||
            limits.beads.maximum_gap_error.value()<=0 || limits.beads.maximum_gap_error.value()>.05 ||
            limits.beads.packets.maximum_width_error.value()<=0 || limits.beads.packets.maximum_width_error.value()>.05 ||
            limits.beads.packets.maximum_volume_error.value()<=0 || !limits.volumes.max_cells || limits.volumes.max_cells>65535 ||
            !limits.volumes.max_evaluations || limits.volumes.max_evaluations>2000000 || !limits.volumes.max_depth || limits.volumes.max_depth>32 ||
            limits.volumes.maximum_interval_width.value()<=0 ||
            policy.xy_separation.value()<=0 || policy.minimum_covered_gain.value()<=0)
            reject("INVALID_REMAINING_HATCH_INPUT");
        const auto stack=hatches->source;const auto target=before->target;const auto cursor=before->occupied->source;
        const auto sequence=cursor->sequence;
        const auto poll=[&] {
            stop(limits,sequence->revision,started);stop(limits.beads,sequence->revision,started);
            stop(limits.beads.packets,sequence->revision,started);stop(limits.volumes,sequence->revision,started);
        };poll();
        const auto &a=target->target,&b=stack->surfaces.front().cell;
        if (target->source!=stack->source || a.footprint.min_x!=b.footprint.min_x || a.footprint.max_x!=b.footprint.max_x ||
            a.footprint.min_y!=b.footprint.min_y || a.footprint.max_y!=b.footprint.max_y || a.z00!=b.z00 || a.z10!=b.z10 || a.z01!=b.z01)
            reject("REMAINING_HATCH_TARGET_SOURCE_MISMATCH");
        const auto &line=hatches->passes.front().lines[line_index];
        const bool x_axis=line.start.y()==line.end.y();
        if (x_axis==(line.start.x()==line.end.x())) reject("REMAINING_HATCH_REQUIRES_AXIS_LINE");
        const Exact begin(x_axis ? line.start.x() : line.start.y()),end(x_axis ? line.end.x() : line.end.y());
        if (begin>=end) reject("REMAINING_HATCH_REQUIRES_FORWARD_LINE");
        const Exact half=(Exact(line.width.value())+Exact(limits.beads.packets.maximum_width_error.value()))/Exact(2);
        const Exact x0=Exact(line.start.x())-(x_axis ? Exact(0) : half),x1=Exact(line.end.x())+(x_axis ? Exact(0) : half);
        const Exact y0=Exact(line.start.y())-(x_axis ? half : Exact(0)),y1=Exact(line.end.y())+(x_axis ? half : Exact(0));
        const Polygon strip{{x0,y0},{x1,y0},{x1,y1},{x0,y1}};
        size_t work=0,cells=0,bead_work=0,leaves=0,packets=0;
        const auto charge=[&](size_t count) {if (count>limits.max_evaluations-work) reject("REMAINING_HATCH_WORK_LIMIT");work+=count;poll();};
        std::vector<std::pair<Exact,Exact>> blocked;
        const size_t active=cursor->completed_records+(cursor->current_progress>0 && cursor->completed_records<sequence->records.size());
        for (size_t i=0;i<active;++i) {
            charge(1);const auto &row=sequence->records[i];if (!row.bead) continue;
            const double fraction=i<cursor->completed_records ? 1 : cursor->current_progress;
            const auto projected=roof_projection(row,sequence->model,strip,fraction,Representation::Upper);
            if (!projected) continue;
            Exact lo=projected->polygon.front()[x_axis ? 0 : 1],hi=lo;
            for (const auto &p : projected->polygon) {lo=std::min(lo,p[x_axis ? 0 : 1]);hi=std::max(hi,p[x_axis ? 0 : 1]);}
            blocked.emplace_back(std::max(begin,lo-Exact(policy.xy_separation.value())),std::min(end,hi+Exact(policy.xy_separation.value())));
        }
        std::sort(blocked.begin(),blocked.end(),[&](const auto &a,const auto &b) {charge(1);return a<b;});poll();
        std::vector<std::pair<Exact,Exact>> free;Exact position=begin;
        for (const auto &interval : blocked) {
            if (interval.first>position) free.emplace_back(position,interval.first);
            position=std::max(position,interval.second);
        }
        if (position<end) free.emplace_back(position,end);
        if (free.empty()) reject("REMAINING_HATCH_NO_FREE_INTERVAL");
        if (free.size()>limits.max_paths) reject("REMAINING_HATCH_PATH_LIMIT");
        std::vector<std::shared_ptr<const FirstHatchBeadSnapshot>> paths;Exact amount_lower(0),amount_upper(0),delivered(0);
        double numeric=sequence->model.numerical_coordinate_error.value();
        const auto endpoint=[&](double along) {
            const Exact t=(Exact(along)-begin)/(end-begin);
            const auto z=stored_exact(Exact(line.start.z())+(Exact(line.end.z())-Exact(line.start.z()))*t);
            return std::pair<PhysicalPosition,double>{x_axis ? PhysicalPosition(along,line.start.y(),z.first) :
                PhysicalPosition(line.start.x(),along,z.first),z.second};
        };
        for (const auto &window : free) {
            poll();const double lo=exact_interval(window.first).hi,hi=exact_interval(window.second).lo;
            if (lo>=hi) reject("REMAINING_HATCH_UNREPRESENTABLE_INTERVAL");
            // The first-bead solver hashes its full source and collects the
            // active records before its reported roof queries.
            charge(2*stack->source->sequence->records.size());
            auto slice=line;const auto first=endpoint(lo),last=endpoint(hi);slice.start=first.first;slice.end=last.first;
            auto remaining=limits.beads;
            if (bead_work>=remaining.max_evaluations || leaves>=remaining.max_roof_segments || packets>=remaining.packets.max_segments)
                reject("REMAINING_HATCH_BEAD_WORK_LIMIT");
            remaining.max_evaluations=std::min(remaining.max_evaluations-bead_work,limits.max_evaluations-work);
            remaining.max_roof_segments-=leaves;remaining.packets.max_segments-=packets;
            // All fragments share the original whole-line amount budget.
            const auto fraction=exact_interval((Exact(hi)-Exact(lo))/(end-begin));
            remaining.packets.maximum_volume_error=Volume((Interval(limits.beads.packets.maximum_volume_error.value())*fraction/Interval(2)).lo);
            const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
            remaining.timeout-=elapsed;remaining.packets.timeout-=elapsed;
            remaining.cancelled=[&] {poll();return false;};remaining.is_current={};remaining.packets.cancelled=remaining.cancelled;remaining.packets.is_current={};
            const auto plan=FirstHatchBeadSnapshot::plan({"",hatches},line_index,remaining,FirstHatchRoofDomain::FiniteWidth,&slice,
                (Interval(first.second)+Interval(last.second)).hi);
            if (!plan.snapshot) throw Rejection(plan.reason);
            charge(plan.snapshot->evaluations);bead_work+=plan.snapshot->evaluations;leaves+=plan.snapshot->roof_segments;packets+=plan.snapshot->pieces.size();
            numeric=std::max(numeric,plan.snapshot->numerical_error_upper_mm);
            amount_lower+=Exact(plan.snapshot->actual_target_volume_mm3.lower);amount_upper+=Exact(plan.snapshot->actual_target_volume_mm3.upper);
            for (const auto &piece : plan.snapshot->pieces) delivered+=Exact(piece.volume.value());
            paths.push_back(plan.snapshot);
        }
        const auto deviation=exact_interval(delivered)-Interval(exact_interval(amount_lower).lo,exact_interval(amount_upper).hi);
        if (std::max(std::abs(deviation.lo),std::abs(deviation.hi))>limits.beads.packets.maximum_volume_error.value())
            reject("REMAINING_HATCH_GLOBAL_AMOUNT_ERROR");
        const MaterialRecord *context=nullptr;
        for (const auto &row : sequence->records) {charge(1);if (row.bead) {context=&row;break;}}
        if (!context) reject("REMAINING_HATCH_MISSING_DEPOSITION_CONTEXT");
        const auto metadata=std::get<Deposition>(context->motion.payload);
        std::vector<MaterialRecord> rows;
        for (const auto &path : paths) {
            if (!rows.empty()) {const size_t i=rows.size();rows.push_back({{i+1,i,0,rows.back().motion.end,path->path_start,
                context->motion.speed_limit,context->motion.acceleration_limit,Travel{}},{}});}
            for (const auto &piece : path->pieces) {
                charge(1);const size_t i=rows.size();rows.push_back({{i+1,i,context->motion.source_patch_id,piece.start,piece.end,
                    context->motion.speed_limit,context->motion.acceleration_limit,Deposition{piece.volume,piece.nominal_width,
                    VerticalGap(std::min(piece.section.gap_begin_mm,piece.section.gap_end_mm)),VerticalGap(std::max(piece.section.gap_begin_mm,piece.section.gap_end_mm)),
                    metadata.material,metadata.support_provenance_id,metadata.contact_model_id}},piece.section});
            }
        }
        auto model=sequence->model;model.numerical_coordinate_error=Length(numeric);
        MaterialLimits capture;capture.timeout=limits.timeout-std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        capture.cancelled=[&] {poll();return false;};
        // Capture validates and hashes the rows; material_at walks the prefix.
        charge(3*rows.size());
        const auto ledger=capture_material_sequence(rows,model,sequence->revision,sequence->source_fingerprint,capture);
        if (!ledger.snapshot) throw Rejection(ledger.reason);
        const auto prefix=material_at(ledger.snapshot,rows.size(),0,capture);if (!prefix.nominal.snapshot) throw Rejection(prefix.reason);
        auto volume_limits=limits.volumes;volume_limits.max_cells=std::min(volume_limits.max_cells,limits.max_cells);
        volume_limits.max_evaluations=std::min(volume_limits.max_evaluations,limits.max_evaluations-work);
        volume_limits.maximum_interval_width=Volume(limits.volumes.maximum_interval_width.value()/4);
        volume_limits.timeout-=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        volume_limits.cancelled=[&] {poll();return false;};volume_limits.is_current={};
        const auto occupied=integrate_material_union(prefix.nominal,before->occupied->domain,volume_limits);
        if (!occupied.snapshot) throw Rejection(occupied.reason);charge(occupied.evaluations);cells+=occupied.cells;
        if (cells>=limits.max_cells) reject("REMAINING_HATCH_CELL_LIMIT");
        MaterialFillLimits fill_limits;static_cast<MaterialUnionLimits &>(fill_limits)=volume_limits;
        fill_limits.max_cells=std::min(limits.volumes.max_cells,limits.max_cells-cells);
        fill_limits.max_evaluations=std::min(limits.volumes.max_evaluations,limits.max_evaluations-work);
        fill_limits.maximum_interval_width=limits.volumes.maximum_interval_width;
        fill_limits.timeout=limits.volumes.timeout-std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        MaterialIntegralResult target_result;target_result.proof=target;
        const auto extra=reconcile_material_fill(target_result,occupied,fill_limits);
        if (!extra.snapshot) throw Rejection(extra.reason);charge(extra.evaluations);cells+=extra.cells;
        // Every new nominal footprint was excluded from every laid D_upper
        // projection. Thus old/new sets are disjoint, including inside T. Keep
        // both immutable prefixes; never round or rewrite the old current butt.
        const auto covered=interval(before->covered_target_mm3)+interval(extra.snapshot->covered_target_mm3);
        const auto spill=interval(before->outside_target_mm3)+interval(extra.snapshot->outside_target_mm3);
        const Interval outside(std::max(0.,spill.lo),spill.hi);
        const auto v=interval(before->target_volume_mm3),d=v-covered;
        if (covered.lo>v.hi) reject("REMAINING_HATCH_INCONSISTENT_COVERAGE");
        const Interval c(covered.lo,std::min(v.hi,covered.hi)),missing(std::max(0.,d.lo),std::min(v.hi,std::max(0.,d.hi)));
        for (auto measure : {c,missing,outside}) if ((Interval(measure.hi)-Interval(measure.lo)).hi>limits.maximum_result_width.value())
            reject("REMAINING_HATCH_RESULT_PRECISION");
        const double gain=extra.snapshot->covered_target_mm3.lower;
        if (gain<policy.minimum_covered_gain.value()) reject("REMAINING_HATCH_INSUFFICIENT_COVERED_GAIN");
        if (outside.hi>policy.maximum_outside_target.value()) reject("REMAINING_HATCH_OUTSIDE_TARGET_LIMIT");
        poll();auto snapshot=std::shared_ptr<const RemainingHatchSnapshot>(new RemainingHatchSnapshot(before,extra.snapshot,policy,std::move(paths),
            bounds(c),bounds(missing),bounds(outside),gain,cells,work));
        poll();return {"BOUNDED_DISJOINT_REMAINING_FIRST_HATCH_WITH_POSITIVE_NOMINAL_FILL_GAIN_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) {return {e.what(),{}};}
    catch (const std::exception &e) {return {"REMAINING_HATCH_NUMERIC_FAILURE: "+std::string(e.what()),{}};}
}

namespace {
bool valid_first_hatch_layer_limits(const FirstHatchLayerLimits &l)
{
    return l.max_paths && l.max_paths<=4096 && l.max_records && l.max_records<=200000 && l.max_cells && l.max_cells<=65535 &&
        l.max_evaluations && l.max_evaluations<=2000000 && valid_timeout(l.timeout) && valid_timeout(l.beads.timeout) &&
        valid_timeout(l.beads.packets.timeout) && valid_timeout(l.volumes.timeout) && l.beads.max_evaluations && l.beads.max_evaluations<=200000 &&
        l.beads.max_roof_segments && l.beads.max_roof_segments<=65535 && l.beads.max_depth && l.beads.max_depth<=32 &&
        l.beads.packets.max_segments && l.beads.packets.max_segments<=65535 && l.beads.packets.max_depth && l.beads.packets.max_depth<=32 &&
        l.beads.maximum_gap_error.value()>0 && l.beads.maximum_gap_error.value()<=.05 &&
        l.beads.packets.maximum_width_error.value()>0 && l.beads.packets.maximum_width_error.value()<=.05 && l.beads.packets.maximum_volume_error.value()>0 &&
        l.volumes.max_cells && l.volumes.max_cells<=65535 && l.volumes.max_evaluations && l.volumes.max_evaluations<=2000000 &&
        l.volumes.max_depth && l.volumes.max_depth<=32 && l.volumes.maximum_interval_width.value()>0;
}
struct FirstCandidateMeasurement {
    std::shared_ptr<const MaterialFillSnapshot> fill;
    ScalarBounds target,delivered;
    double error,numeric;
    size_t roofs,cells;
};
FirstCandidateMeasurement measure_first_candidate(const std::shared_ptr<const AffineHatchSnapshot> &hatches,
    const std::vector<std::shared_ptr<const FirstHatchBeadSnapshot>> &paths,const SceneBox &box,const FirstHatchLayerLimits &limits,
    std::chrono::steady_clock::time_point started,size_t &work)
{
    const auto stack=hatches->source;const auto cursor=stack->source;const auto sequence=cursor->sequence;
    const auto poll=[&] {
        stop(limits,sequence->revision,started);stop(limits.beads,sequence->revision,started);
        stop(limits.beads.packets,sequence->revision,started);stop(limits.volumes,sequence->revision,started);
    };
    const auto charge=[&](size_t count) {if (count>limits.max_evaluations-work) reject("FIRST_HATCH_LAYER_WORK_LIMIT");work+=count;poll();};
    const auto elapsed=[&] {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);};
    Exact target_lower(0),target_upper(0),amount(0);double numeric=sequence->model.numerical_coordinate_error.value();size_t roofs=0,packets=0,cells=0;
    for (const auto &path : paths) {
        charge(1);
        if (!path || path->source!=hatches || path->roof_domain!=FirstHatchRoofDomain::FiniteWidth) reject("FIRST_HATCH_LAYER_PATH_SOURCE_MISMATCH");
        target_lower+=Exact(path->actual_target_volume_mm3.lower);target_upper+=Exact(path->actual_target_volume_mm3.upper);
        for (const auto &piece : path->pieces) {charge(1);amount+=Exact(piece.volume.value());}
        numeric=std::max(numeric,path->numerical_error_upper_mm);roofs+=path->roof_segments;packets+=path->pieces.size();
    }
    const Interval target(exact_interval(target_lower).lo,exact_interval(target_upper).hi),delivered=exact_interval(amount);
    const auto difference=delivered-target;const double error=std::max(std::abs(difference.lo),std::abs(difference.hi));
    if (error>limits.beads.packets.maximum_volume_error.value()) reject("FIRST_HATCH_LAYER_GLOBAL_AMOUNT_ERROR");
    const MaterialRecord *context=nullptr;
    const size_t active=cursor->completed_records+(cursor->current_progress>0 && cursor->completed_records<sequence->records.size());
    for (size_t i=0;i<active;++i) {charge(1);if (sequence->records[i].bead) {context=&sequence->records[i];break;}}
    if (!context) reject("FIRST_HATCH_LAYER_MISSING_DEPOSITION_CONTEXT");
    const auto metadata=std::get<Deposition>(context->motion.payload);
    std::vector<MaterialRecord> rows;
    const auto append=[&](MaterialRecord row) {
        if (rows.size()>=limits.max_records) reject("FIRST_HATCH_LAYER_RECORD_LIMIT");
        charge(1);rows.push_back(std::move(row));
    };
    for (const auto &path : paths) {
        if (!rows.empty() && (rows.back().motion.end.x()!=path->path_start.x() ||
            rows.back().motion.end.y()!=path->path_start.y() || rows.back().motion.end.z()!=path->path_start.z())) {const size_t i=rows.size();append({{i+1,i,0,rows.back().motion.end,path->path_start,
            context->motion.speed_limit,context->motion.acceleration_limit,Travel{}},{}});}
        for (const auto &piece : path->pieces) {
            const bool x=piece.start.y()==piece.end.y();
            if (x==(piece.start.x()==piece.end.x())) reject("FIRST_HATCH_LAYER_REQUIRES_AXIS_PACKETS");
            const auto half=Interval(piece.section.width_mm.upper)/Interval(2);
            const auto xmin=Interval(std::min(piece.start.x(),piece.end.x()))-(x ? Interval(0) : half);
            const auto xmax=Interval(std::max(piece.start.x(),piece.end.x()))+(x ? Interval(0) : half);
            const auto ymin=Interval(std::min(piece.start.y(),piece.end.y()))-(x ? half : Interval(0));
            const auto ymax=Interval(std::max(piece.start.y(),piece.end.y()))+(x ? half : Interval(0));
            const auto bottom=Interval(std::min(piece.start.z(),piece.end.z()))-Interval(std::max(piece.section.gap_begin_mm,piece.section.gap_end_mm));
            if (xmin.lo<box.min.x() || xmax.hi>box.max.x() || ymin.lo<box.min.y() || ymax.hi>box.max.y() ||
                bottom.lo<box.min.z() || std::max(piece.start.z(),piece.end.z())>box.max.z())
                reject("FIRST_HATCH_LAYER_INCOMPLETE_NOMINAL_DOMAIN");
            const size_t i=rows.size();append({{i+1,i,context->motion.source_patch_id,piece.start,piece.end,
                context->motion.speed_limit,context->motion.acceleration_limit,Deposition{piece.volume,piece.nominal_width,
                VerticalGap(std::min(piece.section.gap_begin_mm,piece.section.gap_end_mm)),VerticalGap(std::max(piece.section.gap_begin_mm,piece.section.gap_end_mm)),
                metadata.material,metadata.support_provenance_id,metadata.contact_model_id}},piece.section});
        }
    }
    auto model=sequence->model;model.numerical_coordinate_error=Length(numeric);
    MaterialLimits capture;capture.max_records=limits.max_records;capture.timeout=limits.timeout-elapsed();capture.cancelled=[&] {poll();return false;};
    charge(3*rows.size()); // Validation/hash, then prefix walk.
    const auto ledger=capture_material_sequence(rows,model,sequence->revision,sequence->source_fingerprint,capture);
    if (!ledger.snapshot) throw Rejection(ledger.reason);
    const auto prefix=material_at(ledger.snapshot,rows.size(),0,capture);if (!prefix.nominal.snapshot) throw Rejection(prefix.reason);
    auto volume=limits.volumes;volume.max_cells=std::min(volume.max_cells,limits.max_cells);
    volume.max_evaluations=std::min(volume.max_evaluations,limits.max_evaluations-work);volume.timeout-=elapsed();
    volume.maximum_interval_width=Volume(limits.volumes.maximum_interval_width.value()/4);
    volume.cancelled=[&] {poll();return false;};volume.is_current={};
    const auto occupied=integrate_material_union(prefix.nominal,box,volume);
    if (!occupied.snapshot) throw Rejection(occupied.reason+" paths="+std::to_string(paths.size())+" packets="+std::to_string(packets)+
        " cells="+std::to_string(occupied.cells)+" work="+std::to_string(occupied.evaluations)+
        (occupied.provisional_union_mm3 ? " union_width="+std::to_string(occupied.provisional_union_mm3->upper-occupied.provisional_union_mm3->lower) : ""));
    charge(occupied.evaluations);cells+=occupied.cells;
    if (cells>=limits.max_cells) reject("FIRST_HATCH_LAYER_CELL_LIMIT");
    MaterialFillLimits fill;static_cast<MaterialUnionLimits &>(fill)=volume;
    fill.max_cells=std::min(limits.volumes.max_cells,limits.max_cells-cells);
    fill.max_evaluations=std::min(limits.volumes.max_evaluations,limits.max_evaluations-work);fill.timeout=limits.volumes.timeout-elapsed();
    fill.maximum_interval_width=limits.volumes.maximum_interval_width;
    const auto measured=reconcile_material_fill(stack->first_pass,occupied,fill);
    if (!measured.snapshot) throw Rejection(measured.reason);charge(measured.evaluations);cells+=measured.cells;
    poll();return {measured.snapshot,bounds(target),bounds(delivered),error,numeric,roofs,cells};
}

}

FirstHatchLayerResult plan_first_hatch_layer(const AffineHatchResult &requested,const SceneBox &requested_box,
                                            const FirstHatchLayerLimits &requested_limits)
{
    const auto hatches=requested.snapshot;const auto box=requested_box;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!hatches || !hatches->source || hatches->passes.empty() || hatches->passes.front().lines.empty() ||
            !hatches->source->first_pass.proof || !valid_first_hatch_layer_limits(limits) ||
            box.min.x()>=box.max.x() || box.min.y()>=box.max.y() || box.min.z()>=box.max.z())
            reject("INVALID_FIRST_HATCH_LAYER_INPUT");
        const auto stack=hatches->source;const auto cursor=stack->source;const auto sequence=cursor->sequence;
        const auto &lines=hatches->passes.front().lines;const auto &roi=stack->surfaces.front().cell.footprint;
        if (lines.size()>limits.max_paths) reject("FIRST_HATCH_LAYER_PATH_LIMIT");
        if (box.min.x()!=roi.min_x || box.max.x()!=roi.max_x || box.min.y()!=roi.min_y || box.max.y()!=roi.max_y)
            reject("FIRST_HATCH_LAYER_XY_DOMAIN_MISMATCH");
        const auto poll=[&] {
            stop(limits,sequence->revision,started);stop(limits.beads,sequence->revision,started);
            stop(limits.beads.packets,sequence->revision,started);stop(limits.volumes,sequence->revision,started);
        };poll();
        size_t work=0,bead_work=0,roofs=0,packets=0;
        const auto charge=[&](size_t count) {if (count>limits.max_evaluations-work) reject("FIRST_HATCH_LAYER_WORK_LIMIT");work+=count;poll();};
        const auto elapsed=[&] {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);};
        std::vector<std::shared_ptr<const FirstHatchBeadSnapshot>> paths;
        for (size_t i=0;i<lines.size();++i) {
            poll();charge(2*sequence->records.size()); // Source hash and candidate collection in each bead solver.
            if (bead_work>=limits.beads.max_evaluations || roofs>=limits.beads.max_roof_segments || packets>=limits.beads.packets.max_segments)
                reject("FIRST_HATCH_LAYER_BEAD_WORK_LIMIT");
            auto bead_limits=limits.beads;
            bead_limits.max_evaluations=std::min(limits.beads.max_evaluations-bead_work,limits.max_evaluations-work);
            bead_limits.max_roof_segments-=roofs;bead_limits.packets.max_segments-=packets;
            bead_limits.packets.maximum_volume_error=Volume((Interval(limits.beads.packets.maximum_volume_error.value())/Interval(double(lines.size()))/Interval(2)).lo);
            bead_limits.timeout-=elapsed();bead_limits.packets.timeout-=elapsed();
            bead_limits.cancelled=[&] {poll();return false;};bead_limits.is_current={};bead_limits.packets.cancelled=bead_limits.cancelled;bead_limits.packets.is_current={};
            const auto path=plan_first_hatch_footprint_bead({"",hatches},i,bead_limits);
            if (!path.snapshot) throw Rejection(path.reason);
            charge(path.snapshot->evaluations);bead_work+=path.snapshot->evaluations;roofs+=path.snapshot->roof_segments;packets+=path.snapshot->pieces.size();
            paths.push_back(path.snapshot);
        }
        const auto measured=measure_first_candidate(hatches,paths,box,limits,started,work);
        poll();auto snapshot=std::shared_ptr<const FirstHatchLayerSnapshot>(new FirstHatchLayerSnapshot(hatches,std::move(paths),measured.fill,
            measured.target,measured.delivered,measured.error,measured.numeric,measured.roofs,measured.cells,work));
        poll();return {"BOUNDED_COMPLETE_FIRST_HATCH_CANDIDATE_WITH_MEASURED_UNION_FILL_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) {return {e.what(),{}};}
    catch (const std::exception &e) {return {"FIRST_HATCH_LAYER_NUMERIC_FAILURE: "+std::string(e.what()),{}};}
}

FirstHatchReplanResult replan_first_hatch_ends(const FirstHatchLayerResult &requested,const FirstHatchReplanPolicy &requested_policy,
                                             const FirstHatchLayerLimits &requested_limits)
{
    const auto before=requested.snapshot;const auto policy=requested_policy;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!before || !before->source || !before->fill || !valid_first_hatch_layer_limits(limits) ||
            policy.minimum_covered_gain.value()<=0 || policy.maximum_outside_target.value()<0)
            reject("INVALID_FIRST_HATCH_REPLAN_INPUT");
        const auto hatches=before->source;const auto stack=hatches->source;
        if (hatches->first_pass_extent!=FirstHatchExtent::CapsuleInset) reject("FIRST_HATCH_EXTENT_ALREADY_REPLANNED");
        const auto revision=stack->source->sequence->revision;
        const auto poll=[&] {
            stop(limits,revision,started);stop(limits.beads,revision,started);
            stop(limits.beads.packets,revision,started);stop(limits.volumes,revision,started);
        };poll();size_t work=0;
        const auto charge=[&](size_t count) {if (count>limits.max_evaluations-work) reject("FIRST_HATCH_REPLAN_WORK_LIMIT");work+=count;poll();};
        charge(hatches->line_count+hatches->passes.size());auto passes=hatches->passes;
        if (passes.front().lines.size()>limits.max_paths) reject("FIRST_HATCH_REPLAN_PATH_LIMIT");
        const auto &cell=stack->surfaces.front().cell;const auto &roi=cell.footprint;
        const auto inset=Interval(hatches->policy.boundary_band.value())+Interval(hatches->numerical_error_upper_mm);
        const auto point=[&](double x,double y) {
            const Exact z=Exact(cell.z00)+(Exact(cell.z10)-Exact(cell.z00))*(Exact(x)-Exact(roi.min_x))/(Exact(roi.max_x)-Exact(roi.min_x))+
                (Exact(cell.z01)-Exact(cell.z00))*(Exact(y)-Exact(roi.min_y))/(Exact(roi.max_y)-Exact(roi.min_y));
            const auto stored=stored_exact(z);return std::pair<PhysicalPosition,double>{{x,y,stored.first},stored.second};
        };
        double extra_error=0;
        for (auto &line : passes.front().lines) {
            charge(1);const bool x=line.start.y()==line.end.y();
            if (x==(line.start.x()==line.end.x())) reject("FIRST_HATCH_REPLAN_REQUIRES_AXIS");
            const double low=(Interval(x ? roi.min_x : roi.min_y)+inset).hi,high=(Interval(x ? roi.max_x : roi.max_y)-inset).lo;
            const double a=x ? line.start.x() : line.start.y(),b=x ? line.end.x() : line.end.y();
            if (low>=std::min(a,b) || high<=std::max(a,b)) reject("FIRST_HATCH_REPLAN_NO_EXTENSION_DOMAIN");
            const auto first=point(x ? (a<b ? low : high) : line.start.x(),x ? line.start.y() : (a<b ? low : high));
            const auto last=point(x ? (a<b ? high : low) : line.end.x(),x ? line.end.y() : (a<b ? high : low));
            line.start=first.first;line.end=last.first;line.reverse_start=line.end;line.reverse_end=line.start;
            line.projected_length_mm=bounds(exact_interval(Exact(high)-Exact(low)));
            const double error=std::max(first.second,last.second);extra_error=std::max(extra_error,error);
            line.coordinate_error_upper_mm=(Interval(line.coordinate_error_upper_mm)+Interval(error)).hi;
        }
        const double numeric=(Interval(hatches->numerical_error_upper_mm)+Interval(extra_error)).hi;
        if (numeric>.05) reject("FIRST_HATCH_REPLAN_NUMERICAL_BUDGET");
        const auto next=std::shared_ptr<const AffineHatchSnapshot>(new AffineHatchSnapshot(stack,hatches->policy,std::move(passes),
            hatches->total_prospective_volume_mm3,hatches->line_count,numeric,FirstHatchExtent::FiniteButtInset));
        if (work>=limits.max_evaluations) reject("FIRST_HATCH_REPLAN_WORK_LIMIT");
        auto remaining=limits;remaining.max_evaluations-=work;
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        remaining.timeout-=elapsed;remaining.beads.timeout-=elapsed;remaining.beads.packets.timeout-=elapsed;remaining.volumes.timeout-=elapsed;
        remaining.cancelled=[&] {poll();return false;};remaining.is_current={};
        remaining.beads.cancelled={};remaining.beads.is_current={};remaining.beads.packets.cancelled={};remaining.beads.packets.is_current={};
        remaining.volumes.cancelled={};remaining.volumes.is_current={};
        const auto after=plan_first_hatch_layer({"",next},before->fill->occupied->domain,remaining);
        if (!after.snapshot) throw Rejection(after.reason);charge(after.snapshot->evaluations);
        if (after.snapshot->fill->target!=before->fill->target) reject("FIRST_HATCH_REPLAN_TARGET_CHANGED");
        const auto gain=interval(after.snapshot->fill->covered_target_mm3)-interval(before->fill->covered_target_mm3);
        const auto reduction=interval(before->fill->missing_target_mm3)-interval(after.snapshot->fill->missing_target_mm3);
        if (gain.lo<policy.minimum_covered_gain.value() || reduction.lo<policy.minimum_covered_gain.value()) reject("FIRST_HATCH_REPLAN_INSUFFICIENT_GAIN");
        if (after.snapshot->fill->outside_target_mm3.upper>policy.maximum_outside_target.value()) reject("FIRST_HATCH_REPLAN_OUTSIDE_TARGET_LIMIT");
        poll();auto snapshot=std::shared_ptr<const FirstHatchReplanSnapshot>(new FirstHatchReplanSnapshot(before,after.snapshot,policy,
            bounds(gain),bounds(reduction),after.snapshot->cells,work));
        poll();return {"BOUNDED_WHOLE_FIRST_CANDIDATE_END_REPLAN_WITH_MEASURED_FILL_GAIN_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) {return {e.what(),{}};}
    catch (const std::exception &e) {return {"FIRST_HATCH_REPLAN_NUMERIC_FAILURE: "+std::string(e.what()),{}};}
}

FirstHatchWidthReplanResult replan_first_hatch_width(const FirstHatchLayerResult &requested,WidthXY width,
    const FirstHatchWidthReplanPolicy &requested_policy,const FirstHatchLayerLimits &requested_limits)
{
    const auto before=requested.snapshot;const auto policy=requested_policy;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!before || !before->source || !before->fill || !valid_first_hatch_layer_limits(limits) ||
            width.value()<=0 || policy.minimum_repeated_reduction.value()<=0 || policy.maximum_covered_loss.value()<0 ||
            policy.maximum_outside_target.value()<0) reject("INVALID_FIRST_HATCH_WIDTH_REPLAN_INPUT");
        const auto hatches=before->source;const auto stack=hatches->source;const auto revision=stack->source->sequence->revision;
        const auto poll=[&] {
            stop(limits,revision,started);stop(limits.beads,revision,started);
            stop(limits.beads.packets,revision,started);stop(limits.volumes,revision,started);
        };poll();size_t work=0,cells=0;
        const auto charge=[&](size_t count) {if (count>limits.max_evaluations-work) reject("FIRST_HATCH_WIDTH_REPLAN_WORK_LIMIT");work+=count;poll();};
        const auto elapsed=[&] {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);};
        charge(hatches->line_count+hatches->passes.size());auto passes=hatches->passes;auto &pass=passes.front();
        if (pass.lines.size()<2 || pass.lines.size()>limits.max_paths) reject("FIRST_HATCH_WIDTH_REPLAN_PATH_LIMIT");
        if (!stack->first_pass.first_pass.gap_mm || width.value()<=stack->first_pass.first_pass.gap_mm->upper)
            reject("FIRST_HATCH_WIDTH_REPLAN_WIDTH_HEIGHT_DOMAIN");
        const auto &surface=stack->surfaces.front().cell;const auto &roi=surface.footprint;
        const bool x=pass.direction==HatchDirection::AlongX;
        const auto inset=Interval(width.value())/Interval(2)+Interval(hatches->policy.boundary_band.value())+Interval(hatches->numerical_error_upper_mm);
        const double low=(Interval(x ? roi.min_y : roi.min_x)+inset).hi,high=(Interval(x ? roi.max_y : roi.max_x)-inset).lo;
        if (low>=high) reject("FIRST_HATCH_WIDTH_REPLAN_TRANSVERSE_DOMAIN");
        std::vector<double> centres,cuts{x ? roi.min_y : roi.min_x};double extra_error=0;
        const auto point=[&](double along,double normal) {
            const double px=x ? along : normal,py=x ? normal : along;
            const Exact z=Exact(surface.z00)+(Exact(surface.z10)-Exact(surface.z00))*(Exact(px)-Exact(roi.min_x))/(Exact(roi.max_x)-Exact(roi.min_x))+
                (Exact(surface.z01)-Exact(surface.z00))*(Exact(py)-Exact(roi.min_y))/(Exact(roi.max_y)-Exact(roi.min_y));
            const auto stored=stored_exact(z);return std::pair<PhysicalPosition,double>{{px,py,stored.first},stored.second};
        };
        Interval pitch(0);
        for (size_t i=0;i<pass.lines.size();++i) {
            charge(1);auto &line=pass.lines[i];
            if (width.value()>=line.width.value()) reject("FIRST_HATCH_WIDTH_REPLAN_REQUIRES_NARROWER_WIDTH");
            const auto centre=stored_exact(Exact(low)+(Exact(high)-Exact(low))*Exact(int(i))/Exact(int(pass.lines.size()-1)));
            centres.push_back(centre.first);
            if (i) {
                const auto gap=exact_interval(Exact(centres[i])-Exact(centres[i-1]));
                if (gap.lo<=0 || gap.hi>hatches->policy.maximum_pitch.value() ||
                    gap.hi>(Interval(width.value())-Interval(2)*Interval(hatches->numerical_error_upper_mm)).lo)
                    reject("FIRST_HATCH_WIDTH_REPLAN_SPARSE_PITCH");
                pitch=i==1 ? gap : Interval(std::min(pitch.lo,gap.lo),std::max(pitch.hi,gap.hi));
                cuts.push_back(stored_exact((Exact(centres[i-1])+Exact(centres[i]))/Exact(2)).first);
            }
            const auto a=point(x ? line.start.x() : line.start.y(),centre.first),b=point(x ? line.end.x() : line.end.y(),centre.first);
            const double error=(Interval(line.coordinate_error_upper_mm)+Interval(centre.second)+Interval(std::max(a.second,b.second))).hi;
            line.start=a.first;line.end=b.first;line.reverse_start=b.first;line.reverse_end=a.first;line.width=width;
            line.coordinate_error_upper_mm=error;extra_error=std::max(extra_error,error);
        }
        cuts.push_back(x ? roi.max_y : roi.max_x);pass.pitch_mm=bounds(pitch);
        auto split_limits=limits.volumes;split_limits.max_evaluations=std::min({size_t(200000),split_limits.max_evaluations,limits.max_evaluations-work});
        split_limits.max_cells=std::min(split_limits.max_cells,limits.max_cells);split_limits.timeout-=elapsed();
        split_limits.cancelled=[&] {poll();return false;};split_limits.is_current={};
        const auto split=split_material_integral(stack->first_pass,x ? IntegralSplitAxis::Y : IntegralSplitAxis::X,cuts,split_limits);
        if (!split.snapshot) throw Rejection(split.reason);charge(split.snapshot->evaluations);cells+=split.snapshot->proof_cells;
        for (size_t i=0;i<pass.lines.size();++i) {
            charge(1);pass.lines[i].volume_cell=split.snapshot->strips[i].footprint;
            pass.lines[i].prospective_cell_volume_mm3=split.snapshot->strips[i].volume_mm3;
        }
        pass.prospective_volume_mm3=split.snapshot->total_volume_mm3;
        Exact total_lower(0),total_upper(0);
        for (const auto &p : passes) {charge(1);total_lower+=Exact(p.prospective_volume_mm3.lower);total_upper+=Exact(p.prospective_volume_mm3.upper);}
        const Interval total(exact_interval(total_lower).lo,exact_interval(total_upper).hi);
        if (total.hi<stack->total_volume_mm3.lower || total.lo>stack->total_volume_mm3.upper ||
            (Interval(total.hi)-Interval(total.lo)).hi>std::min(limits.volumes.maximum_interval_width.value(),stack->policy.total_volume_error.value()))
            reject("FIRST_HATCH_WIDTH_REPLAN_QUOTA_PRECISION");
        const double numeric=(Interval(hatches->numerical_error_upper_mm)+Interval(extra_error)).hi;
        if (numeric>.05) reject("FIRST_HATCH_WIDTH_REPLAN_NUMERICAL_BUDGET");
        const auto next=std::shared_ptr<const AffineHatchSnapshot>(new AffineHatchSnapshot(stack,hatches->policy,std::move(passes),
            bounds(total),hatches->line_count,numeric,hatches->first_pass_extent));
        if (work>=limits.max_evaluations || cells>=limits.max_cells) reject("FIRST_HATCH_WIDTH_REPLAN_SHARED_LIMIT");
        auto remaining=limits;remaining.max_evaluations-=work;remaining.max_cells-=cells;
        const auto used=elapsed();remaining.timeout-=used;remaining.beads.timeout-=used;remaining.beads.packets.timeout-=used;remaining.volumes.timeout-=used;
        remaining.cancelled=[&] {poll();return false;};remaining.is_current={};
        remaining.beads.cancelled={};remaining.beads.is_current={};remaining.beads.packets.cancelled={};remaining.beads.packets.is_current={};
        remaining.volumes.cancelled={};remaining.volumes.is_current={};
        const auto after=plan_first_hatch_layer({"",next},before->fill->occupied->domain,remaining);
        if (!after.snapshot) throw Rejection(after.reason);charge(after.snapshot->evaluations);cells+=after.snapshot->cells;
        if (after.snapshot->fill->target!=before->fill->target) reject("FIRST_HATCH_WIDTH_REPLAN_TARGET_CHANGED");
        const auto amount=interval(before->fill->occupied->individual_volume_mm3)-interval(after.snapshot->fill->occupied->individual_volume_mm3);
        const auto repeated=interval(before->fill->occupied->repeated_volume_mm3)-interval(after.snapshot->fill->occupied->repeated_volume_mm3);
        const auto covered=interval(after.snapshot->fill->covered_target_mm3)-interval(before->fill->covered_target_mm3);
        const auto missing=interval(after.snapshot->fill->missing_target_mm3)-interval(before->fill->missing_target_mm3);
        if (amount.lo<policy.minimum_repeated_reduction.value() || repeated.lo<policy.minimum_repeated_reduction.value())
            reject("FIRST_HATCH_WIDTH_REPLAN_INSUFFICIENT_REDUCTION");
        if (covered.lo<-policy.maximum_covered_loss.value() || missing.hi>policy.maximum_covered_loss.value())
            reject("FIRST_HATCH_WIDTH_REPLAN_COVERAGE_LOSS");
        if (after.snapshot->fill->outside_target_mm3.upper>policy.maximum_outside_target.value()) reject("FIRST_HATCH_WIDTH_REPLAN_OUTSIDE_TARGET_LIMIT");
        poll();auto snapshot=std::shared_ptr<const FirstHatchWidthReplanSnapshot>(new FirstHatchWidthReplanSnapshot(before,after.snapshot,width,policy,
            bounds(amount),bounds(repeated),bounds(covered),cells,work));
        poll();return {"BOUNDED_FIRST_CANDIDATE_WIDTH_REPLAN_WITH_MEASURED_EXCESS_REDUCTION_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) {return {e.what(),{}};}
    catch (const std::exception &e) {return {"FIRST_HATCH_WIDTH_REPLAN_NUMERIC_FAILURE: "+std::string(e.what()),{}};}
}

std::vector<std::shared_ptr<const FirstHatchBeadSnapshot>> FirstHatchBeadSnapshot::construct_first_paths(
    const std::shared_ptr<const AffineHatchSnapshot> &source,const FirstContourPolicy &policy,const FirstHatchLayerLimits &limits,
    bool with_infill,std::chrono::steady_clock::time_point started,size_t &work,std::vector<size_t> &replaced,
    FirstCapHatchExtent extent,const std::vector<std::shared_ptr<const FirstHatchBeadSnapshot>> *retained)
{
    const auto stack=source->source;const auto sequence=stack->source->sequence;
    const auto &surface=stack->surfaces.front().cell;const auto &roi=surface.footprint;
    const auto poll=[&] {
        stop(limits,sequence->revision,started);stop(limits.beads,sequence->revision,started);
        stop(limits.beads.packets,sequence->revision,started);stop(limits.volumes,sequence->revision,started);
    };
    const auto charge=[&](size_t count) {if (count>limits.max_evaluations-work) reject("FIRST_CONTOUR_WORK_LIMIT");work+=count;poll();};
    const auto elapsed=[&] {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);};
    const auto inset=(Interval(policy.width.value())+Interval(limits.beads.packets.maximum_width_error.value()))/Interval(2)+
        Interval(source->policy.boundary_band.value())+Interval(source->numerical_error_upper_mm);
    RectangleXY centres{(Interval(roi.min_x)+inset).hi,(Interval(roi.min_y)+inset).hi,
        (Interval(roi.max_x)-inset).lo,(Interval(roi.max_y)-inset).lo};
    if (retained) {
        if (!with_infill || extent!=FirstCapHatchExtent::BoundaryBand || retained->size()<5 || !retained->front()) reject("FIRST_CAP_REPLAN_CONTOUR_SOURCE");
        const auto first=retained->front()->path_start;centres={first.x(),first.y(),first.x(),first.y()};
        for (size_t i=0;i<4;++i) {
            charge(1);const auto &p=retained->at(i);
            if (!p || p->source!=source || p->line_index || p->roof_domain!=FirstHatchRoofDomain::FiniteWidth ||
                p->maximum_gap_error_mm>limits.beads.maximum_gap_error.value() ||
                p->maximum_width_error_mm>limits.beads.packets.maximum_width_error.value()) reject("FIRST_CAP_REPLAN_CONTOUR_PROOF_LIMIT");
            centres.min_x=std::min(centres.min_x,p->path_start.x());centres.max_x=std::max(centres.max_x,p->path_start.x());
            centres.min_y=std::min(centres.min_y,p->path_start.y());centres.max_y=std::max(centres.max_y,p->path_start.y());
        }
    }
    if (centres.min_x>=centres.max_x || centres.min_y>=centres.max_y) reject("FIRST_CONTOUR_ROI_TOO_THIN");
    const auto point=[&](double x,double y) {
        const Exact z=Exact(surface.z00)+(Exact(surface.z10)-Exact(surface.z00))*(Exact(x)-Exact(roi.min_x))/(Exact(roi.max_x)-Exact(roi.min_x))+
            (Exact(surface.z01)-Exact(surface.z00))*(Exact(y)-Exact(roi.min_y))/(Exact(roi.max_y)-Exact(roi.min_y));
        const auto stored=stored_exact(z);return std::pair<PhysicalPosition,double>{{x,y,stored.first},stored.second};
    };
    charge(4);
    const std::array<std::pair<PhysicalPosition,double>,4> vertices{{point(centres.min_x,centres.min_y),point(centres.max_x,centres.min_y),
        point(centres.max_x,centres.max_y),point(centres.min_x,centres.max_y)}};
    struct Geometry {std::optional<size_t> owner;AffineHatchLine line;double error;};std::vector<Geometry> geometry;
    for (size_t n=0;n<4;++n) {
        const size_t a=(policy.seam_corner+(policy.clockwise ? 4-n : n))%4,b=(policy.seam_corner+(policy.clockwise ? 3-n : n+1))%4;
        const auto &start=vertices[a],&end=vertices[b];const double error=(Interval(start.second)+Interval(end.second)).hi;
        // Geometry only: a contour edge has no infill strip owner/quota.
        geometry.push_back({std::nullopt,{start.first,end.first,end.first,start.first,policy.width,roi,{0,0},
            bounds(detail::root(length_squared(start.first,end.first))),error},error});
    }
    if (with_infill) {
        const bool x=source->passes.front().direction==HatchDirection::AlongX;
        const auto end_inset=Interval(source->policy.boundary_band.value())+Interval(source->numerical_error_upper_mm);
        const double lo=extent==FirstCapHatchExtent::ContourCentres ? (x ? centres.min_x : centres.min_y) :
            (Interval(x ? roi.min_x : roi.min_y)+end_inset).hi;
        const double hi=extent==FirstCapHatchExtent::ContourCentres ? (x ? centres.max_x : centres.max_y) :
            (Interval(x ? roi.max_x : roi.max_y)-end_inset).lo;
        if (lo>=hi) reject("FIRST_CAP_REPLAN_NO_EXTENSION_DOMAIN");
        const double low=x ? centres.min_y : centres.min_x,high=x ? centres.max_y : centres.max_x;
        const auto &lines=source->passes.front().lines;
        for (size_t i=0;i<lines.size();++i) {
            charge(1);const auto &line=lines[i];const double centre=x ? line.start.y() : line.start.x();
            if (line.width.value()!=policy.width.value()) reject("FIRST_CAP_REQUIRES_COMMON_OWNED_WIDTH");
            if ((x ? line.start.y()!=line.end.y() : line.start.x()!=line.end.x()) ||
                (x ? line.start.x()>=line.end.x() : line.start.y()>=line.end.y())) reject("FIRST_CAP_REQUIRES_FORWARD_SOURCE_HATCHES");
            if (centre<=low || centre>=high) {replaced.push_back(i);continue;}
            if (extent==FirstCapHatchExtent::ContourCentres &&
                (lo<(x ? line.start.x() : line.start.y()) || hi>(x ? line.end.x() : line.end.y()))) reject("FIRST_CAP_INCOMPLETE_PARENT_EXTENT");
            const auto first=point(x ? lo : centre,x ? centre : lo),last=point(x ? hi : centre,x ? centre : hi);
            auto slice=line;slice.start=first.first;slice.end=last.first;slice.reverse_start=last.first;slice.reverse_end=first.first;
            slice.projected_length_mm=bounds(exact_interval(Exact(hi)-Exact(lo)));
            geometry.push_back({i,std::move(slice),(Interval(first.second)+Interval(last.second)).hi});
        }
        if (geometry.size()==4) reject("FIRST_CAP_NO_INTERIOR_HATCH");
    }
    if (geometry.size()>limits.max_paths) reject("FIRST_CONTOUR_PATH_LIMIT");
    size_t bead_work=0,roofs=0,packets=0;std::vector<std::shared_ptr<const FirstHatchBeadSnapshot>> paths;
    for (size_t i=0;i<geometry.size();++i) {
        const auto &g=geometry[i];
        std::shared_ptr<const FirstHatchBeadSnapshot> middle;
        if (retained) {
            if (i>=retained->size()) reject("FIRST_CAP_REPLAN_PATH_SOURCE");
            middle=retained->at(i);
            if (!middle || middle->source!=source || middle->line_index!=g.owner ||
                middle->roof_domain!=FirstHatchRoofDomain::FiniteWidth ||
                middle->maximum_gap_error_mm>limits.beads.maximum_gap_error.value() ||
                middle->maximum_width_error_mm>limits.beads.packets.maximum_width_error.value())
                reject("FIRST_CAP_REPLAN_RETAINED_PROOF_LIMIT");
        }
        if (middle) {
            const auto &p=middle;charge(1);
            if (p->pieces.size()>limits.beads.packets.max_segments-packets || p->roof_segments>limits.beads.max_roof_segments-roofs)
                reject("FIRST_CAP_REPLAN_RETAINED_PROOF_LIMIT");
            roofs+=p->roof_segments;packets+=p->pieces.size();
            if (i<4) {paths.push_back(p);continue;}
        }
        const auto solve=[&](const AffineHatchLine &line) {
            charge(2*sequence->records.size()); // Source hash and active-prefix walks in each first-bead solver.
            if (bead_work>=limits.beads.max_evaluations || roofs>=limits.beads.max_roof_segments || packets>=limits.beads.packets.max_segments)
                reject("FIRST_CONTOUR_BEAD_WORK_LIMIT");
            auto remaining=limits.beads;remaining.max_evaluations=std::min(remaining.max_evaluations-bead_work,limits.max_evaluations-work);
            remaining.max_roof_segments-=roofs;remaining.packets.max_segments-=packets;
            remaining.packets.maximum_volume_error=Volume((Interval(limits.beads.packets.maximum_volume_error.value())/Interval(double(geometry.size()))/Interval(middle ? 4 : 2)).lo);
            remaining.timeout-=elapsed();remaining.packets.timeout-=elapsed();remaining.cancelled=[&] {poll();return false;};remaining.is_current={};
            remaining.packets.cancelled=remaining.cancelled;remaining.packets.is_current={};
            const auto bead=plan({"",source},g.owner,remaining,FirstHatchRoofDomain::FiniteWidth,&line,g.error);
            if (!bead.snapshot) throw Rejection(bead.reason);charge(bead.snapshot->evaluations);
            bead_work+=bead.snapshot->evaluations;roofs+=bead.snapshot->roof_segments;packets+=bead.snapshot->pieces.size();return bead.snapshot;
        };
        if (!middle) {paths.push_back(solve(g.line));continue;}
        // Retain every already qualified central packet and its actual dose.
        // Only the two new finite strips need another body-roof proof.
        const auto fragment=[&](PhysicalPosition start,PhysicalPosition end) {
            auto line=g.line;line.start=start;line.end=end;line.reverse_start=end;line.reverse_end=start;
            line.projected_length_mm=bounds(detail::root(length_squared(start,end)));return solve(line);
        };
        const auto low=fragment(g.line.start,middle->path_start),high=fragment(middle->path_end,g.line.end);
        std::vector<FixedWidthBeadPiece> pieces;Exact target_lo(0),target_hi(0),deposited(0);
        double gap=0,width=0,numeric=0;size_t segments=0,proof_work=0;
        for (const auto &p : {low,middle,high}) {
            charge(p->pieces.size()+1);pieces.insert(pieces.end(),p->pieces.begin(),p->pieces.end());
            target_lo+=Exact(p->actual_target_volume_mm3.lower);target_hi+=Exact(p->actual_target_volume_mm3.upper);
            for (const auto &piece : p->pieces) deposited+=Exact(piece.volume.value());
            gap=std::max(gap,p->maximum_gap_error_mm);width=std::max(width,p->maximum_width_error_mm);
            numeric=std::max(numeric,p->numerical_error_upper_mm);segments+=p->roof_segments;proof_work+=p->evaluations;
        }
        const Interval target(exact_interval(target_lo).lo,exact_interval(target_hi).hi),delivered=exact_interval(deposited);
        const auto difference=delivered-target;const double error=std::max(std::abs(difference.lo),std::abs(difference.hi));
        paths.push_back(std::shared_ptr<const FirstHatchBeadSnapshot>(new FirstHatchBeadSnapshot(source,g.owner,
            FirstHatchRoofDomain::FiniteWidth,g.line.start,g.line.end,std::move(pieces),bounds(target),bounds(delivered),
            gap,width,error,numeric,segments,proof_work)));
    }
    poll();return paths;
}

FirstContourResult plan_first_contour(const AffineHatchResult &requested,const FirstContourPolicy &requested_policy,
    const SceneBox &requested_box,const FirstHatchLayerLimits &requested_limits)
{
    const auto source=requested.snapshot;const auto policy=requested_policy;const auto box=requested_box;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!source || !source->source || !source->source->first_pass.proof || !valid_first_hatch_layer_limits(limits) ||
            policy.width.value()<=0 || policy.seam_corner>=4 || policy.maximum_outside_target.value()<0 ||
            box.min.x()>=box.max.x() || box.min.y()>=box.max.y() || box.min.z()>=box.max.z()) reject("INVALID_FIRST_CONTOUR_INPUT");
        if (limits.max_paths<4) reject("FIRST_CONTOUR_PATH_LIMIT");
        const auto stack=source->source;const auto sequence=stack->source->sequence;
        const auto &surface=stack->surfaces.front().cell;const auto &roi=surface.footprint;
        if (box.min.x()!=roi.min_x || box.max.x()!=roi.max_x || box.min.y()!=roi.min_y || box.max.y()!=roi.max_y)
            reject("FIRST_CONTOUR_XY_DOMAIN_MISMATCH");
        if (!stack->first_pass.first_pass.gap_mm || policy.width.value()<=stack->first_pass.first_pass.gap_mm->upper)
            reject("FIRST_CONTOUR_WIDTH_HEIGHT_DOMAIN");
        const auto poll=[&] {
            stop(limits,sequence->revision,started);stop(limits.beads,sequence->revision,started);
            stop(limits.beads.packets,sequence->revision,started);stop(limits.volumes,sequence->revision,started);
        };poll();size_t work=0;std::vector<size_t> replaced;
        auto edges=FirstHatchBeadSnapshot::construct_first_paths(source,policy,limits,false,started,work,replaced);
        const auto measured=measure_first_candidate(source,edges,box,limits,started,work);
        if (measured.fill->outside_target_mm3.upper>policy.maximum_outside_target.value()) reject("FIRST_CONTOUR_OUTSIDE_TARGET_LIMIT");
        if (measured.fill->covered_target_mm3.lower<=0) reject("FIRST_CONTOUR_NO_POSITIVE_COVERAGE");
        poll();auto snapshot=std::shared_ptr<const FirstContourSnapshot>(new FirstContourSnapshot(source,policy,std::move(edges),measured.fill,
            measured.target,measured.delivered,measured.error,measured.numeric,measured.roofs,measured.cells,work));
        poll();return {"BOUNDED_CLOSED_FIRST_ROI_CONTOUR_WITH_MEASURED_CORNER_UNION_FILL_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) {return {e.what(),{}};}
    catch (const std::exception &e) {return {"FIRST_CONTOUR_NUMERIC_FAILURE: "+std::string(e.what()),{}};}
}

FirstCapResult plan_first_cap(const AffineHatchResult &requested,const FirstContourPolicy &requested_policy,
    const SceneBox &requested_box,const FirstHatchLayerLimits &requested_limits)
{
    const auto source=requested.snapshot;const auto policy=requested_policy;const auto box=requested_box;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!source || !source->source || !source->source->first_pass.proof || !valid_first_hatch_layer_limits(limits) ||
            policy.width.value()<=0 || policy.seam_corner>=4 || policy.maximum_outside_target.value()<0 ||
            box.min.x()>=box.max.x() || box.min.y()>=box.max.y() || box.min.z()>=box.max.z()) reject("INVALID_FIRST_CAP_INPUT");
        const auto stack=source->source;const auto sequence=stack->source->sequence;const auto &roi=stack->surfaces.front().cell.footprint;
        if (box.min.x()!=roi.min_x || box.max.x()!=roi.max_x || box.min.y()!=roi.min_y || box.max.y()!=roi.max_y) reject("FIRST_CAP_XY_DOMAIN_MISMATCH");
        if (!stack->first_pass.first_pass.gap_mm || policy.width.value()<=stack->first_pass.first_pass.gap_mm->upper) reject("FIRST_CAP_WIDTH_HEIGHT_DOMAIN");
        const auto poll=[&] {
            stop(limits,sequence->revision,started);stop(limits.beads,sequence->revision,started);
            stop(limits.beads.packets,sequence->revision,started);stop(limits.volumes,sequence->revision,started);
        };poll();size_t work=0;std::vector<size_t> replaced;
        auto paths=FirstHatchBeadSnapshot::construct_first_paths(source,policy,limits,true,started,work,replaced);
        const auto measured=measure_first_candidate(source,paths,box,limits,started,work);
        if (measured.fill->outside_target_mm3.upper>policy.maximum_outside_target.value()) reject("FIRST_CAP_OUTSIDE_TARGET_LIMIT");
        if (measured.fill->covered_target_mm3.lower<=0) reject("FIRST_CAP_NO_POSITIVE_COVERAGE");
        poll();auto snapshot=std::shared_ptr<const FirstCapSnapshot>(new FirstCapSnapshot(source,policy,std::move(paths),std::move(replaced),measured.fill,
            measured.target,measured.delivered,measured.error,measured.numeric,measured.roofs,measured.cells,work));
        poll();return {"BOUNDED_FIRST_CONTOUR_AND_INTERIOR_HATCH_CANDIDATE_WITH_MEASURED_JOINT_FILL_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) {return {e.what(),{}};}
    catch (const std::exception &e) {return {"FIRST_CAP_NUMERIC_FAILURE: "+std::string(e.what()),{}};}
}

FirstCapReplanResult replan_first_cap_ends(const FirstCapResult &requested,const FirstCapReplanPolicy &requested_policy,
    const FirstHatchLayerLimits &requested_limits)
{
    const auto before=requested.snapshot;const auto policy=requested_policy;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!before || !before->source || !before->source->source || !before->fill || !valid_first_hatch_layer_limits(limits) ||
            policy.minimum_covered_gain.value()<=0 || policy.maximum_outside_target.value()<0) reject("INVALID_FIRST_CAP_REPLAN_INPUT");
        if (before->hatch_extent!=FirstCapHatchExtent::ContourCentres) reject("FIRST_CAP_EXTENT_ALREADY_REPLANNED");
        const auto source=before->source;const auto sequence=source->source->source->sequence;
        const auto poll=[&] {
            stop(limits,sequence->revision,started);stop(limits.beads,sequence->revision,started);
            stop(limits.beads.packets,sequence->revision,started);stop(limits.volumes,sequence->revision,started);
        };poll();size_t work=0;std::vector<size_t> replaced;
        auto paths=FirstHatchBeadSnapshot::construct_first_paths(source,before->policy,limits,true,started,work,replaced,
            FirstCapHatchExtent::BoundaryBand,&before->paths);
        if (paths.size()!=before->paths.size() || replaced!=before->replaced_boundary_lines) reject("FIRST_CAP_REPLAN_OWNER_MISMATCH");
        const bool x=source->passes.front().direction==HatchDirection::AlongX;
        const auto &roi=source->source->surfaces.front().cell.footprint;
        const Exact band=Exact(source->policy.boundary_band.value())+Exact(source->numerical_error_upper_mm);
        for (size_t i=0;i<paths.size();++i) {
            poll();const auto &old=*before->paths[i],&next=*paths[i];
            if (i>=4 && (next.line_index!=old.line_index || (x ? next.path_start.x()>=old.path_start.x() || next.path_end.x()<=old.path_end.x() :
                next.path_start.y()>=old.path_start.y() || next.path_end.y()<=old.path_end.y()))) reject("FIRST_CAP_REPLAN_NO_EXTENSION_DOMAIN");
            for (const auto &p : next.pieces) {
                if (work>=limits.max_evaluations) reject("FIRST_CAP_REPLAN_WORK_LIMIT");++work;poll();
                const bool along_x=p.start.y()==p.end.y();const Exact half=Exact(p.section.width_mm.upper)/Exact(2);
                const Exact xmin=Exact(std::min(p.start.x(),p.end.x()))-(along_x ? Exact(0) : half);
                const Exact xmax=Exact(std::max(p.start.x(),p.end.x()))+(along_x ? Exact(0) : half);
                const Exact ymin=Exact(std::min(p.start.y(),p.end.y()))-(along_x ? half : Exact(0));
                const Exact ymax=Exact(std::max(p.start.y(),p.end.y()))+(along_x ? half : Exact(0));
                if (xmin<Exact(roi.min_x)+band || xmax>Exact(roi.max_x)-band || ymin<Exact(roi.min_y)+band || ymax>Exact(roi.max_y)-band)
                    reject("FIRST_CAP_REPLAN_FINITE_WIDTH_BOUNDARY_BAND");
            }
        }
        const auto measured=measure_first_candidate(source,paths,before->fill->occupied->domain,limits,started,work);
        if (measured.fill->outside_target_mm3.upper>std::min(before->policy.maximum_outside_target.value(),policy.maximum_outside_target.value()))
            reject("FIRST_CAP_REPLAN_OUTSIDE_TARGET_LIMIT");
        const auto gain=interval(measured.fill->covered_target_mm3)-interval(before->fill->covered_target_mm3);
        const auto reduction=interval(before->fill->missing_target_mm3)-interval(measured.fill->missing_target_mm3);
        if (gain.lo<policy.minimum_covered_gain.value() || reduction.lo<policy.minimum_covered_gain.value()) reject("FIRST_CAP_REPLAN_INSUFFICIENT_GAIN");
        const auto amount=interval(measured.fill->occupied->individual_volume_mm3)-interval(before->fill->occupied->individual_volume_mm3);
        const auto repeated=interval(measured.fill->occupied->repeated_volume_mm3)-interval(before->fill->occupied->repeated_volume_mm3);
        poll();auto after=std::shared_ptr<const FirstCapSnapshot>(new FirstCapSnapshot(source,before->policy,std::move(paths),std::move(replaced),
            measured.fill,measured.target,measured.delivered,measured.error,measured.numeric,measured.roofs,measured.cells,work,FirstCapHatchExtent::BoundaryBand));
        poll();auto snapshot=std::shared_ptr<const FirstCapReplanSnapshot>(new FirstCapReplanSnapshot(before,after,policy,bounds(gain),bounds(reduction),
            bounds(amount),bounds(repeated),measured.cells,work));
        poll();return {"BOUNDED_WHOLE_FIRST_CAP_END_REPLAN_WITH_MEASURED_FILL_GAIN_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) {return {e.what(),{}};}
    catch (const std::exception &e) {return {"FIRST_CAP_REPLAN_NUMERIC_FAILURE: "+std::string(e.what()),{}};}
}

namespace {
double run_coordinate(PhysicalPosition p,MaterialRunAxis axis) {return axis==MaterialRunAxis::X ? p.x() : p.y();}
double run_progress(const MaterialPrefixSnapshot &source,size_t index) {return index<source.completed_records ? 1 : source.current_progress;}
Exact run_end(const MaterialRunSnapshot &run)
{
    const auto &m=run.source->sequence->records[run.last_record].motion;
    return Exact(run_coordinate(m.start,run.axis))+(Exact(run_coordinate(m.end,run.axis))-Exact(run_coordinate(m.start,run.axis)))*Exact(run_progress(*run.source,run.last_record));
}
SceneBox expand_run_box(const MaterialRunSnapshot &run,const SceneBox &box)
{
    const auto &model=run.source->sequence->model;
    const auto xy=Interval(model.inner_xy_loss.value())+Interval(model.numerical_coordinate_error.value());
    const auto z=Interval(model.inner_z_loss.value())+Interval(model.numerical_coordinate_error.value());
    return {{(Interval(box.min.x())-xy).lo,(Interval(box.min.y())-xy).lo,(Interval(box.min.z())-z).lo},
            {(Interval(box.max.x())+xy).hi,(Interval(box.max.y())+xy).hi,(Interval(box.max.z())+z).hi}};
}
// Certify the inflated box against every actual nominal section that owns a
// longitudinal slice. Internal butt faces are closed; the real ends are strict.
// Callbacks share the caller's counters and deadline, including nested join searches.
bool cover_run_box(const MaterialRunSnapshot &run,const SceneBox &domain,size_t max_depth,
    const std::function<void()> &charge,const std::function<void()> &visit)
{
    const auto box=expand_run_box(run,domain);const auto &sequence=*run.source->sequence;
    const Exact lo(run_coordinate(box.min,run.axis)),hi(run_coordinate(box.max,run.axis));
    const Exact start(run_coordinate(sequence.records[run.first_record].motion.start,run.axis)),end=run_end(run);
    if (lo<=std::min(start,end) || hi>=std::max(start,end)) return false;
    for (size_t index=run.first_record;index<=run.last_record;++index) {
        charge();const auto &row=sequence.records[index];const auto &m=row.motion;
        const Exact a(run_coordinate(m.start,run.axis)),delta=Exact(run_coordinate(m.end,run.axis))-a;
        const double fraction=run_progress(*run.source,index);const Exact b=a+delta*Exact(fraction);
        const Exact left=std::max(lo,std::min(a,b)),right=std::min(hi,std::max(a,b));
        if (left>right) continue;
        struct Slice {Exact lo,hi;size_t depth;};std::vector<Slice> pending{{left,right,0}};
        while (!pending.empty()) {
            visit();charge();const auto node=pending.back();pending.pop_back();
            const Exact ta=(node.lo-a)/delta,tb=(node.hi-a)/delta;
            // Clip exact longitudinal coordinates before rounding. 0, 1 and
            // current_progress are exact binary bounds on these closed slices.
            Projection projection{{std::max(0.,exact_interval(std::min(ta,tb)).lo),std::min(fraction,exact_interval(std::max(ta,tb)).hi)},
                run.axis==MaterialRunAxis::X ? Interval(box.min.y(),box.max.y())-Interval(m.start.y()) :
                    Interval(box.min.x(),box.max.x())-Interval(m.start.x()),{box.min.z(),box.max.z()}};
            const auto membership=piece(row,sequence.model,projection,fraction,Representation::Nominal,true);
            if (membership==MaterialMembership::Inside) continue;
            if (membership==MaterialMembership::Outside || node.depth>=max_depth || node.lo==node.hi) return false;
            const Exact mid=(node.lo+node.hi)/Exact(2);
            pending.push_back({mid,node.hi,node.depth+1});pending.push_back({node.lo,mid,node.depth+1});
        }
    }
    return true;
}
bool valid_coverage_limits(const MaterialCoverageLimits &limits)
{
    return limits.max_cells && limits.max_cells<=65535 && limits.max_depth && limits.max_depth<=32 &&
        limits.max_evaluations && limits.max_evaluations<=200000 && valid_timeout(limits.timeout);
}
bool valid_join_limits(const MaterialJoinLimits &limits)
{
    return valid_coverage_limits(limits) && limits.minimum_box_volume.value()>0;
}
void validate_join_source(const std::shared_ptr<const MaterialPrefixSnapshot> &source,const SceneBox &box)
{
    if (!source || !source->sequence || source->completed_records>source->sequence->records.size() ||
        !std::isfinite(source->current_progress) || source->current_progress<0 || source->current_progress>1 ||
        (source->completed_records==source->sequence->records.size() && source->current_progress!=0)) reject("INVALID_MATERIAL_JOIN_SOURCE");
    for (const auto &p : {box.min,box.max}) {coordinate(p.x());coordinate(p.y());coordinate(p.z());}
    if (box.min.x()>=box.max.x() || box.min.y()>=box.max.y() || box.min.z()>=box.max.z()) reject("INVALID_MATERIAL_JOIN_DOMAIN");
}
ScalarBounds join_box_volume(const SceneBox &box)
{
    return bounds(exact_interval((Exact(box.max.x())-Exact(box.min.x()))*(Exact(box.max.y())-Exact(box.min.y()))*
        (Exact(box.max.z())-Exact(box.min.z()))));
}
struct JoinSearch {
    struct Witness {size_t first,second;SceneBox box;};
    std::shared_ptr<const MaterialPrefixSnapshot> source;
    MaterialJoinLimits limits;
    std::chrono::steady_clock::time_point started;
    size_t cells=0,evaluations=0;
    std::vector<std::shared_ptr<const MaterialRunSnapshot>> runs;
    void poll() const {stop(limits,source->sequence->revision,started);}
    void charge(size_t count=1) {
        if (count>limits.max_evaluations-evaluations) reject("MATERIAL_JOIN_WORK_LIMIT");evaluations+=count;poll();
    }
    double progress(size_t index) const {
        if (index>=source->sequence->records.size() || (index>=source->completed_records &&
            (index!=source->completed_records || source->current_progress==0))) reject("MATERIAL_JOIN_FUTURE_RECORD");
        if (!source->sequence->records[index].bead) reject("MATERIAL_JOIN_REQUIRES_TWO_BEADS");
        return index<source->completed_records ? 1 : source->current_progress;
    }
    std::optional<SceneBox> outer(size_t index,double fraction) const {
        if (!runs.empty()) {
            auto box=runs.at(index)->nominal_bounds;const auto &run=*runs[index];
            const Exact a(run_coordinate(source->sequence->records[run.first_record].motion.start,run.axis)),b=run_end(run);
            const Exact loss=Exact(source->sequence->model.inner_xy_loss.value())+Exact(source->sequence->model.numerical_coordinate_error.value());
            const auto lo=exact_interval(std::min(a,b)+loss),hi=exact_interval(std::max(a,b)-loss);
            if (lo.lo>=hi.hi) return {};
            if (run.axis==MaterialRunAxis::X) box={{lo.lo,box.min.y(),box.min.z()},{hi.hi,box.max.y(),box.max.z()}};
            else box={{box.min.x(),lo.lo,box.min.z()},{box.max.x(),hi.hi,box.max.z()}};
            return box;
        }
        const auto &row=source->sequence->records[index];const auto &m=row.motion;const auto &b=*row.bead;
        const auto dx=Interval(m.end.x())-Interval(m.start.x()),dy=Interval(m.end.y())-Interval(m.start.y()),length=detail::root(length_squared(row));
        const auto loss=(Interval(source->sequence->model.inner_xy_loss.value())+Interval(source->sequence->model.numerical_coordinate_error.value()))/length;
        const Interval begin(loss.hi),end((Interval(fraction)-loss).lo);if (begin.lo>=end.hi) return {};
        const auto xbegin=Interval(m.start.x())+dx*begin,ybegin=Interval(m.start.y())+dy*begin;
        const auto xend=Interval(m.start.x())+dx*end,yend=Interval(m.start.y())+dy*end;
        const auto dz=Interval(m.end.z())-Interval(m.start.z()),zbegin=Interval(m.start.z())+dz*begin,zend=Interval(m.start.z())+dz*end;
        const auto half=Interval(b.width_mm.upper)/Interval(2),xhalf=half*absolute(dy)/length,yhalf=half*absolute(dx)/length;
        const auto height=Interval(b.gap_begin_mm)+(Interval(b.gap_end_mm)-Interval(b.gap_begin_mm))*Interval(fraction);
        return SceneBox{{(detail::minimum(xbegin,xend)-xhalf).lo,(detail::minimum(ybegin,yend)-yhalf).lo,
                         (detail::minimum(zbegin,zend)-detail::maximum(Interval(b.gap_begin_mm),height)).lo},
                        {(detail::maximum(xbegin,xend)+xhalf).hi,(detail::maximum(ybegin,yend)+yhalf).hi,detail::maximum(zbegin,zend).hi}};
    }
    std::optional<Witness> find(const std::vector<size_t> &first,const std::vector<size_t> &second,const SceneBox &domain) {
        // Search all packet pairs together. An uncertain tiny first pair must
        // not consume the shared budget before a later pair can supply a box.
        struct Node {SceneBox box;size_t depth,first,second;};std::vector<Node> pending;size_t next=0;
        for (size_t i : first) for (size_t j : second) {
            charge(2);if (i==j) reject("MATERIAL_JOIN_REQUIRES_DISTINCT_BEADS");
            auto region=domain;
            const auto one=outer(i,runs.empty() ? progress(i) : 1),two=outer(j,runs.empty() ? progress(j) : 1);if (!one || !two) continue;
            // Nominal transverse bounds, but the original D_lower finite-butt
            // erosion already prunes impossible endpoint slivers. This never
            // substitutes an enclosure for continuous inner certification.
            for (const auto &box : {*one,*two}) {
                region={{std::max(region.min.x(),box.min.x()),std::max(region.min.y(),box.min.y()),std::max(region.min.z(),box.min.z())},
                        {std::min(region.max.x(),box.max.x()),std::min(region.max.y(),box.max.y()),std::min(region.max.z(),box.max.z())}};
            }
            if (region.min.x()<region.max.x() && region.min.y()<region.max.y() && region.min.z()<region.max.z() &&
                join_box_volume(region).upper>=limits.minimum_box_volume.value()) pending.push_back({region,0,i,j});
        }
        // Broad cells first, rather than a depth walk along boundary slivers.
        while (next<pending.size()) {
            poll();const auto node=pending[next++];
            if (cells>=limits.max_cells) reject("MATERIAL_JOIN_CELL_LIMIT");++cells;
            const auto volume=join_box_volume(node.box);if (volume.upper<limits.minimum_box_volume.value()) continue;
            charge(2);const auto &sequence=*source->sequence;
            const auto p=[&](size_t index,const SceneBox &box) {
                if (!runs.empty()) return cover_run_box(*runs.at(index),box,limits.max_depth-node.depth,[&]{charge();},[&] {
                    poll();if (cells>=limits.max_cells) reject("MATERIAL_JOIN_CELL_LIMIT");++cells;
                }) ? MaterialMembership::Inside : MaterialMembership::Unknown;
                return piece(sequence.records[index],sequence.model,
                project(sequence.records[index],{box.min.x(),box.max.x()},{box.min.y(),box.max.y()},
                    {box.min.z(),box.max.z()}),progress(index),Representation::Lower);};
            const auto one=p(node.first,node.box),two=p(node.second,node.box);
            if (one==MaterialMembership::Outside || two==MaterialMembership::Outside) continue;
            if (one==MaterialMembership::Inside && two==MaterialMembership::Inside && volume.lower>=limits.minimum_box_volume.value()) {poll();return Witness{node.first,node.second,node.box};}
            std::array<double,3> lo{node.box.min.x(),node.box.min.y(),node.box.min.z()},hi{node.box.max.x(),node.box.max.y(),node.box.max.z()};
            // A finite-butt enclosure touches both open longitudinal ends.
            // Offer its central box before equalizing long transverse axes;
            // certify its complete volume with exactly the same predicate.
            // This is an extra bounded proposal, never a sampled-point PASS.
            std::array<double,3> middle_lo,middle_hi;
            for (size_t i=0;i<3;++i) {
                middle_lo[i]=stored_exact((Exact(3)*Exact(lo[i])+Exact(hi[i]))/Exact(4)).first;
                middle_hi[i]=stored_exact((Exact(lo[i])+Exact(3)*Exact(hi[i]))/Exact(4)).first;
            }
            const SceneBox middle{{middle_lo[0],middle_lo[1],middle_lo[2]},{middle_hi[0],middle_hi[1],middle_hi[2]}};
            if (middle.min.x()<middle.max.x() && middle.min.y()<middle.max.y() && middle.min.z()<middle.max.z() &&
                join_box_volume(middle).lower>=limits.minimum_box_volume.value()) {
                if (cells>=limits.max_cells) reject("MATERIAL_JOIN_CELL_LIMIT");++cells;charge(2);
                if (p(node.first,middle)==MaterialMembership::Inside && p(node.second,middle)==MaterialMembership::Inside) {poll();return Witness{node.first,node.second,middle};}
            }
            if (node.depth>=limits.max_depth) continue;
            size_t axis=0;for (size_t i=1;i<3;++i) if (Exact(hi[i])-Exact(lo[i])>Exact(hi[axis])-Exact(lo[axis])) axis=i;
            const double mid=stored_exact((Exact(lo[axis])+Exact(hi[axis]))/Exact(2)).first;
            if (mid<=lo[axis] || mid>=hi[axis]) continue;
            auto lower_hi=hi,upper_lo=lo;lower_hi[axis]=mid;upper_lo[axis]=mid;
            pending.push_back({{{upper_lo[0],upper_lo[1],upper_lo[2]},{hi[0],hi[1],hi[2]}},node.depth+1,node.first,node.second});
            pending.push_back({{{lo[0],lo[1],lo[2]},{lower_hi[0],lower_hi[1],lower_hi[2]}},node.depth+1,node.first,node.second});
        }
        poll();return {};
    }
};
}

MaterialRunResult reconstruct_material_run(const NominalMaterialView &requested,size_t first,size_t last,const MaterialLimits &requested_limits)
{
    const auto source=requested.snapshot;const auto limits=requested_limits;const auto started=std::chrono::steady_clock::now();size_t work=0;
    try {
        detail::require_interval_environment();
        if (!source || !source->sequence || !limits.max_records || limits.max_records>200000 || !valid_timeout(limits.timeout) ||
            first>last || last>=source->sequence->records.size() || last-first>=limits.max_records ||
            source->sequence->geometry.size()!=source->sequence->records.size() || source->sequence->records.size()>200000 ||
            source->completed_records>source->sequence->records.size() || !std::isfinite(source->current_progress) ||
            source->current_progress<0 || source->current_progress>1 ||
            (source->completed_records==source->sequence->records.size() && source->current_progress!=0)) reject("INVALID_MATERIAL_RUN");
        if (last>=source->completed_records && (last!=source->completed_records || source->current_progress==0)) reject("MATERIAL_RUN_FUTURE_RECORD");
        const auto poll=[&] {stop(limits,source->sequence->revision,started);};poll();
        const auto &rows=source->sequence->records;const auto &origin=rows[first];const auto &m=origin.motion;
        const auto *deposition=std::get_if<Deposition>(&m.payload);
        if (!deposition || !origin.bead || !source->sequence->geometry[first]) reject("MATERIAL_RUN_REQUIRES_DEPOSITION");
        const bool x=m.start.y()==m.end.y(),y=m.start.x()==m.end.x();if (x==y) reject("MATERIAL_RUN_AXIS_DOMAIN");
        const auto axis=x ? MaterialRunAxis::X : MaterialRunAxis::Y;const bool positive=run_coordinate(m.end,axis)>run_coordinate(m.start,axis);
        SceneBox box{m.start,m.start};
        for (size_t i=first;i<=last;++i) {
            poll();++work;const auto &row=rows[i];const auto &event=row.motion;const auto *d=std::get_if<Deposition>(&event.payload);
            if (!d || !row.bead || !source->sequence->geometry[i]) reject("MATERIAL_RUN_REQUIRES_DEPOSITION");
            if ((x ? event.start.y()!=event.end.y() || event.start.y()!=m.start.y() : event.start.x()!=event.end.x() || event.start.x()!=m.start.x()) ||
                (event.start.x()==event.end.x() && event.start.y()==event.end.y()) ||
                (run_coordinate(event.end,axis)>run_coordinate(event.start,axis))!=positive) reject("MATERIAL_RUN_DIRECTION_DOMAIN");
            if (i>first) {
                const auto &p=rows[i-1].motion.end;
                if (p.x()!=event.start.x() || p.y()!=event.start.y() || p.z()!=event.start.z()) reject("MATERIAL_RUN_DISCONTINUITY");
            }
            if (row.bead->kind!=origin.bead->kind || event.source_patch_id!=m.source_patch_id || event.nominal_layer_label!=m.nominal_layer_label ||
                d->material.nominal.value()!=deposition->material.nominal.value() || d->material.upper.value()!=deposition->material.upper.value() ||
                d->material.lower.value()!=deposition->material.lower.value() || d->support_provenance_id!=deposition->support_provenance_id ||
                d->contact_model_id!=deposition->contact_model_id) reject("MATERIAL_RUN_CONTEXT_MISMATCH");
            const Interval fraction(run_progress(*source,i));const auto &b=*row.bead;
            const auto px=Interval(event.start.x())+(Interval(event.end.x())-Interval(event.start.x()))*fraction;
            const auto py=Interval(event.start.y())+(Interval(event.end.y())-Interval(event.start.y()))*fraction;
            const auto pz=Interval(event.start.z())+(Interval(event.end.z())-Interval(event.start.z()))*fraction;
            const auto height=detail::maximum(Interval(b.gap_begin_mm),Interval(b.gap_begin_mm)+(Interval(b.gap_end_mm)-Interval(b.gap_begin_mm))*fraction);
            const auto hx=x ? Interval(0) : Interval(b.width_mm.upper)/Interval(2),hy=x ? Interval(b.width_mm.upper)/Interval(2) : Interval(0);
            const SceneBox current{{(detail::minimum(Interval(event.start.x()),px)-hx).lo,(detail::minimum(Interval(event.start.y()),py)-hy).lo,
                                    (detail::minimum(Interval(event.start.z()),pz)-height).lo},
                                   {(detail::maximum(Interval(event.start.x()),px)+hx).hi,(detail::maximum(Interval(event.start.y()),py)+hy).hi,
                                    detail::maximum(Interval(event.start.z()),pz).hi}};
            if (i==first) box=current;
            else box={{std::min(box.min.x(),current.min.x()),std::min(box.min.y(),current.min.y()),std::min(box.min.z(),current.min.z())},
                      {std::max(box.max.x(),current.max.x()),std::max(box.max.y(),current.max.y()),std::max(box.max.z(),current.max.z())}};
        }
        poll();auto snapshot=std::shared_ptr<const MaterialRunSnapshot>(new MaterialRunSnapshot(source,first,last,axis,positive,box));poll();
        return {"QUALIFIED_DECLARED_CONTINUOUS_AXIS_RUN_ONLY",std::move(snapshot),work};
    } catch (const Rejection &e) {return {e.what(),{},work};}
    catch (const std::exception &e) {return {"MATERIAL_RUN_NUMERIC_FAILURE: "+std::string(e.what()),{},work};}
}

MaterialRunCoverResult cover_material_run_lower(const MaterialRunResult &requested,const SceneBox &requested_box,const MaterialCoverageLimits &requested_limits)
{
    const auto source=requested.snapshot;const auto box=requested_box;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();size_t cells=0,work=0;
    try {
        detail::require_interval_environment();
        if (!source || !valid_coverage_limits(limits) || box.min.x()>box.max.x() || box.min.y()>box.max.y() || box.min.z()>box.max.z()) reject("INVALID_MATERIAL_RUN_COVERAGE");
        for (const auto &p : {box.min,box.max}) {coordinate(p.x());coordinate(p.y());coordinate(p.z());}
        const auto poll=[&] {stop(limits,source->source->sequence->revision,started);};poll();
        const auto charge=[&] {poll();if (work>=limits.max_evaluations) reject("MATERIAL_RUN_WORK_LIMIT");++work;};
        const auto visit=[&] {poll();if (cells>=limits.max_cells) reject("MATERIAL_RUN_CELL_LIMIT");++cells;};
        if (!cover_run_box(*source,box,limits.max_depth,charge,visit)) reject("MATERIAL_RUN_LOWER_NOT_CERTIFIED");
        poll();auto snapshot=std::shared_ptr<const MaterialRunCoverSnapshot>(new MaterialRunCoverSnapshot(source,box,expand_run_box(*source,box),cells,work));poll();
        return {"WHOLE_INFLATED_BOX_IN_ACTUAL_NOMINAL_RUN_ONLY",std::move(snapshot),cells,work};
    } catch (const Rejection &e) {return {e.what(),{},cells,work};}
    catch (const std::exception &e) {return {"MATERIAL_RUN_COVER_NUMERIC_FAILURE: "+std::string(e.what()),{},cells,work};}
}

MaterialRunJoinResult find_material_run_join(const MaterialRunResult &requested_first,const MaterialRunResult &requested_second,
    const SceneBox &requested_box,const MaterialJoinLimits &requested_limits)
{
    const auto first=requested_first.snapshot,second=requested_second.snapshot;const auto box=requested_box;const auto limits=requested_limits;
    JoinSearch search{first ? first->source : nullptr,limits,std::chrono::steady_clock::now()};
    try {
        detail::require_interval_environment();
        if (!first || !second || first->source!=second->source || !(first->last_record<second->first_record || second->last_record<first->first_record) ||
            !valid_join_limits(limits)) reject("INVALID_MATERIAL_RUN_JOIN");
        validate_join_source(search.source,box);search.poll();search.runs={first,second};
        const auto witness=search.find({0},{1},box);if (!witness) reject("MATERIAL_RUN_JOIN_NOT_CERTIFIED");
        search.poll();auto snapshot=std::shared_ptr<const MaterialRunJoinSnapshot>(new MaterialRunJoinSnapshot(first,second,box,witness->box,
            join_box_volume(witness->box),search.cells,search.evaluations));search.poll();
        return {"COMMON_POSITIVE_CONTINUOUS_RUN_LOWER_BOX_ONLY",std::move(snapshot),search.cells,search.evaluations};
    } catch (const Rejection &e) {return {e.what(),{},search.cells,search.evaluations};}
    catch (const std::exception &e) {return {"MATERIAL_RUN_JOIN_NUMERIC_FAILURE: "+std::string(e.what()),{},search.cells,search.evaluations};}
}

MaterialJoinResult find_material_join(const LowerMaterialView &requested,size_t first,size_t second,
    const SceneBox &requested_box,const MaterialJoinLimits &requested_limits)
{
    const auto source=requested.snapshot;const auto box=requested_box;const auto limits=requested_limits;
    JoinSearch search{source,limits,std::chrono::steady_clock::now()};
    try {
        detail::require_interval_environment();if (!valid_join_limits(limits)) reject("INVALID_MATERIAL_JOIN_LIMITS");validate_join_source(source,box);search.poll();
        const auto witness=search.find({first},{second},box);if (!witness) reject("MATERIAL_JOIN_NOT_CERTIFIED");
        search.poll();auto snapshot=std::shared_ptr<const MaterialJoinSnapshot>(new MaterialJoinSnapshot(source,first,second,box,witness->box,
            join_box_volume(witness->box),search.cells,search.evaluations));search.poll();
        return {"COMMON_POSITIVE_D_LOWER_BOX_ONLY",std::move(snapshot),search.cells,search.evaluations};
    } catch (const Rejection &e) {return {e.what(),{},search.cells,search.evaluations};}
    catch (const std::exception &e) {return {"MATERIAL_JOIN_NUMERIC_FAILURE: "+std::string(e.what()),{},search.cells,search.evaluations};}
}

namespace {
struct CapJoinRequest {
    std::vector<std::vector<size_t>> records;
    std::vector<std::pair<size_t,size_t>> pairs;
};
CapJoinRequest first_cap_join_request(const FirstCapSnapshot &source,size_t max_records,JoinSearch &search)
{
    const auto &rows=search.source->sequence->records;
    if (rows.size()>max_records || search.source->completed_records!=rows.size() || search.source->current_progress!=0)
        reject("FIRST_CAP_JOIN_INCOMPLETE_CANDIDATE");
    std::vector<std::vector<size_t>> records;size_t row=0;
    for (const auto &path : source.paths) {
        search.charge();records.emplace_back();
        if (row<rows.size() && !rows[row].bead) {search.charge();++row;}
        for (const auto &packet : path->pieces) {
            search.charge();if (row>=rows.size() || !rows[row].bead || std::get<Deposition>(rows[row].motion.payload).volume.value()!=packet.volume.value())
                reject("FIRST_CAP_JOIN_LEDGER_MISMATCH");
            records.back().push_back(row++);
        }
    }
    if (row!=rows.size()) reject("FIRST_CAP_JOIN_LEDGER_MISMATCH");
    std::vector<std::pair<size_t,size_t>> pairs;
    for (size_t i=0;i<4;++i) pairs.emplace_back(i,(i+1)%4);
    for (size_t i=4;i<source.paths.size();++i) for (const auto &point : {source.paths[i]->path_start,source.paths[i]->path_end}) {
        search.charge();const bool x=source.paths[i]->path_start.y()==source.paths[i]->path_end.y();std::optional<size_t> edge;
        for (size_t j=0;j<4;++j) {
            search.charge();const auto a=source.paths[j]->path_start,b=source.paths[j]->path_end;
            const auto coordinate=[&](PhysicalPosition p) {return x ? p.x() : p.y();};
            const Exact first(coordinate(source.paths[i]->path_start)),last(coordinate(source.paths[i]->path_end));
            const bool start=coordinate(point)==coordinate(source.paths[i]->path_start);
            const bool crossing=source.hatch_extent==FirstCapHatchExtent::ContourCentres ? coordinate(a)==coordinate(point) :
                start ? Exact(coordinate(a))>=first && Exact(coordinate(a))<(first+last)/Exact(2) :
                        Exact(coordinate(a))<=last && Exact(coordinate(a))>(first+last)/Exact(2);
            if (crossing && (x ? a.x()==b.x() && point.y()>=std::min(a.y(),b.y()) && point.y()<=std::max(a.y(),b.y()) :
                a.y()==b.y() && point.x()>=std::min(a.x(),b.x()) && point.x()<=std::max(a.x(),b.x()))) {
                if (edge) reject("FIRST_CAP_JOIN_AMBIGUOUS_EDGE");edge=j;
            }
        }
        if (!edge) reject("FIRST_CAP_JOIN_MISSING_EDGE");pairs.emplace_back(i,*edge);
    }
    return {std::move(records),std::move(pairs)};
}
}

FirstCapJoinsResult assess_first_cap_joins(const FirstCapResult &requested,const FirstCapJoinLimits &requested_limits)
{
    const auto source=requested.snapshot;const auto limits=requested_limits;
    JoinSearch search{source && source->fill && source->fill->occupied ? source->fill->occupied->source : nullptr,limits,std::chrono::steady_clock::now()};
    try {
        detail::require_interval_environment();
        if (!source || !search.source || !valid_join_limits(limits) || limits.max_joins<4 || limits.max_joins>8192 ||
            !limits.max_records || limits.max_records>200000 || source->paths.size()<5 ||
            source->paths.size()-4>(limits.max_joins-4)/2) reject("INVALID_FIRST_CAP_JOIN_INPUT");
        const auto domain=source->fill->occupied->domain;validate_join_source(search.source,domain);search.poll();
        const auto request=first_cap_join_request(*source,limits.max_records,search);
        const auto &records=request.records;const auto &pairs=request.pairs;
        std::vector<FirstCapJoin> joins;
        for (const auto &pair : pairs) {
            const size_t cells=search.cells,work=search.evaluations;
            std::optional<JoinSearch::Witness> witness;
            try {witness=search.find(records[pair.first],records[pair.second],domain);}
            catch (const Rejection &e) {throw Rejection(std::string(e.what())+" paths="+std::to_string(pair.first)+","+std::to_string(pair.second));}
            if (!witness) throw Rejection("FIRST_CAP_JOIN_NOT_CERTIFIED paths="+std::to_string(pair.first)+","+std::to_string(pair.second));
            auto proof=std::shared_ptr<const MaterialJoinSnapshot>(new MaterialJoinSnapshot(search.source,witness->first,witness->second,domain,witness->box,
                join_box_volume(witness->box),search.cells-cells,search.evaluations-work));joins.push_back({pair.first,pair.second,std::move(proof)});
        }
        search.poll();auto snapshot=std::shared_ptr<const FirstCapJoinsSnapshot>(new FirstCapJoinsSnapshot(source,std::move(joins),search.cells,search.evaluations));
        search.poll();return {"ALL_REQUESTED_LOCAL_FIRST_CAP_JOINS_HAVE_COMMON_D_LOWER_BOXES_ONLY",std::move(snapshot),search.cells,search.evaluations};
    } catch (const Rejection &e) {return {e.what(),{},search.cells,search.evaluations};}
    catch (const std::exception &e) {return {"FIRST_CAP_JOIN_NUMERIC_FAILURE: "+std::string(e.what()),{},search.cells,search.evaluations};}
}

FirstCapRunJoinsResult assess_first_cap_run_joins(const FirstCapResult &requested,const FirstCapJoinLimits &requested_limits)
{
    const auto source=requested.snapshot;const auto limits=requested_limits;
    JoinSearch search{source && source->fill && source->fill->occupied ? source->fill->occupied->source : nullptr,limits,std::chrono::steady_clock::now()};
    try {
        detail::require_interval_environment();
        if (!source || !search.source || !valid_join_limits(limits) || limits.max_joins<4 || limits.max_joins>8192 ||
            !limits.max_records || limits.max_records>200000 || source->paths.size()<5 ||
            source->paths.size()-4>(limits.max_joins-4)/2) reject("INVALID_FIRST_CAP_RUN_JOIN_INPUT");
        const auto domain=source->fill->occupied->domain;validate_join_source(search.source,domain);search.poll();
        const auto request=first_cap_join_request(*source,limits.max_records,search);
        for (const auto &records : request.records) {
            if (records.empty()) reject("FIRST_CAP_RUN_JOIN_EMPTY_PATH");
            MaterialLimits capture;capture.max_records=std::min(limits.max_records,limits.max_evaluations-search.evaluations);
            capture.timeout=limits.timeout;capture.cancelled=[&] {search.poll();return false;};
            const auto run=reconstruct_material_run({search.source},records.front(),records.back(),capture);
            search.charge(run.evaluations);if (!run.snapshot) throw Rejection(run.reason);
            search.runs.push_back(run.snapshot);
        }
        std::vector<FirstCapRunJoin> joins;
        for (const auto &pair : request.pairs) {
            const size_t cells=search.cells,work=search.evaluations;std::optional<JoinSearch::Witness> witness;
            try {witness=search.find({pair.first},{pair.second},domain);}
            catch (const Rejection &e) {throw Rejection(std::string(e.what())+" paths="+std::to_string(pair.first)+","+std::to_string(pair.second));}
            if (!witness) throw Rejection("FIRST_CAP_RUN_JOIN_NOT_CERTIFIED paths="+std::to_string(pair.first)+","+std::to_string(pair.second));
            auto proof=std::shared_ptr<const MaterialRunJoinSnapshot>(new MaterialRunJoinSnapshot(search.runs[pair.first],search.runs[pair.second],domain,witness->box,
                join_box_volume(witness->box),search.cells-cells,search.evaluations-work));joins.push_back({pair.first,pair.second,std::move(proof)});
        }
        search.poll();auto snapshot=std::shared_ptr<const FirstCapRunJoinsSnapshot>(new FirstCapRunJoinsSnapshot(source,std::move(joins),search.cells,search.evaluations));
        search.poll();return {"ALL_REQUESTED_LOCAL_FIRST_CAP_JOINS_HAVE_COMMON_CONTINUOUS_RUN_LOWER_BOXES_ONLY",std::move(snapshot),search.cells,search.evaluations};
    } catch (const Rejection &e) {return {e.what(),{},search.cells,search.evaluations};}
    catch (const std::exception &e) {return {"FIRST_CAP_RUN_JOIN_NUMERIC_FAILURE: "+std::string(e.what()),{},search.cells,search.evaluations};}
}

FirstCapInterfaceResult assess_first_cap_interface(const FirstCapResult &requested,const FirstCapInterfacePolicy &requested_policy,
    const FirstCapInterfaceLimits &requested_limits)
{
    const auto source=requested.snapshot;const auto policy=requested_policy;const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();size_t cells=0,work=0;
    try {
        detail::require_interval_environment();
        if (!source || !source->source || !source->source->source || !source->fill || !source->fill->target ||
            !valid_coverage_limits(limits) || !limits.max_records || limits.max_records>200000 ||
            !limits.max_patches || limits.max_patches>65535 || policy.support_depth.value()<=0 ||
            policy.maximum_support_separation.value()<=0 || policy.maximum_nominal_gap.value()<=0 ||
            policy.minimum_flat_floor_width.value()<=0) reject("INVALID_FIRST_CAP_INTERFACE");
        for (auto v : {policy.support_depth,policy.maximum_support_separation,policy.maximum_nominal_gap,
            policy.maximum_nominal_overlap,policy.minimum_flat_floor_width}) coordinate(v.value());
        const auto stack=source->source->source;const auto body=stack->source;
        if (!body || !body->sequence || body->sequence->records.size()>limits.max_records || stack->surfaces.empty() ||
            !source->fill->occupied || !source->fill->occupied->source ||
            source->fill->occupied->source->sequence->records.size()>limits.max_records ||
            source->fill->target->source!=body || stack->first_pass.proof!=source->fill->target) reject("FIRST_CAP_INTERFACE_BODY_MISMATCH");
        const auto &roi=stack->surfaces.front().cell.footprint;const double plane=stack->support_plane_z_mm;
        const SceneBox anchor{{roi.min_x,roi.min_y,(Interval(plane)-Interval(policy.support_depth.value())).lo},{roi.max_x,roi.max_y,plane}};
        const auto poll=[&] {stop(limits,body->sequence->revision,started);};poll();
        const auto charge=[&](size_t count=1) {
            if (count>limits.max_evaluations-work) reject("FIRST_CAP_INTERFACE_WORK_LIMIT");work+=count;poll();
        };
        const auto visit=[&] {poll();if (cells>=limits.max_cells) reject("FIRST_CAP_INTERFACE_CELL_LIMIT");++cells;};
        const size_t end=body->completed_records+(body->current_progress>0 && body->completed_records<body->sequence->records.size());
        charge(end); // Coverage also walks the active source before its section evaluations.
        MaterialCoverageLimits support=limits;support.max_evaluations-=work;
        support.timeout-=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        support.cancelled=[&] {poll();return false;};support.is_current={};
        const auto covered=cover_material(LowerMaterialView{body},anchor,support);
        charge(covered.evaluations);cells+=covered.cells;
        if (covered.status!=MaterialCoverageStatus::Covered) throw Rejection("FIRST_CAP_INTERFACE_ANCHOR_NOT_CERTIFIED: "+covered.reason);
        const auto volume=join_box_volume(anchor);if (volume.lower<=0) reject("FIRST_CAP_INTERFACE_EMPTY_ANCHOR");
        std::vector<size_t> active;
        for (size_t i=0;i<end;++i) {
            charge();const auto &row=body->sequence->records[i];if (!row.bead) continue;
            const auto top=Interval(row.motion.start.z())+(Interval(row.motion.end.z())-Interval(row.motion.start.z()))*Interval(run_progress(*body,i));
            if (std::max(row.motion.start.z(),top.hi)<plane) continue;
            // Nominal outer AABB only prunes distant rows; it never certifies
            // support or contact. Retain the actual partial current endpoint.
            const Interval fraction(run_progress(*body,i)),half=Interval(row.bead->width_mm.upper)/Interval(2);
            const auto px=Interval(row.motion.start.x())+(Interval(row.motion.end.x())-Interval(row.motion.start.x()))*fraction;
            const auto py=Interval(row.motion.start.y())+(Interval(row.motion.end.y())-Interval(row.motion.start.y()))*fraction;
            if ((detail::maximum(Interval(row.motion.start.x()),px)+half).hi<roi.min_x ||
                (detail::minimum(Interval(row.motion.start.x()),px)-half).lo>roi.max_x ||
                (detail::maximum(Interval(row.motion.start.y()),py)+half).hi<roi.min_y ||
                (detail::minimum(Interval(row.motion.start.y()),py)-half).lo>roi.max_y) continue;
            active.push_back(i);
        }
        const auto error=Interval(body->sequence->model.numerical_coordinate_error.value())+Interval(source->numerical_error_upper_mm);
        std::vector<FirstCapFloorPatch> patches;
        for (size_t path_index=0;path_index<source->paths.size();++path_index) {
            charge();const auto &path=source->paths[path_index];
            if (!path || path->source!=source->source || path->roof_domain!=FirstHatchRoofDomain::FiniteWidth) reject("FIRST_CAP_INTERFACE_PATH_MISMATCH");
            for (size_t index=0;index<path->pieces.size();++index) {
                charge();if (patches.size()>=limits.max_patches) reject("FIRST_CAP_INTERFACE_PATCH_LIMIT");
                const auto &piece=path->pieces[index];const auto &b=piece.section;
                const bool x=piece.start.y()==piece.end.y();if (x==(piece.start.x()==piece.end.x())) reject("FIRST_CAP_INTERFACE_AXIS_DOMAIN");
                const auto area=Interval(piece.volume.value())/detail::root(length_squared(piece.start,piece.end));
                const auto half_at=[&](Interval h) {return b.kind==BeadSectionKind::RoundedRectangle ?
                    (section_width(area,h,b.kind)-h)/Interval(2) : area/h/Interval(2);};
                const auto minimum=half_at(Interval(std::max(b.gap_begin_mm,b.gap_end_mm)));
                const auto maximum=half_at(Interval(std::min(b.gap_begin_mm,b.gap_end_mm)));
                const double centre=x ? piece.start.y() : piece.start.x();
                const double low=(Interval(centre)-Interval(maximum.hi)).lo,high=(Interval(centre)+Interval(maximum.hi)).hi;
                const ScalarBounds widths=bounds(Interval(2)*Interval(minimum.lo,maximum.hi));
                if (widths.lower<policy.minimum_flat_floor_width.value()) reject("FIRST_CAP_INTERFACE_FLAT_FLOOR_TOO_NARROW");
                const RectangleXY floor=x ? RectangleXY{std::min(piece.start.x(),piece.end.x()),low,std::max(piece.start.x(),piece.end.x()),high} :
                                            RectangleXY{low,std::min(piece.start.y(),piece.end.y()),high,std::max(piece.start.y(),piece.end.y())};
                const Exact xy(error.hi);
                if (Exact(floor.min_x)-xy<Exact(roi.min_x) || Exact(floor.max_x)+xy>Exact(roi.max_x) ||
                    Exact(floor.min_y)-xy<Exact(roi.min_y) || Exact(floor.max_y)+xy>Exact(roi.max_y)) reject("FIRST_CAP_INTERFACE_FLOOR_OUTSIDE_ANCHOR");
                const Exact z0=Exact(piece.start.z())-Exact(b.gap_begin_mm),dz=Exact(piece.end.z())-Exact(b.gap_end_mm)-z0;
                const auto floor_height=Interval(exact_interval(std::min(z0,z0+dz)).lo,exact_interval(std::max(z0,z0+dz)).hi)+Interval(-error.hi,error.hi);
                const auto distance=floor_height-Interval(plane);
                if (distance.lo<0 || distance.hi>policy.maximum_support_separation.value()) reject("FIRST_CAP_INTERFACE_SUPPORT_DISTANCE_NOT_CERTIFIED");
                struct Node {Exact a,b;size_t depth;std::vector<size_t> candidates;};std::vector<Node> pending{{Exact(0),Exact(1),0,active}};
                double gap_lower=std::numeric_limits<double>::infinity(),gap_upper=-std::numeric_limits<double>::infinity();
                while (!pending.empty()) {
                    visit();auto node=std::move(pending.back());pending.pop_back();
                    const auto along=[&](const Exact &t) {return Exact(x ? piece.start.x() : piece.start.y())+
                        (Exact(x ? piece.end.x() : piece.end.y())-Exact(x ? piece.start.x() : piece.start.y()))*t;};
                    const Exact a=along(node.a),bb=along(node.b),lo=std::min(a,bb),hi=std::max(a,bb);
                    const Exact ha=Exact(b.gap_begin_mm)+(Exact(b.gap_end_mm)-Exact(b.gap_begin_mm))*node.a;
                    const Exact hb=Exact(b.gap_begin_mm)+(Exact(b.gap_end_mm)-Exact(b.gap_begin_mm))*node.b;
                    const auto local=half_at(exact_interval(std::min(ha,hb)));
                    const Exact transverse_lo((Interval(centre)-Interval(local.hi)).lo),transverse_hi((Interval(centre)+Interval(local.hi)).hi);
                    // Include original coordinate uncertainty on every axis,
                    // rather than checking only the nominal floor centre.
                    const Polygon polygon=x ? Polygon{{lo-xy,transverse_lo-xy},{hi+xy,transverse_lo-xy},{hi+xy,transverse_hi+xy},{lo-xy,transverse_hi+xy}} :
                                              Polygon{{transverse_lo-xy,lo-xy},{transverse_hi+xy,lo-xy},{transverse_hi+xy,hi+xy},{transverse_lo-xy,hi+xy}};
                    auto roof=nominal_roof_bounds(*body,polygon,node.candidates,plane,[&] {charge();});
                    const Exact fa=z0+dz*node.a,fb=z0+dz*node.b;
                    const auto separation=Interval(exact_interval(std::min(fa,fb)).lo,exact_interval(std::max(fa,fb)).hi)-roof.height+Interval(-error.hi,error.hi);
                    if (separation.lo>=-policy.maximum_nominal_overlap.value() && separation.hi<=policy.maximum_nominal_gap.value()) {
                        gap_lower=std::min(gap_lower,separation.lo);gap_upper=std::max(gap_upper,separation.hi);continue;
                    }
                    if (node.depth>=limits.max_depth) reject("FIRST_CAP_INTERFACE_NOMINAL_SEPARATION_NOT_CERTIFIED");
                    const Exact mid=(node.a+node.b)/Exact(2);
                    pending.push_back({mid,node.b,node.depth+1,roof.active});pending.push_back({node.a,mid,node.depth+1,std::move(roof.active)});
                }
                patches.push_back({path_index,index,floor,widths,bounds(floor_height),{gap_lower,gap_upper},bounds(distance)});
            }
        }
        if (patches.empty()) reject("FIRST_CAP_INTERFACE_EMPTY_CAP");
        poll();auto snapshot=std::shared_ptr<const FirstCapInterfaceSnapshot>(new FirstCapInterfaceSnapshot(source,body,policy,anchor,volume,
            std::move(patches),cells,work));poll();
        return {"BOUNDED_FLAT_FLOOR_BODY_ANCHOR_AND_NOMINAL_INTERFACE_ONLY",std::move(snapshot),cells,work};
    } catch (const Rejection &e) {return {e.what(),{},cells,work};}
    catch (const std::exception &e) {return {"FIRST_CAP_INTERFACE_NUMERIC_FAILURE: "+std::string(e.what()),{},cells,work};}
}

}
