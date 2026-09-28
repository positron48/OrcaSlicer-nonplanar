#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/ProfileScene.hpp>
#include <cfenv>
#include <thread>
using namespace Slic3r::nptop;
namespace {
SimulationScene fixture()
{
    // Synthetic dimensions, never a Snapmaker U1 profile.
    SimulationScene s{1,1,1,ProfileOrigin::Synthetic,false,
        {{0,0,0},Length(.2),Length(.5)}, {},
        {{10,10,1},{20,20,5}}, {{0,0,0},{40,40,30}}, {},true,Length(40),Length(.1)};
    uint64_t id = 1;
    for (auto part : {HeadPart::NozzleBody,HeadPart::Heater,HeadPart::Sock,
                      HeadPart::Duct,HeadPart::Sensor,HeadPart::Mount})
        s.head.push_back({id++,part,{{-1,-2,.1},{3,2,10}},false,false});
    s.head[3].moving = true;
    s.head[3].all_configurations_enclosed = true;
    return s;
}
std::vector<MotionEvent> motions()
{
    return {{1,0,0,{11,11,2},{19,19,4},Speed(10),Acceleration(100),Travel{},0}};
}
}
TEST_CASE("PRF-01 complete synthetic scene is usable only for simulation", "[Nonplanar][A07]")
{
    REQUIRE(assess_simulation_scene(fixture(),motions()) == SceneApplicability::SimulationOnly);
    auto s=fixture(); s.obstacles.push_back({{25,25,1},{27,27,12}});
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::SimulationOnly);
    // This check deliberately does not certify collision freedom.
    s.obstacles.push_back({{12,12,2},{14,14,4}});
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::SimulationOnly);
}
TEST_CASE("PRF-06 confirmation flags cannot qualify synthetic geometry", "[Nonplanar][A07]")
{
    auto s=fixture(); s.operator_confirmed_claim=true;
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::UnsupportedQualification);
    s.origin=ProfileOrigin::OperatorMeasured;
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::UnsupportedQualification);
    s.operator_confirmed_claim=false;
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::UnsupportedQualification);
}
TEST_CASE("ORC-38 missing head dimensions inventory and moving envelope reject", "[Nonplanar][A07]")
{
    for (size_t i=0;i<6;++i) {
        auto s=fixture(); s.head.erase(s.head.begin()+i);
        REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::IncompleteGeometry);
    }
    auto s=fixture(); s.head[0].outer.max=s.head[0].outer.min;
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::InvalidInput);
    s=fixture(); s.head[3].all_configurations_enclosed=false;
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::IncompleteGeometry);
    s=fixture(); s.obstacle_inventory_complete=false;
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::IncompleteGeometry);
    s=fixture(); s.tip.outer_radius=s.tip.opening_radius;
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::IncompleteGeometry);
}
TEST_CASE("A07 full head and omitted upper parts must fit declared scene coverage", "[Nonplanar][A07]")
{
    auto s=fixture(); s.scene_domain.max=PhysicalPosition(22,40,30);
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::OutsideCoverage);
    s=fixture(); s.tip.outer_radius=Length(15);
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::OutsideCoverage);
    s=fixture(); s.unmodelled_parts_min_local_z=Length(28);
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::OutsideCoverage);
    s=fixture(); s.obstacles.push_back({{39,1,1},{41,2,2}});
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::OutsideCoverage);
    s=fixture(); auto m=motions(); m[0].end=PhysicalPosition(21,19,4);
    REQUIRE(assess_simulation_scene(s,m) == SceneApplicability::OutsideCoverage);
    // Exact lower boundary needs the declared uncertainty, not an epsilon pass.
    s=fixture(); s.nozzle_domain.min=PhysicalPosition(0,10,1);
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::OutsideCoverage);
}
TEST_CASE("A07 malformed versions identities chronology and numeric state fail closed", "[Nonplanar][A07]")
{
    auto s=fixture(); s.version=2;
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::UnsupportedVersion);
    s=fixture(); s.profile_id=0;
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::InvalidInput);
    s=fixture(); s.head[1].id=s.head[0].id;
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::InvalidInput);
    s=fixture(); s.revision=0;
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::InvalidInput);
    s=fixture(); s.head.resize(65,s.head.front());
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::InvalidInput);
    REQUIRE(assess_simulation_scene(fixture(),{}) == SceneApplicability::InvalidInput);
    auto m=motions(); m[0].sequence_index=1;
    REQUIRE(assess_simulation_scene(fixture(),m) == SceneApplicability::InvalidInput);
    const int previous=std::fegetround();
    std::fesetround(FE_UPWARD);
    const auto result=assess_simulation_scene(fixture(),motions());
    std::fesetround(previous);
    REQUIRE(result == SceneApplicability::NumericalFailure);
}
TEST_CASE("A07 analytic Minkowski bounds include uncertainty at coverage boundaries", "[Nonplanar][A07]")
{
    // Independent arithmetic: largest X = 20 + 3 + 0.1 = 23.1 mm.
    auto s=fixture(); s.scene_domain.max=PhysicalPosition(23.11,40,30);
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::SimulationOnly);
    s.scene_domain.max=PhysicalPosition(23.09,40,30);
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::OutsideCoverage);
    // Lowest omitted Z = 1 + h - 0.1; it must be strictly above 30.
    s=fixture(); s.unmodelled_parts_min_local_z=Length(29.11);
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::SimulationOnly);
    s.unmodelled_parts_min_local_z=Length(29.09);
    REQUIRE(assess_simulation_scene(s,motions()) == SceneApplicability::OutsideCoverage);
}

