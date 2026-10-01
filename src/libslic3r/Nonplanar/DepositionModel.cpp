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
MaterialMembership piece(const MaterialRecord &row, const MaterialModel &model, Projection projection, double progress, Representation rep)
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
    const bool along_inside=t.lo>begin.hi && t.hi<end.lo;
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
        const auto clipped=roof_projection(row,sequence.model,polygon,progress,Representation::Nominal);
        if (!clipped) continue;
        const auto possible=nominal_roof(row,clipped->projected,progress);
        if (!possible || possible->height.hi<floor) continue;
        active.push_back(i);
        if (splitter==sequence.records.size() || possible->height.hi>upper) splitter=i;
        upper=std::max(upper,possible->height.hi);
        const auto guaranteed=nominal_roof(row,project_polygon(row,polygon,Interval(0)),progress);
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
            const auto radius=Interval(source->policy.width.value())/Interval(2);
            // Inward rounding keeps every owner inside the finite nominal
            // flat-ended path footprint. Rounded transverse voids are not filled.
            const RectangleXY core=x_axis ? RectangleXY{first.start.x(),(Interval(first.start.y())-radius).hi,
                    first.end.x(),(Interval(last.start.y())+radius).lo} :
                RectangleXY{(Interval(first.start.x())-radius).hi,first.start.y(),
                    (Interval(last.start.x())+radius).lo,first.end.y()};
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

