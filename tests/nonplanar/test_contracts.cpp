#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <libslic3r/Nonplanar/Contracts.hpp>
#include <limits>
#include <random>
#include <type_traits>

using namespace Slic3r;
using namespace Slic3r::nptop;
using Catch::Approx;

namespace {
struct RestoreScale {
    double original = SCALING_FACTOR;
    ~RestoreScale() { SCALING_FACTOR = original; }
};
constexpr double nonfinite = std::numeric_limits<double>::quiet_NaN();
constexpr double inf = std::numeric_limits<double>::infinity();
}

TEST_CASE("ORC-05 scaled XYZ preserves absolute nonzero coordinates at both native scales", "[Nonplanar][ORC-05]")
{
    RestoreScale restore;
    for (double scale : {1e-6, 1e-5}) {
        SCALING_FACTOR = scale;
        auto context = NativeScale::capture();
        Point3 native(1234567, -3456789, 9876543);
        auto mm = from_native_scaled<Frame::MachinePhysical>(native, context);
        REQUIRE(mm.position.x() == Approx(1234567 * scale));
        REQUIRE(mm.position.y() == Approx(-3456789 * scale));
        REQUIRE(mm.position.z() == Approx(9876543 * scale));
        auto roundtrip = to_native_scaled(mm.position, context);
        REQUIRE(roundtrip.position == native);
        REQUIRE(roundtrip.error_mm > scale * 0.5);
        PhysicalPosition arbitrary(10.1234567, -20.2345678, 30.3456789);
        auto quantized = to_native_scaled(arbitrary, context);
        auto back = from_native_scaled<Frame::MachinePhysical>(quantized.position, context);
        double error = std::hypot(std::hypot(back.position.x() - arbitrary.x(),
                                          back.position.y() - arbitrary.y()),
                                 back.position.z() - arbitrary.z());
        REQUIRE(error <= quantized.error_mm + back.error_mm);
        // Deterministic domain sweep also covers large and negative coordinates.
        std::mt19937 generator(4705);
        std::uniform_real_distribution<double> coordinates(-9999.99, 9999.99);
        for (int i = 0; i < 1000; ++i) {
            PhysicalPosition sample(coordinates(generator), coordinates(generator), coordinates(generator));
            auto q = to_native_scaled(sample, context);
            auto decoded = from_native_scaled<Frame::MachinePhysical>(q.position, context);
            const double distance = std::hypot(std::hypot(sample.x() - decoded.position.x(), sample.y() - decoded.position.y()),
                                               sample.z() - decoded.position.z());
            REQUIRE(distance <= q.error_mm + decoded.error_mm);
        }
    }
}

