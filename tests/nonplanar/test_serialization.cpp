#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <libslic3r/Nonplanar/GCodeAdapter.hpp>
#include <nonplanar_verify/Replay.hpp>
#include <nonplanar_verify/FullStopReplay.hpp>
#include <nonplanar_verify/LinearRates.hpp>
#include "full_stop_oracle.hpp"
#include <libslic3r/Nonplanar/StlImport.hpp>
#include <cfenv>
#include <thread>

using namespace Slic3r::nptop;
using Catch::Approx;
namespace {
std::vector<MotionEvent> plan()
{
    Deposition bead{Volume(2), WidthXY(.4), VerticalGap(.1), VerticalGap(.3),
                    {NominalMaterialId(1), UpperMaterialId(2), LowerMaterialId(3)}, 4, 5};
    // Already placed on a plate translated by (100, 200). Labels deliberately
    // decrease; they must neither sort events nor contribute nominal Z.
    return {{1, 0, 1, {101,202,3}, {102,202,4.2344}, Speed(12), Acceleration(100), bead, 20},
            {2, 1, 1, {102,202,4.2344}, {103,202,4.2344}, Speed(10), Acceleration(100), bead, 2},
            {3, 2, 0, {103,202,4.2344}, {104,203,2}, Speed(8), Acceleration(100), Travel{}, 1}};
}
std::string candidate() { return serialize_candidate(plan(), Length(1.75), FlowCompensation(1)); }
bool matches(const std::string &bytes, double flow = 1)
{
    const auto moves = nptop_verify::replay(bytes, {101,202,3});
    const auto events = plan();
    if (moves.size() != events.size()) return false;
    for (size_t i = 0; i < moves.size(); ++i) {
        const auto &m = moves[i];
        const auto &p = events[i].end;
        if (std::abs(m.end[0]-p.x()) > .000501 || std::abs(m.end[1]-p.y()) > .000501 ||
            std::abs(m.end[2]-p.z()) > .000501 || std::abs(m.feed-events[i].speed_limit.value()*60) > .000501)
            return false;
        // Independent analytic E, without calling production filament_feed.
        const double expected = i < 2 ? 8 * flow / (std::acos(-1.) * 1.75 * 1.75) : 0;
        if (std::abs(m.e-expected) > .00000501) return false;
    }
    return true;
}
}
TEST_CASE("ORC-06 ordered native serialization preserves absolute placed XYZ and modal Z", "[Nonplanar][A06]")
{
    const auto bytes = candidate();
    INFO(bytes);
    REQUIRE(matches(bytes));
    REQUIRE(bytes.find("G1 X103 Y202 E") != std::string::npos);
    const auto moves = nptop_verify::replay(bytes, {101,202,3});
    REQUIRE(moves.size() == 3);
    REQUIRE(moves.back().e == 0);
    REQUIRE(moves[1].end[2] == Approx(4.234));
}
TEST_CASE("ORC-07 native E applies compensation once without slope correction", "[Nonplanar][A06]")
{
    for (double k : {1., 1.17}) {
        const auto bytes = serialize_candidate(plan(), Length(1.75), FlowCompensation(k));
        INFO(bytes);
        REQUIRE(matches(bytes, k));
    }
}
TEST_CASE("GCD-01 replay exposes XYZ E feed and rounding mutations", "[Nonplanar][A06]")
{
    const auto bytes = candidate();
    for (auto pair : {std::pair<std::string,std::string>{"X102", "X112"}, {"Z4.234", "Z4.236"},
                      {"E.", "E1."}, {"F720", "F721"}}) {
        auto bad = bytes;
        auto pos = bad.find(pair.first);
        REQUIRE(pos != std::string::npos);
        bad.replace(pos, pair.first.size(), pair.second);
        REQUIRE_FALSE(matches(bad));
    }
    REQUIRE_THROWS(nptop_verify::replay("G90\nM83\nG1 F60\nG1 X101 Y202 Z3 E1\n", {101,202,3}));
}
TEST_CASE("GCD-01 parser fails closed on unsupported modal state and grammar", "[Nonplanar][A06]")
{
    for (const std::string command : {"G91", "M82", "G92 X0", "G92 E0", "PRINT_START", "G2 X1", "T1",
                                      "G1 Xnan", "G1 X1 X2", "G1 X1e2", "G1 X1garbage", "G1 E1",
                                      "G1 X2 E-1", "G1 X2 F0", "G1 X2 ; safe"})
        REQUIRE_THROWS(nptop_verify::replay("G90\nM83\nG1 F60\n" + command + "\n", {0,0,0}));
    REQUIRE_THROWS(nptop_verify::replay("G1 X2 F60\n", {0,0,0}));
    REQUIRE_THROWS(nptop_verify::replay("G90\nG1 X2 F60\n", {0,0,0}));
    REQUIRE_THROWS(nptop_verify::replay(std::string(2000001, ' '), {0,0,0}));
}
TEST_CASE("A06 candidate rejects broken order continuity and unsupported events", "[Nonplanar][A06]")
{
    auto check = [](const auto &events) { return serialize_candidate(events, Length(1.75), FlowCompensation(1)); };
    auto events = plan(); events[1].sequence_index = 5; REQUIRE_THROWS(check(events));
    events = plan(); events[1].event_id = 1; REQUIRE_THROWS(check(events));
    events = plan(); events[1].start = PhysicalPosition(0,0,0); REQUIRE_THROWS(check(events));
    events = plan(); events[0].speed_limit = Speed(2000); REQUIRE_THROWS(check(events));
    events = plan(); events[0].end = events[0].start;
    events[0].payload = Retraction{FilamentLength(1), RetractionState::Ready, RetractionState::Retracted};
    REQUIRE_THROWS(check(events));
    events = plan(); events[0].end = PhysicalPosition(101.00001,202,3); REQUIRE_THROWS(check(events));
    REQUIRE_THROWS(check(std::vector<MotionEvent>{}));
    REQUIRE_THROWS(serialize_candidate(plan(), Length(0), FlowCompensation(1)));
}
TEST_CASE("A06 native roundtrip bounds cover negative coordinates and travel between beads", "[Nonplanar][A06]")
{
    for (int sample = 0; sample < 100; ++sample) {
        auto events = plan();
        events[1].payload = Travel{};
        events[2].payload = events[0].payload;
        events[2].source_patch_id = 1;
        const double shift = -500 + sample * .001137;
        for (auto &event : events) {
            event.start = PhysicalPosition(event.start.x()+shift, event.start.y()+shift, event.start.z()+shift);
            event.end = PhysicalPosition(event.end.x()+shift, event.end.y()+shift, event.end.z()+shift);
        }
        const auto bytes = serialize_candidate(events, Length(1.75), FlowCompensation(1.17));
        const auto moves = nptop_verify::replay(bytes, {101+shift,202+shift,3+shift});
        REQUIRE(moves.size() == 3);
        for (size_t i = 0; i < moves.size(); ++i) {
            REQUIRE(std::abs(moves[i].end[0]-events[i].end.x()) <= .000501);
            REQUIRE(std::abs(moves[i].end[1]-events[i].end.y()) <= .000501);
            REQUIRE(std::abs(moves[i].end[2]-events[i].end.z()) <= .000501);
            const double e = i == 1 ? 0 : 8*1.17/(std::acos(-1.)*1.75*1.75);
            REQUIRE(std::abs(moves[i].e-e) <= .00000501);
        }
    }
}
TEST_CASE("A08 serialization rejects feed rounded outside the replay domain", "[Nonplanar][A06][A08]")
{
    auto events = plan();
    events[0].speed_limit = Speed(99999.9996 / 60);
    // Input F is below 100000, but the native formatter rounds to F100000.
    // Rejection must occur before returning bytes outside the replay dialect.
    REQUIRE_THROWS_AS(serialize_candidate(events, Length(1.75), FlowCompensation(1)), std::invalid_argument);
    events[0].speed_limit = Speed(99999.999 / 60);
    const auto bytes = serialize_candidate(events, Length(1.75), FlowCompensation(1));
    const auto moves = nptop_verify::replay(bytes, {101,202,3});
    REQUIRE(moves.front().feed == Approx(99999.999));
    REQUIRE(moves.front().feed < 100000);
}
TEST_CASE("A08 bounded 10000 move candidate roundtrip preserves endpoint and total E", "[Nonplanar][A08][A08Stress]")
{
    auto prototype = plan().front();
    prototype.start = PhysicalPosition(101,202,3);
    std::vector<MotionEvent> events;
    events.reserve(10000);
    for (unsigned i = 1; i <= 10000; ++i) {
        prototype.event_id = i;
        prototype.sequence_index = i-1;
        prototype.end = PhysicalPosition(101+.01*i,202,3+.001*(i%11));
        events.push_back(prototype);
        prototype.start = prototype.end;
    }
    const auto bytes = serialize_candidate(events, Length(1.75), FlowCompensation(1));
    const auto replay = nptop_verify::replay(bytes, {101,202,3});
    REQUIRE(replay.size() == 10000);
    REQUIRE(replay.back().end[0] == Approx(201));
    REQUIRE(replay.back().end[1] == Approx(202));
    REQUIRE(replay.back().end[2] == Approx(3.001));
    double total_e = 0;
    for (const auto &move : replay) total_e += move.e;
    const double expected = 80000/(std::acos(-1.)*1.75*1.75);
    // Sum of 10000 half-E-quantum bounds, plus floating arithmetic allowance.
    REQUIRE(std::abs(total_e-expected) <= .0501);
    events.push_back(prototype);
    REQUIRE_THROWS_AS(serialize_candidate(events, Length(1.75), FlowCompensation(1)), std::invalid_argument);
    REQUIRE_THROWS(nptop_verify::replay(bytes+"G1 X202\n", {101,202,3}));
}

