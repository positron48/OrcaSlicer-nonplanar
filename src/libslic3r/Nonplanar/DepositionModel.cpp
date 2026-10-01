#include "DepositionModel.hpp"
#include "Canonical.hpp"
#include "Interval.hpp"
#include "StlImport.hpp"
#include <set>
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
struct RoofProjection { Projection projected; Interval top_growth; };
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
    return RoofProjection{project_polygon(row,polygon,Interval(0)),top_growth};
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
struct NominalRoof { Interval height; bool whole_footprint; };
std::optional<NominalRoof> nominal_roof(const MaterialRecord &row, Projection projection, double progress)
{
    const auto &b=*row.bead; const auto &m=row.motion;
    const auto local=detail::maximum(Interval(0),detail::minimum(projection.t,Interval(progress)));
    const auto height=Interval(b.gap_begin_mm)+(Interval(b.gap_end_mm)-Interval(b.gap_begin_mm))*local;
    const auto top=Interval(m.start.z())+(Interval(m.end.z())-Interval(m.start.z()))*local;
    const auto area=Interval(std::get<Deposition>(m.payload).volume.value())/detail::root(length_squared(row));
    const auto width=section_width(area,height,b.kind), normal=absolute(projection.normal);
    const bool whole=projection.t.lo>0 && projection.t.hi<progress && normal.hi<(width/Interval(2)).lo;
    if (b.kind==BeadSectionKind::Rectangle) return NominalRoof{top,whole};
    const auto core=(width-height)/Interval(2), radius=height/Interval(2);
    const auto transverse=detail::maximum(normal-core,Interval(0));
    const auto radicand=square(radius)-square(transverse);
    if (radicand.hi<0) return {};
    // The nonnegative root encloses every real cross-section in the projected
    // cell. A lower roof is usable only if that bead covers the entire XY cell.
    return NominalRoof{top-height/Interval(2)+detail::root(radicand),whole};
}
std::pair<Interval,Interval> affine_integral(const Polygon &polygon, const AffineCapCell &cell, Exact *exact_area=nullptr)
{
    Exact twice_area(0), x_moment(0), y_moment(0);
    for (size_t i=0; i<polygon.size(); ++i) {
        const auto &a=polygon[i], &b=polygon[(i+1)%polygon.size()];
        const Exact cross=a[0]*b[1]-b[0]*a[1];
        twice_area+=cross; x_moment+=(a[0]+b[0])*cross; y_moment+=(a[1]+b[1])*cross;
    }
    if (twice_area==0 && exact_area) { *exact_area=Exact(0);return {Interval(0),Interval(0)}; }
    if (twice_area<=0) reject("MATERIAL_INTEGRAL_DEGENERATE_CELL");
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
            double lower=plane, upper=plane; size_t splitter=sequence->records.size(); std::vector<size_t> active;
            for (size_t i : candidates) {
                evaluate(); const auto &row=sequence->records[i]; const double progress=fraction(i);
                const auto end=Interval(row.motion.start.z())+(Interval(row.motion.end.z())-Interval(row.motion.start.z()))*Interval(progress);
                if (std::max(row.motion.start.z(),end.hi)<plane) continue;
                const auto clipped=roof_projection(row,sequence->model,polygon,progress,Representation::Nominal);
                if (!clipped) continue;
                const auto possible=nominal_roof(row,clipped->projected,progress);
                if (!possible || possible->height.hi<plane) continue;
                active.push_back(i);
                if (splitter==sequence->records.size() || possible->height.hi>upper) splitter=i;
                upper=std::max(upper,possible->height.hi);
                const auto guaranteed=nominal_roof(row,project_polygon(row,polygon,Interval(0)),progress);
                if (guaranteed && guaranteed->whole_footprint) lower=std::max(lower,guaranteed->height.lo);
            }
            if (active.empty() || lower>upper) reject("MATERIAL_INTEGRAL_INCONSISTENT_ROOF");
            const auto integral=affine_integral(polygon,cell);
            const auto volume=integral.second-integral.first*Interval(lower,upper);
            return Node{std::move(polygon),std::move(active),volume,Interval(lower,upper),depth,id,splitter,(Interval(volume.hi)-Interval(volume.lo)).hi};
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
        const auto stored=[&](const Exact &value) {
            const auto range=exact_interval(value); const double midpoint=(range.lo+range.hi)/2; coordinate(midpoint);
            const auto error=range-Interval(midpoint);
            return std::pair<double,double>{midpoint,std::max(std::abs(error.lo),std::abs(error.hi))};
        };
        const auto point=[&](double x,double y,const AffineCapCell &c) {
            const Exact z=Exact(c.z00)+(Exact(c.z10)-Exact(c.z00))*(Exact(x)-Exact(r.min_x))/(Exact(r.max_x)-Exact(r.min_x))+
                (Exact(c.z01)-Exact(c.z00))*(Exact(y)-Exact(r.min_y))/(Exact(r.max_y)-Exact(r.min_y));
            const auto value=stored(z); return std::pair<PhysicalPosition,double>{{x,y,value.first},value.second};
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
                poll(); const auto value=stored(Exact(low)+(Exact(high)-Exact(low))*Exact(int(i))/Exact(int(intervals)));
                centers.push_back(value.first); errors.push_back(value.second);
                if (i) cuts.push_back(stored((Exact(centers[i-1])+Exact(centers[i]))/Exact(2)).first);
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
        const auto stored=[](const Exact &exact) {
            const auto range=exact_interval(exact);const double value=(range.lo+range.hi)/2;coordinate(value);
            const auto delta=range-Interval(value);
            return std::pair<double,double>{value,std::max(std::abs(delta.lo),std::abs(delta.hi))};
        };
        struct Endpoint { PhysicalPosition point; double gap, error; };
        const auto endpoint=[&](const Exact &t) {
            const auto x=stored(Exact(request.start.x())+(Exact(request.end.x())-Exact(request.start.x()))*t);
            const auto y=stored(Exact(request.start.y())+(Exact(request.end.y())-Exact(request.start.y()))*t);
            const auto z=stored(Exact(request.start.z())+(Exact(request.end.z())-Exact(request.start.z()))*t);
            const auto h=stored(Exact(request.gap_begin.value())+(Exact(request.gap_end.value())-Exact(request.gap_begin.value()))*t);
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
}
