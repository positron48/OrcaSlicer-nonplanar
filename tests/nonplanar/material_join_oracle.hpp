#pragma once
#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/DepositionModel.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <limits>
#include <map>

namespace Slic3r::nptop::test {
// Independent complete Travel-leaf inequalities in 113 bits. Reconstruct the
// swept cell from its corners, then bound the declared Upper cross-section by
// monotone core/radius formulae. No planner interval or membership code is used.
inline void independent_travel_material_partition(const MaterialMotionSnapshot &proof)
{
    using Q=boost::multiprecision::cpp_bin_float_quad;
    // Restricted analytical fixtures: 113-bit reference arithmetic retains a
    // separate 1e-20 exclusion guard. This never reduces a planner margin.
    const Q reference_error("1e-20");
    const auto &ledger=*proof.source->ledger;const auto &event=ledger.records.at(proof.event_index).motion;
    REQUIRE(std::holds_alternative<Travel>(event.payload));
    const Q margin=Q(proof.policy.required.value())+proof.policy.numeric.total_mm()+proof.policy.tool_measurement.value()+
        proof.policy.positioning.value()+proof.policy.material.value()+proof.policy.scene_geometry.value();
    const Q xy=Q(ledger.model.outer_xy_growth.value())+ledger.model.numerical_coordinate_error.value();
    const Q ze=Q(ledger.model.outer_z_growth.value())+ledger.model.numerical_coordinate_error.value(),pi=acos(Q(-1));
    const auto minimum_abs=[](Q lo,Q hi) {return lo<=0 && hi>=0 ? Q(0) : std::min(abs(lo),abs(hi));};
    std::map<std::pair<size_t,size_t>,std::vector<const MaterialMotionLeaf*>> groups;
    for (const auto &leaf : proof.leaves) {
        groups[{leaf.component_index,leaf.material_record}].push_back(&leaf);
        REQUIRE(leaf.material_record<proof.event_index);const auto &row=ledger.records.at(leaf.material_record);REQUIRE(row.bead);
        const auto &m=row.motion;const auto &b=*row.bead;
        REQUIRE(leaf.parameter.lower>=0);REQUIRE(leaf.parameter.upper<=1);REQUIRE(leaf.parameter.lower<leaf.parameter.upper);
        if (leaf.outside_tool) {
            const auto &tip=std::get<FiniteTip>(proof.tools.at(leaf.component_index).geometry);
            const Q xlo=Q(leaf.local_domain.min.x())-tip.center.x(),xhi=Q(leaf.local_domain.max.x())-tip.center.x();
            const Q ylo=Q(leaf.local_domain.min.y())-tip.center.y(),yhi=Q(leaf.local_domain.max.y())-tip.center.y();
            const Q near=pow(minimum_abs(xlo,xhi),2)+pow(minimum_abs(ylo,yhi),2);
            const Q far=pow(std::max(abs(xlo),abs(xhi)),2)+pow(std::max(abs(ylo),abs(yhi)),2);
            REQUIRE((far<Q(tip.opening_radius.value())*tip.opening_radius.value()-reference_error || near>Q(tip.outer_radius.value())*tip.outer_radius.value()+reference_error));
            continue;
        }
        std::array<Q,3> lo,hi;
        const double a[]={event.start.x(),event.start.y(),event.start.z()},e[]={event.end.x(),event.end.y(),event.end.z()};
        const double l[]={leaf.local_domain.min.x(),leaf.local_domain.min.y(),leaf.local_domain.min.z()},h[]={leaf.local_domain.max.x(),leaf.local_domain.max.y(),leaf.local_domain.max.z()};
        for (size_t i=0;i<3;++i) {
            REQUIRE(std::max({std::abs(a[i]),std::abs(e[i]),std::abs(l[i]),std::abs(h[i])})<=100);
            const Q begin=Q(a[i])+Q(leaf.parameter.lower)*(Q(e[i])-a[i]),end=Q(a[i])+Q(leaf.parameter.upper)*(Q(e[i])-a[i]);
            lo[i]=std::min(begin,end)+Q(l[i])-margin;hi[i]=std::max(begin,end)+Q(h[i])+margin;
        }
        const Q dx=Q(m.end.x())-m.start.x(),dy=Q(m.end.y())-m.start.y(),l2=dx*dx+dy*dy,length=sqrt(l2);
        REQUIRE(length>=Q(".001"));
        Q tlo=Q("1e20"),thi=-tlo,nlo=tlo,nhi=-tlo;
        for (const Q &x : {lo[0],hi[0]}) for (const Q &y : {lo[1],hi[1]}) {
            const Q t=(dx*(x-m.start.x())+dy*(y-m.start.y()))/l2,n=(dx*(y-m.start.y())-dy*(x-m.start.x()))/length;
            tlo=std::min(tlo,t);thi=std::max(thi,t);nlo=std::min(nlo,n);nhi=std::max(nhi,n);
        }
        if (thi<-xy/length-reference_error || tlo>1+xy/length+reference_error) continue;
        const Q t0=std::max(Q(0),std::min(Q(1),tlo)),t1=std::max(Q(0),std::min(Q(1),thi));
        const Q dh=Q(b.gap_end_mm)-b.gap_begin_mm,dz=Q(m.end.z())-m.start.z(),area=Q(std::get<Deposition>(m.payload).volume.value())/length;
        const Q h0=Q(b.gap_begin_mm)+dh*t0,h1=Q(b.gap_begin_mm)+dh*t1,hmin=std::min(h0,h1),hmax=std::max(h0,h1);
        REQUIRE(hmin>=Q(".001"));REQUIRE(hmax<=1);REQUIRE(std::get<Deposition>(m.payload).volume.value()<=10);
        const Q prefix_min=std::min(Q(b.gap_begin_mm),Q(b.gap_end_mm)),shift=std::min(xy/length,Q(1)),nmin=minimum_abs(nlo,nhi);
        const Q top0=Q(m.start.z())+dz*t0,top1=Q(m.start.z())+dz*t1;
        if (b.kind==BeadSectionKind::RoundedRectangle) {
            const Q core=area/(2*hmin)-pi*hmin/8+(area/(prefix_min*prefix_min)+pi/4)*abs(dh)/2*shift;
            const Q radius=hmax/2+xy+ze+(abs(dz-dh/2)+abs(dh)/2)*shift;
            const Q centre0=top0-h0/2,centre1=top1-h1/2;
            const Q vertical=std::max({Q(0),lo[2]-std::max(centre0,centre1),std::min(centre0,centre1)-hi[2]});
            const Q transverse=std::max(Q(0),nmin-core);
            REQUIRE(core<=10);REQUIRE(radius<=10);REQUIRE(transverse*transverse+vertical*vertical>radius*radius+reference_error);
        } else {
            const Q half=area/(2*hmin)+xy+area/(prefix_min*prefix_min)*abs(dh)*shift/2;
            REQUIRE(half<=10);REQUIRE((nmin>half+reference_error || lo[2]>std::max(top0,top1)+ze+abs(dz)*shift+reference_error || hi[2]<std::min(top0-h0,top1-h1)-ze-abs(dz-dh)*shift-reference_error));
        }
    }
    // Exact binary partition domains: verify containment, positive interiors,
    // pairwise disjointness and the complete time x local volume/plane measure.
    for (size_t component=0;component<proof.tools.size();++component) for(size_t row=0;row<proof.event_index;++row) {
        if (!ledger.records[row].bead) continue;
        const auto &parts=groups.at({component,row});const auto &tool=proof.tools[component];
        ToolBox domain{{0,0,0},{0,0,0}};bool plane=false;
        if (const auto *box=std::get_if<ToolBox>(&tool.geometry)) domain=*box;
        else {
            const auto &tip=std::get<FiniteTip>(tool.geometry);plane=true;
            const double infinity=std::numeric_limits<double>::infinity();
            domain={{std::nextafter(tip.center.x()-tip.outer_radius.value(),-infinity),std::nextafter(tip.center.y()-tip.outer_radius.value(),-infinity),tip.center.z()},
                {std::nextafter(tip.center.x()+tip.outer_radius.value(),infinity),std::nextafter(tip.center.y()+tip.outer_radius.value(),infinity),tip.center.z()}};
        }
        const double low[]={domain.min.x(),domain.min.y(),domain.min.z()},high[]={domain.max.x(),domain.max.y(),domain.max.z()};
        Q measure=0;const size_t dimensions=plane ? 2 : 3;
        for(size_t i=0;i<parts.size();++i) {
            const auto &leaf=*parts[i];const double a[]={leaf.local_domain.min.x(),leaf.local_domain.min.y(),leaf.local_domain.min.z()},b[]={leaf.local_domain.max.x(),leaf.local_domain.max.y(),leaf.local_domain.max.z()};
            Q volume=Q(leaf.parameter.upper)-leaf.parameter.lower;
            for(size_t d=0;d<3;++d) {REQUIRE(a[d]>=low[d]);REQUIRE(b[d]<=high[d]);if(d<dimensions){REQUIRE(a[d]<b[d]);volume*=Q(b[d])-a[d];}}
            measure+=volume;
            for(size_t j=0;j<i;++j) {
                const auto &other=*parts[j];bool disjoint=leaf.parameter.upper<=other.parameter.lower || other.parameter.upper<=leaf.parameter.lower;
                const double c[]={other.local_domain.min.x(),other.local_domain.min.y(),other.local_domain.min.z()},e[]={other.local_domain.max.x(),other.local_domain.max.y(),other.local_domain.max.z()};
                for(size_t d=0;d<dimensions;++d) disjoint=disjoint || b[d]<=c[d] || e[d]<=a[d];REQUIRE(disjoint);
            }
        }
        Q expected=1;for(size_t d=0;d<dimensions;++d) expected*=Q(high[d])-low[d];
        REQUIRE(abs(measure-expected)<Q("1e-28")); // reference accumulation rounding only
    }
}
// Independent 113-bit point refutation, never a continuous PASS certificate or
// measured physical-material assertion. No planner interval/query code is used.
inline void independent_annulus_upper_witness(const MaterialMotionResult &result)
{
    using Q=boost::multiprecision::cpp_bin_float_quad;
    REQUIRE(result.status==ClearanceStatus::Fail);REQUIRE(result.source);REQUIRE(result.witness);
    const auto &w=*result.witness;const auto &ledger=*result.source->ledger;
    REQUIRE(w.parameter>=0);REQUIRE(w.parameter<=1);REQUIRE(w.material_record<=result.event_index);
    const auto &tip=std::get<FiniteTip>(result.tools.at(w.component_index).geometry);
    const Q tx=Q(w.local_point.x())-tip.center.x(),ty=Q(w.local_point.y())-tip.center.y();
    REQUIRE(w.local_point.z()==tip.center.z());
    REQUIRE(tx*tx+ty*ty>Q(tip.opening_radius.value())*tip.opening_radius.value());
    REQUIRE(tx*tx+ty*ty<Q(tip.outer_radius.value())*tip.outer_radius.value());
    const auto &event=ledger.records.at(result.event_index).motion;
    const Q x=Q(event.start.x())+Q(w.parameter)*(Q(event.end.x())-event.start.x())+w.local_point.x();
    const Q y=Q(event.start.y())+Q(w.parameter)*(Q(event.end.y())-event.start.y())+w.local_point.y();
    const Q z=Q(event.start.z())+Q(w.parameter)*(Q(event.end.z())-event.start.z())+w.local_point.z();
    const auto &row=ledger.records.at(w.material_record);REQUIRE(row.bead);
    const auto &m=row.motion;const auto &b=*row.bead;
    const Q dx=Q(m.end.x())-m.start.x(),dy=Q(m.end.y())-m.start.y(),l2=dx*dx+dy*dy,l=sqrt(l2);
    const Q t=(dx*(x-m.start.x())+dy*(y-m.start.y()))/l2,n=abs((dx*(y-m.start.y())-dy*(x-m.start.x()))/l);
    const Q progress=w.material_record<result.event_index ? Q(1) : Q(w.parameter);
    const Q xy=Q(ledger.model.outer_xy_growth.value())+ledger.model.numerical_coordinate_error.value();
    const Q ze=Q(ledger.model.outer_z_growth.value())+ledger.model.numerical_coordinate_error.value();
    REQUIRE(progress>0);REQUIRE(t>-xy/l);REQUIRE(t<progress+xy/l);
    const Q local=std::max(Q(0),std::min(progress,t)),dh=Q(b.gap_end_mm)-b.gap_begin_mm,dz=Q(m.end.z())-m.start.z();
    const Q h=Q(b.gap_begin_mm)+dh*local,top=Q(m.start.z())+dz*local;
    const Q hmin=std::min(Q(b.gap_begin_mm),Q(b.gap_begin_mm)+dh*progress),shift=std::min(xy/l,progress);
    const Q a=Q(std::get<Deposition>(m.payload).volume.value())/l;
    if (b.kind==BeadSectionKind::RoundedRectangle) {
        const Q pi=acos(Q(-1)),core=a/(2*h)-pi*h/8+(a/(hmin*hmin)+pi/4)*abs(dh)/2*shift;
        const Q radius=h/2+xy+ze+(abs(dz-dh/2)+abs(dh)/2)*shift;
        const Q transverse=std::max(n-core,Q(0)),vertical=z-(top-h/2);
        REQUIRE(core>=0);REQUIRE(radius>0);REQUIRE(transverse*transverse+vertical*vertical<radius*radius);
    } else {
        const Q half=a/(2*h)+xy+a/(hmin*hmin)*abs(dh)*shift/2;
        REQUIRE(n<half);REQUIRE(z<top+ze+abs(dz)*shift);REQUIRE(z>top-h-ze-abs(dz-dh)*shift);
    }
}
// Independent whole-box oracle. Bound every affine centre and normal over the
// box, then use the smallest local eroded stadium core/radius. Derivative losses
// retain the full actual-prefix height domain, including the finite-butt shift.
// This is a continuous bound, rather than a sample of witness points.
inline void independent_lower_box(const MaterialPrefixSnapshot &source,size_t index,const SceneBox &box)
{
    using Q=boost::multiprecision::cpp_bin_float_quad;
    const auto &sequence=*source.sequence;
    {
        const auto &row=sequence.records[index];const auto &m=row.motion;const auto &b=*row.bead;
        const Q dx=Q(m.end.x())-m.start.x(),dy=Q(m.end.y())-m.start.y(),l=sqrt(dx*dx+dy*dy);
        Q tlo=2,thi=-1,nmax=0;
        for (double x : {box.min.x(),box.max.x()}) for (double y : {box.min.y(),box.max.y()}) {
            const Q px=Q(x)-m.start.x(),py=Q(y)-m.start.y(),t=(dx*px+dy*py)/(l*l);
            tlo=std::min(tlo,t);thi=std::max(thi,t);nmax=std::max(nmax,abs((dx*py-dy*px)/l));
        }
        const Q progress=index<source.completed_records ? 1 : source.current_progress;
        const Q xy=Q(sequence.model.inner_xy_loss.value())+sequence.model.numerical_coordinate_error.value();
        const Q ze=Q(sequence.model.inner_z_loss.value())+sequence.model.numerical_coordinate_error.value();
        REQUIRE(tlo>xy/l);REQUIRE(thi<progress-xy/l);
        const Q dh=Q(b.gap_end_mm)-b.gap_begin_mm,dz=Q(m.end.z())-m.start.z();
        const Q hp=Q(b.gap_begin_mm)+dh*progress,hmin=std::min(Q(b.gap_begin_mm),hp),hmax=std::max(Q(b.gap_begin_mm),hp);
        const Q area=Q(std::get<Deposition>(m.payload).volume.value())/l,shift=std::min(xy/l,progress);
        const Q hl=Q(b.gap_begin_mm)+dh*tlo,hh=Q(b.gap_begin_mm)+dh*thi;
        const Q c0=Q(m.start.z())+dz*tlo-hl/2,c1=Q(m.start.z())+dz*thi-hh/2;
        const Q vertical=std::max(abs(Q(box.min.z())-std::max(c0,c1)),abs(Q(box.max.z())-std::min(c0,c1)));
        if (b.kind==BeadSectionKind::RoundedRectangle) {
            const Q pi=acos(Q(-1)),error=(area/(hmin*hmin)+pi/4)*abs(dh)/2*shift;
            const Q global_core=(area/hmax-pi*hmax/4)/2-error,local_hmax=std::max(hl,hh);
            const Q core=global_core<=0 ? Q(0) : (area/local_hmax-pi*local_hmax/4)/2-error;
            const Q radius_loss=xy+ze+(abs(dz-dh/2)+abs(dh)/2)*shift,radius=std::min(hl,hh)/2-radius_loss;
            REQUIRE(core>=0);REQUIRE((radius>0 && hmin/2-radius_loss>0));
            const Q n=std::max(Q(0),nmax-core);REQUIRE(n*n+vertical*vertical<radius*radius);
        } else {
            const Q half=area/std::max(hl,hh)/2-xy-area/(hmin*hmin)*abs(dh)*shift/2;
            const Q top0=Q(m.start.z())+dz*tlo,top1=Q(m.start.z())+dz*thi;
            REQUIRE(nmax<half);
            REQUIRE(Q(box.max.z())<std::min(top0,top1)-ze-abs(dz)*shift);
            REQUIRE(Q(box.min.z())>std::max(top0-hl,top1-hh)+ze+abs(dz-dh)*shift);
        }
    }
}
inline void independent_join_box(const MaterialJoinSnapshot &join)
{
    using Q=boost::multiprecision::cpp_bin_float_quad;const auto &box=join.witness;
    for (size_t index : {join.first_record,join.second_record}) independent_lower_box(*join.source,index,box);
    const Q volume=(Q(box.max.x())-box.min.x())*(Q(box.max.y())-box.min.y())*(Q(box.max.z())-box.min.z());
    REQUIRE(join.box_volume_mm3.lower>0);REQUIRE(join.box_volume_mm3.lower<=volume);REQUIRE(join.box_volume_mm3.upper>=volume);
}

// Independent 113-bit continuous bound of the mathematically inflated box.
// Use actual binary packet flux, not nominal_width or production predicates.
inline void independent_run_box(const MaterialRunSnapshot &run,const SceneBox &box,bool lower=true)
{
    using Q=boost::multiprecision::cpp_bin_float_quad;
    const auto &source=*run.source;const auto &sequence=*source.sequence;
    const auto s=[&](PhysicalPosition p) {return Q(run.axis==MaterialRunAxis::X ? p.x() : p.y());};
    const auto n=[&](PhysicalPosition p) {return Q(run.axis==MaterialRunAxis::X ? p.y() : p.x());};
    const Q xy=lower ? Q(sequence.model.inner_xy_loss.value())+sequence.model.numerical_coordinate_error.value() : Q(0);
    const Q ze=lower ? Q(sequence.model.inner_z_loss.value())+sequence.model.numerical_coordinate_error.value() : Q(0);
    const Q lo=s(box.min)-xy,hi=s(box.max)+xy,nlo=n(box.min)-xy,nhi=n(box.max)+xy;
    const Q zlo=Q(box.min.z())-ze,zhi=Q(box.max.z())+ze;
    const auto fraction=[&](size_t i) {return i<source.completed_records ? Q(1) : Q(source.current_progress);};
    const auto &last=sequence.records[run.last_record].motion;
    const Q start=s(sequence.records[run.first_record].motion.start),end=s(last.start)+(s(last.end)-s(last.start))*fraction(run.last_record);
    REQUIRE(lo>std::min(start,end));REQUIRE(hi<std::max(start,end));Q covered=0;
    for (size_t i=run.first_record;i<=run.last_record;++i) {
        REQUIRE((i<source.completed_records || (i==source.completed_records && source.current_progress>0)));
        const auto &row=sequence.records[i];const auto &m=row.motion;const auto &b=*row.bead;
        const Q a=s(m.start),delta=s(m.end)-a,endpoint=a+delta*fraction(i);
        const Q left=std::max(lo,std::min(a,endpoint)),right=std::min(hi,std::max(a,endpoint));if (left>right) continue;
        covered+=right-left;const Q t0=(left-a)/delta,t1=(right-a)/delta;
        REQUIRE(std::min(t0,t1)>=0);REQUIRE(std::max(t0,t1)<=fraction(i));
        const Q dh=Q(b.gap_end_mm)-b.gap_begin_mm,dz=Q(m.end.z())-m.start.z();
        const Q h0=Q(b.gap_begin_mm)+dh*t0,h1=Q(b.gap_begin_mm)+dh*t1,hmin=std::min(h0,h1),hmax=std::max(h0,h1);
        const Q top0=Q(m.start.z())+dz*t0,top1=Q(m.start.z())+dz*t1;
        const Q area=Q(std::get<Deposition>(m.payload).volume.value())/abs(delta),normal=std::max(abs(nlo-n(m.start)),abs(nhi-n(m.start)));
        if (b.kind==BeadSectionKind::RoundedRectangle) {
            const Q core=(area/hmax-acos(Q(-1))*hmax/4)/2,radius=hmin/2;
            const Q c0=top0-h0/2,c1=top1-h1/2;
            const Q vertical=std::max(abs(zlo-std::max(c0,c1)),abs(zhi-std::min(c0,c1))),horizontal=std::max(Q(0),normal-core);
            REQUIRE(core>=0);REQUIRE(radius>0);REQUIRE(horizontal*horizontal+vertical*vertical<radius*radius);
        } else {
            REQUIRE(normal<area/hmax/2);REQUIRE(zlo>std::max(top0-h0,top1-h1));REQUIRE(zhi<std::min(top0,top1));
        }
    }
    // Only accumulated high-precision arithmetic rounding is tolerated here.
    REQUIRE(abs(covered-(hi-lo))<Q("1e-30"));
}
// Independent analytic point roof, used for dose checks and geometric negative
// cases only. Acceptance still requires whole-domain certificates.
inline std::optional<boost::multiprecision::cpp_bin_float_quad> independent_nominal_roof(const MaterialPrefixSnapshot &source,double x,double y)
{
    using Q=boost::multiprecision::cpp_bin_float_quad;std::optional<Q> highest;
    const size_t end=source.completed_records+(source.current_progress>0 && source.completed_records<source.sequence->records.size());
    for (size_t i=0;i<end;++i) {
        const auto &row=source.sequence->records[i];if (!row.bead) continue;
        const auto &m=row.motion;const auto &b=*row.bead;
        const Q dx=Q(m.end.x())-m.start.x(),dy=Q(m.end.y())-m.start.y(),l2=dx*dx+dy*dy,l=sqrt(l2);
        const Q t=(dx*(Q(x)-m.start.x())+dy*(Q(y)-m.start.y()))/l2;
        const Q progress=i<source.completed_records ? Q(1) : Q(source.current_progress);if (t<0 || t>progress) continue;
        const Q n=abs((-dy*(Q(x)-m.start.x())+dx*(Q(y)-m.start.y()))/l),h=Q(b.gap_begin_mm)+(Q(b.gap_end_mm)-b.gap_begin_mm)*t;
        const Q a=Q(std::get<Deposition>(m.payload).volume.value())/l,top=Q(m.start.z())+(Q(m.end.z())-m.start.z())*t;
        Q roof=top;
        if (b.kind==BeadSectionKind::Rectangle) {if (n>a/h/2) continue;}
        else {
            const Q radius=h/2,core=(a/h-acos(Q(-1))*h/4)/2,horizontal=std::max(Q(0),n-core);
            if (horizontal>radius) continue;roof=top-radius+sqrt(radius*radius-horizontal*horizontal);
        }
        highest=highest ? std::max(*highest,roof) : roof;
    }
    return highest;
}

}