namespace {
SimulationScene travel_scene()
{
    auto scene=fixture();
    for (auto &part : scene.head) part.outer={{-.25,-.25,.1},{.25,.25,.5}};
    scene.head[3].outer={{2,-.5,.1},{4,.5,2}};
    // .25 lower Z leaves .05 beyond the .1 geometry + .1 clearance hull.
    scene.obstacles={{{1,1,.25},{30,30,.5}},{{30,30,1},{31,31,2}}};
    return scene;
}
std::vector<MotionEvent> travel_events()
{ return {{1,0,0,{11,10,2},{11,20,2},Speed(10),Acceleration(100),Travel{},0}}; }
ClearancePolicy travel_policy()
{ return {Length(.1),NumericBudget(0,0,0,0),Length(0),Length(0),Length(0)}; }
}
TEST_CASE("A07 complete travel checks every declared component obstacle and event", "[Nonplanar][A07][SimulationTravel]")
{
    auto scene=travel_scene();
    const auto result=check_simulation_travel(scene,travel_events(),travel_policy());
    REQUIRE(result.applicability==SceneApplicability::SimulationOnly);
    REQUIRE(result.status==ClearanceStatus::Pass);
    REQUIRE(result.required_pairs==14);
    REQUIRE(result.checked_pairs==14);
    REQUIRE(result.tip_component_id==7);
    REQUIRE(result.profile_id==scene.profile_id);
    REQUIRE(result.revision==scene.revision);
    REQUIRE(result.limiting_check);
    REQUIRE(result.limiting_check->bounds->lower_mm>.1);
    REQUIRE(result.limiting_check->bounds->bound_error_mm>=.2);
    auto events=travel_events();
    auto second=events.front(); second.start=events.front().end; second.end=events.front().start;
    second.event_id=2; second.sequence_index=1; events.push_back(second);
    const auto complete=check_simulation_travel(scene,events,travel_policy());
    REQUIRE(complete.status==ClearanceStatus::Pass);
    REQUIRE(complete.checked_pairs==28);
    scene.obstacles.clear();
    const auto empty=check_simulation_travel(scene,events,travel_policy());
    REQUIRE(empty.status==ClearanceStatus::Pass);
    REQUIRE(empty.checked_pairs==0);
    REQUIRE_FALSE(empty.limiting_check);
    scene.obstacle_inventory_complete=false;
    REQUIRE(check_simulation_travel(scene,events,travel_policy()).status==ClearanceStatus::Unknown);
}
TEST_CASE("A07 whole-head travel detects a duct collision between clear endpoint poses", "[Nonplanar][A07][SimulationTravel]")
{
    auto scene=travel_scene(); scene.obstacles.push_back({{14,14,2.25},{14.5,16,3}});
    const auto events=travel_events();
    for (auto point : {events.front().start,events.front().end}) {
        auto fixed=events; fixed.front().start=point; fixed.front().end=point;
        REQUIRE(check_simulation_travel(scene,fixed,travel_policy()).status==ClearanceStatus::Pass);
    }
    REQUIRE(query_clearance(events.front(),{7,scene.tip},{3,scene.obstacles.back()},travel_policy()).status==ClearanceStatus::Pass);
    const auto result=check_simulation_travel(scene,events,travel_policy());
    REQUIRE(result.status==ClearanceStatus::Fail);
    REQUIRE(result.limiting_check);
    REQUIRE(result.limiting_check->component_id==scene.head[3].id);
    REQUIRE(result.limiting_check->obstacle_id==3);
    REQUIRE(result.limiting_check->event_id==1);
    REQUIRE(result.limiting_check->witness);
    REQUIRE(result.checked_pairs<result.required_pairs);
}
TEST_CASE("A07 travel charges both scene envelopes and rejects unsupported material state", "[Nonplanar][A07][SimulationTravel]")
{
    auto scene=travel_scene(); scene.obstacles[0].max=PhysicalPosition(30,30,1.75);
    const auto events=travel_events();
    REQUIRE(query_clearance(events.front(),{7,scene.tip},{1,scene.obstacles.front()},travel_policy()).status==ClearanceStatus::Pass);
    const auto uncertain=check_simulation_travel(scene,events,travel_policy());
    REQUIRE(uncertain.status==ClearanceStatus::Unknown);
    REQUIRE(uncertain.limiting_check);
    REQUIRE(uncertain.limiting_check->bounds->bound_error_mm>=.2);
    scene.uncertainty=Length(0);
    REQUIRE(check_simulation_travel(scene,events,travel_policy()).status==ClearanceStatus::Pass);
    auto changed=events; changed.front().end=changed.front().start;
    changed.front().payload=Retraction{FilamentLength(1),RetractionState::Ready,RetractionState::Retracted};
    REQUIRE(check_simulation_travel(scene,changed,travel_policy()).status==ClearanceStatus::Unknown);
    scene=travel_scene(); scene.operator_confirmed_claim=true;
    REQUIRE(check_simulation_travel(scene,events,travel_policy()).status==ClearanceStatus::Unknown);
    scene=travel_scene(); scene.head.pop_back();
    REQUIRE(check_simulation_travel(scene,events,travel_policy()).status==ClearanceStatus::Unknown);
}
TEST_CASE("A07 travel budgets must fit declared scene coverage even with no obstacles", "[Nonplanar][A07][SimulationTravel][SimulationCoverageBudget]")
{
    auto scene=travel_scene(); scene.obstacles.clear();
    REQUIRE(check_simulation_travel(scene,travel_events(),travel_policy()).status==ClearanceStatus::Pass);
    // Negative X extent becomes 10 - .25 - .1 - 10 = -.35, outside [0,40].
    for (int term=0; term<6; ++term) {
        auto policy=travel_policy();
        if (term==0) policy.numeric.import_mm=10;
        if (term==1) policy.tool_measurement=Length(10);
        if (term==2) policy.positioning=Length(10);
        if (term==3) policy.material=Length(10);
        if (term==4) policy.scene_geometry=Length(10);
        if (term==5) policy.required=Length(10);
        const auto result=check_simulation_travel(scene,travel_events(),policy);
        CHECK(result.status==ClearanceStatus::Unknown);
        CHECK(result.applicability==SceneApplicability::OutsideCoverage);
    }
}

