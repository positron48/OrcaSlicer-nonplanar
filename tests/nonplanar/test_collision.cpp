#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <libslic3r/Nonplanar/Collision.hpp>
#include <algorithm>
#include <array>
#include <cfenv>
#include <random>

using namespace Slic3r::nptop;
using Catch::Matchers::WithinAbs;

namespace {
MotionEvent motion(PhysicalPosition from, PhysicalPosition to)
{ return {41, 7, 0, from, to, Speed(10), Acceleration(100), Travel{}}; }
ClearancePolicy policy(double clearance = 0, double error = 0)
{ return {Length(clearance), NumericBudget(error, 0, 0, 0), Length(0), Length(0), Length(0)}; }
ToolComponent box()
{ return {12, ToolBox{ToolPosition(-.5, -.5, 0), ToolPosition(.5, .5, 1)}}; }
ToolComponent tip(double outer = .5)
{ return {13, FiniteTip{ToolPosition(0, 0, 0), Length(.2), Length(outer)}}; }
void contains(const ClearanceResult &result, double expected)
{
    REQUIRE(result.bounds.has_value());
    REQUIRE(result.bounds->lower_mm <= expected);
    REQUIRE(result.bounds->upper_mm >= expected);
}
}

TEST_CASE("GEO-02 finite outer tip uses full plane gradient", "[Nonplanar][GEO-02]")
{
    // Gradient (3/4, 0): radius .5 gives vertical loss .375, normalizer 1.25.
    SceneObstacle plane{21, PlaneObstacle{.75, 0, 0}};
    const auto path = motion({0, 0, .25}, {0, 10, .25});
    auto result = query_clearance(path, tip(), plane, policy());
    REQUIRE(result.status == ClearanceStatus::Fail);
    contains(result, -.1);
    auto opening_only = query_clearance(path, tip(.21), plane, policy());
    REQUIRE(opening_only.status == ClearanceStatus::Pass);
    auto clear = query_clearance(motion({0, 0, 1}, {4, 0, 4}), tip(), plane, policy(.1));
    REQUIRE(clear.status == ClearanceStatus::Pass);
    contains(clear, .5);
    REQUIRE(clear.bounds->lower_mm > .49);
}

TEST_CASE("GEO-03 transverse slope collides along a constant Z path", "[Nonplanar][GEO-03]")
{
    // (a,b) = (.3,.4) has gradient norm .5, while the path tangent (4,-3,0)
    // has dot product zero. Height along the centreline is constant.
    SceneObstacle plane{21, PlaneObstacle{.3, .4, 0}};
    auto result = query_clearance(motion({0, 0, .2}, {4, -3, .2}), tip(), plane, policy());
    REQUIRE(result.status == ClearanceStatus::Fail);
    contains(result, -.05 / std::sqrt(1.25));
    REQUIRE(result.witness.has_value());
}

TEST_CASE("GEO-01 asymmetric tool axes stay fixed when the path turns", "[Nonplanar][GEO-01]")
{
    ToolComponent duct{14, ToolBox{ToolPosition(2, -.5, 0), ToolPosition(4, .5, 2)}};
    SceneObstacle wall{22, SceneBox{PhysicalPosition(3, 4, 0), PhysicalPosition(5, 6, 2)}};
    // At nozzle (0,5,0) the duct still occupies x=[2,4], y=[4.5,5.5].
    const PhysicalPosition points[] = {{0, 0, 0}, {0, 5, 0}, {-5, 5, 0}};
    for (size_t i = 1; i < 3; ++i) {
        auto result = query_clearance(motion(points[i-1], points[i]), duct, wall, policy());
        REQUIRE(result.status == ClearanceStatus::Fail);
        REQUIRE(result.component_id == 14);
    }
    // No tangent at all is also a supported pose query.
    REQUIRE(query_clearance(motion(points[1], points[1]), duct, wall, policy()).status == ClearanceStatus::Fail);
}