namespace {
LinearMotionPlanResult full_stop_plan(bool mismatch=false,bool collapsed=false,bool tiny_pressure=false)
{
    const MaterialModel model{1,Length(0),Length(0),Length(0),Length(0),Length(0)};
    const MaterialIds ids{NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)};
    const Deposition deposit{Volume(.24),WidthXY(.4),VerticalGap(.2),VerticalGap(.2),ids,1,1};
    const BeadSection bead{BeadSectionKind::Rectangle,.2,.2,{.399999999,.400000001}};
    const double retract=tiny_pressure ? 1e-10 : .8;
    const PhysicalPosition a{1.001234,2.002345,3.003456},b{4.001234,2.002345,3.053456},c{4.001234,5.002345,3.103456};
    std::vector<MaterialRecord> rows{
        {{1,0,1,a,b,Speed(4),Acceleration(100),deposit,20},bead},
        {{2,1,0,b,b,Speed(10),Acceleration(100),Retraction{FilamentLength(retract),RetractionState::Ready,RetractionState::Retracted},2},{}},
        {{3,2,0,b,c,Speed(4),Acceleration(100),Travel{},1},{}},
        {{4,3,0,c,c,Speed(10),Acceleration(100),Retraction{FilamentLength(mismatch ? .9 : retract),RetractionState::Retracted,RetractionState::Ready},0},{}},
        {{5,4,0,c,c,Speed(10),Acceleration(100),Travel{},-1},{}}};
    if(collapsed) {
        const PhysicalPosition end{c.x()+1e-7,c.y(),c.z()};
        rows.back().motion.end=end;
        rows.push_back({{6,5,0,end,end,Speed(10),Acceleration(100),Travel{},-1},{}});
    }
    const auto material=prepare_material_motion(capture_material_sequence(rows,model,7,std::string(64,'a')));INFO(material.reason);
    if(mismatch){REQUIRE_FALSE(material.snapshot);return {};}
    REQUIRE(material.snapshot);
    SimulationScene scene{1,1,1,ProfileOrigin::Synthetic,false,{{0,0,0},Length(.2),Length(.5)},{},
        {{1,1,1},{10,10,8}},{{-1,-1,-1},{20,20,20}},{{{0,0,.1},{10,10,.2}}},true,Length(20),Length(0)};
    uint64_t id=1;for(auto part : {HeadPart::NozzleBody,HeadPart::Heater,HeadPart::Sock,HeadPart::Duct,HeadPart::Sensor,HeadPart::Mount})
        scene.head.push_back({id++,part,{{-.4,-.4,.4},{.4,.4,1}},false,false});
    const ClearancePolicy geometry{Length(.005),NumericBudget(0,0,0,.002),Length(0),Length(0),Length(0)};
    const auto source=prepare_simulation_motion(scene,material,geometry);INFO(source.reason);REQUIRE(source.snapshot);
    const LinearMotionPolicy policy{1,2,1,ProfileOrigin::Synthetic,false,LinearPlannerModel::FullStop,LinearKinematics::CoreXY,
        scene.nozzle_domain,{100,100,2},{1000,1000,20},{100,100,2},{1000,1000,20},Length(1.75),FlowCompensation(1.17),
        Speed(3),Acceleration(40),Length(2),2,1,100};
    const auto result=plan_linear_motion(source,policy);INFO(result.reason);REQUIRE(result.snapshot);return result;
}
}
TEST_CASE("B11 owned native candidate emits every full-stop step with pressure dwell precision and byte identity", "[Nonplanar][B11][LinearCandidate]")
{
    STATIC_REQUIRE(linear_candidate_version==1);STATIC_REQUIRE_FALSE(std::is_aggregate<LinearCandidateSnapshot>::value);
    const auto plan=full_stop_plan();const LinearCandidatePolicy policy{3,1,Acceleration(100)};
    const auto candidate=serialize_linear_candidate(plan,policy);INFO(candidate.reason);REQUIRE(candidate.snapshot);
    const auto &c=*candidate.snapshot;REQUIRE(c.plan==plan.snapshot);REQUIRE(c.events.size()==plan.snapshot->steps.size());
    REQUIRE(c.sha256==sha256_bytes(c.bytes));REQUIRE(c.policy_fingerprint==policy.fingerprint());
    auto changed_policy=policy;changed_policy.revision++;REQUIRE(changed_policy.fingerprint()!=policy.fingerprint());
    for(unsigned digit=0;digit<5;++digit) {changed_policy=policy;switch(digit) {case 0:++changed_policy.xyz_digits;break;case 1:--changed_policy.e_digits;break;case 2:++changed_policy.feed_digits;break;case 3:++changed_policy.acceleration_digits;break;case 4:++changed_policy.dwell_digits;break;}REQUIRE(changed_policy.fingerprint()!=policy.fingerprint());}
    changed_policy=policy;changed_policy.initial_acceleration=Acceleration(99);REQUIRE(changed_policy.fingerprint()!=policy.fingerprint());REQUIRE(c.bytes.find("SET_VELOCITY_LIMIT")==std::string::npos);
    const auto replay=nptop_verify::replay_full_stop(c.bytes,{c.initial_position.x(),c.initial_position.y(),c.initial_position.z()});
    nptop_test::check_full_stop_rates(replay,plan.snapshot->policy,true);
    REQUIRE(replay.size()==c.events.size());const auto &rows=plan.snapshot->planned->material->ledger->records;
    size_t end=c.events.front().begin;
    for(size_t i=0;i<replay.size();++i) {
        REQUIRE(c.events[i].begin==end);REQUIRE(c.events[i].end>c.events[i].begin);end=c.events[i].end;
        REQUIRE(c.bytes.substr(c.events[i].end-5,5)=="M400\n");
        REQUIRE((replay[i].acceleration<=plan.snapshot->steps[i].acceleration_mm_s2 || replay[i].kind==nptop_verify::FullStopKind::Dwell));
        const auto &event=rows[i].motion;
        REQUIRE(std::abs(replay[i].end[0]-event.end.x())<=.000000501);
        REQUIRE(std::abs(replay[i].end[1]-event.end.y())<=.000000501);
        REQUIRE(std::abs(replay[i].end[2]-event.end.z())<=.000000501);
    }
    REQUIRE(end==c.bytes.size());REQUIRE(replay[1].kind==nptop_verify::FullStopKind::Pressure);REQUIRE(replay[1].e==-.8);
    REQUIRE(replay[3].kind==nptop_verify::FullStopKind::Pressure);REQUIRE(replay[3].e==.8);
    REQUIRE(replay[4].kind==nptop_verify::FullStopKind::Dwell);REQUIRE(replay[4].dwell_seconds>=.01);
    const long double e=.24L*1.17L/(acosl(-1.L)*1.75L*1.75L/4);
    REQUIRE(std::abs((long double)replay[0].e-e)<=.000000000501L);
    for(const std::string word : {"X4.001234","E-.8","F"}) {
        auto changed=c.bytes;const auto at=changed.find(word);REQUIRE(at!=std::string::npos);
        if(word=="F")changed.insert(at+1,"1");else changed[at+word.size()-1]='5';
        REQUIRE(sha256_bytes(changed)!=c.sha256);
    }
    REQUIRE_FALSE(serialize_linear_candidate(full_stop_plan(true),policy).snapshot);
}
TEST_CASE("B12 independent full-stop replay refuses missing barriers modal reset pure-E purge and malformed fields", "[Nonplanar][B12][FullStopReplay]")
{
    const std::string header="G90\nM83\nM400\nM204 S10\n";
    for(const std::string body : {"G1 X1 Y0 Z0 F60\n", "G1 X1 Y0 Z0 F60\nG1 X2 Y0 Z0 F60\nM400\n", "G1 E1 F60\nM400\n",
        "G1 X0 Y0 Z0 E1 F60\nM400\n", "G1 E-.8 F60\nM400\nG1 E.9 F60\nM400\n", "G1 E-.8 F60\nM400\nG1 X1 Y0 Z0 E1 F60\nM400\n",
        "G1 X1 Y0 Z0 Fnan\nM400\n", "G1 X1e2 Y0 Z0 F60\nM400\n", "G1 X1 X2 Y0 Z0 F60\nM400\n", "G1 X1 Y0 Z0\nM400\n",
        "G1 X1 Y0 Z0 F60 ; safe\nM400\n", "G92 E0\n", "M82\n", "G91\n", "SET_VELOCITY_LIMIT ACCEL=10000\n", "PRINT_START\n"})
        REQUIRE_THROWS(nptop_verify::replay_full_stop(header+body,{0,0,0}));
    const auto pressure=nptop_verify::replay_full_stop(header+"G1 E-.8 F60\nM400\nG1 E.8 F60\nM400\nG4 P10\nM400\n",{0,0,0});
    REQUIRE(pressure.size()==3);REQUIRE(pressure[0].e==-pressure[1].e);REQUIRE(pressure[2].e==0);
}
TEST_CASE("B11 candidate owns callbacks and never returns partial stale cancelled exhausted or collapsed bytes", "[Nonplanar][B11][LinearCandidate]")
{
    const auto plan=full_stop_plan();const LinearCandidatePolicy policy{3,1,Acceleration(100)};
    REQUIRE_FALSE(serialize_linear_candidate({},policy).snapshot);
    for(int mode=0;mode<8;++mode) {
        LinearCandidateLimits limits;if(mode==0)limits.max_bytes=20;if(mode==1)limits.max_records=1;if(mode==2)limits.max_evaluations=1;
        if(mode==3)limits.cancelled=[] {return true;};if(mode==4)limits.is_current=[](uint64_t) {return false;};
        if(mode==5)limits.is_policy_current=[](uint64_t,uint64_t) {return false;};
        if(mode==6){limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};}
        if(mode==7)limits.is_scene_current=[](uint64_t,uint64_t) {return false;};
        REQUIRE_FALSE(serialize_linear_candidate(plan,policy,limits).snapshot);
    }
    REQUIRE(serialize_linear_candidate(full_stop_plan(false,true),policy).reason=="CANDIDATE_XYZ_COLLAPSES");
    REQUIRE_FALSE(serialize_linear_candidate(full_stop_plan(false,false,true),policy).snapshot);
    auto bad=policy;bad.initial_acceleration=Acceleration(1000001);REQUIRE_FALSE(serialize_linear_candidate(plan,bad).snapshot);
    bad=policy;bad.xyz_digits=2;REQUIRE_FALSE(serialize_linear_candidate(plan,bad).snapshot);
    bad=policy;bad.xyz_digits=10;REQUIRE_FALSE(serialize_linear_candidate(plan,bad).snapshot);
    auto mutable_plan=plan;auto mutable_policy=policy;LinearCandidateLimits limits;
    limits.cancelled=[&] {mutable_plan={};mutable_policy.xyz_digits=10;limits.max_bytes=0;return false;};
    const auto owned=serialize_linear_candidate(mutable_plan,mutable_policy,limits);INFO(owned.reason);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->plan==plan.snapshot);
    limits={};size_t calls=0;limits.cancelled=[&] {++calls;return false;};REQUIRE(serialize_linear_candidate(plan,policy,limits).snapshot);
    const size_t last=calls;calls=0;limits.cancelled=[&] {return ++calls==last;};REQUIRE_FALSE(serialize_linear_candidate(plan,policy,limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_UPWARD);return false;};
    const auto rounded=serialize_linear_candidate(plan,policy,limits);std::fesetround(FE_TONEAREST);REQUIRE_FALSE(rounded.snapshot);
}

