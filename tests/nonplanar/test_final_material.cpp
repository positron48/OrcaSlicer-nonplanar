#include <catch2/catch_test_macros.hpp>
#include <nonplanar_verify/LinearMaterial.hpp>
#include <nonplanar_verify/TravelJson.hpp>
#include <nonplanar_verify/FormingContactJson.hpp>
#include <nonplanar_verify/SupportedDepositionJson.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <cfenv>
#include <iomanip>
#include <set>
#include <type_traits>
#include "final_material_oracle.hpp"
#include "final_travel_oracle.hpp"
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
// Independent 113-bit whole-box inequalities from the known decimal fixture
// poses and binary section/policy inputs. No verifier geometry helpers.
void independent_solid_box(const MaterialRegion &box,MaterialRepresentation rep,const MaterialSection &section,
 const std::array<High,3> &start,const std::array<High,3> &end)
{
 const auto p=material_policy();const High dx=end[0]-start[0],dy=end[1]-start[1],length=sqrt(dx*dx+dy*dy);
 const High pi=acos(High(-1)),volume=High(".2")*pi*High("1.75")*High("1.75")/4/High(rate_policy().flow);
 const High dose=rep==MaterialRepresentation::Lower ? volume*(1-High(p.relative_dose_error))-High(p.absolute_dose_error_mm3) :
  rep==MaterialRepresentation::Upper ? volume*(1+High(p.relative_dose_error))+High(p.absolute_dose_error_mm3) : volume;
 High tlo=2,thi=-1,normal=0;
 for(double x:{box.min[0],box.max[0]})for(double y:{box.min[1],box.max[1]}) {
  const High px=High(x)-start[0],py=High(y)-start[1],t=(dx*px+dy*py)/(length*length);
  tlo=std::min(tlo,t);thi=std::max(thi,t);normal=std::max(normal,abs((dx*py-dy*px)/length));
 }
 // For Upper, containment in its largest-dose unexpanded solid suffices.
 const High xy=rep==MaterialRepresentation::Lower ? High(p.inner_xy_loss_mm)+High(p.numerical_coordinate_error_mm) : High(0);
 const High z=rep==MaterialRepresentation::Lower ? High(p.inner_z_loss_mm)+High(p.numerical_coordinate_error_mm) : High(0);
 tlo-=xy/length;thi+=xy/length;normal+=xy;REQUIRE(tlo>0);REQUIRE(thi<1);
 const High h0(section.gap_begin_mm),dh=High(section.gap_end_mm)-h0,dz=end[2]-start[2];
 const High ha=h0+dh*tlo,hb=h0+dh*thi,hmin=std::min(ha,hb),hmax=std::max(ha,hb),area=dose/length;
 const High ta=start[2]+dz*tlo,tb=start[2]+dz*thi;
 if(section.kind==MaterialSectionKind::Rectangle){
  REQUIRE(normal<area/hmax/2);REQUIRE(High(box.max[2])+z<std::min(ta,tb));REQUIRE(High(box.min[2])-z>std::max(ta-ha,tb-hb));
 }else {
  const High core=(area/hmax-pi*hmax/4)/2,radius=hmin/2,ca=ta-ha/2,cb=tb-hb/2;
  const High lateral=std::max(High(0),normal-core),vertical=std::max(abs(High(box.min[2])-z-std::max(ca,cb)),abs(High(box.max[2])+z-std::min(ca,cb)));
  REQUIRE(core>0);REQUIRE(lateral*lateral+vertical*vertical<radius*radius);
 }
}
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