TEST_CASE("GEO-04 continuous sweep catches a wall between free endpoints", "[Nonplanar][GEO-04]")
{
    SceneObstacle wall{23, SceneBox{PhysicalPosition(4, -1, 0), PhysicalPosition(6, 1, 1)}};
    for (double x : {0., 10.})
        REQUIRE(query_clearance(motion({x,0,0}, {x,0,0}), box(), wall, policy()).status == ClearanceStatus::Pass);
    auto result = query_clearance(motion({0,0,0}, {10,0,0}), box(), wall, policy());
    REQUIRE(result.status == ClearanceStatus::Fail);
    contains(result, -1);
    REQUIRE(result.witness->parameter >= .35);
    REQUIRE(result.witness->parameter <= .65);
    REQUIRE(result.event_id == 41);
    REQUIRE(result.sequence_index == 7);
    REQUIRE(result.obstacle_id == 23);
    // Narrow collision away from midpoint; no fixed endpoint/midpoint sampler suffices.
    ToolComponent tiny{15, ToolBox{ToolPosition(-.001,-.001,0), ToolPosition(.001,.001,.01)}};
    SceneObstacle thin{24, SceneBox{PhysicalPosition(1.234,-1,0), PhysicalPosition(1.236,1,1)}};
    auto narrow = query_clearance(motion({0,0,0}, {10,0,0}), tiny, thin, policy());
    REQUIRE(narrow.status == ClearanceStatus::Fail);
    REQUIRE(narrow.witness->parameter > .1233);
    REQUIRE(narrow.witness->parameter < .1237);
}

TEST_CASE("GEO-05 duct collision is independent of nozzle clearance", "[Nonplanar][GEO-05]")
{
    auto path = motion({0,0,0}, {0,10,0});
    SceneObstacle wall{25, SceneBox{PhysicalPosition(3,4,0), PhysicalPosition(5,6,2)}};
    REQUIRE(query_clearance(path, box(), wall, policy()).status == ClearanceStatus::Pass);
    ToolComponent duct{16, ToolBox{ToolPosition(2,-.5,0), ToolPosition(4,.5,2)}};
    auto result = query_clearance(path, duct, wall, policy());
    REQUIRE(result.status == ClearanceStatus::Fail);
    REQUIRE(result.component_id == 16);
    REQUIRE(result.obstacle_id == 25);
}

TEST_CASE("GEO-04 bounded subdivision proves a diagonal sweep clear", "[Nonplanar][GEO-04]")
{
    // Whole swept AABB overlaps this obstacle; the actual diagonal route does not.
    SceneObstacle remote{26, SceneBox{PhysicalPosition(0,8,0), PhysicalPosition(1,9,1)}};
    auto result = query_clearance(motion({0,0,0}, {10,10,0}), box(), remote, policy(.1));
    REQUIRE(result.status == ClearanceStatus::Pass);
    REQUIRE(result.bounds->lower_mm > .1);
    REQUIRE(result.evaluations > 1);
    // Independent 3-4-5 distance between static box faces.
    SceneObstacle diagonal{27, SceneBox{PhysicalPosition(3.5,4.5,0), PhysicalPosition(4.5,5.5,1)}};
    auto fixed = query_clearance(motion({0,0,0}, {0,0,0}), box(), diagonal, policy(4.9));
    REQUIRE(fixed.status == ClearanceStatus::Pass);
    contains(fixed, 5);
    REQUIRE_THAT(fixed.bounds->lower_mm, WithinAbs(5, 1e-10));
}

TEST_CASE("GEO-07 uncertain clearance never passes at the boundary", "[Nonplanar][GEO-07]")
{
    SceneObstacle plane{28, PlaneObstacle{0,0,0}};
    SceneObstacle block{29, SceneBox{PhysicalPosition(-1,-1,-1), PhysicalPosition(1,1,0)}};
    for (double z : {.07, .1, .13}) {
        DYNAMIC_SECTION("clearance at z=" << z) {
            auto expected = z < .08 ? ClearanceStatus::Fail : z > .12 ? ClearanceStatus::Pass : ClearanceStatus::Unknown;
            auto path = motion({0,0,z}, {0,0,z});
            auto a = query_clearance(path, tip(), plane, policy(.1,.02));
            auto b = query_clearance(path, box(), block, policy(.1,.02));
            REQUIRE(a.status == expected);
            REQUIRE(b.status == expected);
            contains(a,z);
            contains(b,z);
        }
    }
    REQUIRE(query_clearance(motion({0,0,0},{0,0,0}), tip(), plane, policy()).status == ClearanceStatus::Unknown);
    auto charged = policy(.1);
    charged.numeric = NumericBudget(.005,.005,.005,.005);
    charged.tool_measurement = Length(.01);
    charged.positioning = Length(.01);
    charged.material = Length(.01);
    auto result = query_clearance(motion({0,0,.13},{0,0,.13}), tip(), plane, charged);
    REQUIRE(result.status == ClearanceStatus::Unknown);
    REQUIRE(result.bounds->bound_error_mm >= .05);
}

