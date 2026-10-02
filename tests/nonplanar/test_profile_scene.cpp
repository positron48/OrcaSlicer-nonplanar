#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/ProfileScene.hpp>
#include <libslic3r/Nonplanar/MotionPlan.hpp>
#include <cfenv>
#include <thread>
#include <limits>
#include <boost/multiprecision/cpp_bin_float.hpp>
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

namespace {
MaterialMotionSourceResult scene_material(bool descending=false,double growth=0)
{
    const double z0=descending ? 2 : 1,z1=descending ? 1 : 2;
    const double h=.2,width=.4,area=h*(width-(1-std::acos(-1.L)/4)*h);
    const MaterialRecord row{{1,0,33,{0,0,z0},{2,0,z1},Speed(10),Acceleration(100),
        Deposition{Volume(2*area),WidthXY(width),VerticalGap(h),VerticalGap(h),
            {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},7,8}},
        BeadSection{BeadSectionKind::RoundedRectangle,h,h,{width-1e-12,width+1e-12}}};
    const auto ledger=capture_material_sequence({row},{17,Length(growth),Length(growth),Length(0),Length(0),Length(0)},23,std::string(64,'a'));
    REQUIRE(ledger.snapshot);const auto prepared=prepare_material_motion(ledger);REQUIRE(prepared.snapshot);return prepared;
}
SimulationScene material_scene()
{
    // Complete synthetic inventory, not measured U1 geometry.
    SimulationScene s{1,41,7,ProfileOrigin::Synthetic,false,
        {{0,0,0},Length(.25),Length(.5)}, {},
        {{-1,-1,.5},{3,1,2.5}}, {{-4,-4,-1},{6,4,6}},
        {{{-3,-3,-.8},{5,3,-.6}}},true,Length(10),Length(0)};
    uint64_t id=1;
    for (auto part : {HeadPart::NozzleBody,HeadPart::Heater,HeadPart::Sock,HeadPart::Duct,HeadPart::Sensor,HeadPart::Mount})
        s.head.push_back({id++,part,{{-.05,-.05,.5},{.05,.05,.8}},false,false});
    return s;
}
ClearancePolicy material_scene_policy()
{ return {Length(.005),NumericBudget(0,0,0,0),Length(0),Length(0),Length(0)}; }
}
TEST_CASE("B08 full simulation head composes original deposition material and static scene", "[Nonplanar][B08][SceneMaterialMotion]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<SimulationMotionSourceSnapshot>::value);
    STATIC_REQUIRE_FALSE(std::is_aggregate<SimulationMotionSnapshot>::value);
    const auto material=scene_material();const auto prepared=prepare_simulation_motion(material_scene(),material,material_scene_policy());
    INFO(prepared.reason);REQUIRE(prepared.snapshot);REQUIRE(prepared.applicability==SceneApplicability::SimulationOnly);
    REQUIRE(prepared.snapshot->components.size()==7);REQUIRE(prepared.snapshot->tip_component_id==7);
    REQUIRE(prepared.snapshot->material==material.snapshot);REQUIRE(material.snapshot->full_upper_z_mm);
    REQUIRE(material.snapshot->full_upper_z_mm->lower<=.8);REQUIRE(material.snapshot->full_upper_z_mm->upper>=2);
    const auto checked=check_simulation_motion(prepared,0);INFO(checked.reason);REQUIRE(checked.status==ClearanceStatus::Pass);REQUIRE(checked.snapshot);
    REQUIRE(checked.snapshot->source==prepared.snapshot);REQUIRE(checked.snapshot->scene_checks.size()==7);
    REQUIRE(checked.snapshot->material->source==material.snapshot);REQUIRE(checked.snapshot->material->event_index==0);
    REQUIRE(checked.snapshot->material->tools.size()==7);REQUIRE_FALSE(checked.snapshot->material->leaves.empty());
    for (const auto &pair : checked.snapshot->scene_checks) {
        REQUIRE(pair.status==ClearanceStatus::Pass);REQUIRE(pair.event_id==1);REQUIRE(pair.sequence_index==0);REQUIRE(pair.bounds);
    }
    auto uncertain=material_scene();uncertain.uncertainty=Length(.001);
    const auto budget=prepare_simulation_motion(uncertain,material,material_scene_policy());REQUIRE(budget.snapshot);
    REQUIRE(budget.snapshot->static_policy.scene_geometry.value()>=.002);
    REQUIRE(budget.snapshot->material_policy.scene_geometry.value()>=.001);
    REQUIRE(check_simulation_motion(budget,0).snapshot);
    auto rows=material.snapshot->ledger->records;const auto &ledger=*material.snapshot->ledger;
    rows.push_back({{2,1,0,rows.back().motion.end,{0,0,2.4},Speed(10),Acceleration(100),Travel{}},{}});
    const auto ordered=prepare_material_motion(capture_material_sequence(rows,ledger.model,ledger.revision,ledger.source_fingerprint));REQUIRE(ordered.snapshot);
    const auto travel_source=prepare_simulation_motion(material_scene(),ordered,material_scene_policy());REQUIRE(travel_source.snapshot);
    const auto travel=check_simulation_motion(travel_source,1);INFO(travel.reason);REQUIRE(travel.snapshot);
    REQUIRE(travel.event_index==1);REQUIRE(travel.snapshot->material->event_index==1);
    for (const auto &pair : travel.snapshot->scene_checks) {REQUIRE(pair.sequence_index==1);REQUIRE(pair.event_id==2);}
    // The original A07 scene maximum of 64 boxes needs 65 complete material
    // components with its separately represented tip; none may be dropped.
    auto many=material_scene();while (many.head.size()<64) {auto part=many.head.front();part.id=many.head.size()+1;many.head.push_back(part);}
    const auto complete=prepare_simulation_motion(many,material,material_scene_policy());REQUIRE(complete.snapshot);
    REQUIRE(complete.snapshot->components.size()==65);REQUIRE(check_simulation_motion(complete,0).snapshot);
}
TEST_CASE("B08 full head localizes a static duct collision and an actual current tip collision", "[Nonplanar][B08][SceneMaterialMotion]")
{
    auto scene=material_scene();scene.head[3].outer={{1,-.05,.5},{1.2,.05,.7}};
    scene.obstacles.push_back({{2,-.05,1.99},{2.1,.05,2.01}});
    const auto prepared=prepare_simulation_motion(scene,scene_material(),material_scene_policy());INFO(prepared.reason);REQUIRE(prepared.snapshot);
    const auto blocked=check_simulation_motion(prepared,0);INFO(blocked.reason);REQUIRE(blocked.status==ClearanceStatus::Fail);REQUIRE_FALSE(blocked.snapshot);
    REQUIRE(blocked.scene_check);REQUIRE(blocked.scene_check->component_id==scene.head[3].id);REQUIRE(blocked.scene_check->obstacle_id==2);
    REQUIRE(blocked.scene_check->witness);const double t=blocked.scene_check->witness->parameter;
    // Independent strict overlap at the returned pose, and disjoint X at both ends.
    REQUIRE(2*t+1<2.1);REQUIRE(2*t+1.2>2);REQUIRE(1+t+.5<2.01);REQUIRE(1+t+.7>1.99);
    REQUIRE(1.2<2);REQUIRE(2+1>2.1);
    const auto falling=prepare_simulation_motion(material_scene(),scene_material(true),material_scene_policy());REQUIRE(falling.snapshot);
    const auto struck=check_simulation_motion(falling,0);INFO(struck.reason);REQUIRE(struck.status==ClearanceStatus::Fail);REQUIRE_FALSE(struck.snapshot);
    REQUIRE(struck.material_check);REQUIRE(struck.material_check->witness);REQUIRE(struck.material_check->witness->component_index==0);
    REQUIRE(struck.material_check->witness->material_record==0);REQUIRE(struck.material_check->tools[0].id==7);
    const auto &w=*struck.material_check->witness;const long double x=2.L*w.parameter+w.local_point.x();
    const long double top=2-x/2,z=2.L-w.parameter;
    REQUIRE(x>0);REQUIRE(x<2.L*w.parameter);REQUIRE(z<top);REQUIRE(z>top-.2L);
}
TEST_CASE("B08 full scene source and motion refuse incomplete coverage stale ownership and budgets", "[Nonplanar][B08][SceneMaterialMotion]")
{
    auto scene=material_scene();auto material=scene_material();auto policy=material_scene_policy();
    const auto source=prepare_simulation_motion(scene,material,policy);REQUIRE(source.snapshot);
    REQUIRE_FALSE(prepare_simulation_motion(scene,{},policy).snapshot);
    auto missing=scene;missing.head.pop_back();REQUIRE_FALSE(prepare_simulation_motion(missing,material,policy).snapshot);
    missing=scene;missing.obstacle_inventory_complete=false;REQUIRE_FALSE(prepare_simulation_motion(missing,material,policy).snapshot);
    missing=scene;missing.operator_confirmed_claim=true;REQUIRE_FALSE(prepare_simulation_motion(missing,material,policy).snapshot);
    const auto tall=prepare_simulation_motion(scene,scene_material(false,10),policy);
    INFO(tall.reason);REQUIRE_FALSE(tall.snapshot);REQUIRE(tall.applicability==SceneApplicability::OutsideCoverage);
    auto huge=policy;huge.required=Length(10);REQUIRE_FALSE(prepare_simulation_motion(scene,material,huge).snapshot);
    SimulationMotionPreparationLimits capture;capture.max_evaluations=1;REQUIRE_FALSE(prepare_simulation_motion(scene,material,policy,capture).snapshot);
    const auto old=material.snapshot;capture={};capture.cancelled=[&] {scene.head.clear();material={};policy.required=Length(10);return false;};
    const auto owned=prepare_simulation_motion(scene,material,policy,capture);INFO(owned.reason);REQUIRE(owned.snapshot);
    REQUIRE(owned.snapshot->material==old);REQUIRE(owned.snapshot->components.size()==7);REQUIRE(owned.snapshot->policy.required.value()==.005);
    REQUIRE_FALSE(check_simulation_motion({},0).snapshot);REQUIRE_FALSE(check_simulation_motion(source,1).snapshot);
    for (int mode=0;mode<9;++mode) {
        SimulationMotionLimits limits;
        if (mode==0) limits.max_evaluations=1;
        if (mode==1) limits.max_scene_pairs=6;
        if (mode==2) limits.max_evaluations_per_scene_pair=0;
        if (mode==3) limits.max_cells=1;
        if (mode==4) limits.max_depth=1;
        if (mode==5) limits.cancelled=[] {return true;};
        if (mode==6) limits.is_scene_current=[](uint64_t,uint64_t) {return false;};
        if (mode==7) limits.is_current=[](uint64_t) {return false;};
        if (mode==8) {limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};}
        const auto denied=check_simulation_motion(source,0,limits);INFO(mode << ' ' << denied.reason);REQUIRE(denied.status==ClearanceStatus::Unknown);REQUIRE_FALSE(denied.snapshot);
    }
    auto mutable_source=source;SimulationMotionLimits limits;limits.cancelled=[&] {mutable_source={};return false;};
    REQUIRE(check_simulation_motion(mutable_source,0,limits).snapshot);
    limits={};size_t calls=0;limits.cancelled=[&] {++calls;return false;};REQUIRE(check_simulation_motion(source,0,limits).snapshot);
    const size_t final_call=calls;calls=0;limits.cancelled=[&] {return ++calls==final_call;};
    REQUIRE_FALSE(check_simulation_motion(source,0,limits).snapshot);REQUIRE(calls==final_call);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
    const auto rounding=check_simulation_motion(source,0,limits);std::fesetround(FE_TONEAREST);REQUIRE_FALSE(rounding.snapshot);
}

