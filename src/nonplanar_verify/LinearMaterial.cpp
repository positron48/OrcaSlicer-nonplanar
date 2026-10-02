#include "LinearMaterial.hpp"
#include "Exact.hpp"
#include <algorithm>
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
}