TEST_CASE("B12 replay owns input and rejects cancellation deadline resources final truncation and rounding", "[Nonplanar][B12][FullStopReplay]")
{
    const std::string bytes="G90\nM83\nM400\nM204 S10\nG1 X1 Y0 Z0 F60\nM400\n";
    REQUIRE_THROWS(nptop_verify::replay_full_stop(bytes.substr(0,bytes.size()-1),{0,0,0}));
    for(int mode=0;mode<5;++mode) {
        nptop_verify::FullStopReplayLimits limits;
        if(mode==0)limits.max_bytes=1;if(mode==1)limits.max_events=0;if(mode==2)limits.cancelled=[] {return true;};
        if(mode==3){limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};}
        if(mode==4)limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
        REQUIRE_THROWS(nptop_verify::replay_full_stop(bytes,{0,0,0},limits));std::fesetround(FE_TONEAREST);
    }
    auto mutable_bytes=bytes;nptop_verify::FullStopReplayLimits limits;
    limits.cancelled=[&] {mutable_bytes="M82\n";limits.max_events=0;return false;};
    REQUIRE(nptop_verify::replay_full_stop(mutable_bytes,{0,0,0},limits).size()==1);
    limits={};size_t calls=0;limits.cancelled=[&] {++calls;return false;};REQUIRE(nptop_verify::replay_full_stop(bytes,{0,0,0},limits).size()==1);
    const size_t last=calls;calls=0;limits.cancelled=[&] {return ++calls==last;};REQUIRE_THROWS(nptop_verify::replay_full_stop(bytes,{0,0,0},limits));
}

