#include <catch2/catch_test_macros.hpp>
#include <nonplanar_verify/LinearMaterial.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <cfenv>
#include <iomanip>
#include <type_traits>
#include "final_material_oracle.hpp"
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
