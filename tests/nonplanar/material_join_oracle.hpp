#pragma once
#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/DepositionModel.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>

namespace Slic3r::nptop::test {
// Independent whole-box oracle. Bound every affine centre and normal over the
// box, then use the smallest local eroded stadium core/radius. Derivative losses
// retain the full actual-prefix height domain, including the finite-butt shift.
// This is a continuous bound, rather than a sample of witness points.
inline void independent_join_box(const MaterialJoinSnapshot &join)
{
    using Q=boost::multiprecision::cpp_bin_float_quad;
    const auto &sequence=*join.source->sequence;const auto &box=join.witness;
    for (size_t index : {join.first_record,join.second_record}) {
        const auto &row=sequence.records[index];const auto &m=row.motion;const auto &b=*row.bead;
        const Q dx=Q(m.end.x())-m.start.x(),dy=Q(m.end.y())-m.start.y(),l=sqrt(dx*dx+dy*dy);
        Q tlo=2,thi=-1,nmax=0;
        for (double x : {box.min.x(),box.max.x()}) for (double y : {box.min.y(),box.max.y()}) {
            const Q px=Q(x)-m.start.x(),py=Q(y)-m.start.y(),t=(dx*px+dy*py)/(l*l);
            tlo=std::min(tlo,t);thi=std::max(thi,t);nmax=std::max(nmax,abs((dx*py-dy*px)/l));
        }
        const Q progress=index<join.source->completed_records ? 1 : join.source->current_progress;
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
    const Q volume=(Q(box.max.x())-box.min.x())*(Q(box.max.y())-box.min.y())*(Q(box.max.z())-box.min.z());
    REQUIRE(join.box_volume_mm3.lower>0);REQUIRE(join.box_volume_mm3.lower<=volume);REQUIRE(join.box_volume_mm3.upper>=volume);
}
}