namespace {
nptop_verify::LinearRatePolicy rate_policy()
{
    nptop_verify::LinearRatePolicy p;p.version=1;p.profile_id=71;p.revision=1;
    p.kinematics=nptop_verify::RateKinematics::CoreXY;p.initial_acceleration=20;
    p.position_min={-100,-100,-100};p.position_max={100,100,100};
    p.axis_speed={10,10,2};p.axis_acceleration={20,20,5};p.drive_speed={15,15,2};p.drive_acceleration={30,30,5};
    p.filament_diameter=1.75;p.flow=1.17;p.filament_speed=5;p.filament_acceleration=10;
    p.max_retraction=2;p.max_volume_rate=2;p.max_cross_section=.5;p.max_event_rate=100;return p;
}
std::string rate_bytes()
{
    return "G90\nM83\nM400\nM204 S4\nG1 X3 Y4 Z0 E.2 F300\nM400\nG1 E-.8 F120\nM400\nG1 X3 Y4 Z.05 F60\nM400\nG1 E.8 F120\nM400\nG4 P10\nM400\n";
}
}
TEST_CASE("B12 independent exact final byte limits certify rates dose and ideal full stop timing without phantom pressure material", "[Nonplanar][B12][FinalByteRates]")
{
    using High=boost::multiprecision::cpp_bin_float_quad;
    STATIC_REQUIRE(nptop_verify::linear_rate_version==1);STATIC_REQUIRE_FALSE(std::is_aggregate<nptop_verify::LinearRateSnapshot>::value);
    const auto result=nptop_verify::verify_linear_rates(rate_bytes(),{0,0,0},rate_policy());INFO(result.reason);
    REQUIRE(result.status==nptop_verify::RateStatus::Pass);REQUIRE(result.snapshot);const auto &r=*result.snapshot;
    REQUIRE(r.bytes==rate_bytes());REQUIRE(r.moves.size()==5);REQUIRE(r.steps.size()==5);
    const auto encloses=[](auto bound,High value) {REQUIRE(High(bound.lower)<=value);REQUIRE(value<=High(bound.upper));};
    const High first_peak=sqrt(High(20));encloses(r.steps[0].distance,High(5));encloses(r.steps[0].peak,first_peak);
    encloses(r.steps[0].duration,High(5)/first_peak+first_peak/4);
    const High volume=High(".2")*acos(High(-1))*High("1.75")*High("1.75")/4;
    encloses(r.command_volume,volume);encloses(r.nominal_volume,volume/High(rate_policy().flow));
    const High pressure_peak=sqrt(High("3.2"));
    const High duration=2*High(5)/first_peak+4*High(".8")/pressure_peak+2*sqrt(High(".05")/4)+High(".01");
    encloses(r.duration,duration);
    for(size_t i : {1,3,4}) {REQUIRE(r.steps[i].command_volume.lower==0);REQUIRE(r.steps[i].command_volume.upper==0);}
    REQUIRE(r.moves[1].kind==nptop_verify::FullStopKind::Pressure);REQUIRE(r.moves[4].kind==nptop_verify::FullStopKind::Dwell);
}
TEST_CASE("B12 final byte rate limit mutations fail with original record and no partial certificate", "[Nonplanar][B12][FinalByteRates]")
{
    for(int mode=0;mode<10;++mode) {
        auto p=rate_policy();if(mode==0)p.position_max[0]=2;if(mode==1)p.axis_speed[1]=1;if(mode==2)p.axis_acceleration[0]=1;
        if(mode==3)p.drive_speed[0]=1;if(mode==4)p.drive_acceleration[0]=1;if(mode==5)p.filament_speed=.01;
        if(mode==6)p.filament_acceleration=.01;if(mode==7)p.max_volume_rate=.01;if(mode==8)p.max_cross_section=.01;if(mode==9)p.max_event_rate=.1;
        const auto result=nptop_verify::verify_linear_rates(rate_bytes(),{0,0,0},p);INFO(mode << " " << result.reason);
        REQUIRE(result.status==nptop_verify::RateStatus::Fail);REQUIRE_FALSE(result.snapshot);REQUIRE(result.record);REQUIRE(*result.record==0);
    }
    auto p=rate_policy();p.max_retraction=.7;const auto pressure=nptop_verify::verify_linear_rates(rate_bytes(),{0,0,0},p);
    REQUIRE(pressure.status==nptop_verify::RateStatus::Fail);REQUIRE(pressure.record);REQUIRE(*pressure.record==1);
    p=rate_policy();p.initial_acceleration=3;REQUIRE(nptop_verify::verify_linear_rates(rate_bytes(),{0,0,0},p).status==nptop_verify::RateStatus::Fail);
    REQUIRE(nptop_verify::verify_linear_rates(rate_bytes()+"G92 E0\n",{0,0,0},rate_policy()).status==nptop_verify::RateStatus::Fail);
}
TEST_CASE("B12 exact decimal boundaries and triangular peaks distinguish CoreXY from Cartesian drives", "[Nonplanar][B12][FinalByteRates]")
{
    const std::string header="G90\nM83\nM400\n";auto p=rate_policy();p.initial_acceleration=20;
    p.axis_speed={1,10,2};p.axis_acceleration={20,20,5};p.max_event_rate=10;
    const auto boundary=nptop_verify::verify_linear_rates(header+"M204 S10\nG1 X.1 Y0 Z0 F60\nM400\n",{0,0,0},p);INFO(boundary.reason);
    REQUIRE(boundary.status==nptop_verify::RateStatus::Pass);REQUIRE(boundary.snapshot);
    const auto over=nptop_verify::verify_linear_rates(header+"M204 S10.000001\nG1 X.1 Y0 Z0 F60.000001\nM400\n",{0,0,0},p);
    REQUIRE(over.status==nptop_verify::RateStatus::Fail);REQUIRE_FALSE(over.snapshot);
    p=rate_policy();p.axis_speed[0]=.04;
    REQUIRE(nptop_verify::verify_linear_rates(header+"M204 S.1\nG1 X.01 Y0 Z0 F6000\nM400\n",{0,0,0},p).status==nptop_verify::RateStatus::Pass);
    p=rate_policy();p.drive_speed[0]=6;p.initial_acceleration=200;p.axis_acceleration={200,200,200};p.drive_acceleration={400,400,400};
    const auto bytes=header+"M204 S100\nG1 X3 Y4 Z0 F300\nM400\n";
    REQUIRE(nptop_verify::verify_linear_rates(bytes,{0,0,0},p).status==nptop_verify::RateStatus::Fail);
    p.kinematics=nptop_verify::RateKinematics::Cartesian;REQUIRE(nptop_verify::verify_linear_rates(bytes,{0,0,0},p).status==nptop_verify::RateStatus::Pass);
}
TEST_CASE("B12 final rate verifier owns final bytes policies callbacks and refuses resources stale publication and unsupported arithmetic", "[Nonplanar][B12][FinalByteRates]")
{
    for(int mode=0;mode<8;++mode) {
        nptop_verify::LinearRateLimits limits;auto p=rate_policy();
        if(mode==0)limits.max_bytes=1;if(mode==1)limits.max_events=1;if(mode==2)limits.max_evaluations=1;
        if(mode==3)limits.cancelled=[] {return true;};if(mode==4)limits.is_current=[](uint64_t,uint64_t) {return false;};
        if(mode==5){limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};}
        if(mode==6)p.kinematics=static_cast<nptop_verify::RateKinematics>(9);
        if(mode==7)limits.cancelled=[] {std::fesetround(FE_UPWARD);return false;};
        const auto result=nptop_verify::verify_linear_rates(rate_bytes(),{0,0,0},p,limits);std::fesetround(FE_TONEAREST);
        INFO(mode << " " << result.reason);REQUIRE(result.status==nptop_verify::RateStatus::Unknown);REQUIRE_FALSE(result.snapshot);
    }
    auto bytes=rate_bytes();auto policy=rate_policy();nptop_verify::LinearRateLimits limits;
    limits.cancelled=[&] {bytes="M82\n";policy.max_volume_rate=.0001;limits.max_events=0;return false;};
    const auto owned=nptop_verify::verify_linear_rates(bytes,{0,0,0},policy,limits);INFO(owned.reason);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->bytes==rate_bytes());
    limits={};size_t calls=0;limits.cancelled=[&] {++calls;return false;};REQUIRE(nptop_verify::verify_linear_rates(rate_bytes(),{0,0,0},rate_policy(),limits).snapshot);
    const size_t last=calls;calls=0;limits.cancelled=[&] {return ++calls==last;};
    const auto late=nptop_verify::verify_linear_rates(rate_bytes(),{0,0,0},rate_policy(),limits);REQUIRE_FALSE(late.snapshot);REQUIRE(late.status==nptop_verify::RateStatus::Unknown);
}