FirstHatchBeadResult FirstHatchBeadSnapshot::plan(const AffineHatchResult &requested, size_t line_index,
    const FirstHatchBeadLimits &requested_limits, FirstHatchRoofDomain domain,const AffineHatchLine *slice,double slice_error)
{
    const auto source=requested.snapshot; const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    try {
        detail::require_interval_environment();
        if (!source || !source->source || source->passes.empty() || line_index>=source->passes.front().lines.size() ||
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
        const auto sequence=cursor->sequence; const auto line=slice ? *slice : source->passes.front().lines[line_index];
        const bool x_axis=line.start.y()==line.end.y(), y_axis=line.start.x()==line.end.x();
        if (x_axis==y_axis) reject("FIRST_HATCH_REQUIRES_AXIS_ALIGNED_CENTERLINE");
        // Any admitted actual-gap width lies in nominal +/- this requested
        // error. Query its exact outer strip before deriving the gap/amount;
        // shrinking the query to the nominal centerline would be circular.
        const Exact half=(Exact(source->policy.width.value())+Exact(limits.packets.maximum_width_error.value()))/Exact(2);
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
            if (h0>0 && h1>0 && std::max(h0,h1)<source->policy.width.value() && gap_error<=limits.maximum_gap_error.value()) {
                auto packet_limits=limits.packets;
                if (pieces.size()>=packet_limits.max_segments) reject("FIRST_HATCH_PACKET_COUNT_LIMIT");
                packet_limits.max_segments-=pieces.size();packet_limits.maximum_volume_error=Volume(budget/4);
                packet_limits.timeout=std::min(limits.timeout,limits.packets.timeout)-
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
                packet_limits.cancelled=[&] { poll();return false; };packet_limits.is_current={};
                packets=plan_fixed_width_bead({node.a.point,node.b.point,source->policy.width,VerticalGap(h0),VerticalGap(h1),
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
                        width_error=std::max({width_error,(Interval(source->policy.width.value())-wmin).hi,(wmax-Interval(source->policy.width.value())).hi});
                    }
                    const auto length=detail::root(length_squared(node.a.point,node.b.point));
                    const auto maximum_h=Interval(std::max(h0,h1))+Interval(gap_error);
                    const auto uncertainty=length*Interval(gap_error)*(Interval(source->policy.width.value())+Interval(2)*correction*maximum_h);
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
enum class UnionClip { Box, BelowRoof, AboveSurface };
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
            if (full_individual) {
                if (xmin.lo>=domain.min.x() && xmax.hi<=domain.max.x() && ymin.lo>=domain.min.y() && ymax.hi<=domain.max.y() && zmin>=domain.min.z() && zmax<=domain.max.z())
                    *full_individual+=Exact(std::get<Deposition>(m.payload).volume.value())*Exact(fraction);
                else full_individual.reset();
            }
        }
        if (clip_kind==UnionClip::AboveSurface) {
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
        const AffineCapCell flat{{domain.min.x(),domain.min.y(),domain.max.x(),domain.max.y()},0,0,0};
        const auto body_fraction=[&](size_t i) {return i<roof_source->source->completed_records ? 1 : roof_source->source->current_progress;};
        std::vector<size_t> body_active;
        if (clip_kind==UnionClip::BelowRoof) {
            const auto &body=*roof_source->source->sequence;
            const size_t end=roof_source->source->completed_records+(roof_source->source->current_progress>0 && roof_source->source->completed_records<body.records.size());
            for (size_t i=0;i<end;++i) {evaluate();if (body.records[i].bead) body_active.push_back(i);}
        }
        const auto surface_q=clip_kind==UnionClip::AboveSurface ? exact_interval(Exact(surface->z00)-shear_x*Exact(surface->footprint.min_x)-
            shear_y*Exact(surface->footprint.min_y)) : Interval(0);
        struct Node {Polygon polygon;std::vector<size_t> candidates;Exact union_lo,union_hi,sum_lo,sum_hi,repeated_lo,repeated_hi;size_t depth,id,splitter;double uncertainty;bool potential_chain;Interval roof;std::vector<size_t> body_candidates;size_t roof_splitter;};
        const auto make_node=[&](Polygon polygon,const std::vector<size_t> &candidates,size_t depth,const std::vector<size_t> &body_candidates) {
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
            if (clip_kind==UnionClip::BelowRoof) {
                // Refine the protected actual source with the target
                // integrator's same continuous whole-cell roof bounds.
                auto bound=nominal_roof_bounds(*roof_source->source,polygon,body_candidates,roof_source->minimum,evaluate);
                roof=bound.height;next_body=std::move(bound.active);roof_splitter=bound.splitter;
            }
            const auto span=[&](double lo,double hi,bool guaranteed=false) {
                if (full_individual) return Span{lo,hi}; // Every complete bead is inside the original XYZ box.
                double bottom=guaranteed ? (Interval(domain.min.z())-Interval(reference.lo)).hi : (Interval(domain.min.z())-Interval(reference.hi)).lo;
                double top=guaranteed ? (Interval(domain.max.z())-Interval(reference.hi)).lo : (Interval(domain.max.z())-Interval(reference.lo)).hi;
                if (clip_kind==UnionClip::BelowRoof) top=std::min(top,guaranteed ? (Interval(roof.lo)-Interval(reference.hi)).lo : (Interval(roof.hi)-Interval(reference.lo)).hi);
                if (clip_kind==UnionClip::AboveSurface) bottom=std::max(bottom,guaranteed ? surface_q.hi : surface_q.lo);
                return Span{std::max(bottom,lo),std::min(top,hi)};
            };
            std::vector<Span> lower,upper;std::vector<size_t> next;Exact sum_lo(0),sum_hi(0),largest_lower(0);
            struct Strip {Exact begin,end;Span vertical,possible;bool guaranteed;Projection projected;size_t index;};
            std::map<std::pair<bool,double>,std::vector<Strip>> strips;
            std::vector<Span> count_lower,count_upper;
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
                const auto inside=vertical(i,x_axis!=y_axis ? project_bounds(row,cell_bounds) : project_polygon(row,polygon,Interval(0)),fraction);
                if (inside && inside->second.whole_transverse && longitudinal(row,polygon,fraction))
                    lower.push_back(span(inside->first.hi,inside->second.height.lo,true));
                // Adjacent finite packets can cover a cell together even when
                // none covers its full length. Prove that longitudinal union;
                // never replace disconnected packets by one continuous line.
                if (x_axis!=y_axis) {
                    const auto &m=row.motion;const auto coordinate=[&](PhysicalPosition p) {return x_axis ? p.x() : p.y();};
                    const Exact start(coordinate(m.start)),last=start+(Exact(coordinate(m.end))-start)*Exact(fraction);
                    const auto s=span(possible->first.hi,possible->second.height.lo,true);
                    strips[{x_axis,x_axis ? m.start.y() : m.start.x()}].push_back({std::min(start,last),std::max(start,last),s,outside,
                        inside && inside->second.whole_transverse && s.first<s.second,footprint->projected,i});
                } else {
                    count_upper.push_back(outside);
                    if (inside && inside->second.whole_footprint) count_lower.push_back(span(inside->first.hi,inside->second.height.lo,true));
                }
                double uncertainty=outside.second-outside.first;
                if (!full_individual) {
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
                    uncertainty=exact_interval(individual_hi-individual_lo).hi;
                } else if (inside && inside->second.whole_transverse) {
                    const auto s=span(inside->first.hi,inside->second.height.lo,true);
                    uncertainty-=std::max(0.,s.second-s.first);
                }
                if (uncertainty>worst) {worst=uncertainty;splitter=i;}
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
                    Exact covered=begin;double bottom=0,top=0,possible_bottom=chain.rows.front().possible.first,possible_top=chain.rows.front().possible.second;bool have=false;
                    Exact potential_covered=begin;
                    for (const auto &row : chain.rows) {
                        possible_bottom=std::min(possible_bottom,row.possible.first);possible_top=std::max(possible_top,row.possible.second);
                        if (row.begin<=potential_covered) potential_covered=std::max(potential_covered,row.end);
                        if (!row.guaranteed || row.end<begin || row.begin>end || row.begin>covered) continue;
                        covered=std::max(covered,row.end);
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
                    if (have && covered>=end && bottom<top) {
                        lower.push_back({bottom,top});count_lower.push_back({bottom,top});
                        covered_chains.push_back({group.first.first,chain.rows});
                    }
                }
            }
            const Exact union_lo=std::max(area*measure(lower),largest_lower),union_hi=full_individual ? area*measure(upper) : std::min(area*measure(upper),sum_hi);
            if (union_lo>union_hi || sum_lo>sum_hi) reject("MATERIAL_UNION_INCONSISTENT_SECTION");
            const auto excess=[&](const std::vector<Span> &spans) {
                Exact sum(0);for (auto s : spans) if (s.first<s.second) sum+=Exact(s.second)-Exact(s.first);
                return sum-measure(spans);
            };
            Exact repeated_lo=area*excess(count_lower),repeated_hi=area*excess(count_upper);
            if (clip_kind==UnionClip::Box && count_upper.size()==2 && covered_chains.size()==2 && repeated_lo>0 &&
                covered_chains[0].first==covered_chains[1].first) {
                Exact xmin=polygon.front()[0],xmax=xmin,ymin=polygon.front()[1],ymax=ymin;
                for (const auto &p : polygon) {xmin=std::min(xmin,p[0]);xmax=std::max(xmax,p[0]);ymin=std::min(ymin,p[1]);ymax=std::max(ymax,p[1]);}
                if (area==(xmax-xmin)*(ymax-ymin)) {
                    // For each fixed longitudinal point the two guaranteed
                    // convex sections have concave intersection height. On a
                    // rectangle, Hermite-Hadamard bounds its transverse mean
                    // between endpoint trapezoid and midpoint height. Interval
                    // envelopes retain all longitudinal packets and Z clipping.
                    const bool x_axis=covered_chains.front().first;
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
                                auto normal=coordinate-Interval(x_axis ? m.start.y() : m.start.x());
                                if (x_axis ? m.end.x()<m.start.x() : m.end.y()>m.start.y()) normal=Interval(0)-normal;
                                projection.normal=normal;
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
            if (repeated_lo>repeated_hi) reject("MATERIAL_UNION_INCONSISTENT_SECTION_EXCESS");
            const double uncertainty=exact_interval(clip_kind!=UnionClip::Box ? union_hi-union_lo : full_individual ? repeated_hi-repeated_lo : union_hi-union_lo+sum_hi-sum_lo).hi;
            bool splitter_chain=false;
            if (splitter<sequence->records.size()) {
                const auto &m=sequence->records[splitter].motion;const bool x_axis=m.start.y()==m.end.y();
                splitter_chain=potential_chains.count({x_axis,x_axis ? m.start.y() : m.start.x()})!=0;
            }
            return Node{std::move(polygon),std::move(next),union_lo,union_hi,sum_lo,sum_hi,repeated_lo,repeated_hi,depth,id,splitter,uncertainty,splitter_chain,roof,std::move(next_body),roof_splitter};
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
                p.normal.hi-p.normal.lo<row.bead->width_mm.lower/2);
            bool along=finite_end;
            if (clip_kind==UnionClip::BelowRoof && !finite_end) {
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
            if (along) normal={dx,dy};
            if (clip_kind==UnionClip::BelowRoof && node.depth%2==0 && node.roof.hi-node.roof.lo>1e-12 &&
                node.roof_splitter<roof_source->source->sequence->records.size()) {
                const auto &support=roof_source->source->sequence->records[node.roof_splitter];const auto &m=support.motion;
                const Exact x=Exact(m.end.x())-Exact(m.start.x()),y=Exact(m.end.y())-Exact(m.start.y());normal={-y,x};
                const auto projected=project_polygon(support,node.polygon,Interval(0));const double span=projected.normal.hi-projected.normal.lo;
                if (((projected.t.lo<=0 || projected.t.hi>=body_fraction(node.roof_splitter)) && span<support.bead->width_mm.lower/2) ||
                    std::max(std::abs(m.end.z()-m.start.z()),std::abs(support.bead->gap_end_mm-support.bead->gap_begin_mm))*(projected.t.hi-projected.t.lo)>span)
                    normal={x,y};
            }
            auto lo=normal[0]*node.polygon.front()[0]+normal[1]*node.polygon.front()[1],hi=lo;
            for (auto point : node.polygon) {const Exact value=normal[0]*point[0]+normal[1]*point[1];lo=std::min(lo,value);hi=std::max(hi,value);}
            if (lo==hi) reject("MATERIAL_UNION_INVALID_SPLIT");const Exact middle=(lo+hi)/Exact(2);
            auto first=make_node(clip(node.polygon,normal,middle,false),node.candidates,node.depth+1,node.body_candidates);
            auto second=make_node(clip(node.polygon,normal,middle,true),node.candidates,node.depth+1,node.body_candidates);
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
        const Exact half=(Exact(hatches->policy.width.value())+Exact(limits.beads.packets.maximum_width_error.value()))/Exact(2);
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

}