TEST_CASE("A07 travel owns callback inputs and never publishes cancelled stale or exhausted work", "[Nonplanar][A07][SimulationTravel]")
{
    auto scene=travel_scene(); auto events=travel_events(); auto policy=travel_policy();
    SimulationTravelLimits limits; limits.max_pairs=13;
    REQUIRE(check_simulation_travel(scene,events,policy,limits).reason=="PAIR_LIMIT");
    limits={}; limits.max_evaluations_per_pair=0;
    const auto exhausted=check_simulation_travel(scene,events,policy,limits);
    REQUIRE(exhausted.status==ClearanceStatus::Unknown);
    REQUIRE(exhausted.limiting_check->reason==ClearanceReason::WorkLimit);
    limits={}; limits.timeout=std::chrono::milliseconds(0);
    REQUIRE(check_simulation_travel(scene,events,policy,limits).reason=="INVALID_LIMITS");
    limits={}; limits.timeout=std::chrono::milliseconds(60001);
    REQUIRE(check_simulation_travel(scene,events,policy,limits).reason=="INVALID_LIMITS");
    limits={}; size_t calls=0;
    limits.is_current=[&](uint64_t id,uint64_t rev) { REQUIRE(id==1); REQUIRE(rev==1); ++calls; return true; };
    REQUIRE(check_simulation_travel(scene,events,policy,limits).status==ClearanceStatus::Pass);
    const auto final_call=calls; REQUIRE(final_call>14); calls=0;
    limits.is_current=[&](uint64_t,uint64_t) { return ++calls<final_call; };
    const auto stale=check_simulation_travel(scene,events,policy,limits);
    REQUIRE(stale.status==ClearanceStatus::Unknown);
    REQUIRE(stale.reason=="STALE_REVISION");
    REQUIRE(stale.checked_pairs==14);
    limits={}; calls=0; limits.cancelled=[&] { return ++calls==final_call; };
    const auto cancelled=check_simulation_travel(scene,events,policy,limits);
    REQUIRE(cancelled.status==ClearanceStatus::Unknown);
    REQUIRE(cancelled.reason=="CANCELLED");
    limits={}; limits.timeout=std::chrono::milliseconds(1);
    limits.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(3)); return false; };
    REQUIRE(check_simulation_travel(scene,events,policy,limits).reason=="DEADLINE");
    limits={}; limits.cancelled=[]() -> bool { throw std::runtime_error("cancel failure"); };
    REQUIRE(check_simulation_travel(scene,events,policy,limits).status==ClearanceStatus::Unknown);
    struct RestoreRounding { ~RestoreRounding() { std::fesetround(FE_TONEAREST); } } restore;
    limits={}; limits.is_current=[](uint64_t,uint64_t) { std::fesetround(FE_DOWNWARD); return true; };
    REQUIRE(check_simulation_travel(scene,events,policy,limits).status==ClearanceStatus::Unknown);
    std::fesetround(FE_TONEAREST);
    limits={}; bool mutated=false;
    limits.cancelled=[&] {
        if (!mutated) {
            mutated=true; scene.head.clear(); scene.obstacles.clear(); events.clear();
            policy.required=Length(1000); limits.timeout=std::chrono::milliseconds(0);
            limits.is_current=[](uint64_t,uint64_t) { return false; };
        }
        return false;
    };
    const auto owned=check_simulation_travel(scene,events,policy,limits);
    REQUIRE(owned.status==ClearanceStatus::Pass);
    REQUIRE(owned.checked_pairs==14);
    REQUIRE(mutated);
    REQUIRE(events.empty());
}