TEST_CASE("B12 protected native candidate rate check binds original source policies bytes and shared publication guards", "[Nonplanar][B12][FinalByteRates]")
{
    const auto candidate=serialize_linear_candidate(full_stop_plan(),LinearCandidatePolicy{3,1,Acceleration(100)});REQUIRE(candidate.snapshot);
    const auto rates=verify_linear_candidate_rates(candidate);INFO(rates.reason);REQUIRE(rates.snapshot);REQUIRE(rates.status==nptop_verify::RateStatus::Pass);
    REQUIRE(rates.snapshot->bytes==candidate.snapshot->bytes);REQUIRE(rates.snapshot->policy.flow==candidate.snapshot->plan->policy.flow.value());
    REQUIRE(rates.evaluations>candidate.evaluations);REQUIRE(rates.snapshot->moves.size()==candidate.snapshot->events.size());
    REQUIRE_FALSE(verify_linear_candidate_rates({}).snapshot);
    for(int mode=0;mode<4;++mode) {
        LinearCandidateLimits limits;
        if(mode==0)limits.is_current=[](uint64_t) {return false;};if(mode==1)limits.is_scene_current=[](uint64_t,uint64_t) {return false;};
        if(mode==2)limits.is_policy_current=[](uint64_t id,uint64_t) {return id!=3;};if(mode==3)limits.max_evaluations=candidate.evaluations;
        const auto denied=verify_linear_candidate_rates(candidate,limits);REQUIRE_FALSE(denied.snapshot);REQUIRE(denied.status==nptop_verify::RateStatus::Unknown);
    }
    auto mutable_candidate=candidate;LinearCandidateLimits limits;limits.cancelled=[&] {mutable_candidate={};limits.max_records=0;return false;};
    REQUIRE(verify_linear_candidate_rates(mutable_candidate,limits).snapshot);
    limits={};size_t calls=0;limits.cancelled=[&] {++calls;return false;};REQUIRE(verify_linear_candidate_rates(candidate,limits).snapshot);
    const size_t last=calls;calls=0;limits.cancelled=[&] {return ++calls==last;};REQUIRE_FALSE(verify_linear_candidate_rates(candidate,limits).snapshot);
}

