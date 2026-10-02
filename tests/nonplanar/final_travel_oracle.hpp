#pragma once
#include "final_material_oracle.hpp"
#include <map>
namespace nptop_test {
// Independent 113-bit replay/equations for complete cells and partitions. This
// restricted reference calls no verifier/planner geometry or broad-phase code.
template<class Proof> void check_final_motion(const Proof &proof)
{
 using namespace nptop_verify;using H=FinalMaterialHigh;const auto &m=*proof.source;const auto &s=proof.scene;
 const auto steps=replay_material_steps(m);const H guard("1e-20");
 const bool depositing=m.declarations[proof.first_record].kind==MaterialEventKind::Deposit;
 for(size_t i=proof.first_record;i<proof.first_record+proof.record_count;++i)REQUIRE(m.declarations[i].kind==m.declarations[proof.first_record].kind);
 REQUIRE(proof.prefix->source==proof.source);REQUIRE(proof.prefix->completed_records==proof.first_record);REQUIRE(proof.prefix->current_progress==0);
 H margin(s.uncertainty_mm);for(double v:s.clearance_mm)margin+=H(v);
 const H xy=H(m.policy.outer_xy_growth_mm)+H(m.policy.numerical_coordinate_error_mm),z=H(m.policy.outer_z_growth_mm)+H(m.policy.numerical_coordinate_error_mm);
 const H pi=acos(H(-1)),scale=pi*H(m.rates->policy.filament_diameter)*H(m.rates->policy.filament_diameter)/4/H(m.rates->policy.flow);
 struct Bead {size_t record;FinalMaterialStep step;H length,area,h0,dh,top,bottom,half;MaterialSectionKind kind;};std::vector<Bead> beads;
 for(size_t i=0;i<proof.first_record+(depositing ? proof.record_count : 0);++i){if(m.declarations[i].kind!=MaterialEventKind::Deposit)continue;const auto &p=steps[i];const auto section=*m.declarations[i].section;
  for(size_t a=0;a<3;++a){REQUIRE(abs(p.start[a])<=100);REQUIRE(abs(p.end[a])<=100);}
  const H length=sqrt((p.end[0]-p.start[0])*(p.end[0]-p.start[0])+(p.end[1]-p.start[1])*(p.end[1]-p.start[1]));
  REQUIRE(length>=H(".001"));const H h0(section.gap_begin_mm),h1(section.gap_end_mm),dose=p.e*scale*(1+H(m.policy.relative_dose_error))+H(m.policy.absolute_dose_error_mm3);
  REQUIRE(h0>=H(".001"));REQUIRE(h1>=H(".001"));REQUIRE(dose>0);REQUIRE(dose<=10);
  const H area=dose/length,half=area/std::min(h0,h1)/2+(section.kind==MaterialSectionKind::Rectangle ? H(0) : std::max(h0,h1)/2);
  REQUIRE(half<=10);beads.push_back({i,p,length,area,h0,h1-h0,std::max(p.start[2],p.end[2]),std::min(p.start[2]-h0,p.end[2]-h1),half,section.kind});
 }
 std::map<std::pair<size_t,size_t>,std::vector<const LinearTravelLeaf*>> groups;
 for(const auto &leaf:proof.leaves){REQUIRE(leaf.record>=proof.first_record);REQUIRE(leaf.record<proof.first_record+proof.record_count);
  REQUIRE(leaf.component<=s.head.size());REQUIRE(leaf.progress.lower>=0);REQUIRE(leaf.progress.upper<=1);REQUIRE(leaf.progress.lower<leaf.progress.upper);
  const auto &motion=steps[leaf.record];const H t0(leaf.progress.lower),t1(leaf.progress.upper);
  for(size_t a=0;a<3;++a){const H p0=motion.start[a]+t0*(motion.end[a]-motion.start[a]),p1=motion.start[a]+t1*(motion.end[a]-motion.start[a]);
   REQUIRE(H(leaf.world.min[a])<=std::min(p0,p1)+H(leaf.local.min[a])-margin+H("1e-28"));
   REQUIRE(H(leaf.world.max[a])>=std::max(p0,p1)+H(leaf.local.max[a])+margin-H("1e-28"));}
  groups[{leaf.record,leaf.component}].push_back(&leaf);
  if(leaf.outside_annulus){REQUIRE(leaf.component==0);H low=0,high=0;
   for(size_t a=0;a<2;++a){const H lo=H(leaf.local.min[a])-H(s.tip_center[a]),hi=H(leaf.local.max[a])-H(s.tip_center[a]);
    low+=lo<=0 && hi>=0 ? H(0) : std::min(lo*lo,hi*hi);high+=std::max(lo*lo,hi*hi);}
   REQUIRE((high<H(s.opening_radius_mm)*H(s.opening_radius_mm)-guard || low>H(s.outer_radius_mm)*H(s.outer_radius_mm)+guard));continue;
  }
  for(const auto &obstacle:s.obstacles){bool outside=false;for(size_t a=0;a<3;++a)outside|=leaf.world.max[a]<obstacle.min[a] || leaf.world.min[a]>obstacle.max[a];REQUIRE(outside);}
  for(const auto &b:beads){
   if(b.record>leaf.record)continue;const H front=b.record==leaf.record ? H(leaf.progress.upper) : H(1);
   if(H(leaf.world.min[2])-z>b.top+guard || H(leaf.world.max[2])+z<b.bottom-guard)continue;
   bool broad=false;for(size_t a=0;a<2;++a)broad|=H(leaf.world.min[a])>std::max(b.step.start[a],b.step.end[a])+b.half+2*xy+guard ||
    H(leaf.world.max[a])<std::min(b.step.start[a],b.step.end[a])-b.half-2*xy-guard;
   if(broad)continue;
   const H dx=b.step.end[0]-b.step.start[0],dy=b.step.end[1]-b.step.start[1],l2=b.length*b.length;
   H ta(std::numeric_limits<double>::max()),tb=-ta,na=ta,nb=tb;
   for(double x:{leaf.world.min[0],leaf.world.max[0]})for(double y:{leaf.world.min[1],leaf.world.max[1]}){
    const H px=H(x)-b.step.start[0],py=H(y)-b.step.start[1],t=(dx*px+dy*py)/l2,n=(dx*py-dy*px)/b.length;
    ta=std::min(ta,t);tb=std::max(tb,t);na=std::min(na,n);nb=std::max(nb,n);}
   ta-=xy/b.length;tb+=xy/b.length;na-=xy;nb+=xy;if(tb<-guard || ta>front+guard)continue;
   const H a=std::max(H(0),ta),c=std::min(front,tb),ha=b.h0+b.dh*a,hb=b.h0+b.dh*c,hmin=std::min(ha,hb),hmax=std::max(ha,hb);
   const H dz=b.step.end[2]-b.step.start[2],za=b.step.start[2]+dz*a,zb=b.step.start[2]+dz*c,nmin=na<=0 && nb>=0 ? H(0) : std::min(abs(na),abs(nb));
   bool outside=H(leaf.world.min[2])-z>std::max(za,zb)+guard || H(leaf.world.max[2])+z<std::min(za-ha,zb-hb)-guard;
   if(b.kind==MaterialSectionKind::Rectangle)outside|=nmin>b.area/hmin/2+guard;
   else {const H core=(b.area/hmin-pi*hmin/4)/2,ca=za-ha/2,cb=zb-hb/2;
    const H lateral=std::max(H(0),nmin-core),vertical=std::max({H(0),H(leaf.world.min[2])-z-std::max(ca,cb),std::min(ca,cb)-H(leaf.world.max[2])-z});
    outside|=lateral*lateral+vertical*vertical>hmax*hmax/4+guard;}
   REQUIRE(outside);
  }
 }
 REQUIRE(groups.size()==proof.record_count*(s.head.size()+1));
 for(const auto &[key,leaves]:groups){
  MaterialRegion root=leaves.front()->local;for(const auto *leaf:leaves)for(size_t a=0;a<3;++a){root.min[a]=std::min(root.min[a],leaf->local.min[a]);root.max[a]=std::max(root.max[a],leaf->local.max[a]);}
  if(key.second){REQUIRE(root.min==s.head[key.second-1].local.min);REQUIRE(root.max==s.head[key.second-1].local.max);}
  else {for(size_t a=0;a<2;++a){REQUIRE(H(root.min[a])<=H(s.tip_center[a])-H(s.outer_radius_mm));REQUIRE(H(root.max[a])>=H(s.tip_center[a])+H(s.outer_radius_mm));}
   REQUIRE(root.min[2]==s.tip_center[2]);REQUIRE(root.max[2]==s.tip_center[2]);}
  H measure=0;for(size_t i=0;i<leaves.size();++i){const auto &leaf=*leaves[i];H part=H(leaf.progress.upper)-H(leaf.progress.lower);
   for(size_t a=0;a<3;++a){REQUIRE(leaf.local.min[a]>=root.min[a]);REQUIRE(leaf.local.max[a]<=root.max[a]);
    if(root.max[a]>root.min[a])part*=(H(leaf.local.max[a])-H(leaf.local.min[a]))/(H(root.max[a])-H(root.min[a]));}
   measure+=part;
   for(size_t j=0;j<i;++j){const auto &other=*leaves[j];bool overlap=std::max(leaf.progress.lower,other.progress.lower)<std::min(leaf.progress.upper,other.progress.upper);
    for(size_t a=0;a<3;++a)if(root.max[a]>root.min[a])overlap&=std::max(leaf.local.min[a],other.local.min[a])<std::min(leaf.local.max[a],other.local.max[a]);REQUIRE_FALSE(overlap);}
  }
  REQUIRE(abs(measure-1)<H("1e-28"));
 }
}
inline void check_final_travel(const nptop_verify::LinearTravelSnapshot &proof){check_final_motion(proof);}
// Independent rectangle witness check. A complete world enclosure lies in a
// largest-dose section with one admissible common along/transverse/Z translation. No
// product membership query and no future/full-end substitution is used.
inline void check_deposition_witness(const nptop_verify::LinearMaterialSnapshot &m,const nptop_verify::LinearTravelScene &scene,
 const nptop_verify::LinearTravelWitness &w)
{
 using namespace nptop_verify;using H=FinalMaterialHigh;const auto steps=replay_material_steps(m);const H guard("1e-20"),t(w.progress.lower);
 REQUIRE(w.progress.lower==w.progress.upper);REQUIRE(w.material_event);REQUIRE(w.component<=scene.head.size());
 const auto found=std::find_if(m.declarations.begin(),m.declarations.end(),[&](const auto &r){return r.event_id==*w.material_event;});REQUIRE(found!=m.declarations.end());
 const size_t i=size_t(found-m.declarations.begin());REQUIRE(i<=w.record);REQUIRE(found->kind==MaterialEventKind::Deposit);
 const H front=i==w.record ? t : H(1);REQUIRE(front>0);const auto section=*found->section;REQUIRE(section.kind==MaterialSectionKind::Rectangle);
 const auto &b=steps[i],&move=steps[w.record];const H dx=b.end[0]-b.start[0],dy=b.end[1]-b.start[1],length=sqrt(dx*dx+dy*dy);
 H lo(std::numeric_limits<double>::max()),hi=-lo,na=lo,nb=hi;
 for(double x:{w.point.min[0],w.point.max[0]})for(double y:{w.point.min[1],w.point.max[1]}){
  const H px=H(x)-b.start[0],py=H(y)-b.start[1],u=(dx*px+dy*py)/(length*length),n=(dx*py-dy*px)/length;
  lo=std::min(lo,u);hi=std::max(hi,u);na=std::min(na,n);nb=std::max(nb,n);}
 const H h0(section.gap_begin_mm),dh=H(section.gap_end_mm)-h0,dz=b.end[2]-b.start[2];
 const H dose=(b.e*acos(H(-1))*H(m.rates->policy.filament_diameter)*H(m.rates->policy.filament_diameter)/4/H(m.rates->policy.flow))*(1+H(m.policy.relative_dose_error))+H(m.policy.absolute_dose_error_mm3);
 const H xy=H(m.policy.outer_xy_growth_mm)+H(m.policy.numerical_coordinate_error_mm),growth=H(m.policy.outer_z_growth_mm)+H(m.policy.numerical_coordinate_error_mm);bool inside=false;
 for(int along:{-1,0,1})for(int transverse:{-1,0,1})for(int vertical:{-1,0,1}){
  const H a=lo+along*xy/length,c=hi+along*xy/length;
  if(a<-guard || c>front+guard)continue;
  const H ha=h0+dh*a,hb=h0+dh*c,za=b.start[2]+dz*a,zb=b.start[2]+dz*c;
  const H n=std::max(abs(na+transverse*xy),abs(nb+transverse*xy));
  inside|=n<=dose/length/std::max(ha,hb)/2+guard && H(w.point.max[2])+vertical*growth<=std::min(za,zb)+guard &&
   H(w.point.min[2])+vertical*growth>=std::max(za-ha,zb-hb)-guard;
 }
 REQUIRE(inside);
 MaterialRegion local;for(size_t a=0;a<3;++a){const H pose=move.start[a]+t*(move.end[a]-move.start[a]);
  const H a0=H(w.point.min[a])-pose,a1=H(w.point.max[a])-pose;
  if(w.component){REQUIRE(a0<=H(scene.head[w.component-1].local.max[a])+guard);REQUIRE(a1>=H(scene.head[w.component-1].local.min[a])-guard);}
  else if(a==2){REQUIRE(a0<=H(scene.tip_center[2])+guard);REQUIRE(a1>=H(scene.tip_center[2])-guard);}
  local.min[a]=a0.convert_to<double>();local.max[a]=a1.convert_to<double>();}
 if(!w.component){H near=0,far=0;for(size_t a=0;a<2;++a){const H a0=H(local.min[a])-H(scene.tip_center[a]),a1=H(local.max[a])-H(scene.tip_center[a]);
   near+=a0<=0 && a1>=0 ? H(0) : std::min(a0*a0,a1*a1);far+=std::max(a0*a0,a1*a1);}
  REQUIRE(near<=H(scene.outer_radius_mm)*H(scene.outer_radius_mm)+guard);REQUIRE(far>=H(scene.opening_radius_mm)*H(scene.opening_radius_mm)-guard);}
}
}
