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
Interval length_squared(const MaterialRecord &row)
{
    return square(Interval(row.motion.end.x())-Interval(row.motion.start.x()))+
           square(Interval(row.motion.end.y())-Interval(row.motion.start.y()));
}
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
std::optional<double> roof_ceiling(const MaterialRecord &row, const MaterialModel &model,
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
    const auto local=detail::maximum(Interval(0),detail::minimum(projected.t,Interval(progress)));
    const auto top=Interval(m.start.z())+dz*local+top_growth;
    coordinate(top.lo); coordinate(top.hi);
    return top.hi;
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
}