TEST_CASE("B12 final event cadence uses complete rest to rest time rather than the peak divided by length", "[Nonplanar][B12][FinalByteRates]")
{
    auto p=rate_policy();p.max_event_rate=7;
    const std::string bytes="G90\nM83\nM400\nM204 S10\nG1 X.1 Y0 Z0 F60\nM400\n";
    // Peak/L is 10, but the exact rest-to-rest duration is .2s: 5 events/s.
    const auto accepted=nptop_verify::verify_linear_rates(bytes,{0,0,0},p);INFO(accepted.reason);
    REQUIRE(accepted.status==nptop_verify::RateStatus::Pass);REQUIRE(accepted.snapshot);
    p.max_event_rate=5;REQUIRE(nptop_verify::verify_linear_rates(bytes,{0,0,0},p).status==nptop_verify::RateStatus::Pass);
    p.max_event_rate=std::nextafter(5.,0.);REQUIRE(nptop_verify::verify_linear_rates(bytes,{0,0,0},p).status==nptop_verify::RateStatus::Fail);
    p=rate_policy();p.max_event_rate=1;
    const std::string trapezoid="G90\nM83\nM400\nM204 S10\nG1 X1 Y0 Z0 F60\nM400\n";
    REQUIRE(nptop_verify::verify_linear_rates(trapezoid,{0,0,0},p).status==nptop_verify::RateStatus::Pass);
    p.max_event_rate=.8;REQUIRE(nptop_verify::verify_linear_rates(trapezoid,{0,0,0},p).status==nptop_verify::RateStatus::Fail);
}