TEST_CASE("GEO-08 unsupported queries and exhausted limits stay unknown", "[Nonplanar][GEO-08]")
{
    auto path = motion({0,0,10}, {1,0,10});
    SceneObstacle plane{30, PlaneObstacle{0,0,0}};
    REQUIRE(query_clearance(path, box(), plane, policy()).reason == ClearanceReason::UnsupportedPair);
    SceneObstacle wall{31, SceneBox{PhysicalPosition(4,-1,0), PhysicalPosition(6,1,1)}};
    // Annulus/box is supported by geometry contract v3; box/sphere is not.
    SceneObstacle sphere{35,SphereObstacle{PhysicalPosition(4,0,0),Length(1)}};
    auto unsupported = query_clearance(path, box(), sphere, policy());
    REQUIRE(unsupported.status == ClearanceStatus::Unknown);
    REQUIRE_FALSE(unsupported.bounds.has_value());
    auto contact = tip();
    contact.interaction = InteractionClass::DepositionContact;
    REQUIRE(query_clearance(path, contact, plane, policy()).reason == ClearanceReason::UnsupportedContact);
    QueryLimits none;
    none.max_evaluations = 0;
    REQUIRE(query_clearance(path, tip(), plane, policy(), none).reason == ClearanceReason::WorkLimit);
    QueryLimits expired;
    expired.deadline = std::chrono::steady_clock::now();
    auto timeout = query_clearance(path, tip(), plane, policy(), expired);
    REQUIRE(timeout.status == ClearanceStatus::Unknown);
    REQUIRE(timeout.reason == ClearanceReason::Timeout);
    QueryLimits one;
    one.max_evaluations = 1;
    auto exhausted = query_clearance(motion({0,0,0},{10,0,0}), box(), wall, policy(), one);
    REQUIRE(exhausted.status == ClearanceStatus::Unknown);
    REQUIRE(exhausted.reason == ClearanceReason::WorkLimit);
}