namespace {
SimulationMotionSourceResult lift_source(const std::vector<SceneBox> &obstacles={},bool exhausted_ids=false)
{
    const auto original=scene_material();const auto &ledger=*original.snapshot->ledger;auto rows=ledger.records;
    rows.push_back({{20,1,33,rows.back().motion.end,{0,0,2.4},Speed(10),Acceleration(100),Travel{}},{}});
    rows.push_back({{30,2,33,{0,0,2.4},{0,0,3.8},Speed(10),Acceleration(100),Travel{}},{}});
    auto future=rows.front();future.motion.event_id=exhausted_ids ? std::numeric_limits<uint64_t>::max() : 40;
    future.motion.sequence_index=3;future.motion.start={0,0,3.8};future.motion.end={2,0,3.8};rows.push_back(future);
    const auto material=prepare_material_motion(capture_material_sequence(rows,ledger.model,ledger.revision,ledger.source_fingerprint));REQUIRE(material.snapshot);
    auto scene=material_scene();scene.nozzle_domain.max={3,1,4};
    scene.obstacles.insert(scene.obstacles.end(),obstacles.begin(),obstacles.end());
    const auto result=prepare_simulation_motion(scene,material,material_scene_policy());INFO(result.reason);REQUIRE(result.snapshot);return result;
}
}
TEST_CASE("B09 lifted travel preserves original deposits and checks all three legs against the actual prefix", "[Nonplanar][B09][LiftedTravel]")
{
    STATIC_REQUIRE(simulation_lift_route_version==1);
    STATIC_REQUIRE_FALSE(std::is_aggregate<SimulationLiftRouteSnapshot>::value);
    const auto source=lift_source({{{.95,-.05,2.75},{1.05,.05,2.8}}});const auto &old=*source.snapshot->material->ledger;
    const auto fingerprint=old.fingerprint();const auto direct=check_simulation_motion(source,1);
    REQUIRE(direct.status==ClearanceStatus::Fail);REQUIRE(direct.scene_check);
    const auto route=plan_simulation_lifted_travel(source,1,3);INFO(route.reason);REQUIRE(route.snapshot);
    REQUIRE(route.status==ClearanceStatus::Pass);const auto &r=*route.snapshot;const auto &planned=*r.planned->material->ledger;
    REQUIRE(r.source==source.snapshot);REQUIRE(r.original_event_index==1);REQUIRE(r.legs.size()==3);
    REQUIRE((r.source_records==std::vector<size_t>{0,1,1,1,2,3}));REQUIRE(planned.records.size()==6);
    REQUIRE(planned.records[1].motion.event_id==20);REQUIRE(planned.records[2].motion.event_id==41);REQUIRE(planned.records[3].motion.event_id==42);
    const std::array<PhysicalPosition,4> points{{{2,0,2},{2,0,3},{0,0,3},{0,0,2.4}}};
    // Independent continuous bounds for the whole old bead, annulus and boxes.
    // A tip point over its XY shadow and outside the opening has |dx|>=tangent.
    // Ahead points are beyond the finite butt; behind points have a lower roof.
    using Q=boost::multiprecision::cpp_bin_float_quad;
    const Q margin=source.snapshot->policy.required.value(),inner=source.snapshot->scene.tip.opening_radius.value();
    const Q max_y=Q(old.records[0].bead->width_mm.upper)/2+margin,tangent=sqrt(inner*inner-max_y*max_y);
    REQUIRE(tangent>margin);REQUIRE(Q(2)-margin>1+(Q(2)-tangent+margin)/2);
    REQUIRE(Q(2)+Q(.5)-margin>2); // all six head boxes throughout every leg
    REQUIRE(Q(3)-margin>2);REQUIRE(Q(2.4)-margin>2); // transfer and descent tip
    REQUIRE(Q(2)-Q(.5)-margin>Q(1.05));REQUIRE(Q(.5)+margin<Q(.95)); // complete vertical sweeps avoid wall
    REQUIRE(Q(3)-margin>Q(2.8));REQUIRE(Q(2)-margin>Q(-.6)); // full transfer/wall and plate

    size_t pairs=0,work=0,cells=0;
    for (size_t i=0;i<3;++i) {
        const auto &row=planned.records[i+1];const auto &proof=*r.legs[i];
        REQUIRE(std::holds_alternative<Travel>(row.motion.payload));REQUIRE_FALSE(row.bead);
        REQUIRE(row.motion.start.x()==points[i].x());REQUIRE(row.motion.start.y()==points[i].y());REQUIRE(row.motion.start.z()==points[i].z());
        REQUIRE(row.motion.end.x()==points[i+1].x());REQUIRE(row.motion.end.y()==points[i+1].y());REQUIRE(row.motion.end.z()==points[i+1].z());
        REQUIRE(proof.source==r.planned);REQUIRE(proof.material->event_index==i+1);REQUIRE(proof.material->tools.size()==7);
        pairs+=proof.scene_checks.size();work+=proof.evaluations;cells+=proof.material->cells;
        for (const auto &leaf : proof.material->leaves) {
            REQUIRE(leaf.material_record==0); // The high future bead must never obstruct this route.
            REQUIRE(leaf.parameter.lower>=0);REQUIRE(leaf.parameter.upper<=1);
        }
        for (const auto &check : proof.scene_checks) {REQUIRE(check.event_id==row.motion.event_id);REQUIRE(check.sequence_index==i+1);REQUIRE(check.bounds);}
    }
    REQUIRE(pairs==42);REQUIRE(route.checked_scene_pairs==pairs);REQUIRE(route.evaluations>work);REQUIRE(route.cells==cells);
    for (size_t i : {size_t(0),size_t(4),size_t(5)}) {
        const size_t original=r.source_records[i];auto expected=old.canonical_record(original);
        const auto from=",\"motion\":["+std::to_string(old.records[original].motion.event_id)+","+std::to_string(original)+",";
        const auto to=",\"motion\":["+std::to_string(old.records[original].motion.event_id)+","+std::to_string(i)+",";
        const auto position=expected.find(from);REQUIRE(position!=std::string::npos);expected.replace(position,from.size(),to);
        REQUIRE(planned.canonical_record(i)==expected);
    }
    REQUIRE(old.fingerprint()==fingerprint);REQUIRE(planned.fingerprint()!=fingerprint);
    auto pressure_rows=old.records;
    const MaterialRecord retract{{50,1,33,old.records[1].motion.start,old.records[1].motion.start,Speed(10),Acceleration(100),
        Retraction{FilamentLength(.8),RetractionState::Ready,RetractionState::Retracted}},{}};
    const MaterialRecord restore{{60,3,33,old.records[1].motion.end,old.records[1].motion.end,Speed(10),Acceleration(100),
        Retraction{FilamentLength(.8),RetractionState::Retracted,RetractionState::Ready}},{}};
    pressure_rows.insert(pressure_rows.begin()+1,retract);pressure_rows.insert(pressure_rows.begin()+3,restore);
    for (size_t i=0;i<pressure_rows.size();++i) pressure_rows[i].motion.sequence_index=i;
    const auto pressure_material=prepare_material_motion(capture_material_sequence(pressure_rows,old.model,old.revision,old.source_fingerprint));REQUIRE(pressure_material.snapshot);
    const auto pressure_source=prepare_simulation_motion(source.snapshot->scene,pressure_material,source.snapshot->policy);REQUIRE(pressure_source.snapshot);
    const auto pressure_route=plan_simulation_lifted_travel(pressure_source,2,3);INFO(pressure_route.reason);REQUIRE(pressure_route.snapshot);
    const auto &pressure=*pressure_route.snapshot->planned->material->ledger;
    REQUIRE(std::holds_alternative<Retraction>(pressure.records[1].motion.payload));
    REQUIRE(std::holds_alternative<Retraction>(pressure.records[5].motion.payload));
    REQUIRE(std::get<Retraction>(pressure.records[1].motion.payload).after==RetractionState::Retracted);
    REQUIRE(std::get<Retraction>(pressure.records[5].motion.payload).after==RetractionState::Ready);
    REQUIRE(std::get<Retraction>(pressure.records[1].motion.payload).amount.value()==.8);
    REQUIRE(std::get<Retraction>(pressure.records[5].motion.payload).amount.value()==.8);
    REQUIRE_FALSE(plan_simulation_lifted_travel(pressure_source,1,3).snapshot);

    REQUIRE(r.planned->scene.origin==ProfileOrigin::Synthetic);REQUIRE_FALSE(r.planned->scene.operator_confirmed_claim);
    SimulationLiftRouteLimits limits;limits.max_scene_pairs=41;REQUIRE_FALSE(plan_simulation_lifted_travel(source,1,3,limits).snapshot);
    limits={};limits.max_evaluations=route.evaluations-r.legs.back()->evaluations;
    REQUIRE_FALSE(plan_simulation_lifted_travel(source,1,3,limits).snapshot);
}
TEST_CASE("B09 an interior exit or descent collision blocks lifted travel despite clear leg endpoints", "[Nonplanar][B09][LiftedTravel]")
{
    for (size_t leg : {size_t(0),size_t(2)}) {
        const SceneBox obstacle=leg==0 ? SceneBox{{1.98,-.02,2.9},{2.02,.02,3.0}} : SceneBox{{-.02,-.02,3.26},{.02,.02,3.28}};
        const auto source=lift_source({obstacle});const auto blocked=plan_simulation_lifted_travel(source,1,3);INFO(blocked.reason);
        REQUIRE(blocked.status==ClearanceStatus::Fail);REQUIRE_FALSE(blocked.snapshot);REQUIRE(blocked.blocked_leg==leg);
        REQUIRE(blocked.motion_check);REQUIRE(blocked.motion_check->scene_check);const auto &check=*blocked.motion_check->scene_check;
        REQUIRE(check.witness);REQUIRE(check.obstacle_id==2);REQUIRE(check.component_id==1);
        const long double t=check.witness->parameter;
        const long double z=leg==0 ? 2+t : 3-.6L*t;
        REQUIRE(z+.5L<=obstacle.max.z());REQUIRE(z+.8L>=obstacle.min.z());
        // Both endpoints of the colliding vertical leg have strict clearance.
        const long double za=leg==0 ? 2 : 3,zb=leg==0 ? 3 : 2.4L;
        for (auto p : {za,zb}) REQUIRE((p+.8L<obstacle.min.z()-.005L || p+.5L>obstacle.max.z()+.005L));
    }
}
TEST_CASE("B09 lifted travel owns callbacks and refuses invalid routes or shared resource exhaustion", "[Nonplanar][B09][LiftedTravel]")
{
    const auto source=lift_source();
    REQUIRE_FALSE(plan_simulation_lifted_travel({},1,3).snapshot);REQUIRE_FALSE(plan_simulation_lifted_travel(source,0,3).snapshot);
    REQUIRE_FALSE(plan_simulation_lifted_travel(source,99,3).snapshot);REQUIRE_FALSE(plan_simulation_lifted_travel(source,1,2).snapshot);
    REQUIRE_FALSE(plan_simulation_lifted_travel(source,1,5).snapshot);
    REQUIRE_FALSE(plan_simulation_lifted_travel(source,1,std::numeric_limits<double>::quiet_NaN()).snapshot);
    REQUIRE(plan_simulation_lifted_travel(lift_source({},true),1,3).reason=="LIFT_ROUTE_EVENT_ID_LIMIT");
    for (int mode=0;mode<7;++mode) {
        SimulationLiftRouteLimits limits;
        if (mode==0) limits.max_records=4;
        if (mode==1) limits.max_evaluations=1;
        if (mode==2) limits.max_cells=1;
        if (mode==3) limits.cancelled=[] {return true;};
        if (mode==4) limits.is_scene_current=[](uint64_t,uint64_t) {return false;};
        if (mode==5) limits.is_current=[](uint64_t) {return false;};
        if (mode==6) {limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};}
        const auto denied=plan_simulation_lifted_travel(source,1,3,limits);INFO(mode << ' ' << denied.reason);
        REQUIRE(denied.status==ClearanceStatus::Unknown);REQUIRE_FALSE(denied.snapshot);
    }
    const auto &ledger=*source.snapshot->material->ledger;auto zero_rows=ledger.records;zero_rows.erase(zero_rows.begin()+2,zero_rows.end());
    zero_rows[1].motion.end=zero_rows[1].motion.start;
    const auto zero_material=prepare_material_motion(capture_material_sequence(zero_rows,ledger.model,ledger.revision,ledger.source_fingerprint));REQUIRE(zero_material.snapshot);
    const auto zero_source=prepare_simulation_motion(source.snapshot->scene,zero_material,source.snapshot->policy);REQUIRE(zero_source.snapshot);
    const auto instant=plan_simulation_lifted_travel(zero_source,1,zero_rows[1].motion.start.z());REQUIRE(instant.snapshot);
    REQUIRE(instant.snapshot->legs.size()==1);REQUIRE(instant.snapshot->planned->material->ledger->fingerprint()==zero_material.snapshot->ledger->fingerprint());
    const auto up_down=plan_simulation_lifted_travel(zero_source,1,3);REQUIRE(up_down.snapshot);REQUIRE(up_down.snapshot->legs.size()==2);
    SimulationLiftRouteLimits limits;auto mutable_source=source;
    limits.cancelled=[&] {mutable_source={};limits.max_records=0;return false;};
    const auto owned=plan_simulation_lifted_travel(mutable_source,1,3,limits);INFO(owned.reason);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->source==source.snapshot);
    limits={};size_t calls=0;limits.cancelled=[&] {++calls;return false;};REQUIRE(plan_simulation_lifted_travel(source,1,3,limits).snapshot);
    const size_t last=calls;calls=0;limits.cancelled=[&] {return ++calls==last;};
    REQUIRE(plan_simulation_lifted_travel(source,1,3,limits).reason=="CANCELLED");
    limits={};limits.cancelled=[] {std::fesetround(FE_UPWARD);return false;};
    const auto rounding=plan_simulation_lifted_travel(source,1,3,limits);std::fesetround(FE_TONEAREST);REQUIRE_FALSE(rounding.snapshot);
}