TEST_CASE("B12 final byte rates retain final retraction state without inventing restoration or material", "[Nonplanar][B12][FinalByteRates]")
{
    const std::string header="G90\nM83\nM400\nM204 S4\n";
    const auto open=nptop_verify::verify_linear_rates(header+"G1 E-.8 F120\nM400\n",{0,0,0},rate_policy());
    REQUIRE(open.status==nptop_verify::RateStatus::Pass);REQUIRE(open.snapshot);
    REQUIRE(open.snapshot->command_volume.upper==0);
    using High=boost::multiprecision::cpp_bin_float_quad;
    REQUIRE(High(open.snapshot->final_pressure_debt.lower)<=High(".8"));
    REQUIRE(High(open.snapshot->final_pressure_debt.upper)>=High(".8"));
    const auto closed=nptop_verify::verify_linear_rates(header+"G1 E-.8 F120\nM400\nG1 E.8 F120\nM400\n",{0,0,0},rate_policy());
    REQUIRE(closed.status==nptop_verify::RateStatus::Pass);REQUIRE(closed.snapshot);
    REQUIRE(closed.snapshot->command_volume.upper==0);
    REQUIRE(closed.snapshot->final_pressure_debt.upper==0);
}

TEST_CASE("B12 final candidate adapter refuses rounding changed by the final source callback", "[Nonplanar][B12][FinalByteRates]")
{
 const auto candidate=serialize_linear_candidate(full_stop_plan(),LinearCandidatePolicy{3,1,Acceleration(100)});REQUIRE(candidate.snapshot);
 LinearCandidateLimits limits;size_t calls=0;limits.cancelled=[&]{++calls;return false;};REQUIRE(verify_linear_candidate_rates(candidate,limits).snapshot);
 const auto last=calls;calls=0;limits.cancelled=[&]{if(++calls==last)std::fesetround(FE_UPWARD);return false;};
 const auto result=verify_linear_candidate_rates(candidate,limits);std::fesetround(FE_TONEAREST);
 REQUIRE_FALSE(result.snapshot);REQUIRE(result.status==nptop_verify::RateStatus::Unknown);
}

