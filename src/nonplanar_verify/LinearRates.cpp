#include "LinearRates.hpp"
#include "Exact.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace nptop_verify {
namespace {
using namespace exact;
struct Refusal : std::runtime_error {
    RateStatus status;
    Refusal(RateStatus s,const char *reason) : std::runtime_error(reason),status(s) {}
};
void unknown(const char *reason) {throw Refusal(RateStatus::Unknown,reason);}
void violation(const char *reason) {throw Refusal(RateStatus::Fail,reason);}
bool peak_within(const Q &coordinate2,const Q &feed2,const Q &acceleration2,const Q &length2,const Q &maximum)
{
    const Q maximum2=square(maximum);
    return coordinate2*feed2<=maximum2*length2 || square(coordinate2)*acceleration2<=square(maximum2)*length2;
}
}
LinearRateResult verify_linear_rates(const std::string &requested_bytes,std::array<double,3> initial,
    const LinearRatePolicy &requested_policy,const LinearRateLimits &requested_limits)
{
    const auto policy=requested_policy;const auto limits=requested_limits;const auto started=std::chrono::steady_clock::now();
    LinearRateResult result;result.evaluations=limits.initial_evaluations;
    try {
        if(!limits.max_bytes || limits.max_bytes>32*1024*1024 || !limits.max_events || limits.max_events>200000 ||
           !limits.max_evaluations || limits.max_evaluations>2000000 || limits.initial_evaluations>=limits.max_evaluations ||
           limits.timeout.count()<=0 || limits.timeout>std::chrono::seconds(30))unknown("INVALID_FINAL_RATE_LIMITS");
        if(requested_bytes.size()>limits.max_bytes)unknown("FINAL_RATE_BYTE_LIMIT");
        const auto bytes=requested_bytes;
        const auto stop=[&] {
            if(limits.cancelled && limits.cancelled())unknown("CANCELLED");
            if(limits.is_current && !limits.is_current(policy.profile_id,policy.revision))unknown("STALE_RATE_POLICY");
            if(std::fegetround()!=FE_TONEAREST)unknown("UNSUPPORTED_RATE_ROUNDING");
            volatile double normal=std::numeric_limits<double>::min(),subnormal=std::numeric_limits<double>::denorm_min();
            if(normal/2==0 || subnormal+subnormal==0)unknown("UNSUPPORTED_RATE_UNDERFLOW");
            if(std::chrono::steady_clock::now()-started>=limits.timeout)unknown("FINAL_RATE_DEADLINE");
        };
        const std::function<void()> work=[&] {if(++result.evaluations>limits.max_evaluations)unknown("FINAL_RATE_WORK_LIMIT");stop();};work();
        if(!std::numeric_limits<double>::is_iec559 || sizeof(double)!=sizeof(uint64_t))unknown("UNSUPPORTED_BINARY64");
        if(policy.version!=linear_rate_version || !policy.profile_id || !policy.revision || !policy.synthetic || policy.operator_confirmed_claim ||
           policy.model!=RateModel::FullStop || (policy.kinematics!=RateKinematics::Cartesian && policy.kinematics!=RateKinematics::CoreXY))unknown("UNSUPPORTED_FINAL_RATE_POLICY");
        for(auto values : {policy.axis_speed,policy.axis_acceleration,policy.drive_speed,policy.drive_acceleration})for(double value : values)
            if(!std::isfinite(value) || value<=0 || value>1000000)unknown("INVALID_FINAL_RATE_POLICY");
        for(double value : {policy.initial_acceleration,policy.filament_diameter,policy.flow,policy.filament_speed,policy.filament_acceleration,
                            policy.max_retraction,policy.max_volume_rate,policy.max_cross_section,policy.max_event_rate})
            if(!std::isfinite(value) || value<=0 || value>1000000)unknown("INVALID_FINAL_RATE_POLICY");
        for(size_t i=0;i<3;++i)if(!std::isfinite(initial[i]) || std::abs(initial[i])>10000 || !std::isfinite(policy.position_min[i]) ||
            !std::isfinite(policy.position_max[i]) || std::abs(policy.position_min[i])>10000 || std::abs(policy.position_max[i])>10000 ||
            policy.position_min[i]>=policy.position_max[i])unknown("INVALID_POSITION_DOMAIN");
        if(size_t(std::count(bytes.begin(),bytes.end(),'\n'))>4+2*limits.max_events)unknown("FINAL_RATE_EVENT_LIMIT");work();
        FullStopReplayLimits parse_limits;parse_limits.max_bytes=limits.max_bytes;parse_limits.max_events=limits.max_events;parse_limits.timeout=limits.timeout;
        parse_limits.cancelled=[&] {work();return false;};auto moves=replay_full_stop(bytes,initial,parse_limits);
        const auto pi=pi_bounds();work();const Q area_scale=square(binary(policy.filament_diameter))/4,flow=binary(policy.flow);
        const Range area{pi.lo*area_scale,pi.hi*area_scale};
        std::istringstream input(bytes);input.imbue(std::locale::classic());std::string line,command,word;
        for(unsigned i=0;i<3;++i)std::getline(input,line);
        std::getline(input,line);std::istringstream header(line);header>>command>>word;const Q acceleration=decimal(word.substr(1));
        result.record=0;if(acceleration>binary(policy.initial_acceleration))violation("ACCELERATION_EXCEEDS_KNOWN_INITIAL");
        std::array<Q,3> position{binary(initial[0]),binary(initial[1]),binary(initial[2])};
        std::vector<LinearRateStep> steps;steps.reserve(moves.size());Range total_v{0,0},total_n{0,0},total_t{0,0};Q pressure_debt=0;
        for(size_t i=0;i<moves.size();++i) {
            work();result.record=i;result.axis.reset();const auto &move=moves[i];std::getline(input,line);
            std::istringstream row(line);row.imbue(std::locale::classic());row>>command;
            auto end=position;Q e=0,feed=0,dwell=0;
            while(row>>word) {
                const Q value=decimal(word.substr(1));const auto axis=std::string("XYZ").find(word[0]);
                if(axis!=std::string::npos)end[axis]=value;else if(word[0]=='E')e=value;else if(word[0]=='F')feed=value/60;else if(word[0]=='P')dwell=value/1000;
            }
            std::getline(input,line); // Independently admitted per-event M400.
            if(move.kind==FullStopKind::Pressure) {
                if(e<0) {
                    if(pressure_debt!=0)violation("REPEATED_EXACT_RETRACTION");
                    pressure_debt=-e;
                } else {
                    if(e!=pressure_debt)violation("UNEQUAL_EXACT_PRESSURE_RESTORE");
                    pressure_debt=0;
                }
            } else if(move.kind==FullStopKind::XYZ && e>0 && pressure_debt!=0)violation("DEPOSIT_WHILE_EXACTLY_RETRACTED");
            for(size_t axis=0;axis<3;++axis) {
                work();result.axis=axis;const Q lo=binary(policy.position_min[axis]),hi=binary(policy.position_max[axis]);
                if(position[axis]<lo || position[axis]>hi || end[axis]<lo || end[axis]>hi)violation("POSITION_LIMIT");
            }
            result.axis.reset();
            if(move.kind==FullStopKind::Dwell) {
                if(dwell*binary(policy.max_event_rate)<1)violation("EVENT_RATE_LIMIT");
                const auto time=enclose(dwell,false,work);steps.push_back({{0,0},{0,0},time,{0,0},{0,0}});total_t.lo+=dwell;total_t.hi+=dwell;continue;
            }
            const std::array<Q,3> delta{end[0]-position[0],end[1]-position[1],end[2]-position[2]};
            const Q length2=move.kind==FullStopKind::Pressure ? square(e) : square(delta[0])+square(delta[1])+square(delta[2]);
            if(length2<=0)violation("ZERO_EXACT_MOTION");
            const Q a2=square(acceleration),f2=square(feed),e2=square(e);
            if(move.kind==FullStopKind::XYZ)for(size_t axis=0;axis<3;++axis) {
                work();result.axis=axis;const Q d2=square(delta[axis]);
                if(!peak_within(d2,f2,a2,length2,binary(policy.axis_speed[axis])))violation("AXIS_SPEED_LIMIT");
                if(d2*a2>square(binary(policy.axis_acceleration[axis]))*length2)violation("AXIS_ACCELERATION_LIMIT");
                const Q drive=policy.kinematics==RateKinematics::CoreXY && axis<2 ? (axis==0 ? delta[0]+delta[1] : delta[0]-delta[1]) : delta[axis];
                const Q drive2=square(drive);
                if(!peak_within(drive2,f2,a2,length2,binary(policy.drive_speed[axis])))violation("DRIVE_SPEED_LIMIT");
                if(drive2*a2>square(binary(policy.drive_acceleration[axis]))*length2)violation("DRIVE_ACCELERATION_LIMIT");
            }
            result.axis.reset();
            if(move.kind==FullStopKind::Pressure && abs(e)>binary(policy.max_retraction))violation("RETRACTION_LENGTH_LIMIT");
            if(!peak_within(e2,f2,a2,length2,binary(policy.filament_speed)))violation("FILAMENT_SPEED_LIMIT");
            if(e2*a2>square(binary(policy.filament_acceleration))*length2)violation("FILAMENT_ACCELERATION_LIMIT");
            const Q frequency=binary(policy.max_event_rate);
            // T is the complete ideal rest-to-rest event time. Compare its
            // reciprocal exactly, including triangular/trapezoidal phases.
            if(square(f2)>=a2*length2) {
                if(16*square(square(frequency))*length2<a2)violation("EVENT_RATE_LIMIT");
            } else {
                const Q remainder=Q(1)/frequency-feed/acceleration;
                if(remainder>0 && length2<square(feed*remainder))violation("EVENT_RATE_LIMIT");
            }
            Range volume{0,0};
            if(move.kind==FullStopKind::XYZ && e>0) {
                volume={e*area.lo,e*area.hi};const Q section2=square(binary(policy.max_cross_section))*length2;
                if(square(volume.lo)>section2)violation("EXTRUDE_CROSS_SECTION_LIMIT");
                if(square(volume.hi)>section2)unknown("UNCERTAIN_CROSS_SECTION_BOUNDARY");
                const Q maximum=binary(policy.max_volume_rate);
                if(!peak_within(square(volume.lo),f2,a2,length2,maximum))violation("VOLUME_RATE_LIMIT");
                if(!peak_within(square(volume.hi),f2,a2,length2,maximum))unknown("UNCERTAIN_VOLUME_RATE_BOUNDARY");
            }
            const auto distance=enclose(length2,true,work);
            const auto low_peak=enclose(acceleration*binary(distance.lower),true,work),high_peak=enclose(acceleration*binary(distance.upper),true,work);
            const Q peak_lo=std::min(feed,binary(low_peak.lower)),peak_hi=std::min(feed,binary(high_peak.upper));
            if(peak_lo<=0)unknown("UNCERTAIN_PEAK_SPEED");
            const Range time{binary(distance.lower)/peak_hi+peak_lo/acceleration,binary(distance.upper)/peak_lo+peak_hi/acceleration};
            const Range nominal{volume.lo/flow,volume.hi/flow};
            steps.push_back({distance,{enclose(peak_lo,false,work).lower,enclose(peak_hi,false,work).upper},
                {enclose(time.lo,false,work).lower,enclose(time.hi,false,work).upper},
                {enclose(volume.lo,false,work).lower,enclose(volume.hi,false,work).upper},
                {enclose(nominal.lo,false,work).lower,enclose(nominal.hi,false,work).upper}});
            total_v.lo+=volume.lo;total_v.hi+=volume.hi;total_n.lo+=nominal.lo;total_n.hi+=nominal.hi;total_t.lo+=time.lo;total_t.hi+=time.hi;position=end;
        }
        work();const RateBounds v{enclose(total_v.lo,false,work).lower,enclose(total_v.hi,false,work).upper};
        const RateBounds n{enclose(total_n.lo,false,work).lower,enclose(total_n.hi,false,work).upper};
        const RateBounds time{enclose(total_t.lo,false,work).lower,enclose(total_t.hi,false,work).upper};
        const auto debt=enclose(pressure_debt,false,work);stop();
        result.snapshot=std::shared_ptr<const LinearRateSnapshot>(new LinearRateSnapshot(bytes,initial,policy,std::move(moves),std::move(steps),v,n,time,debt,result.evaluations));stop();
        result.status=RateStatus::Pass;result.record.reset();result.axis.reset();result.reason="SYNTHETIC_FINAL_BYTE_LINEAR_RATES_PASS_OTHER_JOB_CHECKS_REQUIRED";
    } catch(const Refusal &e) {result.snapshot.reset();result.status=e.status;result.reason=e.what();}
      catch(const std::invalid_argument &) {result.snapshot.reset();result.status=RateStatus::Fail;result.reason="UNSUPPORTED_OR_MALFORMED_FINAL_BYTES";}
      catch(const std::exception &e) {result.snapshot.reset();result.status=RateStatus::Unknown;result.reason=e.what();}
    return result;
}
}