namespace {
LinearMotionPolicy motion_limits()
{
    return {1,77,1,ProfileOrigin::Synthetic,false,LinearPlannerModel::FullStop,LinearKinematics::CoreXY,
        {{-10,-10,0},{10,10,10}},{200,150,2},{1000,800,20},{1,1,2},{50,50,20},Length(1.75),FlowCompensation(1),
        Speed(5),Acceleration(50),Length(2),4,1,200};
}
}
TEST_CASE("B10 full journal limits XYZ E flow acceleration and event rate without changing deposited volume", "[Nonplanar][B10][LinearMotionPlan]")
{
    STATIC_REQUIRE(linear_motion_plan_version==1);
    STATIC_REQUIRE_FALSE(std::is_aggregate<LinearMotionPlanSnapshot>::value);
    const auto source=lift_source();const auto policy=motion_limits();
    const auto result=plan_linear_motion(source,policy);INFO(result.reason);REQUIRE(result.snapshot);
    const auto &plan=*result.snapshot;const auto &old=*source.snapshot->material->ledger;const auto &ledger=*plan.planned->material->ledger;
    REQUIRE(plan.steps.size()==old.records.size());REQUIRE(plan.source==source.snapshot);
    REQUIRE(plan.policy_fingerprint==policy.fingerprint());REQUIRE(ledger.fingerprint()!=old.fingerprint());
    using Q=boost::multiprecision::cpp_bin_float_quad;const Q pi=acos(Q(-1)),area=pi*Q(1.75)*Q(1.75)/4;
    Q volume=0,filament=0,time=0;
    for (size_t i=0;i<plan.steps.size();++i) {
        const auto &step=plan.steps[i];const auto &a=old.records[i].motion;const auto &b=ledger.records[i].motion;
        REQUIRE(b.event_id==a.event_id);REQUIRE(b.sequence_index==a.sequence_index);
        REQUIRE(b.start.x()==a.start.x());REQUIRE(b.start.y()==a.start.y());REQUIRE(b.start.z()==a.start.z());
        REQUIRE(b.end.x()==a.end.x());REQUIRE(b.end.y()==a.end.y());REQUIRE(b.end.z()==a.end.z());
        REQUIRE(b.nominal_layer_label==a.nominal_layer_label);REQUIRE(b.speed_limit.value()<=a.speed_limit.value());
        REQUIRE(b.acceleration_limit.value()<=a.acceleration_limit.value());
        const std::array<Q,3> delta{Q(a.end.x())-Q(a.start.x()),Q(a.end.y())-Q(a.start.y()),Q(a.end.z())-Q(a.start.z())};
        const Q length=sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
        REQUIRE(Q(step.distance_mm.lower)<=length);REQUIRE(length<=Q(step.distance_mm.upper));
        for (size_t axis=0;axis<3;++axis) {
            REQUIRE(Q(step.peak_speed_mm_s)*abs(delta[axis])/length<=Q(policy.axis_speed_mm_s[axis]));
            REQUIRE(Q(step.acceleration_mm_s2)*abs(delta[axis])/length<=Q(policy.axis_acceleration_mm_s2[axis]));
        }
        const std::array<Q,3> drive{delta[0]+delta[1],delta[0]-delta[1],delta[2]};
        for(size_t axis=0;axis<3;++axis) {
            REQUIRE(Q(step.peak_speed_mm_s)*abs(drive[axis])/length<=Q(policy.drive_speed_mm_s[axis]));
            REQUIRE(Q(step.acceleration_mm_s2)*abs(drive[axis])/length<=Q(policy.drive_acceleration_mm_s2[axis]));
        }
        Q e=0;
        if (const auto *deposition=std::get_if<Deposition>(&a.payload)) {
            REQUIRE(std::holds_alternative<Deposition>(b.payload));REQUIRE(std::get<Deposition>(b.payload).volume.value()==deposition->volume.value());
            e=Q(deposition->volume.value())/area;volume+=Q(deposition->volume.value());filament+=e;
            REQUIRE(Q(step.peak_speed_mm_s)*Q(deposition->volume.value())/length<=Q(policy.max_volume_mm3_s));
            REQUIRE(Q(deposition->volume.value())/length<=Q(policy.max_extrude_cross_section_mm2));
        } else REQUIRE(std::holds_alternative<Travel>(b.payload));
        REQUIRE(Q(step.filament_mm.lower)<=e);REQUIRE(e<=Q(step.filament_mm.upper));
        REQUIRE(Q(step.peak_speed_mm_s)*e/length<=Q(policy.filament_speed.value()));
        REQUIRE(Q(step.acceleration_mm_s2)*e/length<=Q(policy.filament_acceleration.value()));
        const Q v=step.peak_speed_mm_s,acc=step.acceleration_mm_s2;
        REQUIRE(v*v<=acc*length);const Q duration=length/v+v/acc;
        REQUIRE(Q(step.duration_s.lower)<=duration);REQUIRE(duration<=Q(step.duration_s.upper));
        REQUIRE(duration>=Q(1)/Q(policy.max_events_per_second));time+=duration;
    }
    REQUIRE(Q(plan.nominal_volume_mm3.lower)<=volume);REQUIRE(volume<=Q(plan.nominal_volume_mm3.upper));
    REQUIRE(Q(plan.deposition_filament_mm.lower)<=filament);REQUIRE(filament<=Q(plan.deposition_filament_mm.upper));
    REQUIRE(Q(plan.duration_s.lower)<=time);REQUIRE(time<=Q(plan.duration_s.upper));
    auto flow=policy;flow.flow=FlowCompensation(1.2);const auto compensated=plan_linear_motion(source,flow);REQUIRE(compensated.snapshot);
    REQUIRE(compensated.snapshot->nominal_volume_mm3.lower==plan.nominal_volume_mm3.lower);
    REQUIRE(compensated.snapshot->nominal_volume_mm3.upper==plan.nominal_volume_mm3.upper);
    REQUIRE(compensated.snapshot->deposition_filament_mm.lower>plan.deposition_filament_mm.upper*1.19);
    REQUIRE(flow.fingerprint()!=policy.fingerprint());
}
TEST_CASE("B10 pressure rows and instant travel retain separate full stop schedules without phantom deposit", "[Nonplanar][B10][LinearMotionPlan]")
{
    const auto source=lift_source();const auto &old=*source.snapshot->material->ledger;auto rows=old.records;
    rows.insert(rows.begin()+1,{{50,1,0,rows[0].motion.end,rows[0].motion.end,Speed(10),Acceleration(100),
        Retraction{FilamentLength(.8),RetractionState::Ready,RetractionState::Retracted}},{}});
    rows.insert(rows.begin()+3,{{60,3,0,rows[2].motion.end,rows[2].motion.end,Speed(10),Acceleration(100),
        Retraction{FilamentLength(.8),RetractionState::Retracted,RetractionState::Ready}},{}});
    rows.insert(rows.begin()+4,{{70,4,0,rows[3].motion.end,rows[3].motion.end,Speed(10),Acceleration(100),Travel{}},{}});
    for(size_t i=0;i<rows.size();++i) rows[i].motion.sequence_index=i;
    const auto material=prepare_material_motion(capture_material_sequence(rows,old.model,old.revision,old.source_fingerprint));REQUIRE(material.snapshot);
    const auto prepared=prepare_simulation_motion(source.snapshot->scene,material,source.snapshot->policy);REQUIRE(prepared.snapshot);
    const auto policy=motion_limits();const auto plan=plan_linear_motion(prepared,policy);INFO(plan.reason);REQUIRE(plan.snapshot);
    REQUIRE(plan.snapshot->steps.size()==rows.size());
    REQUIRE(plan.snapshot->steps[1].coordinate==LinearStepCoordinate::Filament);REQUIRE(plan.snapshot->steps[1].filament_mm.lower==-.8);
    REQUIRE(plan.snapshot->steps[3].coordinate==LinearStepCoordinate::Filament);REQUIRE(plan.snapshot->steps[3].filament_mm.upper==.8);
    REQUIRE(plan.snapshot->steps[4].coordinate==LinearStepCoordinate::Dwell);
    REQUIRE(plan.snapshot->steps[4].duration_s.lower>=1/policy.max_events_per_second);
    for(size_t i : {size_t(1),size_t(3),size_t(4)}) REQUIRE(plan.snapshot->planned->material->ledger->canonical_record(i)==material.snapshot->ledger->canonical_record(i));
    const auto original=plan_linear_motion(source,policy);REQUIRE(original.snapshot);
    REQUIRE(plan.snapshot->nominal_volume_mm3.lower==original.snapshot->nominal_volume_mm3.lower);
    REQUIRE(plan.snapshot->nominal_volume_mm3.upper==original.snapshot->nominal_volume_mm3.upper);
    auto exceeded=policy;exceeded.max_retraction=Length(.7);REQUIRE_FALSE(plan_linear_motion(prepared,exceeded).snapshot);
}
TEST_CASE("B10 CoreXY diagonals short segments and volume flow each constrain the complete plan", "[Nonplanar][B10][LinearMotionPlan]")
{
    const auto original=lift_source();const auto &old=*original.snapshot->material->ledger;auto rows=old.records;
    rows[1].motion.end={2.1,.1,2};rows[2].motion.start=rows[1].motion.end;
    const auto material=prepare_material_motion(capture_material_sequence(rows,old.model,old.revision,old.source_fingerprint));REQUIRE(material.snapshot);
    const auto source=prepare_simulation_motion(original.snapshot->scene,material,original.snapshot->policy);REQUIRE(source.snapshot);
    const auto policy=motion_limits();const auto core=plan_linear_motion(source,policy);REQUIRE(core.snapshot);
    auto cartesian=policy;cartesian.kinematics=LinearKinematics::Cartesian;
    const auto xyz=plan_linear_motion(source,cartesian);REQUIRE(xyz.snapshot);
    using Q=boost::multiprecision::cpp_bin_float_quad;const Q dx=Q(2.1)-Q(2),dy=Q(.1),length=sqrt(dx*dx+dy*dy);
    REQUIRE(Q(core.snapshot->steps[1].peak_speed_mm_s)*(dx+dy)/length<=Q(1));
    REQUIRE(xyz.snapshot->steps[1].peak_speed_mm_s>core.snapshot->steps[1].peak_speed_mm_s*1.99);
    REQUIRE(cartesian.fingerprint()!=policy.fingerprint());
    rows[1].motion.end={2.0001,0,2};rows[2].motion.start=rows[1].motion.end;
    const auto short_material=prepare_material_motion(capture_material_sequence(rows,old.model,old.revision,old.source_fingerprint));REQUIRE(short_material.snapshot);
    const auto short_source=prepare_simulation_motion(original.snapshot->scene,short_material,original.snapshot->policy);REQUIRE(short_source.snapshot);
    const auto short_plan=plan_linear_motion(short_source,policy);REQUIRE(short_plan.snapshot);
    const auto &short_step=short_plan.snapshot->steps[1];REQUIRE(short_step.peak_speed_mm_s<.021);
    REQUIRE(short_step.duration_s.lower>=1/policy.max_events_per_second);
    auto low_flow=policy;low_flow.max_volume_mm3_s=.00001;const auto slowed=plan_linear_motion(original,low_flow);REQUIRE(slowed.snapshot);
    REQUIRE(slowed.snapshot->steps[0].peak_speed_mm_s<core.snapshot->steps[0].peak_speed_mm_s/1000);
    auto diameter=policy;diameter.filament_diameter=Length(2);const auto wider=plan_linear_motion(original,diameter);REQUIRE(wider.snapshot);
    const auto regular=plan_linear_motion(original,policy);REQUIRE(regular.snapshot);
    const Q expected=Q(std::get<Deposition>(old.records[0].motion.payload).volume.value())/acos(Q(-1));
    REQUIRE(Q(wider.snapshot->steps[0].filament_mm.lower)<=expected);REQUIRE(expected<=Q(wider.snapshot->steps[0].filament_mm.upper));
    REQUIRE(wider.snapshot->deposition_filament_mm.upper<regular.snapshot->deposition_filament_mm.lower*.77);
}
TEST_CASE("B10 complete motion plan owns limits and refuses invalid profiles stale publication or resource exhaustion", "[Nonplanar][B10][LinearMotionPlan]")
{
    const auto source=lift_source();const auto policy=motion_limits();
    REQUIRE_FALSE(plan_linear_motion({},policy).snapshot);
    for(int mode=0;mode<11;++mode) {
        auto bad=policy;
        if(mode==0)bad.version=2;if(mode==1)bad.profile_id=0;if(mode==2)bad.revision=0;
        if(mode==3)bad.operator_confirmed_claim=true;if(mode==4)bad.origin=ProfileOrigin::OperatorMeasured;
        if(mode==5)bad.model=LinearPlannerModel::FirmwareLookahead;if(mode==6)bad.axis_speed_mm_s[2]=0;
        if(mode==7)bad.max_events_per_second=std::numeric_limits<double>::quiet_NaN();
        if(mode==8)bad.commanded_domain.max={1,1,1};
        if(mode==9)bad.drive_speed_mm_s[0]=0;if(mode==10)bad.kinematics=static_cast<LinearKinematics>(99);
        REQUIRE_FALSE(plan_linear_motion(source,bad).snapshot);
    }
    auto section=policy;section.max_extrude_cross_section_mm2=.0001;REQUIRE_FALSE(plan_linear_motion(source,section).snapshot);
    for(int mode=0;mode<7;++mode) {
        LinearMotionPlanLimits limits;
        if(mode==0)limits.max_records=1;if(mode==1)limits.max_evaluations=1;
        if(mode==2)limits.cancelled=[] {return true;};if(mode==3)limits.is_current=[](uint64_t) {return false;};
        if(mode==4)limits.is_scene_current=[](uint64_t,uint64_t) {return false;};
        if(mode==5)limits.is_policy_current=[](uint64_t,uint64_t) {return false;};
        if(mode==6){limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};}
        REQUIRE_FALSE(plan_linear_motion(source,policy,limits).snapshot);
    }
    auto mutable_source=source;auto mutable_policy=policy;LinearMotionPlanLimits limits;
    limits.cancelled=[&] {mutable_source={};mutable_policy.max_events_per_second=0;limits.max_records=0;return false;};
    const auto owned=plan_linear_motion(mutable_source,mutable_policy,limits);INFO(owned.reason);REQUIRE(owned.snapshot);
    REQUIRE(owned.snapshot->source==source.snapshot);REQUIRE(owned.snapshot->policy.fingerprint()==policy.fingerprint());
    limits={};size_t calls=0;limits.cancelled=[&] {++calls;return false;};REQUIRE(plan_linear_motion(source,policy,limits).snapshot);
    const size_t last=calls;calls=0;limits.cancelled=[&] {return ++calls==last;};REQUIRE_FALSE(plan_linear_motion(source,policy,limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
    const auto rounded=plan_linear_motion(source,policy,limits);std::fesetround(FE_TONEAREST);REQUIRE_FALSE(rounded.snapshot);
}