TEST_CASE("B12 final solids classify complete rotated boxes without filling bead bounding corners", "[Nonplanar][B12][FinalByteSolids]")
{
 const auto rates=verify_linear_rates(text,{0,0,0},rate_policy());REQUIRE(rates.snapshot);
 const auto material=reconstruct_linear_material(rates.snapshot,declarations(),material_policy());REQUIRE(material.snapshot);
 const auto prefix=linear_material_at(material.snapshot,1,0);REQUIRE(prefix.snapshot);
 const MaterialRegion inside{{1.49,1.99,-.08},{1.51,2.01,-.07}};
 for(auto rep:{MaterialRepresentation::Nominal,MaterialRepresentation::Upper,MaterialRepresentation::Lower}) {
  const auto r=classify_linear_material(prefix.snapshot,inside,rep);INFO(r.reason);
  REQUIRE(r.membership==MaterialMembership::Inside);REQUIRE(r.event_id==1);REQUIRE(r.evaluations>prefix.evaluations);
  const auto cover=cover_linear_material(prefix.snapshot,inside,rep);REQUIRE(cover.status==RateStatus::Pass);REQUIRE(cover.snapshot);
  REQUIRE(cover.snapshot->source==prefix.snapshot);REQUIRE(cover.snapshot->leaves.size()==1);
  independent_solid_box(inside,rep,*declarations()[0].section,{High(0),High(0),High(0)},{High(3),High(4),High(".1")});
 }
 const MaterialRegion bounding_corner{{.05,3.8,-.08},{.06,3.81,-.07}};
 for(auto rep:{MaterialRepresentation::Nominal,MaterialRepresentation::Upper,MaterialRepresentation::Lower})
  REQUIRE(classify_linear_material(prefix.snapshot,bounding_corner,rep).membership==MaterialMembership::Outside);
 const auto half=linear_material_at(material.snapshot,0,.5);REQUIRE(half.snapshot);
 REQUIRE(classify_linear_material(half.snapshot,{{2.39,3.19,-.04},{2.41,3.21,-.03}},MaterialRepresentation::Upper).membership==MaterialMembership::Outside);
 const auto future=linear_material_at(material.snapshot,0,0);REQUIRE(future.snapshot);
 REQUIRE(cover_linear_material(future.snapshot,inside,MaterialRepresentation::Lower).status==RateStatus::Fail);
}
TEST_CASE("B12 final solid dose shells rounded caps and finite butt erosion stay distinct", "[Nonplanar][B12][FinalByteSolids]")
{
 const auto rates=verify_linear_rates(text,{0,0,0},rate_policy());REQUIRE(rates.snapshot);
 const auto material=reconstruct_linear_material(rates.snapshot,declarations(),material_policy());REQUIRE(material.snapshot);
 const auto prefix=linear_material_at(material.snapshot,5,0);REQUIRE(prefix.snapshot);
 const MaterialRegion rounded_center{{4.49,5.99,.04},{4.51,6.01,.05}};
 REQUIRE(classify_linear_material(prefix.snapshot,rounded_center,MaterialRepresentation::Lower).membership==MaterialMembership::Inside);
 independent_solid_box(rounded_center,MaterialRepresentation::Lower,*declarations()[4].section,
  {High(3),High(4),High(".15")},{High(6),High(8),High(".2")});
 // This lies in the rounded bead's rectangular bounding footprint/top range,
 // but beyond the curved stadium side. A filled AABB would falsely cover it.
 const MaterialRegion rounded_corner{{4.62,5.90,.173},{4.63,5.91,.174}};
 REQUIRE(classify_linear_material(prefix.snapshot,rounded_corner,MaterialRepresentation::Nominal).membership==MaterialMembership::Outside);
 const auto full=linear_material_at(material.snapshot,1,0);REQUIRE(full.snapshot);
 const MaterialRegion near_start{{.003,.004,-.10},{.003,.004,-.10}};
 REQUIRE(classify_linear_material(full.snapshot,near_start,MaterialRepresentation::Nominal).membership==MaterialMembership::Inside);
 REQUIRE(classify_linear_material(full.snapshot,near_start,MaterialRepresentation::Lower).membership==MaterialMembership::Outside);
 const auto tiny=linear_material_at(material.snapshot,0,.001);REQUIRE(tiny.snapshot);
 REQUIRE(classify_linear_material(tiny.snapshot,near_start,MaterialRepresentation::Lower).membership==MaterialMembership::Outside);
}
TEST_CASE("B12 final solid union coverage spans different beads and reports an actual missing box", "[Nonplanar][B12][FinalByteSolids]")
{
 const std::string bytes="G90\nM83\nM400\nM204 S4\nG1 X3 Y0 Z0 E.2 F60\nM400\nG1 X0 Y.25 Z0 F60\nM400\nG1 X3 Y.25 Z0 E.2 F60\nM400\n";
 const double amount=(High(".2")*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)).convert_to<double>();
 std::vector<MaterialDeclaration> rows={{1,0,MaterialEventKind::Deposit,{0,0,0},{3,0,0},amount,0,MaterialSection{MaterialSectionKind::Rectangle,.2,.2}},
 {2,1,MaterialEventKind::Travel,{3,0,0},{0,.25,0},0,0,{}},
 {3,2,MaterialEventKind::Deposit,{0,.25,0},{3,.25,0},amount,0,MaterialSection{MaterialSectionKind::Rectangle,.2,.2}}};
 const auto rates=verify_linear_rates(bytes,{0,0,0},rate_policy());REQUIRE(rates.snapshot);
 const auto material=reconstruct_linear_material(rates.snapshot,rows,material_policy());REQUIRE(material.snapshot);
 const auto prefix=linear_material_at(material.snapshot,3,0);REQUIRE(prefix.snapshot);
 const MaterialRegion joined{{1,-.20,-.13},{2,.45,-.08}};
 REQUIRE(classify_linear_material(prefix.snapshot,joined,MaterialRepresentation::Lower).membership==MaterialMembership::Unknown);
 const auto cover=cover_linear_material(prefix.snapshot,joined,MaterialRepresentation::Lower);INFO(cover.reason);
 REQUIRE(cover.status==RateStatus::Pass);REQUIRE(cover.snapshot);REQUIRE(cover.snapshot->leaves.size()>1);
 High partition_volume=0;bool first=false,second=false;
 for(const auto &leaf:cover.snapshot->leaves){first|=leaf.event_id==1;second|=leaf.event_id==3;
  const High y=leaf.event_id==1 ? High(0) : High(".25");
  independent_solid_box(leaf.region,MaterialRepresentation::Lower,*rows[0].section,{High(0),y,High(0)},{High(3),y,High(0)});
  High volume=1;for(size_t axis=0;axis<3;++axis){REQUIRE(leaf.region.min[axis]>=joined.min[axis]);REQUIRE(leaf.region.max[axis]<=joined.max[axis]);volume*=High(leaf.region.max[axis])-High(leaf.region.min[axis]);}partition_volume+=volume;
 }
 High expected=1;for(size_t axis=0;axis<3;++axis)expected*=High(joined.max[axis])-High(joined.min[axis]);REQUIRE(abs(partition_volume-expected)<High("1e-30"));
 for(size_t i=0;i<cover.snapshot->leaves.size();++i)for(size_t j=0;j<i;++j){bool disjoint=false;
  for(size_t axis=0;axis<3;++axis)disjoint|=cover.snapshot->leaves[i].region.min[axis]>=cover.snapshot->leaves[j].region.max[axis] || cover.snapshot->leaves[j].region.min[axis]>=cover.snapshot->leaves[i].region.max[axis];REQUIRE(disjoint);
 }
 REQUIRE(first);REQUIRE(second);
 JoinedMaterialPolicy join;join.policy_id=31;join.revision=1;
 const auto runs=reconstruct_joined_linear_material(prefix.snapshot,join);REQUIRE(runs.snapshot);REQUIRE(runs.snapshot->runs.size()==2);
 const auto joined_cover=cover_joined_linear_material_lower(runs.snapshot,joined);REQUIRE(joined_cover.snapshot);REQUIRE(joined_cover.snapshot->leaves.size()>1);
 nptop_test::check_joined_lower(*joined_cover.snapshot);
 bool run0=false,run1=false;High joined_volume=0;
 for(const auto &leaf:joined_cover.snapshot->leaves){run0|=leaf.run_index==0;run1|=leaf.run_index==1;High volume=1;
  for(size_t axis=0;axis<3;++axis){REQUIRE(leaf.region.min[axis]>=joined.min[axis]);REQUIRE(leaf.region.max[axis]<=joined.max[axis]);volume*=High(leaf.region.max[axis])-High(leaf.region.min[axis]);}joined_volume+=volume;
 }
 REQUIRE(run0);REQUIRE(run1);REQUIRE(abs(joined_volume-expected)<High("1e-30"));
 for(size_t i=0;i<joined_cover.snapshot->leaves.size();++i)for(size_t j=0;j<i;++j){bool disjoint=false;
  for(size_t axis=0;axis<3;++axis)disjoint|=joined_cover.snapshot->leaves[i].region.min[axis]>=joined_cover.snapshot->leaves[j].region.max[axis] || joined_cover.snapshot->leaves[j].region.min[axis]>=joined_cover.snapshot->leaves[i].region.max[axis];REQUIRE(disjoint);
 }
 REQUIRE(cover_joined_linear_material_lower(runs.snapshot,{{1,-.20,-.13},{2,1,-.08}}).status==RateStatus::Fail);
 JoinedMaterialLimits budget;budget.max_cells=1;REQUIRE(cover_joined_linear_material_lower(runs.snapshot,joined,budget).status==RateStatus::Unknown);
 budget={};budget.max_depth=1;REQUIRE(cover_joined_linear_material_lower(runs.snapshot,joined,budget).status==RateStatus::Unknown);
 const auto missing=cover_linear_material(prefix.snapshot,{{1,-.20,-.13},{2,1,-.08}},MaterialRepresentation::Lower);
 REQUIRE(missing.status==RateStatus::Fail);REQUIRE_FALSE(missing.snapshot);REQUIRE(missing.uncovered);
 REQUIRE(classify_linear_material(prefix.snapshot,*missing.uncovered,MaterialRepresentation::Lower).membership==MaterialMembership::Outside);
 MaterialCoverLimits small;small.max_cells=1;
 REQUIRE(cover_linear_material(prefix.snapshot,joined,MaterialRepresentation::Lower,small).status==RateStatus::Unknown);
}
TEST_CASE("B12 final solid queries own inputs and refuse stale exhausted or late numeric environments", "[Nonplanar][B12][FinalByteSolids]")
{
 const auto rates=verify_linear_rates(text,{0,0,0},rate_policy());REQUIRE(rates.snapshot);
 const auto material=reconstruct_linear_material(rates.snapshot,declarations(),material_policy());REQUIRE(material.snapshot);
 const auto prefix=linear_material_at(material.snapshot,1,0);REQUIRE(prefix.snapshot);
 const MaterialRegion region{{1.49,1.99,-.08},{1.51,2.01,-.07}};
 for(int mode=0;mode<6;++mode){MaterialCoverLimits limits;
  if(mode==0)limits.max_evaluations=prefix.evaluations+1;
  if(mode==1)limits.is_current=[](uint64_t,uint64_t){return false;};
  if(mode==2)limits.is_source_current=[](uint64_t){return false;};
  if(mode==3)limits.is_rate_current=[](uint64_t,uint64_t){return false;};
  if(mode==4)limits.cancelled=[] {return true;};
  if(mode==5)limits.cancelled=[] {std::fesetround(FE_UPWARD);return false;};
  const auto result=cover_linear_material(prefix.snapshot,region,MaterialRepresentation::Lower,limits);std::fesetround(FE_TONEAREST);
  REQUIRE(result.status==RateStatus::Unknown);REQUIRE_FALSE(result.snapshot);REQUIRE_FALSE(result.uncovered);
 }
 auto captured=region;MaterialCoverLimits limits;limits.cancelled=[&]{captured.min[0]=50;limits.max_cells=0;return false;};
 const auto owned=cover_linear_material(prefix.snapshot,captured,MaterialRepresentation::Lower,limits);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->region.min==region.min);
 limits={};size_t calls=0;limits.cancelled=[&]{++calls;return false;};REQUIRE(cover_linear_material(prefix.snapshot,region,MaterialRepresentation::Lower,limits).snapshot);
 const auto last=calls;calls=0;limits.cancelled=[&]{return ++calls==last;};
 REQUIRE_FALSE(cover_linear_material(prefix.snapshot,region,MaterialRepresentation::Lower,limits).snapshot);
 calls=0;limits.cancelled=[&]{if(++calls==last)std::fesetround(FE_UPWARD);return false;};
 const auto late=cover_linear_material(prefix.snapshot,region,MaterialRepresentation::Lower,limits);std::fesetround(FE_TONEAREST);
 REQUIRE(late.status==RateStatus::Unknown);REQUIRE_FALSE(late.snapshot);
 const auto vacant=linear_material_at(material.snapshot,0,0);REQUIRE(vacant.snapshot);
 calls=0;limits.cancelled=[&]{++calls;return false;};
 const auto absent=cover_linear_material(vacant.snapshot,region,MaterialRepresentation::Lower,limits);REQUIRE(absent.status==RateStatus::Fail);REQUIRE(absent.uncovered);
 const auto negative_last=calls;calls=0;limits.cancelled=[&]{if(++calls==negative_last)std::fesetround(FE_UPWARD);return false;};
 const auto stale_negative=cover_linear_material(vacant.snapshot,region,MaterialRepresentation::Lower,limits);std::fesetround(FE_TONEAREST);
 REQUIRE(stale_negative.status==RateStatus::Unknown);REQUIRE_FALSE(stale_negative.uncovered);
 REQUIRE_FALSE(cover_linear_material(prefix.snapshot,{{2,0,0},{1,1,1}},MaterialRepresentation::Lower).snapshot);
 REQUIRE(classify_linear_material({},region,MaterialRepresentation::Lower).membership==MaterialMembership::Unknown);
}
TEST_CASE("B12 final delivered dose bounds change solids in opposite directions independently of growth", "[Nonplanar][B12][FinalByteSolids]")
{
 const auto rates=verify_linear_rates(text,{0,0,0},rate_policy());REQUIRE(rates.snapshot);
 auto policy=material_policy();policy.outer_xy_growth_mm=policy.outer_z_growth_mm=policy.inner_xy_loss_mm=policy.inner_z_loss_mm=policy.numerical_coordinate_error_mm=0;
 const auto material=reconstruct_linear_material(rates.snapshot,declarations(),policy);REQUIRE(material.snapshot);
 const auto prefix=linear_material_at(material.snapshot,1,0);REQUIRE(prefix.snapshot);
 const std::array<double,3> more{1.5-.8*.168,2+.6*.168,-.075},less{1.5-.8*.161,2+.6*.161,-.075};
 REQUIRE(classify_linear_material(prefix.snapshot,{more,more},MaterialRepresentation::Nominal).membership==MaterialMembership::Outside);
 REQUIRE(classify_linear_material(prefix.snapshot,{more,more},MaterialRepresentation::Upper).membership==MaterialMembership::Inside);
 REQUIRE(classify_linear_material(prefix.snapshot,{less,less},MaterialRepresentation::Nominal).membership==MaterialMembership::Inside);
 REQUIRE(classify_linear_material(prefix.snapshot,{less,less},MaterialRepresentation::Lower).membership==MaterialMembership::Outside);
}
namespace {
JoinedMaterialPolicy join_policy(){JoinedMaterialPolicy p;p.policy_id=31;p.revision=1;return p;}
struct JoinedFixture {std::shared_ptr<const LinearMaterialPrefixSnapshot> prefix;MaterialRegion box;};
JoinedFixture joined_fixture(bool rounded=false,int interruption=0,bool diagonal=false,bool reverse=false,int deformation=0)
{
 std::ostringstream bytes;bytes.imbue(std::locale::classic());bytes<<"G90\nM83\nM400\nM204 S4\n"<<std::fixed<<std::setprecision(6);
 std::vector<MaterialDeclaration> rows;std::array<double,3> previous{0,0,0};
 const double amount=(High(".001")*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)).convert_to<double>();
 for(size_t i=1;i<=5;++i){
  if(i==4 && interruption){
   if(interruption==1){bytes<<"G4 P10\nM400\n";rows.push_back({uint64_t(rows.size()+1),rows.size(),MaterialEventKind::Dwell,previous,previous,0,0,{}});}
   else {for(int sign:{-1,1}){bytes<<"G1 E"<<sign*.01<<" F60\nM400\n";rows.push_back({uint64_t(rows.size()+1),rows.size(),sign<0 ? MaterialEventKind::Retraction : MaterialEventKind::Restore,previous,previous,0,.01,{}});}}
  }
  const double direction=reverse ? -1 : 1;
  std::array<double,3> end{direction*(diagonal ? .012 : .02)*i,diagonal ? .016*i : 0,.01*i};
  if(deformation==3 && i>=4)end[1]+=.000001;
  if(deformation==4 && i>=4)end[0]=.12-.02*i;
  const bool narrow=deformation==1 && i==3;const double gap=deformation==2 && i==3 ? .05 : .2;
  bytes<<"G1 X"<<end[0]<<" Y"<<end[1]<<" Z"<<end[2]<<(narrow ? " E.0001 F30\nM400\n" : " E.001 F30\nM400\n");
  rows.push_back({uint64_t(rows.size()+1),rows.size(),MaterialEventKind::Deposit,previous,end,narrow ? amount/10 : amount,0,
   MaterialSection{(rounded || (deformation==5 && i==4)) ? MaterialSectionKind::RoundedRectangle : MaterialSectionKind::Rectangle,gap,rounded ? .3 : gap}});previous=end;
 }
 const auto rates=verify_linear_rates(bytes.str(),{0,0,0},rate_policy());INFO(rates.reason);REQUIRE(rates.snapshot);
 const auto material=reconstruct_linear_material(rates.snapshot,rows,material_policy());INFO(material.reason);REQUIRE(material.snapshot);
 const auto prefix=linear_material_at(material.snapshot,rows.size(),0);REQUIRE(prefix.snapshot);
 const double x=(reverse ? -1 : 1)*(diagonal ? .03 : .05),y=diagonal ? .04 : 0,z=.025-(rounded ? .25 : .2)/2;
 return {prefix.snapshot,{{x-.001,y-.001,z-.002},{x+.001,y+.001,z+.002}}};
}
}
TEST_CASE("B12 final joined lower covers complete internal packet seams with unchanged losses in rotated frames", "[Nonplanar][B12][FinalByteJoined]")
{
 for(bool rounded:{false,true})for(bool diagonal:{false,true})for(bool reverse:{false,true}){
  const auto fixture=joined_fixture(rounded,0,diagonal,reverse);
  const auto old=cover_linear_material(fixture.prefix,fixture.box,MaterialRepresentation::Lower);REQUIRE(old.status==RateStatus::Fail);
  const auto joined=reconstruct_joined_linear_material(fixture.prefix,join_policy());INFO(joined.reason);REQUIRE(joined.snapshot);
  REQUIRE(joined.snapshot->runs.size()==1);REQUIRE(joined.snapshot->runs[0].first_record==0);REQUIRE(joined.snapshot->runs[0].last_record==4);
  REQUIRE(joined.evaluations>fixture.prefix->evaluations);const auto covered=cover_joined_linear_material_lower(joined.snapshot,fixture.box);INFO(covered.reason);
  REQUIRE(covered.status==RateStatus::Pass);REQUIRE(covered.snapshot);REQUIRE(covered.snapshot->leaves.size()==1);REQUIRE(covered.snapshot->source==joined.snapshot);
  nptop_test::check_joined_lower(*covered.snapshot);
  REQUIRE(joined.snapshot->source->source->policy.inner_xy_loss_mm==material_policy().inner_xy_loss_mm);
 }
}
TEST_CASE("B12 final joined sections retain each narrow dose raised floor and exact direction split", "[Nonplanar][B12][FinalByteJoined]")
{
 for(int deformation=1;deformation<=5;++deformation){const auto f=joined_fixture(false,0,false,false,deformation);
  const auto joined=reconstruct_joined_linear_material(f.prefix,join_policy());REQUIRE(joined.snapshot);
  REQUIRE(joined.snapshot->runs.size()==(deformation<=2 ? 1 : deformation==4 ? 2 : 3));
  const auto covered=cover_joined_linear_material_lower(joined.snapshot,f.box);INFO(deformation<<" "<<covered.reason);
  REQUIRE(covered.status!=RateStatus::Pass);REQUIRE_FALSE(covered.snapshot);
 }
}
TEST_CASE("B12 final joined model preserves interruptions partial fronts and empty-prefix refusal", "[Nonplanar][B12][FinalByteJoined]")
{
 for(int interruption:{1,2}){const auto f=joined_fixture(false,interruption);const auto joined=reconstruct_joined_linear_material(f.prefix,join_policy());REQUIRE(joined.snapshot);
  REQUIRE(joined.snapshot->runs.size()==2);const auto covered=cover_joined_linear_material_lower(joined.snapshot,f.box);REQUIRE(covered.status==RateStatus::Fail);REQUIRE_FALSE(covered.snapshot);
 }
 const auto f=joined_fixture();const auto partial=linear_material_at(f.prefix->source,2,.5);REQUIRE(partial.snapshot);
 const auto joined=reconstruct_joined_linear_material(partial.snapshot,join_policy());REQUIRE(joined.snapshot);
 REQUIRE(cover_joined_linear_material_lower(joined.snapshot,f.box).status==RateStatus::Fail);
 const auto empty=linear_material_at(f.prefix->source,0,0);REQUIRE(empty.snapshot);const auto no_runs=reconstruct_joined_linear_material(empty.snapshot,join_policy());REQUIRE(no_runs.snapshot);REQUIRE(no_runs.snapshot->runs.empty());
 REQUIRE(cover_joined_linear_material_lower(no_runs.snapshot,f.box).status==RateStatus::Fail);
}
TEST_CASE("B12 final joined capture policy ownership and late publication keep independent refusals", "[Nonplanar][B12][FinalByteJoined]")
{
 STATIC_REQUIRE_FALSE(std::is_aggregate<JoinedMaterialSnapshot>::value);
 STATIC_REQUIRE_FALSE(std::is_aggregate<JoinedMaterialCoverSnapshot>::value);
 const auto f=joined_fixture();
 for(int mode=0;mode<4;++mode){auto p=join_policy();if(mode==0)p.version=2;if(mode==1)p.synthetic=false;if(mode==2)p.operator_confirmed_claim=true;if(mode==3)p.model=static_cast<JoinedMaterialModel>(99);
  REQUIRE_FALSE(reconstruct_joined_linear_material(f.prefix,p).snapshot);
 }
 for(int mode=0;mode<6;++mode){JoinedMaterialLimits limits;
  if(mode==0)limits.max_evaluations=f.prefix->evaluations+1;if(mode==1)limits.cancelled=[] {return true;};
  if(mode==2)limits.is_join_current=[](uint64_t,uint64_t){return false;};if(mode==3)limits.is_source_current=[](uint64_t){return false;};
  if(mode==4)limits.is_rate_current=[](uint64_t,uint64_t){return false;};if(mode==5)limits.is_current=[](uint64_t,uint64_t){return false;};
  const auto r=reconstruct_joined_linear_material(f.prefix,join_policy(),limits);REQUIRE_FALSE(r.snapshot);REQUIRE(r.status==RateStatus::Unknown);
 }
 auto p=join_policy();JoinedMaterialLimits limits;limits.cancelled=[&]{p.operator_confirmed_claim=true;limits.max_events=0;return false;};
 const auto owned=reconstruct_joined_linear_material(f.prefix,p,limits);REQUIRE(owned.snapshot);REQUIRE_FALSE(owned.snapshot->policy.operator_confirmed_claim);
 for(int mode=0;mode<6;++mode){JoinedMaterialLimits guard;
  if(mode==0)guard.max_evaluations=owned.evaluations+1;if(mode==1)guard.cancelled=[] {return true;};
  if(mode==2)guard.is_join_current=[](uint64_t,uint64_t){return false;};if(mode==3)guard.is_source_current=[](uint64_t){return false;};
  if(mode==4)guard.is_rate_current=[](uint64_t,uint64_t){return false;};if(mode==5)guard.is_current=[](uint64_t,uint64_t){return false;};
  const auto r=cover_joined_linear_material_lower(owned.snapshot,f.box,guard);REQUIRE(r.status==RateStatus::Unknown);REQUIRE_FALSE(r.snapshot);REQUIRE_FALSE(r.uncovered);
 }
 auto region=f.box;limits={};limits.cancelled=[&]{region.min[0]=50;limits.max_cells=0;return false;};
 const auto captured=cover_joined_linear_material_lower(owned.snapshot,region,limits);REQUIRE(captured.snapshot);REQUIRE(captured.snapshot->region.min==f.box.min);
 limits={};size_t calls=0;limits.cancelled=[&]{++calls;return false;};REQUIRE(cover_joined_linear_material_lower(owned.snapshot,f.box,limits).snapshot);
 const auto last=calls;calls=0;limits.cancelled=[&]{if(++calls==last)std::fesetround(FE_UPWARD);return false;};
 const auto refused=cover_joined_linear_material_lower(owned.snapshot,f.box,limits);std::fesetround(FE_TONEAREST);REQUIRE(refused.status==RateStatus::Unknown);REQUIRE_FALSE(refused.snapshot);
 const auto empty=linear_material_at(f.prefix->source,0,0);REQUIRE(empty.snapshot);const auto no_runs=reconstruct_joined_linear_material(empty.snapshot,join_policy());REQUIRE(no_runs.snapshot);
 limits={};calls=0;limits.cancelled=[&]{++calls;return false;};const auto absent=cover_joined_linear_material_lower(no_runs.snapshot,f.box,limits);REQUIRE(absent.uncovered);
 const auto negative_last=calls;calls=0;limits.is_join_current=[&](uint64_t,uint64_t){if(++calls==negative_last)std::fesetround(FE_UPWARD);return true;};limits.cancelled={};
 const auto stale_absent=cover_joined_linear_material_lower(no_runs.snapshot,f.box,limits);std::fesetround(FE_TONEAREST);REQUIRE(stale_absent.status==RateStatus::Unknown);REQUIRE_FALSE(stale_absent.uncovered);
}
namespace {
LinearRunSupportPolicy support_policy()
{LinearRunSupportPolicy p;p.policy_id=41;p.revision=1;p.cross_slope=.1;p.vertical_min=.14;p.vertical_max=.26;p.normal_min=.14;p.normal_max=.26;return p;}
std::shared_ptr<const JoinedMaterialSnapshot> support_fixture(int mode=0,bool diagonal=false,bool reverse=false,double coordinate_error=1e-7)
{
 const double direction=reverse ? -1 : 1;const auto pose=[&](double t,double z){return std::array<double,3>{direction*(diagonal ? .6 : 1)*t,diagonal ? .8*t : 0,z};};
 std::ostringstream bytes;bytes.imbue(std::locale::classic());bytes<<"G90\nM83\nM400\nM204 S4\n"<<std::fixed<<std::setprecision(9);
 std::vector<MaterialDeclaration> rows;std::array<double,3> previous{0,0,0};
 const auto append=[&](std::array<double,3> end,double e){bytes<<"G1 X"<<end[0]<<" Y"<<end[1]<<" Z"<<end[2];if(e>0)bytes<<" E"<<e;bytes<<" F30\nM400\n";
  const double amount=(High(e)*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)).convert_to<double>();
  rows.push_back({uint64_t(rows.size()+1),rows.size(),e>0 ? MaterialEventKind::Deposit : MaterialEventKind::Travel,previous,end,amount,0,
   e>0 ? std::optional<MaterialSection>(MaterialSection{MaterialSectionKind::Rectangle,.2,.2}) : std::optional<MaterialSection>{}});previous=end;
 };
 append(pose(3,0),mode==1 ? 0 : mode==2 ? .0001 : .2);
 const double z=mode==3 ? .5 : mode==4 ? .05 : .2;append(pose(.5,z),0);append(pose(2.5,z+.02),.03);
 if(mode==1){append(pose(0,0),0);append(pose(3,0),.2);}
 const auto rates=verify_linear_rates(bytes.str(),{0,0,0},rate_policy());INFO(rates.reason);REQUIRE(rates.snapshot);
 auto mp=material_policy();mp.numerical_coordinate_error_mm=coordinate_error;
 const auto material=reconstruct_linear_material(rates.snapshot,rows,mp);INFO(material.reason);REQUIRE(material.snapshot);
 const auto prefix=linear_material_at(material.snapshot,rows.size(),0);REQUIRE(prefix.snapshot);
 const auto joined=reconstruct_joined_linear_material(prefix.snapshot,join_policy());REQUIRE(joined.snapshot);return joined.snapshot;
}
}
TEST_CASE("B12 final run support covers actual complete rotated footprints with both gap bands", "[Nonplanar][B12][FinalByteSupport]")
{
 for(bool diagonal:{false,true})for(bool reverse:{false,true}){
  const auto source=support_fixture(0,diagonal,reverse);const auto result=verify_linear_run_support(source,1,support_policy());INFO(result.reason);
  REQUIRE(result.snapshot);REQUIRE(result.status==RateStatus::Pass);REQUIRE_FALSE(result.witness);REQUIRE(result.evaluations>source->evaluations);
  REQUIRE(result.snapshot->support->source->completed_records==2);REQUIRE(result.snapshot->support->source->pieces.size()==1);
  const High direction=reverse ? -1 : 1,ux=direction*(diagonal ? High(".6") : High(1)),uy=diagonal ? High(".8") : High(0);
  const auto policy=support_policy();const High cross(policy.cross_slope),parallel(".01"),norm=sqrt(1+parallel*parallel+cross*cross);
  const std::array<High,3> normal{(parallel*ux-cross*uy)/norm,(parallel*uy+cross*ux)/norm,-1/norm};
  REQUIRE(result.snapshot->query_error_mm>=4*material_policy().numerical_coordinate_error_mm);
  REQUIRE(result.snapshot->leaves.size()>0);High partition=0;
  for(const auto &leaf:result.snapshot->leaves){REQUIRE(leaf.target_record==2);REQUIRE(leaf.lower_anchor);REQUIRE(leaf.nominal_terminal);nptop_test::check_joined_lower(*leaf.lower_anchor);
   REQUIRE(leaf.vertical_near.min[2]>0);REQUIRE(leaf.normal_near.min[2]>0); // sole actual support has maximum Z=0
   independent_solid_box(leaf.normal_terminal,MaterialRepresentation::Nominal,{MaterialSectionKind::Rectangle,.2,.2},{High(0),High(0),High(0)},{3*ux,3*uy,High(0)});
   for(bool n:{false,true})for(bool terminal:{false,true}){
    const auto &box=n ? (terminal ? leaf.normal_terminal : leaf.normal_near) : (terminal ? leaf.vertical_terminal : leaf.vertical_near);
    const double min=terminal ? (n ? policy.normal_max : policy.vertical_max) : 0,max=terminal ? min : (n ? policy.normal_min : policy.vertical_min);
    // These are exact affine extrema for every t/transverse/distance point in
    // the complete tile, not sampled geometry membership.
    for(double t:{leaf.progress.lower,leaf.progress.upper})for(double transverse:{leaf.transverse.lower,leaf.transverse.upper})for(double d:{min,max}){
     std::array<High,3> p{High(".5")*ux+2*ux*High(t)-uy*High(transverse),High(".5")*uy+2*uy*High(t)+ux*High(transverse),High(".2")+High(".02")*High(t)+cross*High(transverse)};
     for(size_t axis=0;axis<3;++axis){p[axis]+=(n ? normal[axis] : axis==2 ? High(-1) : High(0))*High(d);
      const High error=4*High(material_policy().numerical_coordinate_error_mm);REQUIRE(High(box.min[axis])<=p[axis]-error);REQUIRE(High(box.max[axis])>=p[axis]+error);}
     const High height=High(".2")+High(".02")*High(t)+cross*High(transverse);
     REQUIRE(height>High(policy.vertical_min));REQUIRE(height<High(policy.vertical_max));REQUIRE(height*norm>High(policy.normal_min));REQUIRE(height*norm<High(policy.normal_max));
    }
   }
   partition+=(High(leaf.progress.upper)-High(leaf.progress.lower))*(High(leaf.transverse.upper)-High(leaf.transverse.lower));
  }
  REQUIRE(abs(partition-High(source->source->pieces.back().nominal_width.upper))<High("1e-28"));
  for(size_t i=0;i<result.snapshot->leaves.size();++i)for(size_t j=0;j<i;++j){const auto &a=result.snapshot->leaves[i],&b=result.snapshot->leaves[j];
   REQUIRE((a.progress.upper<=b.progress.lower || b.progress.upper<=a.progress.lower || a.transverse.upper<=b.transverse.lower || b.transverse.upper<=a.transverse.lower));}
  const auto partial=linear_material_at(source->source->source,2,.5);REQUIRE(partial.snapshot);
  const auto partial_runs=reconstruct_joined_linear_material(partial.snapshot,join_policy());REQUIRE(partial_runs.snapshot);
  const auto half=verify_linear_run_support(partial_runs.snapshot,1,policy);REQUIRE(half.snapshot);
  High partial_partition=0;for(const auto &leaf:half.snapshot->leaves){REQUIRE(leaf.progress.lower>=0);REQUIRE(leaf.progress.upper<=.5);
   partial_partition+=(High(leaf.progress.upper)-High(leaf.progress.lower))*(High(leaf.transverse.upper)-High(leaf.transverse.lower));}
  REQUIRE(abs(partial_partition-High(partial.snapshot->pieces.back().nominal_width.upper)/2)<High("1e-28"));
 }
}
TEST_CASE("B12 final run support excludes future support and rejects missing Lower and actual bad gaps", "[Nonplanar][B12][FinalByteSupport]")
{
 for(int mode=1;mode<=4;++mode){const auto source=support_fixture(mode);const size_t run=mode==1 ? 0 : 1;
  const auto result=verify_linear_run_support(source,run,support_policy());INFO(mode<<" "<<result.reason);
  REQUIRE(result.status==RateStatus::Fail);REQUIRE_FALSE(result.snapshot);REQUIRE(result.witness);
  const auto &w=*result.witness;REQUIRE(w.target_record==2);REQUIRE(w.progress.lower>=0);REQUIRE(w.progress.upper<=1);
  const High half=High(".03")*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)/2/High(".2")/2;
  REQUIRE(High(w.transverse.lower)>-half);REQUIRE(High(w.transverse.upper)<half);
  if(mode==1 || mode==3){REQUIRE(result.reason=="FINAL_RUN_VERTICAL_NO_HIT_IN_BAND");if(mode==3)REQUIRE(w.region.min[2]>0);}
  if(mode==2){REQUIRE(result.reason=="FINAL_RUN_DECLARED_LOWER_ANCHOR_MISSING");
   const High support_width=High(".0001")*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)/3/High(".2");
   REQUIRE(support_width<2*(High(material_policy().inner_xy_loss_mm)+High(material_policy().numerical_coordinate_error_mm)));}
  if(mode==4){REQUIRE(result.reason=="FINAL_RUN_VERTICAL_GAP_TOO_SMALL");
   independent_solid_box(w.region,MaterialRepresentation::Nominal,{MaterialSectionKind::Rectangle,.2,.2},{High(0),High(0),High(0)},{High(3),High(0),High(0)});}
 }
}
TEST_CASE("B12 final run support guards policy source budgets ownership and late publication", "[Nonplanar][B12][FinalByteSupport]")
{
 STATIC_REQUIRE_FALSE(std::is_aggregate<LinearRunSupportSnapshot>::value);const auto source=support_fixture();
 for(int mode=0;mode<16;++mode){auto p=support_policy();LinearRunSupportLimits limits;
  if(mode==0)p.operator_confirmed_claim=true;if(mode==1)p.normal_min=p.normal_max;if(mode==2)limits.max_evaluations=source->evaluations+1;
  if(mode==3)limits.is_support_current=[](uint64_t,uint64_t){return false;};if(mode==4)limits.is_join_current=[](uint64_t,uint64_t){return false;};
  if(mode==5)limits.is_source_current=[](uint64_t){return false;};if(mode==6)limits.max_cells=1;if(mode==7)limits.cancelled=[] {return true;};
  if(mode==8)limits.is_current=[](uint64_t,uint64_t){return false;};if(mode==9)limits.is_rate_current=[](uint64_t,uint64_t){return false;};
  if(mode==10)limits.max_depth=0;if(mode==11)limits.timeout=std::chrono::milliseconds(0);if(mode==12)p.synthetic=false;
  if(mode==13)p.version=2;if(mode==14)p.cross_slope=std::numeric_limits<double>::infinity();if(mode==15)p.cross_slope=10.01;
  const auto result=verify_linear_run_support(source,1,p,limits);REQUIRE(result.status==RateStatus::Unknown);REQUIRE_FALSE(result.snapshot);REQUIRE_FALSE(result.witness);
 }
 for(const auto &missing:{std::shared_ptr<const JoinedMaterialSnapshot>{},source}){
  const auto result=verify_linear_run_support(missing,source->runs.size(),support_policy());REQUIRE(result.status==RateStatus::Unknown);REQUIRE_FALSE(result.snapshot);REQUIRE_FALSE(result.witness);}
 for(double allowance:{.01,.02}){const auto error=verify_linear_run_support(support_fixture(0,false,false,allowance),1,support_policy());REQUIRE(error.status==RateStatus::Unknown);REQUIRE(error.reason=="FINAL_SUPPORT_SPATIAL_ERROR_BUDGET");REQUIRE_FALSE(error.snapshot);REQUIRE_FALSE(error.witness);}
 auto p=support_policy();LinearRunSupportLimits limits;limits.cancelled=[&]{p.normal_min=1;limits.max_cells=0;return false;};
 const auto owned=verify_linear_run_support(source,1,p,limits);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->policy.normal_min==.14);
 limits={};size_t calls=0;limits.cancelled=[&]{++calls;return false;};REQUIRE(verify_linear_run_support(source,1,support_policy(),limits).snapshot);
 const size_t last=calls;calls=0;limits.cancelled=[&]{if(++calls==last)std::fesetround(FE_UPWARD);return false;};
 const auto late=verify_linear_run_support(source,1,support_policy(),limits);std::fesetround(FE_TONEAREST);REQUIRE(late.status==RateStatus::Unknown);REQUIRE_FALSE(late.snapshot);REQUIRE_FALSE(late.witness);
 const auto negative_source=support_fixture(1);calls=0;limits.cancelled=[&]{++calls;return false;};REQUIRE(verify_linear_run_support(negative_source,0,support_policy(),limits).witness);
 const size_t negative_last=calls;
 for(bool numeric:{false,true}){calls=0;limits.cancelled=[&]{if(++calls!=negative_last)return false;if(numeric){std::fesetround(FE_UPWARD);return false;}return true;};
  const auto refused=verify_linear_run_support(negative_source,0,support_policy(),limits);std::fesetround(FE_TONEAREST);REQUIRE(refused.status==RateStatus::Unknown);REQUIRE_FALSE(refused.snapshot);REQUIRE_FALSE(refused.witness);}
}

