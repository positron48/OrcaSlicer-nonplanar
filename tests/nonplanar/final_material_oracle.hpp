#pragma once
#include <catch2/catch_test_macros.hpp>
#include <nonplanar_verify/LinearMaterial.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <sstream>

namespace nptop_test {
// Separate 113-bit decimal replay and whole-slice inequalities. No verifier or
// planner parsing, projection, length, dose or shape implementation is called.
using FinalMaterialHigh=boost::multiprecision::cpp_bin_float_quad;
struct FinalMaterialStep {std::array<FinalMaterialHigh,3> start,end;FinalMaterialHigh e;};
inline std::vector<FinalMaterialStep> replay_material_steps(const nptop_verify::LinearMaterialSnapshot &material)
{
 using H=FinalMaterialHigh;const auto &rates=*material.rates;std::vector<FinalMaterialStep> steps;
 std::array<H,3> position;for(size_t i=0;i<3;++i)position[i]=H(rates.initial_position[i]);
 std::istringstream input(rates.bytes);input.imbue(std::locale::classic());std::string line;
 for(unsigned i=0;i<4;++i)REQUIRE(bool(std::getline(input,line)));
 while(std::getline(input,line)){
  std::istringstream words(line);words.imbue(std::locale::classic());std::string word;words>>word;
  REQUIRE((word=="G1" || word=="G4"));FinalMaterialStep step{position,position,0};
  while(words>>word){const auto axis=std::string("XYZ").find(word[0]);const H value(word.substr(1));
   if(axis!=std::string::npos)step.end[axis]=value;else if(word[0]=='E')step.e=value;
  }
  steps.push_back(step);position=step.end;REQUIRE(bool(std::getline(input,line)));REQUIRE(line=="M400");
 }
 REQUIRE(steps.size()==material.declarations.size());return steps;
}
inline void check_joined_sections(const nptop_verify::JoinedMaterialCoverSnapshot &cover,const std::vector<FinalMaterialStep> *replayed=nullptr)
{
 using namespace nptop_verify;REQUIRE((cover.representation==MaterialRepresentation::Nominal || cover.representation==MaterialRepresentation::Lower));const bool nominal=cover.representation==MaterialRepresentation::Nominal;using H=FinalMaterialHigh;
 const auto &prefix=*cover.source->source;const auto &material=*prefix.source;const auto &rates=*material.rates;
 const auto owned=replayed ? std::vector<FinalMaterialStep>{} : replay_material_steps(material);const auto &steps=replayed ? *replayed : owned;
 const auto &policy=material.policy;const H xy=nominal ? H(0) : H(policy.inner_xy_loss_mm)+H(policy.numerical_coordinate_error_mm),z=nominal ? H(0) : H(policy.inner_z_loss_mm)+H(policy.numerical_coordinate_error_mm);
 const H pi=acos(H(-1)),filament_area=pi*H(rates.policy.filament_diameter)*H(rates.policy.filament_diameter)/4;
 for(const auto &leaf:cover.leaves){
  REQUIRE(leaf.run_index<cover.source->runs.size());const auto &run=cover.source->runs[leaf.run_index];const auto &origin=steps[run.first_record];
  REQUIRE(run.last_record<=prefix.completed_records);if(run.last_record==prefix.completed_records)REQUIRE(prefix.current_progress>0);
  const H dx=origin.end[0]-origin.start[0],dy=origin.end[1]-origin.start[1],length=sqrt(dx*dx+dy*dy);
  const auto along=[&](const std::array<H,3> &p){return (dx*(p[0]-origin.start[0])+dy*(p[1]-origin.start[1]))/length;};
  const auto progress=[&](size_t i){return i<prefix.completed_records ? H(1) : H(prefix.current_progress);};
  H lo=std::numeric_limits<double>::max(),hi=-lo,normal=0;
  for(double x:{leaf.region.min[0],leaf.region.max[0]})for(double y:{leaf.region.min[1],leaf.region.max[1]}){
   const std::array<H,3> p{H(x),H(y),0};const H t=along(p);lo=std::min(lo,t);hi=std::max(hi,t);
   normal=std::max(normal,abs((dx*(H(y)-origin.start[1])-dy*(H(x)-origin.start[0]))/length));
  }
  lo-=xy;hi+=xy;normal+=xy;const auto &last=steps[run.last_record];
  const H end=along(last.start)+(along(last.end)-along(last.start))*progress(run.last_record);
  REQUIRE((nominal ? lo>=0 : lo>0));REQUIRE((nominal ? hi<=end : hi<end));H covered=0;
  for(size_t i=run.first_record;i<=run.last_record;++i){
   const auto &s=steps[i];const H start=along(s.start),span=along(s.end)-start,laid=start+span*progress(i);
   REQUIRE(material.declarations[i].kind==MaterialEventKind::Deposit);REQUIRE(material.declarations[i].section);
   REQUIRE(material.declarations[i].section->kind==material.declarations[run.first_record].section->kind);
   if(i>run.first_record)REQUIRE(s.start==steps[i-1].end);
   const H sx=s.end[0]-s.start[0],sy=s.end[1]-s.start[1];REQUIRE(abs(dx*sy-dy*sx)<H("1e-28"));REQUIRE(dx*sx+dy*sy>0);
   const H a=std::max(lo,start),b=std::min(hi,laid);if(a>=b)continue;
   REQUIRE(s.e>0);REQUIRE(material.declarations[i].section);const auto section=*material.declarations[i].section;
   const H t0=(a-start)/span,t1=(b-start)/span,h0(section.gap_begin_mm),dh=H(section.gap_end_mm)-h0;
   const H ha=h0+dh*t0,hb=h0+dh*t1,hmin=std::min(ha,hb),hmax=std::max(ha,hb);
   const H command_dose=s.e*filament_area/H(rates.policy.flow),dose=nominal ? command_dose : std::max(H(0),command_dose*(1-H(policy.relative_dose_error))-H(policy.absolute_dose_error_mm3)),area=dose/span;
   const H ta=s.start[2]+(s.end[2]-s.start[2])*t0,tb=s.start[2]+(s.end[2]-s.start[2])*t1;
   if(section.kind==MaterialSectionKind::Rectangle){
    REQUIRE((nominal ? normal<=area/hmax/2 : normal<area/hmax/2));REQUIRE((nominal ? H(leaf.region.max[2])<=std::min(ta,tb) : H(leaf.region.max[2])+z<std::min(ta,tb)));REQUIRE((nominal ? H(leaf.region.min[2])>=std::max(ta-ha,tb-hb) : H(leaf.region.min[2])-z>std::max(ta-ha,tb-hb)));
   }else{
    const H core=(area/hmax-pi*hmax/4)/2,radius=hmin/2,ca=ta-ha/2,cb=tb-hb/2;
    const H lateral=std::max(H(0),normal-core),vertical=std::max(abs(H(leaf.region.min[2])-z-std::max(ca,cb)),abs(H(leaf.region.max[2])+z-std::min(ca,cb)));
    REQUIRE(core>0);REQUIRE((nominal ? lateral*lateral+vertical*vertical<=radius*radius : lateral*lateral+vertical*vertical<radius*radius));
   }
   covered+=b-a;
  }
  REQUIRE(abs(covered-(hi-lo))<H("1e-28"));
 }
}
inline void check_joined_lower(const nptop_verify::JoinedMaterialCoverSnapshot &cover,const std::vector<FinalMaterialStep> *steps=nullptr)
{REQUIRE(cover.representation==nptop_verify::MaterialRepresentation::Lower);check_joined_sections(cover,steps);}
inline void check_joined_nominal(const nptop_verify::JoinedMaterialCoverSnapshot &cover,const std::vector<FinalMaterialStep> *steps=nullptr)
{REQUIRE(cover.representation==nptop_verify::MaterialRepresentation::Nominal);check_joined_sections(cover,steps);}

// Independent complete-box exclusion from every actual old nominal packet.
inline void check_nominal_empty(const nptop_verify::LinearMaterialPrefixSnapshot &prefix,const nptop_verify::MaterialRegion &box,
 const std::vector<FinalMaterialStep> &steps)
{
 using H=FinalMaterialHigh;using namespace nptop_verify;const auto &m=*prefix.source;
 const H area_scale=acos(H(-1))*H(m.rates->policy.filament_diameter)*H(m.rates->policy.filament_diameter)/4/H(m.rates->policy.flow),pi=acos(H(-1));
 for(size_t i=0;i<prefix.completed_records+(prefix.current_progress>0);++i){if(m.declarations[i].kind!=MaterialEventKind::Deposit)continue;
  const auto &s=steps[i];const H dx=s.end[0]-s.start[0],dy=s.end[1]-s.start[1],l2=dx*dx+dy*dy,length=sqrt(l2);
  H tlo=std::numeric_limits<double>::max(),thi=-tlo,nlo=tlo,nhi=thi;
  for(double x:{box.min[0],box.max[0]})for(double y:{box.min[1],box.max[1]}){const H px=H(x)-s.start[0],py=H(y)-s.start[1],t=(dx*px+dy*py)/l2,n=(dx*py-dy*px)/length;
   tlo=std::min(tlo,t);thi=std::max(thi,t);nlo=std::min(nlo,n);nhi=std::max(nhi,n);}
  const H progress=i<prefix.completed_records ? H(1) : H(prefix.current_progress);if(thi<0 || tlo>progress)continue;
  const H a=std::max(H(0),tlo),b=std::min(progress,thi),dz=s.end[2]-s.start[2];const auto sec=*m.declarations[i].section;
  const H h0(sec.gap_begin_mm),dh=H(sec.gap_end_mm)-h0,ha=h0+dh*a,hb=h0+dh*b,hmin=std::min(ha,hb),hmax=std::max(ha,hb);
  const H ta=s.start[2]+dz*a,tb=s.start[2]+dz*b,nmin=nlo<=0 && nhi>=0 ? H(0) : std::min(abs(nlo),abs(nhi)),area=s.e*area_scale/length;
  bool outside=H(box.min[2])>std::max(ta,tb) || H(box.max[2])<std::min(ta-ha,tb-hb);
  if(sec.kind==MaterialSectionKind::Rectangle)outside|=nmin>area/hmin/2;
  else {const H core=(area/hmin-pi*hmin/4)/2,ca=ta-ha/2,cb=tb-hb/2,center_min=std::min(ca,cb),center_max=std::max(ca,cb);
   const H lateral=std::max(H(0),nmin-core),vertical=std::max({H(0),H(box.min[2])-center_max,center_min-H(box.max[2])});outside|=lateral*lateral+vertical*vertical>hmax*hmax/4;}
  REQUIRE(outside);
 }
}
inline void check_complete_run_support(const nptop_verify::LinearRunSupportSnapshot &cover)
{
 using H=FinalMaterialHigh;using namespace nptop_verify;const auto &prefix=*cover.source->source;const auto &m=*prefix.source;const auto steps=replay_material_steps(m);
 const auto &run=cover.source->runs[cover.run_index];REQUIRE(cover.support->source->completed_records==run.first_record);REQUIRE(cover.support->source->current_progress==0);
 const H cross(cover.policy.cross_slope),error=4*H(m.policy.numerical_coordinate_error_mm);REQUIRE(H(cover.query_error_mm)>=error);REQUIRE(3*error*error<=H(.05)*H(.05));
 for(size_t record=run.first_record;record<=run.last_record;++record){
  const auto &s=steps[record];const H dx=s.end[0]-s.start[0],dy=s.end[1]-s.start[1],dz=s.end[2]-s.start[2],length=sqrt(dx*dx+dy*dy),ux=dx/length,uy=dy/length,parallel=dz/length;
  const H gx=parallel*ux-cross*uy,gy=parallel*uy+cross*ux,norm=sqrt(1+gx*gx+gy*gy);const std::array<H,3> normal{gx/norm,gy/norm,-1/norm};
  const H progress=record<prefix.completed_records ? H(1) : H(prefix.current_progress);H area=0;size_t tiles=0;double half=0;
  for(const auto &piece:prefix.pieces)if(piece.record==record)half=piece.nominal_width.upper/2;REQUIRE(half>0);
  const auto section=*m.declarations[record].section;const H h0(section.gap_begin_mm),h1=h0+progress*(H(section.gap_end_mm)-h0),hmin=std::min(h0,h1),pi=acos(H(-1));
  const H area_scale=pi*H(m.rates->policy.filament_diameter)*H(m.rates->policy.filament_diameter)/4/H(m.rates->policy.flow),actual_width=s.e*area_scale/length/hmin+(section.kind==MaterialSectionKind::RoundedRectangle ? (1-pi/4)*hmin : H(0));
  REQUIRE(2*H(half)>=actual_width);
  for(size_t i=0;i<cover.leaves.size();++i){const auto &leaf=cover.leaves[i];if(leaf.target_record!=record)continue;++tiles;
   REQUIRE(leaf.progress.lower>=0);REQUIRE(H(leaf.progress.upper)<=progress);REQUIRE(leaf.transverse.lower>=-half);REQUIRE(leaf.transverse.upper<=half);
   REQUIRE(leaf.lower_anchor->source==cover.support);REQUIRE(leaf.nominal_terminal->source==cover.support);
   check_joined_lower(*leaf.lower_anchor,&steps);check_joined_nominal(*leaf.nominal_terminal,&steps);
   check_nominal_empty(*cover.support->source,leaf.vertical_near,steps);check_nominal_empty(*cover.support->source,leaf.normal_near,steps);
   for(bool n:{false,true})for(bool terminal:{false,true}){const auto &box=n ? (terminal ? leaf.normal_terminal : leaf.normal_near) : (terminal ? leaf.vertical_terminal : leaf.vertical_near);
    const double lo=terminal ? (n ? cover.policy.normal_max : cover.policy.vertical_max) : 0,hi=terminal ? lo : (n ? cover.policy.normal_min : cover.policy.vertical_min);
    for(double t:{leaf.progress.lower,leaf.progress.upper})for(double transverse:{leaf.transverse.lower,leaf.transverse.upper})for(double d:{lo,hi}){
     std::array<H,3> p{s.start[0]+dx*H(t)-uy*H(transverse),s.start[1]+dy*H(t)+ux*H(transverse),s.start[2]+dz*H(t)+cross*H(transverse)};
     for(size_t axis=0;axis<3;++axis){p[axis]+=(n ? normal[axis] : axis==2 ? H(-1) : H(0))*H(d);REQUIRE(H(box.min[axis])<=p[axis]-error);REQUIRE(H(box.max[axis])>=p[axis]+error);}
    }
   }
   area+=(H(leaf.progress.upper)-H(leaf.progress.lower))*(H(leaf.transverse.upper)-H(leaf.transverse.lower));
   for(size_t j=0;j<i;++j){const auto &other=cover.leaves[j];if(other.target_record!=record)continue;
    REQUIRE((leaf.progress.upper<=other.progress.lower || other.progress.upper<=leaf.progress.lower || leaf.transverse.upper<=other.transverse.lower || other.transverse.upper<=leaf.transverse.lower));}
  }
  REQUIRE(tiles>0);REQUIRE(abs(area-progress*2*H(half))<H("1e-28"));
 }
}
}