TEST_CASE("B12 protected material replay owns source options original losses and publication guards", "[Nonplanar][B12][FinalByteMaterial]")
{
 const auto candidate=serialize_linear_candidate(full_stop_plan(),LinearCandidatePolicy{3,1,Acceleration(100)});REQUIRE(candidate.snapshot);
 const LinearMaterialOptions options{93,1,1e-7,.0001,1e-9};
 const auto material=verify_linear_candidate_material(candidate,options);INFO(material.reason);REQUIRE(material.snapshot);
 REQUIRE(material.snapshot->rates->bytes==candidate.snapshot->bytes);REQUIRE(material.evaluations>candidate.evaluations);
 for(int mode=0;mode<4;++mode){LinearCandidateLimits limits;
  if(mode==0)limits.is_current=[](uint64_t){return false;};if(mode==1)limits.is_scene_current=[](uint64_t,uint64_t){return false;};
  if(mode==2)limits.is_policy_current=[](uint64_t id,uint64_t){return id!=93;};if(mode==3)limits.max_evaluations=candidate.evaluations;
  const auto denied=verify_linear_candidate_material(candidate,options,limits);REQUIRE_FALSE(denied.snapshot);REQUIRE(denied.status==nptop_verify::RateStatus::Unknown);
 }
 auto mutable_candidate=candidate;auto mutable_options=options;LinearCandidateLimits limits;
 limits.cancelled=[&]{mutable_candidate={};mutable_options.policy_id=0;return false;};
 REQUIRE(verify_linear_candidate_material(mutable_candidate,mutable_options,limits).snapshot);
 limits={};size_t calls=0;limits.cancelled=[&]{++calls;return false;};REQUIRE(verify_linear_candidate_material(candidate,options,limits).snapshot);
 const auto last=calls;calls=0;limits.cancelled=[&]{if(++calls==last)std::fesetround(FE_UPWARD);return false;};
 const auto late=verify_linear_candidate_material(candidate,options,limits);std::fesetround(FE_TONEAREST);REQUIRE_FALSE(late.snapshot);
}