TEST_CASE("B12 final material bounds indexes preserve separated solids and actual prefix boundaries", "[Nonplanar][B12][FinalByteSupport]")
{
 std::ostringstream bytes;bytes.imbue(std::locale::classic());bytes<<"G90\nM83\nM400\nM204 S4\n"<<std::fixed<<std::setprecision(9);
 std::vector<MaterialDeclaration> rows;std::array<double,3> previous{0,0,0};
 const auto append=[&](std::array<double,3> end,double e){bytes<<"G1 X"<<end[0]<<" Y"<<end[1]<<" Z"<<end[2];if(e>0)bytes<<" E"<<e;bytes<<" F30\nM400\n";
  const double amount=(High(e)*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)).convert_to<double>();
  rows.push_back({uint64_t(rows.size()+1),rows.size(),e>0 ? MaterialEventKind::Deposit : MaterialEventKind::Travel,previous,end,amount,0,
   e>0 ? std::optional<MaterialSection>(MaterialSection{MaterialSectionKind::Rectangle,.2,.2}) : std::optional<MaterialSection>{}});previous=end;};
 for(size_t i=0;i<20;++i){const double y=2*i,z=.2*(i%3);if(i)append({0,y,z},0);append({3,y,z},.2);}
 const auto rates=verify_linear_rates(bytes.str(),{0,0,0},rate_policy());REQUIRE(rates.snapshot);
 const auto material=reconstruct_linear_material(rates.snapshot,rows,material_policy());REQUIRE(material.snapshot);
 const auto prefix=linear_material_at(material.snapshot,rows.size(),0);REQUIRE(prefix.snapshot);
 const auto joined=reconstruct_joined_linear_material(prefix.snapshot,join_policy());REQUIRE(joined.snapshot);
 const auto check_index=[&](const MaterialBoundsIndex &index,size_t size,auto bounds){
  REQUIRE(index.nodes.size()>1);REQUIRE(index.order.size()==size);std::set<size_t> unique(index.order.begin(),index.order.end());REQUIRE(unique.size()==size);
  REQUIRE(*unique.begin()==0);REQUIRE(*unique.rbegin()==size-1);
  for(const auto &node:index.nodes){REQUIRE(node.begin<node.end);REQUIRE(node.end<=size);
   for(size_t i=node.begin;i<node.end;++i)for(size_t axis=0;axis<3;++axis){const auto &b=bounds(index.order[i]).coordinate[axis];REQUIRE(node.bounds.coordinate[axis].lower<=b.lower);REQUIRE(node.bounds.coordinate[axis].upper>=b.upper);}
   if(node.left){REQUIRE(index.nodes[node.left].begin==node.begin);REQUIRE(index.nodes[node.left].end==index.nodes[node.right].begin);REQUIRE(index.nodes[node.right].end==node.end);}
  }
 };
 check_index(prefix.snapshot->nominal_index,20,[&](size_t i)->const MaterialBox&{return prefix.snapshot->pieces[i].nominal_bounds;});
 check_index(joined.snapshot->outer_index,20,[&](size_t i)->const MaterialBox&{return joined.snapshot->runs[i].outer_bounds;});
 for(size_t i=0;i<20;++i){const double y=2*i,z=.2*(i%3)-.1;MaterialRegion box{{1.499,y-.001,z-.001},{1.501,y+.001,z+.001}};
  REQUIRE(classify_linear_material(prefix.snapshot,box,MaterialRepresentation::Nominal).membership==MaterialMembership::Inside);
  const auto cover=cover_joined_linear_material_lower(joined.snapshot,box);REQUIRE(cover.snapshot);nptop_test::check_joined_lower(*cover.snapshot);
 }
 const auto past=linear_material_at(material.snapshot,rows.size()-1,0);REQUIRE(past.snapshot);
 const auto past_runs=reconstruct_joined_linear_material(past.snapshot,join_policy());REQUIRE(past_runs.snapshot);
 const MaterialRegion future{{1.499,37.999,.099},{1.501,38.001,.101}};
 REQUIRE(classify_linear_material(past.snapshot,future,MaterialRepresentation::Nominal).membership==MaterialMembership::Outside);
 REQUIRE(cover_joined_linear_material_lower(past_runs.snapshot,future).status==RateStatus::Fail);
 const MaterialRegion empty{{1.499,38.999,.099},{1.501,39.001,.101}};
 REQUIRE(classify_linear_material(prefix.snapshot,empty,MaterialRepresentation::Nominal).membership==MaterialMembership::Outside);
 REQUIRE(cover_joined_linear_material_lower(joined.snapshot,empty).status==RateStatus::Fail);
}

