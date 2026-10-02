#pragma once
#include <catch2/catch_test_macros.hpp>
#include <nonplanar_verify/LinearMaterial.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <sstream>

namespace nptop_test {
// Separate 113-bit decimal replay and whole-slice inequalities. No verifier or
// planner parsing, projection, length, dose or shape implementation is called.
inline void check_joined_lower(const nptop_verify::JoinedMaterialCoverSnapshot &cover)
{
 using namespace nptop_verify;using H=boost::multiprecision::cpp_bin_float_quad;
 struct Step{std::array<H,3> start,end;H e;};std::vector<Step> steps;
 const auto &prefix=*cover.source->source;const auto &material=*prefix.source;const auto &rates=*material.rates;
 std::array<H,3> position;for(size_t i=0;i<3;++i)position[i]=H(rates.initial_position[i]);
 std::istringstream input(rates.bytes);input.imbue(std::locale::classic());std::string line;
 for(unsigned i=0;i<4;++i)REQUIRE(bool(std::getline(input,line)));
 while(std::getline(input,line)){
  std::istringstream words(line);words.imbue(std::locale::classic());std::string word;words>>word;
  REQUIRE((word=="G1" || word=="G4"));Step step{position,position,0};
  while(words>>word){const auto axis=std::string("XYZ").find(word[0]);const H value(word.substr(1));
   if(axis!=std::string::npos)step.end[axis]=value;else if(word[0]=='E')step.e=value;
  }
  steps.push_back(step);position=step.end;REQUIRE(bool(std::getline(input,line)));REQUIRE(line=="M400");
 }
 REQUIRE(steps.size()==material.declarations.size());
 const auto &policy=material.policy;const H xy=H(policy.inner_xy_loss_mm)+H(policy.numerical_coordinate_error_mm),z=H(policy.inner_z_loss_mm)+H(policy.numerical_coordinate_error_mm);
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
  REQUIRE(lo>0);REQUIRE(hi<end);H covered=0;
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
   const H dose=std::max(H(0),s.e*filament_area/H(rates.policy.flow)*(1-H(policy.relative_dose_error))-H(policy.absolute_dose_error_mm3)),area=dose/span;
   const H ta=s.start[2]+(s.end[2]-s.start[2])*t0,tb=s.start[2]+(s.end[2]-s.start[2])*t1;
   if(section.kind==MaterialSectionKind::Rectangle){
    REQUIRE(normal<area/hmax/2);REQUIRE(H(leaf.region.max[2])+z<std::min(ta,tb));REQUIRE(H(leaf.region.min[2])-z>std::max(ta-ha,tb-hb));
   }else{
    const H core=(area/hmax-pi*hmax/4)/2,radius=hmin/2,ca=ta-ha/2,cb=tb-hb/2;
    const H lateral=std::max(H(0),normal-core),vertical=std::max(abs(H(leaf.region.min[2])-z-std::max(ca,cb)),abs(H(leaf.region.max[2])+z-std::min(ca,cb)));
    REQUIRE(core>0);REQUIRE(lateral*lateral+vertical*vertical<radius*radius);
   }
   covered+=b-a;
  }
  REQUIRE(abs(covered-(hi-lo))<H("1e-28"));
 }
}
}