TEST_CASE("GEO-10 annular tip box gap preserves the opening and analytic distances", "[Nonplanar][GEO-10]")
{
    const auto fixed=motion({0,0,0},{0,0,0});
    SceneObstacle diagonal{36,SceneBox{{3,4,3},{4,5,4}}};
    const auto separated=query_clearance(fixed,tip(1),diagonal,policy(4.9));
    REQUIRE(separated.status==ClearanceStatus::Pass);
    REQUIRE(separated.metric==ClearanceMetric::UnsignedGap);
    contains(separated,5);
    REQUIRE(separated.bounds->lower_mm>4.999999999);
    ToolComponent annulus{37,FiniteTip{{0,0,0},Length(1),Length(2)}};
    SceneObstacle pin{38,SceneBox{{-.375,-.5,-.5},{.375,.5,.5}}};
    const auto opening=query_clearance(fixed,annulus,pin,policy(.3));
    REQUIRE(opening.status==ClearanceStatus::Pass);
    contains(opening,.375);
    auto disk=annulus; std::get<FiniteTip>(disk.geometry).opening_radius=Length(0);
    const auto solid=query_clearance(fixed,disk,pin,policy(.1));
    REQUIRE(solid.status==ClearanceStatus::Fail);
    contains(solid,0);
    const auto zero=query_clearance(fixed,disk,pin,policy());
    REQUIRE(zero.status==ClearanceStatus::Unknown);
    REQUIRE(zero.reason==ClearanceReason::UncertainBoundary);
    REQUIRE(zero.metric==ClearanceMetric::UnsignedGap);
    contains(zero,0);
}
TEST_CASE("GEO-10 continuous rim collision has clear endpoints and midpoint", "[Nonplanar][GEO-10]")
{
    ToolComponent annulus{39,FiniteTip{{0,0,0},Length(1),Length(2)}};
    SceneObstacle pin{40,SceneBox{{-.125,-.125,-.5},{.125,.125,.5}}};
    for (double x : {-4.,0.,4.})
        REQUIRE(query_clearance(motion({x,0,0},{x,0,0}),annulus,pin,policy(.1)).status==ClearanceStatus::Pass);
    for (bool reverse : {false,true}) {
        const auto collision=query_clearance(motion({reverse?4.:-4.,0,0},{reverse?-4.:4.,0,0}),annulus,pin,policy(.1));
        REQUIRE(collision.status==ClearanceStatus::Fail);
        REQUIRE(collision.witness);
        contains(collision,0);
        const auto t=collision.witness->parameter;
        REQUIRE(((t>.23 && t<.4) || (t>.6 && t<.77)));
        REQUIRE(collision.metric==ClearanceMetric::UnsignedGap);
    }
}
TEST_CASE("GEO-10 annular box gaps retain uncertainty transforms and work limits", "[Nonplanar][GEO-10]")
{
    const auto path=motion({0,0,0},{0,0,0});
    SceneObstacle wall{41,SceneBox{{1,-.125,-1},{2,.125,1}}};
    auto result=query_clearance(path,tip(.5),wall,policy(.49));
    REQUIRE(result.status==ClearanceStatus::Pass);
    contains(result,.5);
    REQUIRE(query_clearance(path,tip(.5),wall,policy(.49,.02)).status==ClearanceStatus::Unknown);
    REQUIRE(query_clearance(path,tip(.75),wall,policy(.49)).status==ClearanceStatus::Fail);
    const auto translated=motion({100,-50,3},{100,-50,3});
    SceneObstacle moved{42,SceneBox{{101,-50.125,2},{102,-49.875,4}}};
    const auto same=query_clearance(translated,tip(.5),moved,policy(.49));
    REQUIRE(same.status==ClearanceStatus::Pass);
    contains(same,.5);
    auto offset=tip(.5); std::get<FiniteTip>(offset.geometry).center=ToolPosition(1,0,0);
    REQUIRE(query_clearance(path,offset,wall,policy(.1)).status==ClearanceStatus::Fail);
    QueryLimits none; none.max_evaluations=0;
    REQUIRE(query_clearance(path,tip(),wall,policy(),none).reason==ClearanceReason::WorkLimit);
    none={}; none.deadline=std::chrono::steady_clock::now();
    REQUIRE(query_clearance(path,tip(),wall,policy(),none).reason==ClearanceReason::Timeout);
}
TEST_CASE("GEO-10 interval subdivision proves a clear diagonal annulus sweep", "[Nonplanar][GEO-10]")
{
    SceneObstacle remote{43,SceneBox{{0,8,-1},{1,9,1}}};
    const auto result=query_clearance(motion({0,0,0},{10,10,0}),tip(),remote,policy(.1));
    REQUIRE(result.status==ClearanceStatus::Pass);
    REQUIRE(result.evaluations>1);
    REQUIRE(result.bounds->lower_mm>.1);
    // Independent nearest corner (1,8) has perpendicular distance 7/sqrt(2)
    // from x=y. The nearest annular rim removes exactly its outer radius .5.
    contains(result,7/std::sqrt(2.)-.5);
    QueryLimits one; one.max_evaluations=1;
    const auto exhausted=query_clearance(motion({0,0,0},{10,10,0}),tip(),remote,policy(.1),one);
    REQUIRE(exhausted.status==ClearanceStatus::Unknown);
    REQUIRE(exhausted.reason==ClearanceReason::WorkLimit);
}

TEST_CASE("GEO-08 malformed geometry and numeric environment fail closed", "[Nonplanar][GEO-08]")
{
    auto path = motion({0,0,10}, {1,0,10});
    SceneObstacle plane{32, PlaneObstacle{0,0,0}};
    auto bad_tip = tip(.1);
    REQUIRE(query_clearance(path, bad_tip, plane, policy()).reason == ClearanceReason::InvalidInput);
    auto bad_budget = policy();
    bad_budget.numeric.distance_mm = -1;
    REQUIRE(query_clearance(path, tip(), plane, bad_budget).status == ClearanceStatus::Unknown);
    plane.geometry = PlaneObstacle{std::numeric_limits<double>::infinity(),0,0};
    REQUIRE(query_clearance(path, tip(), plane, policy()).status == ClearanceStatus::Unknown);
    plane.geometry = PlaneObstacle{1e308,0,0};
    REQUIRE(query_clearance(path, tip(), plane, policy()).status == ClearanceStatus::Unknown);
    plane.geometry = PlaneObstacle{0,0,0};
    struct RestoreRounding { int original = std::fegetround(); ~RestoreRounding() { std::fesetround(original); } } rounding;
    REQUIRE(std::fesetround(FE_DOWNWARD) == 0);
    REQUIRE(query_clearance(path, tip(), plane, policy()).reason == ClearanceReason::NumericalFailure);
}

