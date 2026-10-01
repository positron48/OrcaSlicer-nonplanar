#pragma once
#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/DepositionModel.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>

namespace Slic3r::nptop::test {
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
inline void independent_run_box(const MaterialRunSnapshot &run,const SceneBox &box)
{
    using Q=boost::multiprecision::cpp_bin_float_quad;
    const auto &source=*run.source;const auto &sequence=*source.sequence;
    const auto s=[&](PhysicalPosition p) {return Q(run.axis==MaterialRunAxis::X ? p.x() : p.y());};
    const auto n=[&](PhysicalPosition p) {return Q(run.axis==MaterialRunAxis::X ? p.y() : p.x());};
    const Q xy=Q(sequence.model.inner_xy_loss.value())+sequence.model.numerical_coordinate_error.value();
    const Q ze=Q(sequence.model.inner_z_loss.value())+sequence.model.numerical_coordinate_error.value();
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
}
