#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <libslic3r/Nonplanar/GCodeAdapter.hpp>
#include <nonplanar_verify/Replay.hpp>

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