TEST_CASE("GEO-04 translation reversal and envelope growth preserve clearance decisions", "[Nonplanar][GEO-04]")
{
    SceneObstacle wall{33, SceneBox{PhysicalPosition(4,-1,0),PhysicalPosition(6,1,1)}};
    for (bool reverse : {false,true}) {
        auto result = query_clearance(motion({reverse?10.:0.,0,0},{reverse?0.:10.,0,0}), box(), wall, policy());
        REQUIRE(result.status == ClearanceStatus::Fail);
    }
    SceneObstacle translated{34, SceneBox{PhysicalPosition(104,-31,20),PhysicalPosition(106,-29,21)}};
    REQUIRE(query_clearance(motion({100,-30,20},{110,-30,20}), box(), translated, policy()).status == ClearanceStatus::Fail);
    auto path = motion({0,0,0},{0,0,0});
    auto small = query_clearance(path, box(), wall, policy(.1));
    auto big_box = box();
    big_box.geometry = ToolBox{ToolPosition(-.5,-.5,0),ToolPosition(5,.5,1)};
    auto big = query_clearance(path, big_box, wall, policy(.1));
    REQUIRE(small.status == ClearanceStatus::Pass);
    REQUIRE(big.status == ClearanceStatus::Fail);
    REQUIRE(big.bounds->upper_mm < small.bounds->lower_mm);
    REQUIRE(query_clearance(path, box(), wall, policy(4)).status == ClearanceStatus::Fail);
}

TEST_CASE("GEO-04 entering contact does not starve a later penetration", "[Nonplanar][GEO-04]")
{
    // Seed 4704, scene 29. Exact expanded-box slab interval: [1/2,9/11].
    // A depth-first search of [0,1/2] can get stuck at contact while [1/2,1]
    // already contains penetration. Keep literal inputs across RNG libraries.
    ToolComponent asymmetric{17, ToolBox{ToolPosition(-2,-1,0),ToolPosition(3,2,2)}};
    SceneObstacle fixed{35, SceneBox{PhysicalPosition(-2,-2,-2),PhysicalPosition(2,2,2)}};
    const auto result = query_clearance(motion({5,5,-1},{3,-6,1}), asymmetric, fixed, policy());
    REQUIRE(result.status == ClearanceStatus::Fail);
    REQUIRE(result.witness.has_value());
    REQUIRE(result.witness->parameter > .5);
    REQUIRE(result.witness->parameter < 9./11.);
}

TEST_CASE("GEO-04 sweep agrees with an independent slab intersection oracle", "[Nonplanar][GEO-04]")
{
    // Independent Minkowski/slab intersection in long double, not interval
    // distance evaluation or the production subdivision logic. Integral input
    // coordinates make all slab numerators exact. Tangencies stay unclassified.
    std::mt19937 generator(4704);
    std::uniform_int_distribution<int> coord(-10,10);
    const std::array<long double,3> low{-2,-1,0}, high{3,2,2};
    ToolComponent asymmetric{17, ToolBox{ToolPosition(-2,-1,0),ToolPosition(3,2,2)}};
    SceneObstacle fixed{35, SceneBox{PhysicalPosition(-2,-2,-2),PhysicalPosition(2,2,2)}};
    unsigned clear_count = 0, hit_count = 0;
    for (unsigned trial = 0; trial < 300; ++trial) {
        std::array<double,3> a, b;
        for (size_t axis = 0; axis < 3; ++axis) { a[axis] = coord(generator); b[axis] = coord(generator); }
        long double entry = 0, exit = 1;
        bool parallel_boundary = false;
        for (size_t axis = 0; axis < 3; ++axis) {
            const long double left = -2-high[axis], right = 2-low[axis];
            const long double delta = b[axis]-a[axis];
            if (delta == 0) {
                if (a[axis] < left || a[axis] > right) exit = -1;
                if (a[axis] == left || a[axis] == right) parallel_boundary = true;
            } else {
                long double t0 = (left-a[axis])/delta, t1 = (right-a[axis])/delta;
                if (t0 > t1) std::swap(t0,t1);
                entry = std::max(entry,t0);
                exit = std::min(exit,t1);
            }
        }
        CAPTURE(trial,entry,exit);
        const auto result = query_clearance(motion({a[0],a[1],a[2]},{b[0],b[1],b[2]}), asymmetric, fixed, policy());
        CAPTURE(a,b,result.reason,result.evaluations);
        if (exit < entry-1e-6L) {
            REQUIRE(result.status == ClearanceStatus::Pass);
            ++clear_count;
        } else if (!parallel_boundary && exit > entry+1e-6L) {
            REQUIRE(result.status == ClearanceStatus::Fail);
            REQUIRE(result.witness.has_value());
            REQUIRE(result.witness->parameter >= entry);
            REQUIRE(result.witness->parameter <= exit);
            ++hit_count;
        } else {
            REQUIRE(result.status == ClearanceStatus::Unknown);
        }
    }
    REQUIRE(clear_count > 50);
    REQUIRE(hit_count > 50);
}

