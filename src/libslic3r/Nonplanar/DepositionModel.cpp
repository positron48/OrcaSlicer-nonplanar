#include "DepositionModel.hpp"
#include "Canonical.hpp"
#include "Interval.hpp"
#include "StlImport.hpp"
#include <set>

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
enum class Representation { Nominal, Upper, Lower };
MaterialMembership piece(const MaterialRecord &row, const MaterialModel &model, PhysicalPosition point, double progress, Representation rep)
{
    const auto &b=*row.bead; const auto &m=row.motion;
    const Interval dx=Interval(m.end.x())-Interval(m.start.x()), dy=Interval(m.end.y())-Interval(m.start.y());
    const auto length2=square(dx)+square(dy), length=detail::root(length2);
    const auto x=Interval(point.x())-Interval(m.start.x()), y=Interval(point.y())-Interval(m.start.y());
    const auto t=(dx*x+dy*y)/length2;
    const auto n=absolute((dx*y-dy*x)/length);
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
    const auto local=rep==Representation::Upper ? detail::maximum(Interval(0),detail::minimum(t,Interval(progress))) : t;
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
        if (half.hi<=0 || n.lo>half.hi || point.z()<lower.lo || point.z()>upper.hi) return MaterialMembership::Outside;
        if (along_inside && half.lo>0 && n.hi<half.lo && point.z()>lower.hi && point.z()<upper.lo) return MaterialMembership::Inside;
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
    const auto distance2=square(transverse)+square(Interval(point.z())-center), radius2=square(radius);
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
            const auto membership=piece(row,sequence->model,point,i<cursor->completed_records ? 1 : cursor->current_progress,rep);
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
}