TEST_CASE("ORC-05 rejects stale scale nonfinite and out of domain coordinates", "[Nonplanar][ORC-05]")
{
    RestoreScale restore;
    SCALING_FACTOR = 1e-6;
    auto context = NativeScale::capture();
    auto p = PhysicalPosition(1, 2, 3);
    SCALING_FACTOR = 1e-5;
    REQUIRE_THROWS_AS(to_native_scaled(p, context), std::invalid_argument);
    REQUIRE_THROWS_AS(from_native_scaled<Frame::MachinePhysical>(Point3(1,2,3), context), std::invalid_argument);
    SCALING_FACTOR = 1e-6;
    REQUIRE_THROWS_AS(PhysicalPosition(nonfinite, 0, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(PhysicalPosition(0, inf, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(to_native_scaled(PhysicalPosition(10000.1, 0, 0), context), std::invalid_argument);
    REQUIRE_THROWS_AS(from_native_scaled<Frame::MachinePhysical>(Point3(std::numeric_limits<coord_t>::max(),coord_t(0),coord_t(0)), context), std::invalid_argument);
    for (double edge : {-10000., 10000.}) {
        auto q = to_native_scaled(PhysicalPosition(edge, edge, edge), context);
        REQUIRE(q.position.x() == static_cast<coord_t>(edge * 1000000));
    }
    SCALING_FACTOR = 0;
    REQUIRE_THROWS_AS(NativeScale::capture(), std::invalid_argument);
    SCALING_FACTOR = 1e-4;
    REQUIRE_THROWS_AS(NativeScale::capture(), std::invalid_argument);
    static_assert(!std::is_convertible_v<Position<Frame::MachineCommanded>, PhysicalPosition>);
    static_assert(!std::is_convertible_v<NominalMaterialId, UpperMaterialId>);
    static_assert(!std::is_convertible_v<Volume, FilamentLength>);
    static_assert(!std::is_convertible_v<VerticalGap, NormalGap>);
}

TEST_CASE("VOL-01 XY integral preserves volume on sloped surfaces without a second correction", "[Nonplanar][VOL-01]")
{
    // 10 x 2 mm rectangle, identical 0.2 mm vertical gap between parallel planes.
    auto flat = affine_cell_volume(Length(10), WidthXY(2), VerticalGap(.2), VerticalGap(.2), VerticalGap(.2));
    // Gap is specified independently of the common plane's 3/4 gradient.
    auto slope = affine_cell_volume(Length(10), WidthXY(2), VerticalGap(.2), VerticalGap(.2), VerticalGap(.2));
    REQUIRE(flat.value() == Approx(4));
    REQUIRE(slope.value() == Approx(4));
    REQUIRE(slope.value() != Approx(4 * 1.25)); // mutation: spurious 3D/XY factor
    // h(x,y) = .1 + .01*x + .02*y. Exact integral: 2 + 1 + .4 = 3.4 mm3.
    auto ramp = affine_cell_volume(Length(10), WidthXY(2), VerticalGap(.1), VerticalGap(.2), VerticalGap(.14));
    REQUIRE(ramp.value() == Approx(3.4));
    REQUIRE(filament_feed(flat, Length(2), FlowCompensation(1)).value() == Approx(4 / std::acos(-1.)));
    REQUIRE(filament_feed(flat, Length(2), FlowCompensation(1.1)).value() == Approx(4.4 / std::acos(-1.)));
    REQUIRE(flat.value() == 4); // command compensation never mutates geometry
}

TEST_CASE("VOL-01 invalid deposition dimensions reject instead of producing volume", "[Nonplanar][VOL-01]")
{
    REQUIRE_THROWS_AS(Volume(-1), std::invalid_argument);
    REQUIRE_THROWS_AS(Volume(nonfinite), std::invalid_argument);
    REQUIRE_THROWS_AS(affine_cell_volume(Length(0), WidthXY(2), VerticalGap(.1), VerticalGap(.1), VerticalGap(.1)), std::invalid_argument);
    // Fourth corner of this affine gap would be -.1 mm.
    REQUIRE_THROWS_AS(affine_cell_volume(Length(2), WidthXY(2), VerticalGap(.3), VerticalGap(.1), VerticalGap(.1)), std::invalid_argument);
    REQUIRE_THROWS_AS(affine_cell_volume(Length(1e308), WidthXY(1e308), VerticalGap(1), VerticalGap(1), VerticalGap(1)), std::invalid_argument);
    REQUIRE_THROWS_AS(filament_feed(Volume(1), Length(0), FlowCompensation(1)), std::invalid_argument);
    REQUIRE_THROWS_AS(filament_feed(Volume(1), Length(2), FlowCompensation(0)), std::invalid_argument);
}

TEST_CASE("VOL-02 vertical and normal gaps obey independent plane distance", "[Nonplanar][VOL-02]")
{
    // Planes -3*x - 4*y + z = 0 and 2; normal length is sqrt(26).
    REQUIRE(normal_gap(VerticalGap(2), 3, 4).value() == Approx(2 / std::sqrt(26.)));
    REQUIRE(normal_gap(VerticalGap(.25), 0, 0).value() == .25);
    REQUIRE(normal_gap(VerticalGap(.25), .75, 0).value() == Approx(.2));
    REQUIRE_THROWS_AS(normal_gap(VerticalGap(.2), nonfinite, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(normal_gap(VerticalGap(.2), 0, inf), std::invalid_argument);
}

TEST_CASE("IR deposition travel and retraction carry different measures", "[Nonplanar][IR]")
{
    MaterialIds material{NominalMaterialId(1), UpperMaterialId(2), LowerMaterialId(3)};
    Deposition bead{Volume(.8), WidthXY(.4), VerticalGap(.18), VerticalGap(.22), material, 1, 1};
    MotionEvent event{1, 0, 5, PhysicalPosition(10,20,3), PhysicalPosition(20,20,4), Speed(10), Acceleration(100), bead};
    REQUIRE_NOTHROW(validate_event(event));
    event.payload = Travel{};
    REQUIRE_NOTHROW(validate_event(event));
    event.payload = Retraction{FilamentLength(.8), RetractionState::Ready, RetractionState::Retracted};
    REQUIRE_THROWS_AS(validate_event(event), std::invalid_argument); // moving retract excluded in v1
    event.end = event.start;
    REQUIRE_NOTHROW(validate_event(event));
    event.payload = Retraction{FilamentLength(.8), RetractionState::Retracted, RetractionState::Ready};
    REQUIRE_NOTHROW(validate_event(event)); // unretract is still not deposition
    event.payload = Retraction{FilamentLength(.8), RetractionState::Ready, RetractionState::Ready};
    REQUIRE_THROWS_AS(validate_event(event), std::invalid_argument);
    event.payload = bead;
    REQUIRE_THROWS_AS(validate_event(event), std::invalid_argument); // zero-length deposition
    event.end = PhysicalPosition(20,20,4);
    bead.gap_min = VerticalGap(.3);
    event.payload = bead;
    REQUIRE_THROWS_AS(validate_event(event), std::invalid_argument);
}

TEST_CASE("Numeric budgets charge each component and reject missing invalid or exceeded bounds", "[Nonplanar][IR]")
{
    NumericBudget budget{.01, .02, .01, .01};
    REQUIRE(budget.total_mm() >= .05);
    REQUIRE_NOTHROW(budget.require_conversion(.001));
    REQUIRE_THROWS_AS(budget.require_conversion(.01001), std::invalid_argument);
    REQUIRE_THROWS_AS(budget.require_conversion(nonfinite), std::invalid_argument);
    REQUIRE_THROWS_AS((NumericBudget{.01, -.02, .01, .01}.total_mm()), std::invalid_argument);
    REQUIRE_THROWS_AS((NumericBudget{nonfinite, .02, .01, .01}.total_mm()), std::invalid_argument);
    static_assert(!std::is_default_constructible_v<NumericBudget>);
}