TEST_CASE("GEO-09 finite annulus distinguishes a sphere in the opening from rim contact", "[Nonplanar][GEO-09]")
{
    SceneObstacle sphere{40,SphereObstacle{PhysicalPosition(0,0,0),Length(.1)}};
    const auto stationary = motion({0,0,0},{0,0,0});
    auto gap = query_clearance(stationary,tip(),sphere,policy(.05));
    REQUIRE(gap.status == ClearanceStatus::Pass);
    contains(gap,.1); // Opening radius .2 minus sphere radius .1.
    sphere.geometry = SphereObstacle{PhysicalPosition(.3,0,0),Length(.1)};
    auto hit = query_clearance(stationary,tip(),sphere,policy());
    REQUIRE(hit.status == ClearanceStatus::Fail);
    contains(hit,-.1); // Sphere centre lies on the solid annulus.
    sphere.geometry = SphereObstacle{PhysicalPosition(3.5,0,4),Length(1)};
    auto diagonal = query_clearance(stationary,tip(),sphere,policy(3.9));
    REQUIRE(diagonal.status == ClearanceStatus::Pass);
    contains(diagonal,4); // Independent 3-4-5 distance to outer rim minus 1.
}

TEST_CASE("GEO-09 continuous annulus sweep detects a sphere between free endpoints", "[Nonplanar][GEO-09]")
{
    SceneObstacle sphere{41,SphereObstacle{PhysicalPosition(.3,0,0),Length(.1)}};
    for (double z : {-1.,1.})
        REQUIRE(query_clearance(motion({0,0,z},{0,0,z}),tip(),sphere,policy()).status == ClearanceStatus::Pass);
    auto result = query_clearance(motion({0,0,-1},{0,0,1}),tip(),sphere,policy());
    REQUIRE(result.status == ClearanceStatus::Fail);
    contains(result,-.1);
    REQUIRE(result.witness.has_value());
    REQUIRE(result.witness->parameter > .45);
    REQUIRE(result.witness->parameter < .55);
    // An off-midpoint sphere is also detected without relying on endpoint sampling.
    sphere.geometry = SphereObstacle{PhysicalPosition(.3,0,.234),Length(.001)};
    REQUIRE(query_clearance(motion({0,0,0},{0,0,1}),tip(),sphere,policy()).status == ClearanceStatus::Fail);
}

TEST_CASE("GEO-09 axial sphere oracle checks translation uncertainty and unsupported pairs", "[Nonplanar][GEO-09]")
{
    for (double shift : {0.,100.}) {
        SceneObstacle sphere{42,SphereObstacle{PhysicalPosition(shift,shift,shift),Length(.5)}};
        for (double z : {.3,.6,1.}) {
            const auto path = motion({shift,shift,shift+z},{shift,shift,shift+z+.1});
            // Closest annular point is at the inner radius; min axial height is z.
            const double expected = std::hypot(.2,path.start.z()-shift)-.5;
            auto result = query_clearance(path,tip(),sphere,policy());
            REQUIRE(result.status == (expected>0 ? ClearanceStatus::Pass : ClearanceStatus::Fail));
            contains(result,expected);
        }
        auto uncertain = query_clearance(motion({shift,shift,shift+.6},{shift,shift,shift+.6}),tip(),sphere,policy(.13,.01));
        REQUIRE(uncertain.status == ClearanceStatus::Unknown);
        REQUIRE(query_clearance(motion({0,0,1},{0,0,2}),box(),sphere,policy()).reason == ClearanceReason::UnsupportedPair);
    }
    SceneObstacle invalid{43,SphereObstacle{PhysicalPosition(0,0,0),Length(0)}};
    REQUIRE(query_clearance(motion({0,0,1},{0,0,2}),tip(),invalid,policy()).status == ClearanceStatus::Unknown);
    invalid.geometry = SphereObstacle{PhysicalPosition(0,0,0),Length(1)};
    QueryLimits none; none.max_evaluations=0;
    REQUIRE(query_clearance(motion({0,0,1},{0,0,2}),tip(),invalid,policy(),none).reason == ClearanceReason::WorkLimit);
}