TEST_CASE("B12 exact nominal run union covers complete packet cuts in every frame", "[Nonplanar][B12][FinalByteNominalRun]")
{
 for(bool rounded:{false,true})for(bool diagonal:{false,true})for(bool reverse:{false,true}){
  const auto f=joined_fixture(rounded,0,diagonal,reverse);const auto joined=reconstruct_joined_linear_material(f.prefix,join_policy());REQUIRE(joined.snapshot);
  auto box=f.box;for(size_t axis=0;axis<2;++axis){const double delta=f.prefix->source->declarations[2].end[axis]-(box.min[axis]+box.max[axis])/2;box.min[axis]+=delta;box.max[axis]+=delta;}
  box.min[2]+=.005;box.max[2]+=.005;MaterialCoverLimits shallow;shallow.max_depth=1;
  REQUIRE(cover_linear_material(f.prefix,box,MaterialRepresentation::Nominal,shallow).status==RateStatus::Unknown);
  const auto exact=cover_joined_linear_material(joined.snapshot,box,MaterialRepresentation::Nominal);INFO(exact.reason);REQUIRE(exact.snapshot);REQUIRE(exact.status==RateStatus::Pass);
  REQUIRE(exact.snapshot->representation==MaterialRepresentation::Nominal);REQUIRE(exact.snapshot->leaves.size()==1);REQUIRE(exact.snapshot->source==joined.snapshot);
  nptop_test::check_joined_nominal(*exact.snapshot);
  const auto lower=cover_joined_linear_material_lower(joined.snapshot,box);REQUIRE(lower.snapshot);REQUIRE(lower.snapshot->representation==MaterialRepresentation::Lower);nptop_test::check_joined_lower(*lower.snapshot);
 }
}
TEST_CASE("B12 exact nominal run union retains actual doses floors future and role refusals", "[Nonplanar][B12][FinalByteNominalRun]")
{
 for(int deformation:{1,2}){const auto f=joined_fixture(false,0,false,false,deformation);const auto joined=reconstruct_joined_linear_material(f.prefix,join_policy());REQUIRE(joined.snapshot);
  auto box=f.box;if(deformation==1){box.min[1]=.039;box.max[1]=.041;}
  const auto rejected=cover_joined_linear_material(joined.snapshot,box,MaterialRepresentation::Nominal);INFO(rejected.reason);REQUIRE(rejected.status==RateStatus::Fail);REQUIRE(rejected.uncovered);REQUIRE_FALSE(rejected.snapshot);
 }
 const auto f=joined_fixture();const auto partial=linear_material_at(f.prefix->source,2,.5);REQUIRE(partial.snapshot);const auto joined=reconstruct_joined_linear_material(partial.snapshot,join_policy());REQUIRE(joined.snapshot);
 const MaterialRegion future{{.061,-.001,-.08},{.063,.001,-.07}};REQUIRE(cover_joined_linear_material(joined.snapshot,future,MaterialRepresentation::Nominal).status==RateStatus::Fail);
 const auto complete=reconstruct_joined_linear_material(f.prefix,join_policy());REQUIRE(complete.snapshot);
 for(auto role:{MaterialRepresentation::Upper,static_cast<MaterialRepresentation>(99)}){const auto r=cover_joined_linear_material(complete.snapshot,f.box,role);REQUIRE(r.status==RateStatus::Unknown);REQUIRE_FALSE(r.snapshot);REQUIRE_FALSE(r.uncovered);}
 for(int mode=0;mode<7;++mode){JoinedMaterialLimits limits;
  if(mode==0)limits.is_join_current=[](uint64_t,uint64_t){return false;};if(mode==1)limits.is_current=[](uint64_t,uint64_t){return false;};if(mode==2)limits.is_source_current=[](uint64_t){return false;};
  if(mode==3)limits.is_rate_current=[](uint64_t,uint64_t){return false;};if(mode==4)limits.cancelled=[] {return true;};if(mode==5)limits.max_evaluations=complete.evaluations+1;if(mode==6)limits.max_cells=0;
  const auto r=cover_joined_linear_material(complete.snapshot,f.box,MaterialRepresentation::Nominal,limits);REQUIRE(r.status==RateStatus::Unknown);REQUIRE_FALSE(r.snapshot);REQUIRE_FALSE(r.uncovered);
 }
 JoinedMaterialLimits limits;size_t calls=0;limits.cancelled=[&]{++calls;return false;};REQUIRE(cover_joined_linear_material(complete.snapshot,f.box,MaterialRepresentation::Nominal,limits).snapshot);
 const auto positive_last=calls;calls=0;limits.cancelled=[&]{if(++calls==positive_last)std::fesetround(FE_UPWARD);return false;};
 const auto late=cover_joined_linear_material(complete.snapshot,f.box,MaterialRepresentation::Nominal,limits);std::fesetround(FE_TONEAREST);REQUIRE(late.status==RateStatus::Unknown);REQUIRE_FALSE(late.snapshot);REQUIRE_FALSE(late.uncovered);
 calls=0;limits.cancelled=[&]{++calls;return false;};REQUIRE(cover_joined_linear_material(joined.snapshot,future,MaterialRepresentation::Nominal,limits).uncovered);
 const auto negative_last=calls;calls=0;limits.cancelled=[&]{if(++calls==negative_last)std::fesetround(FE_UPWARD);return false;};
 const auto refused=cover_joined_linear_material(joined.snapshot,future,MaterialRepresentation::Nominal,limits);std::fesetround(FE_TONEAREST);REQUIRE(refused.status==RateStatus::Unknown);REQUIRE_FALSE(refused.snapshot);REQUIRE_FALSE(refused.uncovered);
 auto region=f.box;limits={};limits.cancelled=[&]{region.min[0]=99;limits.max_cells=0;return false;};
 const auto owned=cover_joined_linear_material(complete.snapshot,region,MaterialRepresentation::Nominal,limits);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->region.min==f.box.min);
}
namespace {
LinearTravelScene travel_scene()
{
 LinearTravelScene s;s.profile_id=51;s.revision=1;s.opening_radius_mm=.2;s.outer_radius_mm=.5;
 s.nozzle_domain=s.scene_domain={{-100,-100,-100},{100,100,100}};
 s.obstacle_inventory_complete=true;s.unmodelled_parts_min_local_z_mm=5;
 for(int role=0;role<6;++role)s.head.push_back({uint64_t(role+1),TravelHeadRole(role),{{-.05,-.05,.5},{.05,.05,.8}}});return s;
}
LinearMaterialResult travel_material(bool rounded=false)
{
 const std::string bytes=std::string("G90\nM83\nM400\nM204 S4\nG1 X")+(rounded ? "3.000001" : "3")+" Y4 Z.5 F60\nM400\nG1 X6 Y8 Z.5 E.2 F60\nM400\nG1 X0 Y0 Z.5 F60\nM400\n";
 auto p=material_policy();p.max_coordinate_delta_mm=2e-6;
 const double volume=(High(".2")*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)).convert_to<double>();
 const std::vector<MaterialDeclaration> rows{
 {1,0,MaterialEventKind::Travel,{0,0,.5},{3,4,.5},0,0,{}},
 {2,1,MaterialEventKind::Deposit,{3,4,.5},{6,8,.5},volume,0,MaterialSection{MaterialSectionKind::Rectangle,.2,.2}},
 {3,2,MaterialEventKind::Travel,{6,8,.5},{0,0,.5},0,0,{}}};
 const auto rates=verify_linear_rates(bytes,{0,0,.5},rate_policy());INFO(rates.reason);REQUIRE(rates.snapshot);
 const auto result=reconstruct_linear_material(rates.snapshot,rows,p);INFO(result.reason);REQUIRE(result.snapshot);return result;
}
}
TEST_CASE("B12 final travel checks the complete head against actual past material and excludes future deposits", "[Nonplanar][B12][FinalByteTravel]")
{
 const auto material=travel_material();const auto scene=travel_scene();
 const auto before=verify_linear_travel_geometry(material.snapshot,0,1,scene);INFO(before.reason);REQUIRE(before.snapshot);
 REQUIRE(before.status==RateStatus::Pass);REQUIRE(before.snapshot->source==material.snapshot);REQUIRE(before.snapshot->prefix->completed_records==0);
 REQUIRE(before.snapshot->prefix->pieces.empty());REQUIRE(before.snapshot->leaves.size()==7);
 nptop_test::check_final_travel(*before.snapshot);
 for(const auto &leaf:before.snapshot->leaves){REQUIRE(leaf.record==0);REQUIRE(leaf.progress.lower==0);REQUIRE(leaf.progress.upper==1);}
 const auto after=verify_linear_travel_geometry(material.snapshot,2,1,scene);INFO(after.reason);REQUIRE(after.status==RateStatus::Fail);
 REQUIRE_FALSE(after.snapshot);REQUIRE(after.witness);REQUIRE(after.witness->record==2);REQUIRE(after.witness->material_event==2);
 REQUIRE_FALSE(verify_linear_travel_geometry(material.snapshot,0,2,scene).snapshot);
 REQUIRE_FALSE(verify_linear_travel_geometry(material.snapshot,0,0,scene).snapshot);
 REQUIRE_FALSE(verify_linear_travel_geometry(material.snapshot,3,1,scene).snapshot);
}
TEST_CASE("B12 final travel rejects interior static and full head collisions even when travel endpoints clear", "[Nonplanar][B12][FinalByteTravel]")
{
 const auto material=travel_material();auto scene=travel_scene();
 scene.obstacles.push_back({{1.99,1.99,.49},{2.01,2.01,.51}});
 const auto middle=verify_linear_travel_geometry(material.snapshot,0,1,scene);INFO(middle.reason);REQUIRE(middle.status==RateStatus::Fail);
 REQUIRE(middle.witness);REQUIRE(middle.witness->component==0);REQUIRE(middle.witness->obstacle==0);
 REQUIRE(middle.witness->progress.lower>0);REQUIRE(middle.witness->progress.upper<1);
 scene=travel_scene();scene.obstacles.push_back({{1.49,1.99,1.14},{1.51,2.01,1.16}});
 const auto head=verify_linear_travel_geometry(material.snapshot,0,1,scene);INFO(head.reason);REQUIRE(head.status==RateStatus::Fail);
 REQUIRE(head.witness);REQUIRE(head.witness->component>0);REQUIRE(head.witness->obstacle==0);
}
TEST_CASE("B12 final travel partitions the annulus hole over previous actual material without filling the disk", "[Nonplanar][B12][FinalByteTravel]")
{
 const auto rates=verify_linear_rates("G90\nM83\nM400\nM204 S4\nG1 X.01 Y0 Z.5 E.00001 F30\nM400\nG1 X.01 Y0 Z1 F30\nM400\n",{0,0,.5},rate_policy());REQUIRE(rates.snapshot);
 const double volume=(High(".00001")*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)).convert_to<double>();
 const std::vector<MaterialDeclaration> rows{
  {1,0,MaterialEventKind::Deposit,{0,0,.5},{.01,0,.5},volume,0,MaterialSection{MaterialSectionKind::Rectangle,.2,.2}},
  {2,1,MaterialEventKind::Travel,{.01,0,.5},{.01,0,1},0,0,{}}};
 const auto material=reconstruct_linear_material(rates.snapshot,rows,material_policy());INFO(material.reason);REQUIRE(material.snapshot);
 const auto proof=verify_linear_travel_geometry(material.snapshot,1,1,travel_scene());INFO(proof.reason);REQUIRE(proof.snapshot);
 REQUIRE(proof.snapshot->prefix->pieces.size()==1);
 REQUIRE(std::any_of(proof.snapshot->leaves.begin(),proof.snapshot->leaves.end(),[](const auto &l){return l.outside_annulus;}));
 nptop_test::check_final_travel(*proof.snapshot);
}
TEST_CASE("B12 final travel honors actual rounded decimal positions instead of nominal source poses", "[Nonplanar][B12][FinalByteTravel]")
{
 const auto nominal=travel_material(),rounded=travel_material(true);auto scene=travel_scene();
 // A distinct exact analytic zero-margin scene, not reduced native margins.
 scene.obstacles.push_back({{3.5000009,3.999999,.499999},{3.5000011,4.000001,.500001}});
 const auto old=verify_linear_travel_geometry(nominal.snapshot,0,1,scene);INFO(old.reason);REQUIRE(old.snapshot);
 const auto actual=verify_linear_travel_geometry(rounded.snapshot,0,1,scene);INFO(actual.reason);REQUIRE(actual.status==RateStatus::Fail);
 REQUIRE(actual.witness);REQUIRE(actual.witness->component==0);REQUIRE(actual.witness->progress.lower==1);
 REQUIRE(actual.witness->point.min[0]>3.5);REQUIRE(actual.witness->obstacle==0);
}
TEST_CASE("B12 final travel owns callers and refuses incomplete scenes stale callbacks budgets and late publication", "[Nonplanar][B12][FinalByteTravel]")
{
 auto material=travel_material().snapshot;auto scene=travel_scene();LinearTravelLimits limits;
 const auto source=material;limits.cancelled=[&]{material.reset();scene.head.clear();limits.max_cells=0;return false;};
 const auto owned=verify_linear_travel_geometry(material,0,1,scene,limits);INFO(owned.reason);REQUIRE(owned.snapshot);
 REQUIRE(owned.snapshot->source==source);REQUIRE(owned.snapshot->scene.head.size()==6);
 for(int mode=0;mode<19;++mode){auto head=travel_scene();auto l=LinearTravelLimits{};
  if(mode==0)head.head.pop_back();if(mode==1)head.head[0].role=TravelHeadRole(6);if(mode==2)head.operator_confirmed_claim=true;
  if(mode==3)head.obstacle_inventory_complete=false;if(mode==4)head.head[0].moving=true;
  if(mode==5)head.unmodelled_parts_min_local_z_mm=0; // no past solids: add a static ceiling below the omitted parts
  if(mode==5)head.obstacles.push_back({{20,20,1},{21,21,2}});
  if(mode==6)l.max_cells=owned.cells-1;if(mode==7)l.max_evaluations=owned.evaluations-1;
  if(mode==8)l.is_scene_current=[](uint64_t,uint64_t){return false;};if(mode==9)l.is_source_current=[](uint64_t){return false;};
  if(mode==10)l.cancelled=[] {return true;};if(mode==11)l.timeout=std::chrono::milliseconds(0);
  if(mode==12)l.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
  if(mode==13)l.cancelled=[]()->bool{throw 3;};if(mode==14)l.cancelled=[]()->bool{throw std::runtime_error("");};
  if(mode==15)head.opening_radius_mm=head.outer_radius_mm;if(mode==16)head.nozzle_domain.max[0]=1;
  if(mode==17)l.is_current=[](uint64_t,uint64_t){return false;};if(mode==18)l.is_rate_current=[](uint64_t,uint64_t){return false;};
  const auto r=verify_linear_travel_geometry(source,0,1,head,l);if(mode==12)REQUIRE(std::fesetround(FE_TONEAREST)==0);
  INFO(mode<<' '<<r.reason);REQUIRE(r.status==RateStatus::Unknown);REQUIRE_FALSE(r.snapshot);REQUIRE_FALSE(r.reason.empty());
 }
 limits={};size_t calls=0;limits.cancelled=[&]{++calls;return false;};REQUIRE(verify_linear_travel_geometry(source,0,1,travel_scene(),limits).snapshot);
 const auto last=calls;calls=0;limits.cancelled=[&]{return ++calls==last;};
 const auto late=verify_linear_travel_geometry(source,0,1,travel_scene(),limits);REQUIRE_FALSE(late.snapshot);REQUIRE(late.status==RateStatus::Unknown);REQUIRE(calls==last);
 auto collision=travel_scene();collision.obstacles.push_back({{1.99,1.99,.49},{2.01,2.01,.51}});
 calls=0;limits.cancelled=[&]{++calls;return false;};REQUIRE(verify_linear_travel_geometry(source,0,1,collision,limits).witness);
 const auto negative_last=calls;calls=0;limits.cancelled=[&]{return ++calls==negative_last;};
 const auto refused=verify_linear_travel_geometry(source,0,1,collision,limits);REQUIRE(refused.status==RateStatus::Unknown);REQUIRE_FALSE(refused.snapshot);REQUIRE_FALSE(refused.witness);REQUIRE(calls==negative_last);
}
TEST_CASE("B12 final travel JSON preserves every dependency and rejects duplicate ambiguous numeric or unknown fields", "[Nonplanar][B12][FinalByteTravel]")
{
 const auto document=travel_document({0,1,travel_scene()});const auto parsed=parse_travel_document(document.dump());
 REQUIRE(travel_document(parsed)==document);
 for(int mode=0;mode<12;++mode){auto d=document;
  if(mode==0)d["version"]=2;if(mode==1)d["first_record"]=true;if(mode==2)d["record_count"]=0;
  if(mode==3)d["scene"]["profile_id"]=1.0;if(mode==4)d["scene"]["tip"]["center"].push_back(0);
  if(mode==5)d["scene"]["clearance_mm"][0]=false;if(mode==6)d["scene"]["clearance_mm"].erase(0);
  if(mode==7)d["scene"]["head"][0]["role"]=6;if(mode==8)d["scene"]["head"][0]["id"]=0;
  if(mode==9)d["scene"]["head"][0]["verified"]=true;if(mode==10)d["scene"]["scene_domain"]["max"][0]=nullptr;
  if(mode==11)d["scene"]["obstacles"]={{{"min",{0,0,0}},{"max",{1,1,1}},{"hidden",true}}};
  INFO(mode);REQUIRE_THROWS(parse_travel_document(d.dump()));
 }
 auto duplicate=document.dump();const auto offset=duplicate.find("\"opening_radius_mm\":");REQUIRE(offset!=std::string::npos);
 duplicate.insert(offset,"\"opening_radius_mm\":0.2,");REQUIRE_THROWS(parse_travel_document(duplicate));
 auto nonfinite=document.dump();const auto number=nonfinite.find("\"opening_radius_mm\":0.2");REQUIRE(number!=std::string::npos);
 nonfinite.replace(number,std::string("\"opening_radius_mm\":0.2").size(),"\"opening_radius_mm\":1e999");REQUIRE_THROWS(parse_travel_document(nonfinite));
}
namespace {
LinearMaterialResult deposition_material(bool long_path=false)
{
 const auto p=material_policy();
 const double e=long_path ? .2 : .00001;
 const double volume=(High(e)*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)).convert_to<double>();
 const std::array<double,3> end=long_path ? std::array<double,3>{3,4,.6} : std::array<double,3>{.01,0,.5};
 const std::string bytes=std::string("G90\nM83\nM400\nM204 S4\n")+(long_path ? "G1 X3 Y4 Z.6 E.2 F30\n" : "G1 X.01 Y0 Z.5 E.00001 F30\n")+
  "M400\nG1 X3 Y4 Z.5 F30\nM400\nG1 X6 Y8 Z.5 E.2 F30\nM400\n";
 const double later=(High(".2")*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)).convert_to<double>();
 const std::vector<MaterialDeclaration> rows{
  {1,0,MaterialEventKind::Deposit,{0,0,.5},end,volume,0,MaterialSection{MaterialSectionKind::Rectangle,.2,long_path ? .25 : .2}},
  {2,1,MaterialEventKind::Travel,end,{3,4,.5},0,0,{}},
  {3,2,MaterialEventKind::Deposit,{3,4,.5},{6,8,.5},later,0,MaterialSection{MaterialSectionKind::Rectangle,.2,.2}}};
 const auto rates=verify_linear_rates(bytes,{0,0,.5},rate_policy());REQUIRE(rates.snapshot);
 const auto m=reconstruct_linear_material(rates.snapshot,rows,p);INFO(m.reason);REQUIRE(m.snapshot);return m;
}
}
TEST_CASE("B12 independent deposition geometry retains only simultaneous growing material and no future packets", "[Nonplanar][B12][FinalByteDepositionGeometry]")
{
 const auto material=deposition_material();const auto before=verify_linear_deposition_geometry(material.snapshot,0,1,travel_scene());INFO(before.reason);REQUIRE(before.snapshot);
 REQUIRE(before.snapshot->source==material.snapshot);REQUIRE(before.snapshot->prefix->pieces.empty());REQUIRE(before.snapshot->record_count==1);
 nptop_test::check_final_motion(*before.snapshot);
 const auto long_path=deposition_material(true);const auto blocked=verify_linear_deposition_geometry(long_path.snapshot,0,1,travel_scene());INFO(blocked.reason);REQUIRE(blocked.status==RateStatus::Fail);
 REQUIRE_FALSE(blocked.snapshot);REQUIRE(blocked.witness);REQUIRE(blocked.witness->material_event==1);REQUIRE(blocked.witness->progress.lower>0);
 nptop_test::check_deposition_witness(*long_path.snapshot,travel_scene(),*blocked.witness);
 REQUIRE_FALSE(verify_linear_deposition_geometry(material.snapshot,0,2,travel_scene()).snapshot);
 REQUIRE_FALSE(verify_linear_deposition_geometry(material.snapshot,1,1,travel_scene()).snapshot);
 auto low_head=travel_scene();low_head.head[4].local={{-.05,-.05,-.15},{.05,.05,-.1}};
 const auto fresh=verify_linear_deposition_geometry(material.snapshot,0,1,low_head);INFO(fresh.reason);REQUIRE(fresh.status==RateStatus::Fail);
 REQUIRE(fresh.witness);REQUIRE(fresh.witness->component==5);REQUIRE(fresh.witness->material_event==1);REQUIRE(fresh.witness->progress.lower>0);
 nptop_test::check_deposition_witness(*material.snapshot,low_head,*fresh.witness);
}
TEST_CASE("B12 independent deposition geometry keeps original guards and clears late positive and negative results", "[Nonplanar][B12][FinalByteDepositionGeometry]")
{
 const auto material=deposition_material();const auto scene=travel_scene();const auto normal=verify_linear_deposition_geometry(material.snapshot,0,1,scene);REQUIRE(normal.snapshot);
 auto caller=material.snapshot;auto head=scene;LinearTravelLimits captured;
 captured.cancelled=[&]{caller.reset();head.head.clear();captured.max_cells=0;return false;};
 const auto owned=verify_linear_deposition_geometry(caller,0,1,head,captured);REQUIRE(owned.snapshot);
 REQUIRE(owned.snapshot->source==material.snapshot);REQUIRE(owned.snapshot->scene.head.size()==6);
 for(int mode=0;mode<8;++mode){auto limits=LinearTravelLimits{};
  if(mode==0)limits.cancelled=[] {return true;};if(mode==1)limits.is_scene_current=[](uint64_t,uint64_t){return false;};
  if(mode==2)limits.is_source_current=[](uint64_t){return false;};if(mode==3)limits.is_rate_current=[](uint64_t,uint64_t){return false;};
  if(mode==4)limits.max_evaluations=normal.evaluations-1;if(mode==5)limits.max_cells=normal.cells-1;
  if(mode==6)limits.timeout=std::chrono::milliseconds(0);if(mode==7)limits.cancelled=[]()->bool {throw 1;};
  const auto refused=verify_linear_deposition_geometry(material.snapshot,0,1,scene,limits);INFO(mode<<' '<<refused.reason);
  REQUIRE(refused.status==RateStatus::Unknown);REQUIRE_FALSE(refused.snapshot);REQUIRE_FALSE(refused.witness);
 }
 for(bool collision:{false,true}){const auto m=deposition_material(collision);LinearTravelLimits l;size_t calls=0;
  l.cancelled=[&]{++calls;return false;};const auto complete=verify_linear_deposition_geometry(m.snapshot,0,1,scene,l);REQUIRE(complete.status==(collision ? RateStatus::Fail : RateStatus::Pass));
  const auto last=calls;calls=0;l.cancelled=[&]{return ++calls==last;};const auto late=verify_linear_deposition_geometry(m.snapshot,0,1,scene,l);
  REQUIRE(late.status==RateStatus::Unknown);REQUIRE_FALSE(late.snapshot);REQUIRE_FALSE(late.witness);REQUIRE(calls==last);
 }
}
namespace {
LinearFormingContactModel forming_model(const LinearMaterialSnapshot &source,const LinearTravelScene &scene)
{
 LinearFormingContactModel m;m.model_id=101;m.revision=1;m.profile_id=scene.profile_id;m.profile_revision=scene.revision;m.material_model_id=source.policy.model_id;
 m.working_radius_mm=.5;m.wake_length_mm=.65;m.max_top_above_tip_mm=.08;
 m.gap_min_mm=.1;m.gap_max_mm=.3;m.width_min_mm=.1;m.width_max_mm=.55;m.max_path_gradient=.1;return m;
}
}
TEST_CASE("B12 forming contact checks the whole rigid head and permits only the current run working face", "[Nonplanar][B12][FinalByteFormingContact]")
{
 const auto source=deposition_material(true);const auto scene=travel_scene();const auto model=forming_model(*source.snapshot,scene);
 const auto rigid=verify_linear_deposition_geometry(source.snapshot,0,1,scene);REQUIRE(rigid.status==RateStatus::Fail);REQUIRE(rigid.witness);
 const auto formed=verify_linear_forming_contact_geometry(source.snapshot,0,1,scene,model);INFO(formed.reason);REQUIRE(formed.snapshot);
 REQUIRE(formed.status==RateStatus::Pass);REQUIRE(formed.snapshot->source==source.snapshot);REQUIRE(formed.snapshot->contact.working_radius_mm==.5);
 REQUIRE(std::any_of(formed.snapshot->leaves.begin(),formed.snapshot->leaves.end(),[](const auto &leaf){return leaf.forming_contact;}));
 nptop_test::check_final_forming_contact(*formed.snapshot);
 auto low=scene;low.head[4].local={{-.05,-.05,-.15},{.05,.05,-.1}};
 const auto head=verify_linear_forming_contact_geometry(source.snapshot,0,1,low,model);REQUIRE(head.status==RateStatus::Fail);REQUIRE(head.witness);REQUIRE(head.witness->component==5);
 nptop_test::check_deposition_witness(*source.snapshot,low,*head.witness);
 auto obstacle=scene;obstacle.obstacles.push_back({{1.499,2.499,.549},{1.501,2.501,.551}});
 const auto stat=verify_linear_forming_contact_geometry(source.snapshot,0,1,obstacle,model);REQUIRE(stat.status==RateStatus::Fail);REQUIRE(stat.witness);REQUIRE(stat.witness->obstacle);
 REQUIRE_FALSE(verify_linear_forming_contact_geometry(source.snapshot,1,1,scene,model).snapshot);
 auto partial=model;partial.working_radius_mm=.25;
 const auto outside=verify_linear_forming_contact_geometry(source.snapshot,0,1,scene,partial);REQUIRE(outside.status!=RateStatus::Pass);REQUIRE_FALSE(outside.snapshot);
}
TEST_CASE("B12 forming contact rejects mismatched domains and stale late proofs without changing rigid admission", "[Nonplanar][B12][FinalByteFormingContact]")
{
 STATIC_REQUIRE_FALSE(std::is_aggregate<LinearFormingContactSnapshot>::value);
 const auto source=deposition_material(true);const auto scene=travel_scene();const auto model=forming_model(*source.snapshot,scene);
 for(int mode=0;mode<20;++mode){auto p=model;LinearFormingContactLimits limits;
  if(mode==0)p.profile_id++;if(mode==1)p.profile_revision++;if(mode==2)p.material_model_id++;if(mode==3)p.operator_confirmed_claim=true;
  if(mode==4)p.synthetic=false;if(mode==5)p.version++;if(mode==6)p.working_radius_mm=.51;if(mode==7)p.wake_length_mm=.05;
  if(mode==8)p.max_top_above_tip_mm=.01;if(mode==9)p.gap_min_mm=.21;if(mode==10)p.width_max_mm=.1;
  if(mode==11)p.max_path_gradient=.001;if(mode==12)limits.is_contact_current=[](uint64_t,uint64_t){return false;};
  if(mode==13)limits.max_cells=1;if(mode==14)limits.cancelled=[]()->bool {throw 1;};
  if(mode==15)limits.is_contact_current=[](uint64_t,uint64_t)->bool {throw 1;};
  if(mode==16)limits.is_contact_current=[](uint64_t,uint64_t){std::fesetround(FE_DOWNWARD);return true;};
  if(mode==17)limits.max_evaluations=source.snapshot->evaluations;
  if(mode==18)limits.timeout=std::chrono::milliseconds(0);if(mode==19)limits.cancelled=[] {return true;};
  const auto refused=verify_linear_forming_contact_geometry(source.snapshot,0,1,scene,p,limits);INFO(mode<<' '<<refused.reason);
  if(mode==16)REQUIRE(std::fesetround(FE_TONEAREST)==0);
  REQUIRE(refused.status==RateStatus::Unknown);REQUIRE_FALSE(refused.snapshot);REQUIRE_FALSE(refused.witness);
 }
 auto copy=model;auto head=scene;LinearFormingContactLimits limits;limits.cancelled=[&]{copy.working_radius_mm=5;head.head.clear();limits.max_cells=0;return false;};
 const auto owned=verify_linear_forming_contact_geometry(source.snapshot,0,1,head,copy,limits);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->contact.working_radius_mm==.5);REQUIRE(owned.snapshot->scene.head.size()==6);
 limits={};size_t calls=0;limits.cancelled=[&]{++calls;return false;};const auto normal=verify_linear_forming_contact_geometry(source.snapshot,0,1,scene,model,limits);REQUIRE(normal.snapshot);
 const auto last=calls;calls=0;limits.cancelled=[&]{return ++calls==last;};const auto late=verify_linear_forming_contact_geometry(source.snapshot,0,1,scene,model,limits);
 REQUIRE(late.status==RateStatus::Unknown);REQUIRE_FALSE(late.snapshot);REQUIRE_FALSE(late.witness);
}
namespace {
LinearMaterialResult forming_material(int mode=0,bool reverse=false,bool diagonal=false)
{
 auto p=material_policy();std::array<double,3> previous{0,0,.5};std::vector<MaterialDeclaration> rows;
 std::ostringstream bytes;bytes.imbue(std::locale::classic());bytes<<"G90\nM83\nM400\nM204 S4\n"<<std::fixed<<std::setprecision(9);
 const auto pose=[&](double t){return std::array<double,3>{t*(reverse ? -1 : 1)*(diagonal ? .6 : 1),diagonal ? .8*t : 0,.5+(mode==1 ? .02*t : 0)};};
 const auto append=[&](std::array<double,3> end,double e,double h0=.2,double h1=.2){bytes<<"G1 X"<<end[0]<<" Y"<<end[1]<<" Z"<<end[2];if(e)bytes<<" E"<<e;bytes<<" F30\nM400\n";
  const double volume=(High(e)*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)).convert_to<double>();
  rows.push_back({uint64_t(rows.size()+1),rows.size(),e ? MaterialEventKind::Deposit : MaterialEventKind::Travel,previous,end,volume,0,
   e ? std::optional<MaterialSection>(MaterialSection{MaterialSectionKind::Rectangle,h0,h1}) : std::nullopt});previous=end;};
 if(mode==2){append(pose(4),.16);append(pose(0),0);append(pose(4),.16);}
 else {append(pose(2),.08,.2,mode==1 ? .22 : .2);append(mode==3 ? std::array<double,3>{2,2,.5} : mode==4 ? pose(0) : pose(4),.08,mode==1 ? .22 : .2,mode==1 ? .24 : .2);}
 auto travel=previous;travel[2]+=.1;append(travel,0);travel[0]+=4;append(travel,.16);
 const auto rates=verify_linear_rates(bytes.str(),{0,0,.5},rate_policy());REQUIRE(rates.snapshot);return reconstruct_linear_material(rates.snapshot,rows,p);
}
}
TEST_CASE("B12 forming contact retains packet cuts and rejects old neighbors turns reversals and interrupted material", "[Nonplanar][B12][FinalByteFormingContact]")
{
 const auto scene=travel_scene();
 for(int mode:{0,1})for(bool reverse:{false,true})for(bool diagonal:{false,true}){
  const auto source=forming_material(mode,reverse,diagonal);REQUIRE(source.snapshot);const auto model=forming_model(*source.snapshot,scene);
  const auto result=verify_linear_forming_contact_geometry(source.snapshot,0,2,scene,model);INFO(mode<<' '<<reverse<<' '<<diagonal<<' '<<result.reason);REQUIRE(result.snapshot);
  nptop_test::check_final_forming_contact(*result.snapshot);
  REQUIRE_FALSE(verify_linear_forming_contact_geometry(source.snapshot,0,1,scene,model).snapshot);
  REQUIRE_FALSE(verify_linear_forming_contact_geometry(source.snapshot,1,1,scene,model).snapshot);
 }
 const auto old=forming_material(2);REQUIRE(old.snapshot);const auto model=forming_model(*old.snapshot,scene);
 const auto struck=verify_linear_forming_contact_geometry(old.snapshot,2,1,scene,model);REQUIRE(struck.status==RateStatus::Fail);REQUIRE(struck.witness);REQUIRE(struck.witness->material_event==1);
 nptop_test::check_deposition_witness(*old.snapshot,scene,*struck.witness);
 LinearFormingContactLimits limits;size_t calls=0;limits.cancelled=[&]{++calls;return false;};REQUIRE(verify_linear_forming_contact_geometry(old.snapshot,2,1,scene,model,limits).witness);
 const auto last=calls;calls=0;limits.cancelled=[&]{return ++calls==last;};const auto late=verify_linear_forming_contact_geometry(old.snapshot,2,1,scene,model,limits);
 REQUIRE(late.status==RateStatus::Unknown);REQUIRE_FALSE(late.snapshot);REQUIRE_FALSE(late.witness);
 for(int mode:{3,4}){const auto changed=forming_material(mode);REQUIRE(changed.snapshot);
  const auto refused=verify_linear_forming_contact_geometry(changed.snapshot,0,2,scene,forming_model(*changed.snapshot,scene));REQUIRE(refused.status==RateStatus::Unknown);REQUIRE_FALSE(refused.snapshot);}
}
TEST_CASE("B12 forming contact model JSON owns each bound and rejects duplicate missing unknown and ambiguous data", "[Nonplanar][B12][FinalByteFormingContact]")
{
 const auto source=deposition_material(true);const auto model=forming_model(*source.snapshot,travel_scene());const auto document=forming_contact_document(model);
 REQUIRE(forming_contact_document(parse_forming_contact_document(document.dump()))==document);
 for(const auto &item:document.items()){auto missing=document;missing.erase(item.key());REQUIRE_THROWS(parse_forming_contact_document(missing.dump()));
  auto duplicate=document.dump();const auto at=duplicate.find('\"'+item.key()+'\"');REQUIRE(at!=std::string::npos);
  duplicate.insert(at,'\"'+item.key()+"\":"+item.value().dump()+',');REQUIRE_THROWS(parse_forming_contact_document(duplicate));}
 for(const auto &key:{"model_id","profile_id","material_model_id","working_radius_mm","max_path_gradient"}){auto boolean=document;boolean[key]=true;REQUIRE_THROWS(parse_forming_contact_document(boolean.dump()));}
 auto unknown=document;unknown["allow_all_material"]=true;REQUIRE_THROWS(parse_forming_contact_document(unknown.dump()));
 auto nested=document;nested["working_radius_mm"]=nlohmann::json{{"value",.5}};REQUIRE_THROWS(parse_forming_contact_document(nested.dump()));
 auto version=document;version["version"]=2;REQUIRE_THROWS(parse_forming_contact_document(version.dump()));
}
namespace {
LinearMaterialResult polyline_material(const std::vector<std::array<double,3>> &points)
{
 auto p=material_policy();std::array<double,3> previous{0,0,.5};std::vector<MaterialDeclaration> rows;
 std::ostringstream bytes;bytes.imbue(std::locale::classic());bytes<<"G90\nM83\nM400\nM204 S4\n"<<std::fixed<<std::setprecision(9);
 const auto append=[&](std::array<double,3> end,bool deposit){const double length=std::hypot(end[0]-previous[0],end[1]-previous[1]);
  const double e=deposit ? std::round(.04*length*1e9)/1e9 : 0;
  bytes<<"G1 X"<<end[0]<<" Y"<<end[1]<<" Z"<<end[2];if(e)bytes<<" E"<<e;bytes<<" F30\nM400\n";
  const double volume=(High(e)*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)).convert_to<double>();
  rows.push_back({uint64_t(rows.size()+1),rows.size(),deposit ? MaterialEventKind::Deposit : MaterialEventKind::Travel,previous,end,volume,0,
   deposit ? std::optional<MaterialSection>(MaterialSection{MaterialSectionKind::Rectangle,.2,.2}) : std::nullopt});previous=end;};
 for(const auto &point:points)append(point,true);auto end=previous;end[2]+=.1;append(end,false);end[0]+=4;append(end,true);
 const auto rates=verify_linear_rates(bytes.str(),{0,0,.5},rate_policy());REQUIRE(rates.snapshot);return reconstruct_linear_material(rates.snapshot,rows,p);
}
LinearFormingContactModel polyline_model(const LinearMaterialSnapshot &source,const LinearTravelScene &scene)
{auto model=forming_model(source,scene);model.version=2;model.min_turn_cosine=0;
 // Right-angle geometry also includes the old bead's transverse half width:
 // sqrt(2)*.5 + .55/2 + 2*.0200001 < 1.05 mm, with the original errors.
 model.wake_length_mm=1.05;model.max_path_gradient=.064;return model;}
}
TEST_CASE("B12 polyline working contact covers every turn without resetting material age", "[Nonplanar][B12][FinalBytePolylineContact]")
{
 const auto scene=travel_scene();
 for(const auto &points:std::vector<std::vector<std::array<double,3>>>{{{2,0,.5},{2,2,.5}},{{-2,0,.5},{-2,-2,.5}},
   {{2,0,.54},{2,2,.58},{4,2,.62}},{{1.2,1.6,.5},{-.4,2.8,.5}}}){
  const auto source=polyline_material(points);REQUIRE(source.snapshot);
  const auto old=verify_linear_forming_contact_geometry(source.snapshot,0,points.size(),scene,forming_model(*source.snapshot,scene));REQUIRE(old.status==RateStatus::Unknown);
  const auto formed=verify_linear_forming_contact_geometry(source.snapshot,0,points.size(),scene,polyline_model(*source.snapshot,scene));INFO(formed.reason);REQUIRE(formed.snapshot);
  REQUIRE(std::any_of(formed.snapshot->leaves.begin(),formed.snapshot->leaves.end(),[](const auto &l){return l.contact_records.size()>1;}));
  nptop_test::check_final_forming_contact(*formed.snapshot);
 }
}
TEST_CASE("B12 polyline old seams and nearby earlier legs remain forbidden", "[Nonplanar][B12][FinalBytePolylineContact]")
{
 const auto scene=travel_scene();
 const auto corner=polyline_material({{2,0,.5},{2,2,.5}});REQUIRE(corner.snapshot);auto short_wake=polyline_model(*corner.snapshot,scene);short_wake.wake_length_mm=.8;short_wake.max_path_gradient=.08;
 const auto age=verify_linear_forming_contact_geometry(corner.snapshot,0,2,scene,short_wake);REQUIRE(age.status==RateStatus::Fail);REQUIRE(age.witness);REQUIRE(age.witness->material_event==1);
 nptop_test::check_polyline_contact_witness(*corner.snapshot,scene,short_wake,0,*age.witness);
 for(const auto &points:std::vector<std::vector<std::array<double,3>>>{{{2,0,.5},{2,2,.5},{0,2,.5},{0,0,.5}},{{4,0,.5},{4,.3,.5},{0,.3,.5}}}){
  const auto source=polyline_material(points);REQUIRE(source.snapshot);
  const auto refused=verify_linear_forming_contact_geometry(source.snapshot,0,points.size(),scene,polyline_model(*source.snapshot,scene));INFO(refused.reason);
  REQUIRE(refused.status==RateStatus::Fail);REQUIRE(refused.witness);REQUIRE(refused.witness->material_event==1);
  nptop_test::check_polyline_contact_witness(*source.snapshot,scene,polyline_model(*source.snapshot,scene),0,*refused.witness);
 }
 const auto source=polyline_material({{2,0,.5},{0,0,.5}});REQUIRE(source.snapshot);
 REQUIRE(verify_linear_forming_contact_geometry(source.snapshot,0,2,scene,polyline_model(*source.snapshot,scene)).status==RateStatus::Unknown);
 auto margin=scene;margin.clearance_mm[0]=.01;const auto only_unresolved=polyline_material({{4,0,.74},{4,.25,.74},{3.72,.25,.7232}});REQUIRE(only_unresolved.snapshot);
 auto model=polyline_model(*only_unresolved.snapshot,margin);model.max_top_above_tip_mm=.095;
 const auto incomplete=verify_linear_forming_contact_geometry(only_unresolved.snapshot,0,3,margin,model);
 INFO(incomplete.reason);REQUIRE(incomplete.status==RateStatus::Unknown);REQUIRE_FALSE(incomplete.snapshot);REQUIRE_FALSE(incomplete.witness);REQUIRE(incomplete.unresolved_cell);
}
TEST_CASE("B12 polyline contact version bounds and publication are explicit and fail closed", "[Nonplanar][B12][FinalBytePolylineContact]")
{
 const auto source=polyline_material({{2,0,.5},{2,2,.5}});REQUIRE(source.snapshot);const auto scene=travel_scene();const auto model=polyline_model(*source.snapshot,scene);
 const auto document=forming_contact_document(model);REQUIRE(document.size()==17);REQUIRE(forming_contact_document(parse_forming_contact_document(document.dump()))==document);
 for(const auto &item:document.items()){auto missing=document;missing.erase(item.key());REQUIRE_THROWS(parse_forming_contact_document(missing.dump()));
  auto duplicate=document.dump();duplicate.insert(duplicate.find('\"'+item.key()+'\"'),'\"'+item.key()+"\":"+item.value().dump()+',');REQUIRE_THROWS(parse_forming_contact_document(duplicate));}
 auto boolean=document;boolean["min_turn_cosine"]=true;REQUIRE_THROWS(parse_forming_contact_document(boolean.dump()));
 for(int mode=0;mode<7;++mode){auto copy=model;LinearFormingContactLimits limits;
  if(mode==0)copy.min_turn_cosine.reset();if(mode==1)copy.min_turn_cosine=-1;if(mode==2)copy.min_turn_cosine=1.01;
  if(mode==3)copy.min_turn_cosine=.01;if(mode==4)copy.version=1;if(mode==5)limits.max_cells=1;
  if(mode==6)limits.is_contact_current=[](uint64_t,uint64_t){return false;};
  const auto result=verify_linear_forming_contact_geometry(source.snapshot,0,2,scene,copy,limits);REQUIRE(result.status==RateStatus::Unknown);REQUIRE_FALSE(result.snapshot);REQUIRE_FALSE(result.witness);
 }
 size_t calls=0;LinearFormingContactLimits limits;limits.cancelled=[&]{++calls;return false;};
 const auto positive=verify_linear_forming_contact_geometry(source.snapshot,0,2,scene,model,limits);REQUIRE(positive.snapshot);
 const auto last=calls;calls=0;limits.cancelled=[&]{return ++calls==last;};const auto late=verify_linear_forming_contact_geometry(source.snapshot,0,2,scene,model,limits);
 REQUIRE(late.status==RateStatus::Unknown);REQUIRE_FALSE(late.snapshot);REQUIRE_FALSE(late.witness);
 auto caller_model=model;limits.cancelled=[&]{caller_model.min_turn_cosine=1;return false;};
 const auto captured=verify_linear_forming_contact_geometry(source.snapshot,0,2,scene,caller_model,limits);REQUIRE(captured.snapshot);
 REQUIRE(captured.snapshot->contact.min_turn_cosine==0);nptop_test::check_final_forming_contact(*captured.snapshot);
 const auto closed=polyline_material({{2,0,.5},{2,2,.5},{0,2,.5},{0,0,.5}});REQUIRE(closed.snapshot);
 calls=0;limits.cancelled=[&]{++calls;return false;};const auto negative=verify_linear_forming_contact_geometry(closed.snapshot,0,4,scene,model,limits);
 REQUIRE(negative.status==RateStatus::Fail);REQUIRE(negative.witness);const auto negative_last=calls;
 calls=0;limits.cancelled=[&]{return ++calls==negative_last;};const auto late_negative=verify_linear_forming_contact_geometry(closed.snapshot,0,4,scene,model,limits);
 REQUIRE(late_negative.status==RateStatus::Unknown);REQUIRE_FALSE(late_negative.snapshot);REQUIRE_FALSE(late_negative.witness);
 const auto obtuse=polyline_material({{2,0,.5},{1,2,.5}});REQUIRE(obtuse.snapshot);auto turning=polyline_model(*obtuse.snapshot,scene);
 turning.min_turn_cosine=-.6;turning.wake_length_mm=1.6;turning.max_path_gradient=.03;
 const auto accepted=verify_linear_forming_contact_geometry(obtuse.snapshot,0,2,scene,turning);INFO(accepted.reason);REQUIRE(accepted.snapshot);
 nptop_test::check_final_forming_contact(*accepted.snapshot);
 turning.min_turn_cosine=-.4;REQUIRE(verify_linear_forming_contact_geometry(obtuse.snapshot,0,2,scene,turning).status==RateStatus::Unknown);
}
namespace {
LinearSupportedDepositionPolicy supported_policy(size_t first=2)
{LinearSupportedDepositionPolicy p;p.policy_id=111;p.revision=1;p.join=join_policy();p.runs.push_back({first,first,support_policy()});return p;}
LinearMaterialResult supported_polyline_material()
{
 std::array<double,3> previous{0,0,0};std::vector<MaterialDeclaration> rows;
 std::ostringstream bytes;bytes.imbue(std::locale::classic());bytes<<"G90\nM83\nM400\nM204 S4\n"<<std::fixed<<std::setprecision(9);
 const auto append=[&](std::array<double,3> end,double e){bytes<<"G1 X"<<end[0]<<" Y"<<end[1]<<" Z"<<end[2];if(e)bytes<<" E"<<e;bytes<<" F30\nM400\n";
  const double volume=(High(e)*acos(High(-1))*High("1.75")*High("1.75")/4/High(rate_policy().flow)).convert_to<double>();
  rows.push_back({uint64_t(rows.size()+1),rows.size(),e ? MaterialEventKind::Deposit : MaterialEventKind::Travel,previous,end,volume,0,
   e ? std::optional<MaterialSection>(MaterialSection{MaterialSectionKind::Rectangle,.2,.2}) : std::nullopt});previous=end;};
 append({3,0,0},.4);append({.5,0,.2},0);append({1.5,0,.2},.04);append({1.5,.3,.2},.012);append({1.5,.3,.3},0);
 const auto rates=verify_linear_rates(bytes.str(),{0,0,0},rate_policy());REQUIRE(rates.snapshot);return reconstruct_linear_material(rates.snapshot,rows,material_policy());
}
}
TEST_CASE("B12 complete supported deposition owns geometry and underlying support for final bytes", "[Nonplanar][B12][FinalByteSupportedDeposition]")
{
 STATIC_REQUIRE_FALSE(std::is_aggregate<LinearSupportedDepositionSnapshot>::value);
 for(bool diagonal:{false,true})for(bool reverse:{false,true}){
  const auto joined=support_fixture(0,diagonal,reverse);const auto source=joined->source->source;const auto scene=travel_scene();
  const auto result=verify_linear_supported_deposition(source,2,1,scene,forming_model(*source,scene),supported_policy());INFO(result.reason);
  REQUIRE(result.snapshot);REQUIRE(result.status==RateStatus::Pass);REQUIRE_FALSE(result.geometry_witness);REQUIRE_FALSE(result.support_witness);
  const auto &proof=*result.snapshot;REQUIRE(proof.geometry->source==source);REQUIRE(proof.target->source->source==source);REQUIRE(proof.support.size()==1);
  REQUIRE(proof.support.front()->source==proof.target);REQUIRE(proof.support.front()->support->source->completed_records==2);
  REQUIRE(proof.evaluations>=proof.geometry->evaluations);REQUIRE(proof.cells==proof.geometry->cells+proof.support.front()->cells);
  nptop_test::check_supported_deposition(proof);
 }
 const auto material=supported_polyline_material();REQUIRE(material.snapshot);const auto scene=travel_scene();auto policy=supported_policy();
 policy.runs.front().policy.cross_slope=0;policy.runs.push_back({3,3,policy.runs.front().policy});
 const auto contact=polyline_model(*material.snapshot,scene);
 REQUIRE(verify_linear_forming_contact_geometry(material.snapshot,2,2,scene,contact).snapshot);
 const auto prefix=linear_material_at(material.snapshot,4,0);REQUIRE(prefix.snapshot);const auto runs=reconstruct_joined_linear_material(prefix.snapshot,policy.join);REQUIRE(runs.snapshot);
 const auto naive=verify_linear_run_support(runs.snapshot,2,policy.runs.back().policy);REQUIRE(naive.status==RateStatus::Fail);REQUIRE(naive.reason=="FINAL_RUN_VERTICAL_GAP_TOO_SMALL");
 const auto complete=verify_linear_supported_deposition(material.snapshot,2,2,scene,contact,policy);INFO(complete.reason);REQUIRE(complete.snapshot);
 REQUIRE(complete.snapshot->support.size()==2);for(const auto &s:complete.snapshot->support){REQUIRE(s->support->source->completed_records==2);nptop_test::check_complete_run_support(*s);}
 nptop_test::check_supported_deposition(*complete.snapshot);
}
TEST_CASE("B12 supported deposition cannot promote unsupported motion or hide head collision", "[Nonplanar][B12][FinalByteSupportedDeposition]")
{
 for(int mode:{1,2,3,4}){const auto joined=support_fixture(mode);const auto source=joined->source->source;const auto scene=travel_scene();
  const auto geometry=verify_linear_forming_contact_geometry(source,2,1,scene,forming_model(*source,scene));REQUIRE(geometry.snapshot);
  const auto result=verify_linear_supported_deposition(source,2,1,scene,forming_model(*source,scene),supported_policy());INFO(result.reason);
  REQUIRE(result.status==RateStatus::Fail);REQUIRE_FALSE(result.snapshot);REQUIRE(result.support_witness);REQUIRE_FALSE(result.geometry_witness);
  REQUIRE(result.failed_run==0);REQUIRE(result.support_witness->target_record==2);
 }
 const auto source=support_fixture()->source->source;auto scene=travel_scene();scene.obstacles.push_back({{1.49,-.01,.20},{1.51,.01,.22}});
 const auto hit=verify_linear_supported_deposition(source,2,1,scene,forming_model(*source,scene),supported_policy());REQUIRE(hit.status==RateStatus::Fail);
 REQUIRE_FALSE(hit.snapshot);REQUIRE(hit.geometry_witness);REQUIRE_FALSE(hit.support_witness);REQUIRE(hit.geometry_witness->obstacle==0);
}
TEST_CASE("B12 supported deposition shares root budgets and revokes every component at publication", "[Nonplanar][B12][FinalByteSupportedDeposition]")
{
 const auto source=support_fixture()->source->source;const auto scene=travel_scene();const auto contact=forming_model(*source,scene);const auto policy=supported_policy();
 for(int mode=0;mode<15;++mode){auto p=policy;LinearSupportedDepositionLimits limits;
  if(mode==0)p.runs.clear();if(mode==1)p.runs.front().first_record=1;if(mode==2)p.runs.front().last_record=3;
  if(mode==3)p.runs.push_back(p.runs.front());if(mode==4)p.version=2;if(mode==5)p.policy_id=0;
  if(mode==6)limits.is_supported_current=[](uint64_t,uint64_t){return false;};if(mode==7)limits.is_support_current=[](uint64_t,uint64_t){return false;};
  if(mode==8)limits.is_join_current=[](uint64_t,uint64_t){return false;};if(mode==9)limits.is_scene_current=[](uint64_t,uint64_t){return false;};
  if(mode==10)limits.is_contact_current=[](uint64_t,uint64_t){return false;};if(mode==11)limits.max_cells=8;
  if(mode==12)limits.max_evaluations=source->evaluations+100;if(mode==13)limits.timeout=std::chrono::milliseconds(0);
  if(mode==14)limits.cancelled=[] {throw 7;return false;};
  const auto r=verify_linear_supported_deposition(source,2,1,scene,contact,p,limits);REQUIRE(r.status==RateStatus::Unknown);REQUIRE_FALSE(r.snapshot);REQUIRE_FALSE(r.geometry_witness);REQUIRE_FALSE(r.support_witness);
 }
 auto caller=policy;LinearSupportedDepositionLimits limits;limits.cancelled=[&]{caller.runs.clear();return false;};
 const auto captured=verify_linear_supported_deposition(source,2,1,scene,contact,caller,limits);REQUIRE(captured.snapshot);REQUIRE(captured.snapshot->policy.runs.size()==1);
 limits={};size_t calls=0;limits.cancelled=[&]{++calls;return false;};const auto positive=verify_linear_supported_deposition(source,2,1,scene,contact,policy,limits);REQUIRE(positive.snapshot);
 const auto last=calls;
 for(int mode=0;mode<3;++mode){calls=0;bool support_current=true;limits.is_support_current=[&](uint64_t,uint64_t){return support_current;};
  limits.cancelled=[&]{if(++calls!=last)return false;if(mode==1){std::fesetround(FE_UPWARD);return false;}if(mode==2){support_current=false;return false;}return true;};
  const auto late=verify_linear_supported_deposition(source,2,1,scene,contact,policy,limits);std::fesetround(FE_TONEAREST);
  REQUIRE(late.status==RateStatus::Unknown);REQUIRE_FALSE(late.snapshot);REQUIRE_FALSE(late.geometry_witness);REQUIRE_FALSE(late.support_witness);
 }
 const auto missing=support_fixture(1)->source->source;limits={};calls=0;limits.cancelled=[&]{++calls;return false;};
 const auto negative=verify_linear_supported_deposition(missing,2,1,scene,contact,policy,limits);REQUIRE(negative.support_witness);const auto negative_last=calls;
 calls=0;limits.cancelled=[&]{return ++calls==negative_last;};const auto late_negative=verify_linear_supported_deposition(missing,2,1,scene,contact,policy,limits);
 REQUIRE(late_negative.status==RateStatus::Unknown);REQUIRE_FALSE(late_negative.snapshot);REQUIRE_FALSE(late_negative.support_witness);REQUIRE_FALSE(late_negative.geometry_witness);
 const auto multi=supported_polyline_material();REQUIRE(multi.snapshot);auto two=policy;two.runs.front().policy.cross_slope=0;
 two.runs.push_back({3,3,two.runs.front().policy});two.runs.back().policy.policy_id=42;bool first_current=true;size_t second_checks=0;limits={};
 limits.is_support_current=[&](uint64_t id,uint64_t){if(id==42 && ++second_checks==2)first_current=false;return id!=41 || first_current;};
 const auto stale_first=verify_linear_supported_deposition(multi.snapshot,2,2,scene,polyline_model(*multi.snapshot,scene),two,limits);
 REQUIRE(stale_first.status==RateStatus::Unknown);REQUIRE_FALSE(stale_first.snapshot);REQUIRE_FALSE(stale_first.support_witness);REQUIRE_FALSE(stale_first.geometry_witness);
}
TEST_CASE("B12 supported deposition diagnostic schema rejects ambiguous or incomplete obligations", "[Nonplanar][B12][FinalByteSupportedDeposition]")
{
 const auto j=supported_deposition_document(supported_policy());REQUIRE(supported_deposition_document(parse_supported_deposition_document(j.dump()))==j);
 for(const auto &path:std::vector<std::string>{"","/join","/runs/0","/runs/0/policy"}){
  const auto &object=path.empty() ? j : j.at(nlohmann::json::json_pointer(path));
  for(const auto &item:object.items()){auto missing=j;nlohmann::json::json_pointer p(path);
   (path.empty() ? missing : missing.at(p)).erase(item.key());REQUIRE_THROWS(parse_supported_deposition_document(missing.dump()));
   auto duplicate=j.dump();const auto object_text=object.dump();const auto at=duplicate.find(object_text);REQUIRE(at!=std::string::npos);
   duplicate.insert(at+object_text.size()-1,",\""+item.key()+"\":"+item.value().dump());REQUIRE_THROWS(parse_supported_deposition_document(duplicate));
  }
 }
 for(const auto &path:{"/version","/policy_id","/runs/0/first_record","/runs/0/policy/cross_slope","/runs/0/policy/vertical_min"}){
  auto boolean=j;boolean[nlohmann::json::json_pointer(path)]=true;REQUIRE_THROWS(parse_supported_deposition_document(boolean.dump()));}
 auto extra=j;extra["allow_old_material"]=true;REQUIRE_THROWS(parse_supported_deposition_document(extra.dump()));
 auto empty=j;empty["runs"]=nlohmann::json::array();REQUIRE_THROWS(parse_supported_deposition_document(empty.dump()));
}
