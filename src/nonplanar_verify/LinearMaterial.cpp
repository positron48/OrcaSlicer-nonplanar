#include "LinearMaterial.hpp"
#include "Exact.hpp"
#include <algorithm>
#include <deque>
#include <set>
namespace nptop_verify {
namespace {
using namespace exact;
struct Refusal : std::runtime_error {RateStatus status;Refusal(RateStatus s,const char *r):std::runtime_error(r),status(s){}};
void unknown(const char *r){throw Refusal(RateStatus::Unknown,r);}
void fail(const char *r){throw Refusal(RateStatus::Fail,r);}
struct ExactStep {std::array<Q,3> start,end;Q e=0,xy2=0;Range nominal{0,0},delivered{0,0};};
RateBounds signed_bound(const Q &v,const std::function<void()> &work)
{if(v>=0)return enclose(v,false,work);const auto b=enclose(-v,false,work);return {-b.upper,-b.lower};}
RateBounds bound(Range v,const std::function<void()> &work){return {signed_bound(v.lo,work).lower,signed_bound(v.hi,work).upper};}
struct Work {
 const LinearRateSnapshot &rates;const LinearMaterialPolicy &policy;const LinearMaterialLimits limits;size_t &evaluations;
 const std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
 void stop() const {
  if(limits.cancelled && limits.cancelled())unknown("CANCELLED");
  if(limits.is_current && !limits.is_current(policy.policy_id,policy.revision))unknown("STALE_MATERIAL_POLICY");
  if(limits.is_source_current && !limits.is_source_current(policy.source_revision))unknown("STALE_MATERIAL_SOURCE");
  if(limits.is_rate_current && !limits.is_rate_current(rates.policy.profile_id,rates.policy.revision))unknown("STALE_MATERIAL_RATE_POLICY");
  if(std::fegetround()!=FE_TONEAREST)unknown("UNSUPPORTED_MATERIAL_ROUNDING");
  volatile double normal=std::numeric_limits<double>::min(),subnormal=std::numeric_limits<double>::denorm_min();
  if(normal/2==0 || subnormal+subnormal==0)unknown("UNSUPPORTED_MATERIAL_UNDERFLOW");
  if(std::chrono::steady_clock::now()-started>=limits.timeout)unknown("FINAL_MATERIAL_DEADLINE");
 }
 void operator()(){if(++evaluations>limits.max_evaluations)unknown("FINAL_MATERIAL_WORK_LIMIT");stop();}
 void admission() const {
  if(!limits.max_bytes || limits.max_bytes>32*1024*1024 || rates.bytes.size()>limits.max_bytes || !limits.max_events || limits.max_events>200000 ||
   rates.moves.size()>limits.max_events || !limits.max_evaluations || limits.max_evaluations>2000000 || evaluations>=limits.max_evaluations ||
   limits.timeout.count()<=0 || limits.timeout>std::chrono::seconds(30))unknown("INVALID_FINAL_MATERIAL_LIMITS");
 }
};
void validate_policy(const LinearMaterialPolicy &p)
{
 if(p.version!=linear_material_version || !p.model_id || !p.policy_id || !p.revision || !p.source_revision || !p.synthetic || p.operator_confirmed_claim ||
  p.source_fingerprint.size()!=64 || !std::all_of(p.source_fingerprint.begin(),p.source_fingerprint.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))unknown("UNSUPPORTED_FINAL_MATERIAL_POLICY");
 for(double v : {p.outer_xy_growth_mm,p.outer_z_growth_mm,p.inner_xy_loss_mm,p.inner_z_loss_mm,p.numerical_coordinate_error_mm,
  p.max_coordinate_delta_mm,p.max_nominal_delta_mm3,p.max_total_nominal_delta_mm3,p.max_filament_delta_mm,p.relative_dose_error,p.absolute_dose_error_mm3})
  if(!std::isfinite(v) || v<0 || v>1000000)unknown("INVALID_FINAL_MATERIAL_POLICY");
 if(p.numerical_coordinate_error_mm>.05 || p.max_coordinate_delta_mm>.05 || p.relative_dose_error>=1)unknown("INVALID_FINAL_MATERIAL_POLICY");
}
Range width(Range area,const Q &hmin,const Q &hmax,MaterialSectionKind kind,Range pi)
{
 if(area.lo<=0)unknown("UNSUPPORTED_EMPTY_DELIVERED_SECTION");
 if(kind==MaterialSectionKind::RoundedRectangle && area.lo<=pi.hi*square(hmax)/4)unknown("UNSUPPORTED_FINAL_ROUNDED_SECTION");
 Range result{area.lo/hmax,area.hi/hmin};
 if(kind==MaterialSectionKind::RoundedRectangle){result.lo+=(1-pi.hi/4)*hmax;result.hi+=(1-pi.lo/4)*hmin;}
 if(result.hi>10000)unknown("FINAL_MATERIAL_WIDTH_DOMAIN");return result;
}
ReplayedBead bead(const ExactStep &s,const MaterialDeclaration &row,const LinearMaterialPolicy &p,Range pi,const Q &progress,const std::function<void()> &work)
{
 const auto full_length=enclose(s.xy2,true,work);
 if(full_length.lower<=0)unknown("UNCERTAIN_FINAL_XY_LENGTH");
 const Q lo=binary(full_length.lower),hi=binary(full_length.upper);
 const Range area{s.nominal.lo/hi,s.nominal.hi/lo},delivered{s.delivered.lo/hi,s.delivered.hi/lo};
 const Q h0=binary(row.section->gap_begin_mm),dh=binary(row.section->gap_end_mm)-h0,h1=h0+progress*dh;
 const Q hmin=std::min(h0,h1),hmax=std::max(h0,h1);
 const auto nw=width(area,hmin,hmax,row.section->kind,pi),dw=width(delivered,hmin,hmax,row.section->kind,pi);
 ReplayedBead b;b.record=row.sequence_index;b.event_id=row.event_id;
 std::array<Q,3> end;for(size_t axis=0;axis<3;++axis){work();end[axis]=s.start[axis]+progress*(s.end[axis]-s.start[axis]);b.start[axis]=signed_bound(s.start[axis],work);b.end[axis]=signed_bound(end[axis],work);}
 b.xy_length=bound({lo*progress,hi*progress},work);b.nominal_volume=bound({s.nominal.lo*progress,s.nominal.hi*progress},work);
 b.delivered_volume=bound({s.delivered.lo*progress,s.delivered.hi*progress},work);b.nominal_area=bound(area,work);b.delivered_area=bound(delivered,work);
 b.nominal_width=bound(nw,work);b.delivered_width=bound(dw,work);
 const Q xy=binary(p.outer_xy_growth_mm)+binary(p.numerical_coordinate_error_mm),z=binary(p.outer_z_growth_mm)+binary(p.numerical_coordinate_error_mm);
 const Q dz=s.end[2]-s.start[2],shift=xy/lo;
 // Broad-phase only. Include rotated along/normal growth and varying-section
 // slope allowances; these boxes never become filled material/support solids.
 const Q extra_xy=2*xy+z+(2*abs(dz)+2*abs(dh)+delivered.hi/square(hmin)*abs(dh)+pi.hi*abs(dh))*shift;
 for(size_t axis=0;axis<2;++axis){
  const Q low=std::min(s.start[axis],end[axis]),high=std::max(s.start[axis],end[axis]);
  b.nominal_bounds.coordinate[axis]=bound({low-nw.hi/2,high+nw.hi/2},work);
  b.upper_bounds.coordinate[axis]=bound({low-dw.hi/2-extra_xy,high+dw.hi/2+extra_xy},work);
 }
 const Q bottom=std::min(s.start[2]-h0,end[2]-h1),top=std::max(s.start[2],end[2]);
 const Q extra_z=xy+z+2*(abs(dz)+abs(dh))*shift;
 b.nominal_bounds.coordinate[2]=bound({bottom,top},work);b.upper_bounds.coordinate[2]=bound({bottom-extra_z,top+extra_z},work);
 const Q loss=binary(p.inner_xy_loss_mm)+binary(p.numerical_coordinate_error_mm);
 b.empty_inner=s.xy2*square(progress)<=square(2*loss);return b;
}
}
struct MaterialReplayData {std::vector<ExactStep> steps;Range pi;};
LinearMaterialResult reconstruct_linear_material(std::shared_ptr<const LinearRateSnapshot> rates,const std::vector<MaterialDeclaration> &requested,
 const LinearMaterialPolicy &requested_policy,const LinearMaterialLimits &requested_limits)
{
 LinearMaterialResult result;
 if(requested_policy.source_fingerprint.size()!=64){result.reason="UNSUPPORTED_FINAL_MATERIAL_POLICY";return result;}
 const auto policy=requested_policy;const auto limits=requested_limits;
 try {
  if(!rates)unknown("MISSING_FINAL_RATE_PROOF");result.evaluations=std::max(rates->evaluations,limits.initial_evaluations);
  Work work{*rates,policy,limits,result.evaluations};work.admission();
  if(requested.size()!=rates->moves.size())fail("FINAL_MATERIAL_RECORD_COUNT");
  const auto rows=requested;validate_policy(policy);work();
  auto data=std::make_shared<MaterialReplayData>();data->pi=pi_bounds();data->steps.reserve(rows.size());
  std::vector<std::optional<ReplayedBead>> beads;beads.reserve(rows.size());std::set<uint64_t> ids;
  std::istringstream input(rates->bytes);std::string line,command,word;for(unsigned i=0;i<4;++i)std::getline(input,line);
  std::array<Q,3> position{binary(rates->initial_position[0]),binary(rates->initial_position[1]),binary(rates->initial_position[2])};
  Range total_n{0,0},total_d{0,0},total_error{0,0};Q maximum_error=0;
  const Q filament_scale=square(binary(rates->policy.filament_diameter))/(4*binary(rates->policy.flow));
  const Q pose_delta2=square(binary(policy.max_coordinate_delta_mm)),dose_delta=binary(policy.max_nominal_delta_mm3);
  for(size_t i=0;i<rows.size();++i){work();result.record=i;const auto &r=rows[i];
   if(!r.event_id || r.sequence_index!=i || !ids.insert(r.event_id).second)unknown("INVALID_FINAL_MATERIAL_OWNER");
   for(double v : {r.expected_nominal_volume_mm3,r.expected_filament_mm})if(!std::isfinite(v) || v<0 || v>1000000)unknown("INVALID_FINAL_MATERIAL_DECLARATION");
   ExactStep s;s.start=position;s.end=position;std::getline(input,line);std::istringstream row(line);row>>command;
   while(row>>word){const auto axis=std::string("XYZ").find(word[0]);if(axis!=std::string::npos)s.end[axis]=decimal(word.substr(1));else if(word[0]=='E')s.e=decimal(word.substr(1));}
   std::getline(input,line);
   const auto parsed=rates->moves[i].kind;
   const auto kind=parsed==FullStopKind::Dwell ? MaterialEventKind::Dwell : parsed==FullStopKind::Pressure ? (s.e<0 ? MaterialEventKind::Retraction : MaterialEventKind::Restore) : s.e>0 ? MaterialEventKind::Deposit : MaterialEventKind::Travel;
   if(r.kind!=kind)fail("FINAL_MATERIAL_ROLE_MISMATCH");
   Q start_error=0,end_error=0;
   for(size_t axis=0;axis<3;++axis){work();for(double v : {r.start[axis],r.end[axis]})if(!std::isfinite(v) || std::abs(v)>10000)unknown("INVALID_FINAL_MATERIAL_POSE");
    start_error+=square(s.start[axis]-binary(r.start[axis]));end_error+=square(s.end[axis]-binary(r.end[axis]));}
   if(start_error>pose_delta2 || end_error>pose_delta2)fail("FINAL_MATERIAL_SOURCE_POSE_DELTA");
   position=s.end;
   if(kind!=MaterialEventKind::Deposit){
    if(r.section || r.expected_nominal_volume_mm3!=0)fail("FINAL_MATERIAL_PHANTOM_DECLARATION");
    if(abs(abs(s.e)-binary(r.expected_filament_mm))>binary(policy.max_filament_delta_mm))fail("FINAL_MATERIAL_PRESSURE_DELTA");
    data->steps.push_back(std::move(s));beads.push_back({});continue;
   }
   if(!r.section || r.expected_nominal_volume_mm3<=0 || r.expected_filament_mm!=0)unknown("INVALID_FINAL_MATERIAL_SECTION");
   const auto &section=*r.section;
   if((section.kind!=MaterialSectionKind::Rectangle && section.kind!=MaterialSectionKind::RoundedRectangle) ||
    !std::isfinite(section.gap_begin_mm) || !std::isfinite(section.gap_end_mm) || section.gap_begin_mm<=0 || section.gap_end_mm<=0 ||
    section.gap_begin_mm>10000 || section.gap_end_mm>10000)unknown("INVALID_FINAL_MATERIAL_SECTION");
   s.xy2=square(s.end[0]-s.start[0])+square(s.end[1]-s.start[1]);if(s.xy2<=0)unknown("FINAL_MATERIAL_REQUIRES_XY_LENGTH");
   s.nominal={s.e*filament_scale*data->pi.lo,s.e*filament_scale*data->pi.hi};
   const Q expected=binary(r.expected_nominal_volume_mm3);
   if(s.nominal.hi<expected-dose_delta || s.nominal.lo>expected+dose_delta)fail("FINAL_MATERIAL_NOMINAL_DOSE_DELTA");
   if(s.nominal.lo<expected-dose_delta || s.nominal.hi>expected+dose_delta)unknown("UNCERTAIN_FINAL_MATERIAL_DOSE_DELTA");
   const Q error_hi=std::max(abs(s.nominal.lo-expected),abs(s.nominal.hi-expected));
   const Q error_lo=s.nominal.lo<=expected && expected<=s.nominal.hi ? Q(0) : std::min(abs(s.nominal.lo-expected),abs(s.nominal.hi-expected));
   maximum_error=std::max(maximum_error,error_hi);total_error.lo+=error_lo;total_error.hi+=error_hi;
   const Q relative=binary(policy.relative_dose_error),absolute=binary(policy.absolute_dose_error_mm3);
   s.delivered={std::max(Q(0),s.nominal.lo*(1-relative)-absolute),s.nominal.hi*(1+relative)+absolute};
   beads.push_back(bead(s,r,policy,data->pi,Q(1),[&]{work();}));
   total_n.lo+=s.nominal.lo;total_n.hi+=s.nominal.hi;total_d.lo+=s.delivered.lo;total_d.hi+=s.delivered.hi;data->steps.push_back(std::move(s));
  }
  const Q total_budget=binary(policy.max_total_nominal_delta_mm3);
  if(total_error.lo>total_budget)fail("FINAL_MATERIAL_TOTAL_DOSE_DELTA");if(total_error.hi>total_budget)unknown("UNCERTAIN_FINAL_MATERIAL_TOTAL_DOSE_DELTA");
  const std::function<void()> poll=[&]{work();};const auto n=bound(total_n,poll),d=bound(total_d,poll),error=bound({0,maximum_error},poll),total=bound(total_error,poll);work.stop();
  result.snapshot=std::shared_ptr<const LinearMaterialSnapshot>(new LinearMaterialSnapshot(rates,rows,policy,std::move(beads),n,d,error,total,result.evaluations,std::move(data)));work.stop();
  result.status=RateStatus::Pass;result.record.reset();result.reason="DECLARED_FINAL_BYTE_MATERIAL_RECONSTRUCTED_CONTACT_SUPPORT_JOB_PENDING";
 }catch(const Refusal &e){result.snapshot.reset();result.status=e.status;result.reason=e.what();}
 catch(const std::exception &e){result.snapshot.reset();result.status=RateStatus::Unknown;result.reason=e.what();}return result;
}
LinearMaterialPrefixResult linear_material_at(std::shared_ptr<const LinearMaterialSnapshot> source,size_t completed,double progress,const LinearMaterialLimits &requested_limits)
{
 const auto limits=requested_limits;LinearMaterialPrefixResult result;
 try {
  if(!source)unknown("MISSING_FINAL_MATERIAL_PROOF");result.evaluations=std::max(source->evaluations,limits.initial_evaluations);
  Work work{*source->rates,source->policy,limits,result.evaluations};work.admission();work();
  if(completed>source->beads.size() || !std::isfinite(progress) || progress<0 || progress>1 || (completed==source->beads.size() && progress!=0))unknown("INVALID_FINAL_MATERIAL_PREFIX");
  std::vector<ReplayedBead> pieces;Range nominal{0,0},delivered{0,0};
  for(size_t i=0;i<completed+(progress>0);++i){work();if(!source->beads[i])continue;const Q fraction=i<completed ? Q(1) : binary(progress);
   const auto &s=source->exact->steps[i];pieces.push_back(bead(s,source->declarations[i],source->policy,source->exact->pi,fraction,[&]{work();}));
   nominal.lo+=s.nominal.lo*fraction;nominal.hi+=s.nominal.hi*fraction;delivered.lo+=s.delivered.lo*fraction;delivered.hi+=s.delivered.hi*fraction;
  }
  const std::function<void()> poll=[&]{work();};const auto n=bound(nominal,poll),d=bound(delivered,poll);work.stop();
  result.snapshot=std::shared_ptr<const LinearMaterialPrefixSnapshot>(new LinearMaterialPrefixSnapshot(source,completed,progress,std::move(pieces),n,d,result.evaluations));work.stop();
  result.status=RateStatus::Pass;result.reason="ACTUAL_FINAL_BYTE_PREFIX_ONLY_SUPPORT_CONTACT_PENDING";
 }catch(const Refusal &e){result.snapshot.reset();result.status=e.status;result.reason=e.what();}
 catch(const std::exception &e){result.snapshot.reset();result.status=RateStatus::Unknown;result.reason=e.what();}return result;
}
namespace {
Range affine(const Q &base,const Q &delta,Range t)
{return delta>=0 ? Range{base+delta*t.lo,base+delta*t.hi} : Range{base+delta*t.hi,base+delta*t.lo};}
Range absolute(Range v)
{return {v.lo<=0 && v.hi>=0 ? Q(0) : std::min(abs(v.lo),abs(v.hi)),std::max(abs(v.lo),abs(v.hi))};}
struct SolidProjection {Range t,normal,z;};
void valid_region(const MaterialRegion &b)
{
 for(size_t axis=0;axis<3;++axis)
  if(!std::isfinite(b.min[axis]) || !std::isfinite(b.max[axis]) || b.min[axis]>b.max[axis] ||
   std::abs(b.min[axis])>10000 || std::abs(b.max[axis])>10000)unknown("INVALID_FINAL_MATERIAL_REGION");
}
SolidProjection project_solid(const ExactStep &s,Range length,const MaterialRegion &box,const std::function<void()> &work)
{
  const Q dx=s.end[0]-s.start[0],dy=s.end[1]-s.start[1];Range t{0,0},normal{0,0};bool first=true;
  for(double x:{box.min[0],box.max[0]})for(double y:{box.min[1],box.max[1]}) {
   work();const Q px=binary(x)-s.start[0],py=binary(y)-s.start[1],u=(dx*px+dy*py)/s.xy2,v=dx*py-dy*px;
   if(first){t={u,u};normal={v,v};first=false;}
   else {t.lo=std::min(t.lo,u);t.hi=std::max(t.hi,u);normal.lo=std::min(normal.lo,v);normal.hi=std::max(normal.hi,v);}
  }
  const auto divide=[](Range v,Range l) {
   return Range{v.lo<0 ? v.lo/l.lo : v.lo/l.hi,v.hi<0 ? v.hi/l.hi : v.hi/l.lo};
  };
  return {t,divide(normal,length),{binary(box.min[2]),binary(box.max[2])}};
}
struct SolidQuery {
 const LinearMaterialPrefixSnapshot &prefix;const MaterialReplayData &data;MaterialRepresentation representation;Work &work;
 MaterialMembership section(const ExactStep &s,const MaterialDeclaration &row,Range length,const Q &progress,
  SolidProjection p,bool interior)
 {
  work();
  if(p.t.hi<0 || p.t.lo>progress)return MaterialMembership::Outside;
  const bool along=interior ? p.t.lo>0 && p.t.hi<progress : p.t.lo>=0 && p.t.hi<=progress;
  const Range t{std::max(Q(0),p.t.lo),std::min(progress,p.t.hi)};
  const Q h0=binary(row.section->gap_begin_mm),dh=binary(row.section->gap_end_mm)-h0,dz=s.end[2]-s.start[2];
  const auto h=affine(h0,dh,t),top=affine(s.start[2],dz,t),bottom=affine(s.start[2]-h0,dz-dh,t),n=absolute(p.normal);
  const Range dose=representation==MaterialRepresentation::Nominal ? s.nominal : representation==MaterialRepresentation::Upper ?
   Range{s.delivered.hi,s.delivered.hi} : Range{s.delivered.lo,s.delivered.lo};
  const Range area{dose.lo/length.hi,dose.hi/length.lo};
  if(row.section->kind==MaterialSectionKind::Rectangle) {
   const Range half{area.lo/(2*h.hi),area.hi/(2*h.lo)};
   if(n.lo>half.hi || p.z.lo>top.hi || p.z.hi<bottom.lo)return MaterialMembership::Outside;
   const bool cross=interior ? n.hi<half.lo && p.z.hi<top.lo && p.z.lo>bottom.hi :
    n.hi<=half.lo && p.z.hi<=top.lo && p.z.lo>=bottom.hi;
   return along && cross ? MaterialMembership::Inside : MaterialMembership::Unknown;
  }
  const Range core{(area.lo/h.hi-data.pi.hi*h.hi/4)/2,(area.hi/h.lo-data.pi.lo*h.lo/4)/2};
  const auto center=affine(s.start[2]-h0/2,dz-dh/2,t),vertical=absolute({p.z.lo-center.hi,p.z.hi-center.lo});
  const Range horizontal{std::max(Q(0),n.lo-core.hi),std::max(Q(0),n.hi-core.lo)};
  const Q distance_lo=square(horizontal.lo)+square(vertical.lo),distance_hi=square(horizontal.hi)+square(vertical.hi);
  if(distance_lo>square(h.hi/2))return MaterialMembership::Outside;
  if(along && (interior ? distance_hi<square(h.lo/2) : distance_hi<=square(h.lo/2)))return MaterialMembership::Inside;
  return MaterialMembership::Unknown;
 }
 MaterialMembership piece(const ReplayedBead &b,const MaterialRegion &box)
 {
  work();if(representation==MaterialRepresentation::Lower && b.empty_inner)return MaterialMembership::Outside;
  const auto &s=data.steps[b.record];const auto &row=prefix.source->declarations[b.record];
  const Q progress=b.record<prefix.completed_records ? Q(1) : binary(prefix.current_progress);
  const auto l=prefix.source->beads[b.record]->xy_length;const Range length{binary(l.lower),binary(l.upper)};
  const auto original=project_solid(s,length,box,[&]{work();});
  const auto base=section(s,row,length,progress,original,false);
  if(representation==MaterialRepresentation::Nominal)return base;
  if(representation==MaterialRepresentation::Upper && base==MaterialMembership::Inside)return base;
  if(representation==MaterialRepresentation::Lower && base==MaterialMembership::Outside)return base;
  const auto &policy=prefix.source->policy;
  const Q xy=binary(representation==MaterialRepresentation::Upper ? policy.outer_xy_growth_mm : policy.inner_xy_loss_mm)+binary(policy.numerical_coordinate_error_mm);
  const Q z=binary(representation==MaterialRepresentation::Upper ? policy.outer_z_growth_mm : policy.inner_z_loss_mm)+binary(policy.numerical_coordinate_error_mm);
  const Range shift{xy/length.hi,xy/length.lo};
  const SolidProjection expanded{{original.t.lo-shift.hi,original.t.hi+shift.hi},
   {original.normal.lo-xy,original.normal.hi+xy},{original.z.lo-z,original.z.hi+z}};
  const auto robust=section(s,row,length,progress,expanded,representation==MaterialRepresentation::Lower);
  if(representation==MaterialRepresentation::Lower && robust==MaterialMembership::Inside)return robust;
  if(representation==MaterialRepresentation::Upper && robust==MaterialMembership::Outside)return robust;
  // A single common admissible translation proves containment in the grown
  // solid, or exclusion from the eroded one. This never samples query vertices.
  for(int along:{-1,1})for(int transverse:{-1,1})for(int vertical:{-1,1}) {
   const Range u=along<0 ? Range{-shift.hi,-shift.lo} : shift;
   const SolidProjection moved{{original.t.lo+u.lo,original.t.hi+u.hi},
    {original.normal.lo+transverse*xy,original.normal.hi+transverse*xy},{original.z.lo+vertical*z,original.z.hi+vertical*z}};
   const auto membership=section(s,row,length,progress,moved,false);
   if(representation==MaterialRepresentation::Upper && membership==MaterialMembership::Inside)return membership;
   if(representation==MaterialRepresentation::Lower && membership==MaterialMembership::Outside)return membership;
  }
  return MaterialMembership::Unknown;
 }
 MaterialBoxResult query(const MaterialRegion &box)
 {
  MaterialBoxResult result;bool uncertain=false;
  for(const auto &b:prefix.pieces) {
   const auto membership=piece(b,box);
   if(membership==MaterialMembership::Inside){result.membership=membership;result.event_id=b.event_id;return result;}
   uncertain|=membership==MaterialMembership::Unknown;
  }
  result.membership=uncertain ? MaterialMembership::Unknown : MaterialMembership::Outside;return result;
 }
};
void valid_representation(MaterialRepresentation r)
{if(r!=MaterialRepresentation::Nominal && r!=MaterialRepresentation::Upper && r!=MaterialRepresentation::Lower)unknown("INVALID_FINAL_MATERIAL_REPRESENTATION");}
}
MaterialBoxResult classify_linear_material(std::shared_ptr<const LinearMaterialPrefixSnapshot> prefix,const MaterialRegion &requested,
 MaterialRepresentation representation,const LinearMaterialLimits &requested_limits)
{
 const auto region=requested;const auto limits=requested_limits;MaterialBoxResult result;
 try {
  if(!prefix)unknown("MISSING_FINAL_MATERIAL_PREFIX");result.evaluations=std::max(prefix->evaluations,limits.initial_evaluations);
  Work work{*prefix->source->rates,prefix->source->policy,limits,result.evaluations};work.admission();work();valid_region(region);valid_representation(representation);
  SolidQuery query{*prefix,*prefix->source->exact,representation,work};const auto answer=query.query(region);work.stop();
  result.membership=answer.membership;result.event_id=answer.event_id;result.reason=answer.membership==MaterialMembership::Inside ?
   "WHOLE_REGION_INSIDE_FINAL_MATERIAL" : answer.membership==MaterialMembership::Outside ? "WHOLE_REGION_OUTSIDE_FINAL_MATERIAL" : "FINAL_MATERIAL_BOUNDARY_UNCERTAIN";
 }catch(const std::exception &e){result.membership=MaterialMembership::Unknown;result.event_id.reset();result.reason=e.what();}return result;
}
MaterialCoverResult cover_linear_material(std::shared_ptr<const LinearMaterialPrefixSnapshot> prefix,const MaterialRegion &requested,
 MaterialRepresentation representation,const MaterialCoverLimits &requested_limits)
{
 const auto region=requested;const auto limits=requested_limits;MaterialCoverResult result;
 try {
  if(!prefix)unknown("MISSING_FINAL_MATERIAL_PREFIX");result.evaluations=std::max(prefix->evaluations,limits.initial_evaluations);
  Work work{*prefix->source->rates,prefix->source->policy,limits,result.evaluations};work.admission();work();valid_region(region);valid_representation(representation);
  if(!limits.max_cells || limits.max_cells>1000000 || !limits.max_depth || limits.max_depth>64)unknown("INVALID_FINAL_MATERIAL_COVER_LIMITS");
  SolidQuery query{*prefix,*prefix->source->exact,representation,work};
  // Breadth first exposes missing regions before following a boundary to the
  // depth limit; a boundary alone can only produce UNKNOWN.
  struct Node {MaterialRegion box;unsigned depth;};std::deque<Node> pending{{region,0}};std::vector<MaterialCoverLeaf> leaves;
  while(!pending.empty()) {
   work();if(++result.cells>limits.max_cells)unknown("FINAL_MATERIAL_COVER_CELL_LIMIT");
   const auto node=pending.front();pending.pop_front();const auto answer=query.query(node.box);
   if(answer.membership==MaterialMembership::Inside){leaves.push_back({node.box,*answer.event_id});continue;}
   if(answer.membership==MaterialMembership::Outside){work.stop();result.status=RateStatus::Fail;result.uncovered=node.box;result.reason="FINAL_MATERIAL_UNCOVERED_REGION";return result;}
   if(node.depth>=limits.max_depth)unknown("FINAL_MATERIAL_COVER_DEPTH_LIMIT");
   size_t axis=0;Q span=0;
   for(size_t i=0;i<3;++i){const Q width=binary(node.box.max[i])-binary(node.box.min[i]);if(width>span){span=width;axis=i;}}
   const double middle=node.box.min[axis]+(node.box.max[axis]-node.box.min[axis])/2;
   if(!(middle>node.box.min[axis] && middle<node.box.max[axis]))unknown("FINAL_MATERIAL_COVER_UNSPLITTABLE_BOUNDARY");
   auto lower=node.box,upper=node.box;lower.max[axis]=middle;upper.min[axis]=middle;
   pending.push_back({upper,node.depth+1});pending.push_back({lower,node.depth+1});
  }
  work.stop();result.snapshot=std::shared_ptr<const MaterialCoverSnapshot>(new MaterialCoverSnapshot(prefix,region,representation,std::move(leaves),result.evaluations,result.cells));work.stop();
  result.status=RateStatus::Pass;result.reason="WHOLE_REGION_COVERED_BY_ACTUAL_FINAL_MATERIAL_UNION";
 }catch(const std::exception &e){result.snapshot.reset();result.uncovered.reset();result.status=RateStatus::Unknown;result.reason=e.what();}return result;
}
namespace {
LinearMaterialLimits joined_limits(const JoinedMaterialPolicy &policy,const JoinedMaterialLimits &requested)
{
 LinearMaterialLimits limits=requested;
 limits.cancelled=[policy,current=requested.is_join_current,cancel=requested.cancelled] {
  if(current && !current(policy.policy_id,policy.revision))unknown("STALE_FINAL_JOINED_POLICY");return cancel && cancel();
 };
 return limits;
}
Q along(const ExactStep &origin,const std::array<Q,3> &p)
{return ((origin.end[0]-origin.start[0])*(p[0]-origin.start[0])+(origin.end[1]-origin.start[1])*(p[1]-origin.start[1]))/origin.xy2;}
struct JoinedQuery {
 const JoinedMaterialSnapshot &source;const MaterialReplayData &data;Work &work;
 Q progress(size_t i) const {return i<source.source->completed_records ? Q(1) : binary(source.source->current_progress);}
 MaterialMembership minimum_union(const JoinedMaterialRun &run,SolidProjection p)
 {
  const auto &prefix=*source.source;const auto &origin=data.steps[run.first_record];
  const Q end=along(origin,data.steps[run.last_record].start)+
   (along(origin,data.steps[run.last_record].end)-along(origin,data.steps[run.last_record].start))*progress(run.last_record);
  if(p.t.hi<0 || p.t.lo>end)return MaterialMembership::Outside;
  bool outside=true,inside=true,positive=false,point_inside=false;Q covered=std::max(Q(0),p.t.lo);
  SolidQuery section{prefix,data,MaterialRepresentation::Lower,work};
  for(size_t i=run.first_record;i<=run.last_record;++i) {
   work();const auto &s=data.steps[i];const Q start=along(origin,s.start),span=along(origin,s.end)-start,laid=start+span*progress(i);
   const Range clip{std::max(p.t.lo,start),std::min(p.t.hi,laid)};if(clip.lo>clip.hi)continue;
   const auto length=prefix.source->beads[i]->xy_length;
   const auto membership=section.section(s,prefix.source->declarations[i],{binary(length.lower),binary(length.upper)},progress(i),
    {{(clip.lo-start)/span,(clip.hi-start)/span},p.normal,p.z},false);
   outside&=membership==MaterialMembership::Outside;point_inside|=membership==MaterialMembership::Inside;
   if(clip.lo<clip.hi){positive=true;inside&=membership==MaterialMembership::Inside && clip.lo<=covered;covered=clip.hi;}
  }
  if(outside)return MaterialMembership::Outside;
  if(p.t.lo>=0 && p.t.hi<=end && (positive ? inside && covered>=p.t.hi : point_inside))return MaterialMembership::Inside;
  return MaterialMembership::Unknown;
 }
 MaterialMembership run(const JoinedMaterialRun &run,const MaterialRegion &box)
 {
  work();for(size_t axis=0;axis<3;++axis)if(box.min[axis]>run.outer_bounds.coordinate[axis].upper || box.max[axis]<run.outer_bounds.coordinate[axis].lower)return MaterialMembership::Outside;
  const auto &prefix=*source.source;const auto &origin=data.steps[run.first_record];const auto length=prefix.source->beads[run.first_record]->xy_length;
  const Range l{binary(length.lower),binary(length.upper)};const auto p=project_solid(origin,l,box,[&]{work();});
  const Q end=along(origin,data.steps[run.last_record].start)+
   (along(origin,data.steps[run.last_record].end)-along(origin,data.steps[run.last_record].start))*progress(run.last_record);
  const auto &policy=prefix.source->policy;const Q xy=binary(policy.inner_xy_loss_mm)+binary(policy.numerical_coordinate_error_mm),z=binary(policy.inner_z_loss_mm)+binary(policy.numerical_coordinate_error_mm);
  if(square(end)*origin.xy2<=square(2*xy) || p.t.hi<=xy/l.hi || p.t.lo>=end-xy/l.hi)return MaterialMembership::Outside;
  if(minimum_union(run,p)==MaterialMembership::Outside)return MaterialMembership::Outside;
  const SolidProjection expanded{{p.t.lo-xy/l.lo,p.t.hi+xy/l.lo},{p.normal.lo-xy,p.normal.hi+xy},{p.z.lo-z,p.z.hi+z}};
  if(expanded.t.lo>0 && expanded.t.hi<end && minimum_union(run,expanded)==MaterialMembership::Inside)return MaterialMembership::Inside;
  // A fixed admissible kernel displacement outside the minimum union excludes
  // the entire query from its erosion. This is a negative witness, never a
  // sampled positive proof; uncertain shifted bounds remain UNKNOWN.
  for(size_t axis=0;axis<3;++axis)for(int sign:{-1,1}){
   auto shifted=p;Range &value=axis==0 ? shifted.t : axis==1 ? shifted.normal : shifted.z;
   const Q shift=sign*(axis==0 ? xy/l.hi : axis==1 ? xy : z);value.lo+=shift;value.hi+=shift;
   if(minimum_union(run,shifted)==MaterialMembership::Outside)return MaterialMembership::Outside;
  }
  return MaterialMembership::Unknown;
 }
 std::pair<MaterialMembership,size_t> query(const MaterialRegion &box)
 {
  bool uncertain=false;
  for(size_t i=0;i<source.runs.size();++i){const auto membership=run(source.runs[i],box);if(membership==MaterialMembership::Inside)return {membership,i};uncertain|=membership==MaterialMembership::Unknown;}
  return {uncertain ? MaterialMembership::Unknown : MaterialMembership::Outside,0};
 }
};
}
JoinedMaterialResult reconstruct_joined_linear_material(std::shared_ptr<const LinearMaterialPrefixSnapshot> prefix,const JoinedMaterialPolicy &requested,
 const JoinedMaterialLimits &requested_limits)
{
 const auto policy=requested;const auto limits=requested_limits;JoinedMaterialResult result;
 try {
  if(!prefix)unknown("MISSING_FINAL_JOINED_PREFIX");result.evaluations=std::max(prefix->evaluations,limits.initial_evaluations);
  if(policy.version!=joined_material_version || !policy.policy_id || !policy.revision || !policy.synthetic || policy.operator_confirmed_claim || policy.model!=JoinedMaterialModel::CommonRunEnvelope)unknown("UNSUPPORTED_FINAL_JOINED_POLICY");
  Work work{*prefix->source->rates,prefix->source->policy,joined_limits(policy,limits),result.evaluations};work.admission();work();
  const auto &data=*prefix->source->exact;std::vector<JoinedMaterialRun> runs;
  for(const auto &b:prefix->pieces){work();bool continuous=false;
   if(!runs.empty()) {
    const auto &old=runs.back();const auto &origin=data.steps[old.first_record],&previous=data.steps[old.last_record],&next=data.steps[b.record];
    const Q dx=origin.end[0]-origin.start[0],dy=origin.end[1]-origin.start[1],nx=next.end[0]-next.start[0],ny=next.end[1]-next.start[1];
    continuous=b.record==old.last_record+1 && previous.end==next.start && dx*ny==dy*nx && dx*nx+dy*ny>0 &&
     prefix->source->declarations[old.first_record].section->kind==prefix->source->declarations[b.record].section->kind;
   }
   if(!continuous)runs.push_back({b.record,b.record,b.upper_bounds});
   else {auto &run=runs.back();run.last_record=b.record;for(size_t axis=0;axis<3;++axis){work();auto &v=run.outer_bounds.coordinate[axis];v.lower=std::min(v.lower,b.upper_bounds.coordinate[axis].lower);v.upper=std::max(v.upper,b.upper_bounds.coordinate[axis].upper);}}
  }
  work.stop();result.snapshot=std::shared_ptr<const JoinedMaterialSnapshot>(new JoinedMaterialSnapshot(prefix,policy,std::move(runs),result.evaluations));work.stop();
  result.status=RateStatus::Pass;result.reason="DECLARED_FINAL_BYTE_CONTINUOUS_RUNS_RECONSTRUCTED_PHYSICAL_BONDING_UNQUALIFIED";
 }catch(const std::exception &e){result.snapshot.reset();result.reason=e.what();}return result;
}
JoinedMaterialCoverResult cover_joined_linear_material_lower(std::shared_ptr<const JoinedMaterialSnapshot> source,const MaterialRegion &requested,
 const JoinedMaterialLimits &requested_limits)
{
 const auto region=requested;const auto limits=requested_limits;JoinedMaterialCoverResult result;
 try {
  if(!source)unknown("MISSING_FINAL_JOINED_PROOF");result.evaluations=std::max(source->evaluations,limits.initial_evaluations);
  const auto &prefix=*source->source;Work work{*prefix.source->rates,prefix.source->policy,joined_limits(source->policy,limits),result.evaluations};work.admission();work();valid_region(region);
  if(!limits.max_cells || limits.max_cells>1000000 || !limits.max_depth || limits.max_depth>64)unknown("INVALID_FINAL_JOINED_COVER_LIMITS");
  JoinedQuery query{*source,*prefix.source->exact,work};struct Node{MaterialRegion box;unsigned depth;};std::deque<Node> pending{{region,0}};std::vector<JoinedMaterialCoverLeaf> leaves;
  while(!pending.empty()) {
   work();if(++result.cells>limits.max_cells)unknown("FINAL_JOINED_COVER_CELL_LIMIT");const auto node=pending.front();pending.pop_front();const auto answer=query.query(node.box);
   if(answer.first==MaterialMembership::Inside){leaves.push_back({node.box,answer.second});continue;}
   if(answer.first==MaterialMembership::Outside){work.stop();result.status=RateStatus::Fail;result.uncovered=node.box;result.reason="FINAL_JOINED_LOWER_UNCOVERED_REGION";return result;}
   if(node.depth>=limits.max_depth)unknown("FINAL_JOINED_COVER_DEPTH_LIMIT");size_t axis=0;Q span=0;
   for(size_t i=0;i<3;++i){const Q width=binary(node.box.max[i])-binary(node.box.min[i]);if(width>span){span=width;axis=i;}}
   const double middle=node.box.min[axis]+(node.box.max[axis]-node.box.min[axis])/2;
   if(!(middle>node.box.min[axis] && middle<node.box.max[axis]))unknown("FINAL_JOINED_COVER_UNSPLITTABLE_BOUNDARY");
   auto lower=node.box,upper=node.box;lower.max[axis]=middle;upper.min[axis]=middle;pending.push_back({upper,node.depth+1});pending.push_back({lower,node.depth+1});
  }
  work.stop();result.snapshot=std::shared_ptr<const JoinedMaterialCoverSnapshot>(new JoinedMaterialCoverSnapshot(source,region,std::move(leaves),result.evaluations,result.cells));work.stop();
  result.status=RateStatus::Pass;result.reason="WHOLE_REGION_IN_DECLARED_FINAL_JOINED_LOWER_UNION";
 }catch(const std::exception &e){result.snapshot.reset();result.uncovered.reset();result.status=RateStatus::Unknown;result.reason=e.what();}return result;
}
}
