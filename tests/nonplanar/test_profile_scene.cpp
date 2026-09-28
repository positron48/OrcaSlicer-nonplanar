#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/ProfileScene.hpp>
#include <cfenv>
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
