#include <catch2/catch_test_macros.hpp>
#include <nonplanar_verify/LinearMaterial.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <cfenv>
using namespace nptop_verify;
namespace {
using High=boost::multiprecision::cpp_bin_float_quad;
const std::string text="G90\nM83\nM400\nM204 S4\nG1 X3 Y4 Z.1 E.2 F60\nM400\nG1 E-.8 F120\nM400\nG1 X3 Y4 Z.15 F60\nM400\nG1 E.8 F120\nM400\nG1 X6 Y8 Z.2 E.2 F60\nM400\nG4 P10\nM400\nG1 E-.4 F120\nM400\n";
LinearRatePolicy rate_policy()
{
 LinearRatePolicy p;p.profile_id=11;p.revision=1;p.position_min={-100,-100,-100};p.position_max={100,100,100};
 p.axis_speed={10,10,2};p.drive_speed={15,15,2};p.axis_acceleration={20,20,5};p.drive_acceleration={30,30,5};
 p.initial_acceleration=20;p.filament_diameter=1.75;p.flow=1.17;p.filament_speed=5;p.filament_acceleration=10;
 p.max_retraction=2;p.max_volume_rate=2;p.max_cross_section=.5;p.max_event_rate=100;return p;
}
LinearMaterialPolicy material_policy()
{
 LinearMaterialPolicy p;p.model_id=21;p.policy_id=22;p.revision=1;p.source_revision=7;p.source_fingerprint=std::string(64,'a');
 p.outer_xy_growth_mm=.02;p.outer_z_growth_mm=.01;p.inner_xy_loss_mm=.03;p.inner_z_loss_mm=.02;p.numerical_coordinate_error_mm=1e-7;
 p.max_coordinate_delta_mm=1e-6;p.max_nominal_delta_mm3=1e-9;p.max_total_nominal_delta_mm3=1e-8;p.max_filament_delta_mm=1e-9;
 p.relative_dose_error=.05;p.absolute_dose_error_mm3=1e-5;return p;
}
std::vector<MaterialDeclaration> declarations()
{
 const double volume=(High(".2")*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)).convert_to<double>();
 return {{1,0,MaterialEventKind::Deposit,{0,0,0},{3,4,.1},volume,0,MaterialSection{MaterialSectionKind::Rectangle,.2,.3}},
 {2,1,MaterialEventKind::Retraction,{3,4,.1},{3,4,.1},0,.8,{}},
 {3,2,MaterialEventKind::Travel,{3,4,.1},{3,4,.15},0,0,{}},
 {4,3,MaterialEventKind::Restore,{3,4,.15},{3,4,.15},0,.8,{}},
 {5,4,MaterialEventKind::Deposit,{3,4,.15},{6,8,.2},volume,0,MaterialSection{MaterialSectionKind::RoundedRectangle,.2,.3}},
 {6,5,MaterialEventKind::Dwell,{6,8,.2},{6,8,.2},0,0,{}},
 {7,6,MaterialEventKind::Retraction,{6,8,.2},{6,8,.2},0,.4,{}}};
}
void encloses(RateBounds b,High v){REQUIRE(High(b.lower)<=v);REQUIRE(v<=High(b.upper));}
}
TEST_CASE("B12 final material reconstructs complete final dose sections and only actual pressure free prefixes", "[Nonplanar][B12][FinalByteMaterial]")
{
 const auto rates=verify_linear_rates(text,{0,0,0},rate_policy());REQUIRE(rates.snapshot);
 const auto material=reconstruct_linear_material(rates.snapshot,declarations(),material_policy());INFO(material.reason);
 REQUIRE(material.status==RateStatus::Pass);REQUIRE(material.snapshot);const auto &m=*material.snapshot;
 REQUIRE(m.rates==rates.snapshot);REQUIRE(m.beads.size()==7);REQUIRE(m.beads[0]);REQUIRE(m.beads[4]);
 for(size_t i : {1,2,3,5,6})REQUIRE_FALSE(m.beads[i]);
 const High v=High(".2")*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow);
 encloses(m.nominal_volume,2*v);encloses(m.beads[0]->xy_length,High(5));
 encloses(m.beads[0]->nominal_width, v/5/High(material.snapshot->declarations[0].section->gap_begin_mm));
 encloses(m.beads[0]->nominal_width, v/5/High(material.snapshot->declarations[0].section->gap_end_mm));
 const High h=High(material.snapshot->declarations[4].section->gap_begin_mm);
 encloses(m.beads[4]->nominal_width,v/5/h+(1-acos(High(-1))/4)*h);
 REQUIRE(m.delivered_volume.lower<m.nominal_volume.lower);REQUIRE(m.delivered_volume.upper>m.nominal_volume.upper);
 REQUIRE(material.evaluations>rates.evaluations);REQUIRE(m.maximum_nominal_delta_mm3.upper<=1e-9);
 const auto empty=linear_material_at(material.snapshot,0,0);REQUIRE(empty.snapshot);REQUIRE(empty.snapshot->pieces.empty());
 const auto half=linear_material_at(material.snapshot,0,.5);REQUIRE(half.snapshot);REQUIRE(half.snapshot->pieces.size()==1);
 encloses(half.snapshot->nominal_volume,v/2);encloses(half.snapshot->pieces[0].end[0],High("1.5"));
 const auto travel=linear_material_at(material.snapshot,2,.5);REQUIRE(travel.snapshot);REQUIRE(travel.snapshot->pieces.size()==1);encloses(travel.snapshot->nominal_volume,v);
 const auto later=linear_material_at(material.snapshot,4,.5);REQUIRE(later.snapshot);REQUIRE(later.snapshot->pieces.size()==2);encloses(later.snapshot->nominal_volume,v*High("1.5"));
 const auto complete=linear_material_at(material.snapshot,7,0);REQUIRE(complete.snapshot);REQUIRE(complete.snapshot->pieces.size()==2);encloses(complete.snapshot->nominal_volume,2*v);
 REQUIRE_FALSE(linear_material_at(material.snapshot,7,.1).snapshot);REQUIRE_FALSE(linear_material_at(material.snapshot,8,0).snapshot);
}
TEST_CASE("B12 final material refuses mutated dose poses roles incomplete owners and unsupported sections", "[Nonplanar][B12][FinalByteMaterial]")
{
 const auto rates=verify_linear_rates(text,{0,0,0},rate_policy());REQUIRE(rates.snapshot);
 for(int mode=0;mode<10;++mode){auto rows=declarations();auto policy=material_policy();
  if(mode==0)rows[0].expected_nominal_volume_mm3+=1;if(mode==1)rows[0].end[0]+=1;if(mode==2)rows[1].event_id=1;
  if(mode==3)rows[0].kind=MaterialEventKind::Travel;if(mode==4)rows[0].section->gap_begin_mm=0;
  if(mode==5)rows.pop_back();if(mode==6)policy.operator_confirmed_claim=true;if(mode==7)policy.relative_dose_error=1;
  if(mode==8)policy.max_total_nominal_delta_mm3=0;if(mode==9)rows[1].section=rows[0].section;
  const auto r=reconstruct_linear_material(rates.snapshot,rows,policy);INFO(mode<<" "<<r.reason);REQUIRE_FALSE(r.snapshot);REQUIRE(r.status!=RateStatus::Pass);
 }
 auto rows=declarations();rows[4].section->gap_end_mm=1;
 REQUIRE_FALSE(reconstruct_linear_material(rates.snapshot,rows,material_policy()).snapshot);
 REQUIRE_FALSE(reconstruct_linear_material({},declarations(),material_policy()).snapshot);
}
TEST_CASE("B12 final material ownership stale budgets rounding and final publication cannot return partial geometry", "[Nonplanar][B12][FinalByteMaterial]")
{
 const auto rates=verify_linear_rates(text,{0,0,0},rate_policy());REQUIRE(rates.snapshot);
 for(int mode=0;mode<6;++mode){LinearMaterialLimits limits;
  if(mode==0)limits.max_events=1;if(mode==1)limits.max_evaluations=rates.evaluations+1;
  if(mode==2)limits.cancelled=[] {return true;};if(mode==3)limits.is_current=[](uint64_t,uint64_t){return false;};
  if(mode==4)limits.is_source_current=[](uint64_t){return false;};if(mode==5)limits.cancelled=[] {std::fesetround(FE_UPWARD);return false;};
  const auto r=reconstruct_linear_material(rates.snapshot,declarations(),material_policy(),limits);std::fesetround(FE_TONEAREST);
  INFO(mode<<" "<<r.reason);REQUIRE(r.status==RateStatus::Unknown);REQUIRE_FALSE(r.snapshot);
 }
 auto rows=declarations();auto policy=material_policy();LinearMaterialLimits limits;
 limits.cancelled=[&]{rows.clear();policy.relative_dose_error=1;limits.max_events=0;return false;};
 const auto owned=reconstruct_linear_material(rates.snapshot,rows,policy,limits);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->declarations.size()==7);
 limits={};size_t calls=0;limits.cancelled=[&]{++calls;return false;};REQUIRE(reconstruct_linear_material(rates.snapshot,declarations(),material_policy(),limits).snapshot);
 const auto last=calls;calls=0;limits.cancelled=[&]{return ++calls==last;};REQUIRE_FALSE(reconstruct_linear_material(rates.snapshot,declarations(),material_policy(),limits).snapshot);
}
