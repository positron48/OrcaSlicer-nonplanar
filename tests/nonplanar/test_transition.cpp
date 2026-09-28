#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <libslic3r/Nonplanar/Transition.hpp>
#include <libslic3r/Flow.hpp>
#include <libslic3r/Layer.hpp>
#include <libslic3r/TriangleMesh.hpp>
#include <libslic3r/TriangleMeshSlicer.hpp>
#include "../fff_print/test_data.hpp"
#include <cfenv>
#include <thread>

using namespace Slic3r;
using namespace Slic3r::nptop;
using Catch::Matchers::WithinAbs;

namespace {
ExtrusionPath straight_bead()
{
    Flow flow(.8f,.2f,.4f);
    ExtrusionPath path(erTopSolidInfill,flow.mm3_per_mm(),flow.width(),flow.height());
    path.polyline = Polyline3(Points3{Point3(coord_t(0),coord_t(0),coord_t(0)),
        to_native_scaled(Position<Frame::ModelLocal>(10,0,0),NativeScale::capture()).position});
    return path;
}
PlanarSupportCore core(const ExtrusionPath &path, double top = 1, double xy_error = 0, double z_error = 0)
{
    auto result = reconstruct_planar_core(path,0,top,PhysicalPosition(0,0,0),NativeScale::capture(),17,
                                          Length(.4),Length(xy_error),Length(z_error));
    CAPTURE(result.reason);
    REQUIRE(result.core.has_value());
    return *result.core;
}
TransitionPolicy policy(double error = 0)
{ return {VerticalGap(.1),VerticalGap(.3),Length(error)}; }
void contains(ScalarBounds range, double expected)
{
    REQUIRE(range.lower <= expected);
    REQUIRE(range.upper >= expected);
}
void native_body(TriangleMesh mesh, Print &print, Model &model)
{
    print.is_BBL_printer() = false; // the native test helper does not set printer identity
    auto config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({{"layer_height",.2},{"initial_layer_print_height",.2},
        {"wall_generator","classic"},{"wall_loops",2},{"sparse_infill_density",100},
        {"sparse_infill_pattern","rectilinear"},{"internal_solid_infill_pattern","rectilinear"},
        {"top_surface_pattern","rectilinear"},{"top_surface_line_width",.8},
        {"infill_direction",0},{"solid_infill_direction",0},{"top_shell_layers",3},{"bottom_shell_layers",3},
        {"zaa_enabled",false},{"enable_arc_fitting",false}});
    Test::init_print({std::move(mesh)},print,model,config);
    print.process();
    REQUIRE(print.objects().size() == 1);
    REQUIRE_FALSE(print.objects().front()->layers().empty());
}
PlanarSupportCore native_top_core(Print &print)
{
    const Layer &top = *print.objects().front()->layers().back();
    for (const LayerRegion *region : top.regions()) {
        const auto paths = region->fills.flatten();
        for (const ExtrusionEntity *entity : paths.entities) {
            const auto *path = dynamic_cast<const ExtrusionPath *>(entity);
            if (!path || path->role() != erTopSolidInfill) continue;
            for (size_t i = 0; i+1 < path->polyline.size(); ++i) {
                auto result = reconstruct_planar_core(*path,i,top.print_z,PhysicalPosition(100,100,0),
                    NativeScale::capture(),i+1,Length(.4),Length(.002),Length(.005));
                if (result.core) {
                    const auto &r = result.core->lower_footprint;
                    if (std::max(r.max_x-r.min_x,r.max_y-r.min_y) > 5) return *result.core;
                }
            }
        }
    }
    FAIL("native dense top did not produce a supported long straight bead");
    throw std::runtime_error("missing native support");
}
AffineCapCell inside(const PlanarSupportCore &support, double z)
{
    auto r = support.lower_footprint;
    const double cx = (r.min_x+r.max_x)/2, cy = (r.min_y+r.max_y)/2;
    if (r.max_x-r.min_x > r.max_y-r.min_y) r = {cx-2,cy-.2,cx+2,cy+.2};
    else r = {cx-.2,cy-2,cx+.2,cy+2};
    return {r,z,z+.02,z+.02};
}
}

TEST_CASE("INT-01 native reserved body supports an affine first pass", "[Nonplanar][A05][INT-01][VOL-01]")
{
    auto mesh = its_make_cube(10,8,1);
    for (auto &v : mesh.vertices) if (v.z() > 0) v.z() = 1.5f+.01f*v.x();
    const auto original_vertices = mesh.vertices;
    TriangleMesh original(mesh);
    indexed_triangle_set upper, lower;
    cut_mesh(original.its,1.f,&upper,&lower);
    TriangleMesh body(lower), cap(upper);
    // Independent integral: integral_0^10 integral_0^8 (1.5+.01*x) dy dx = 124.
    REQUIRE_THAT(original.volume(),WithinAbs(124,1e-4));
    REQUIRE_THAT(body.volume(),WithinAbs(80,1e-4));
    REQUIRE_THAT(cap.volume(),WithinAbs(44,1e-4));
    REQUIRE_THAT(body.volume()+cap.volume(),WithinAbs(original.volume(),1e-4));
    const double body_volume = body.volume(), cap_budget = cap.volume();
    REQUIRE(original.its.vertices == original_vertices);
    Print print;
    Model derived_model;
    native_body(std::move(body),print,derived_model);
    for (const Layer *layer : print.objects().front()->layers()) REQUIRE(layer->print_z <= 1.000001);
    REQUIRE_THAT(print.objects().front()->layers().back()->print_z,WithinAbs(1,1e-6));
    REQUIRE_THAT(original.bounding_box().max.z(),WithinAbs(1.6,1e-6));
    const auto support = native_top_core(print);
    const auto cell = inside(support,1.18);
    const auto result = assess_first_pass(cell,support,policy(.001));
    CAPTURE(result.reason);
    REQUIRE(result.status == TransitionStatus::Compatible);
    REQUIRE(result.source_path_id == support.source_path_id);
    REQUIRE(result.gap_mm->lower > .1);
    REQUIRE(result.gap_mm->upper < .3);
    // Integrate the actual binary64 rectangle in long double. Translating its
    // decimal .4 mm width to x/y near 100 mm introduces representation error;
    // that is not permission to widen the production arithmetic interval.
    const auto &r = cell.footprint;
    const long double area = (static_cast<long double>(r.max_x)-r.min_x)*
                             (static_cast<long double>(r.max_y)-r.min_y);
    const long double z11 = static_cast<long double>(cell.z10)+cell.z01-cell.z00;
    const long double expected = area*((cell.z00-1.L)+(cell.z10-1.L)+(cell.z01-1.L)+(z11-1.L))/4;
    REQUIRE(result.nominal_volume_mm3->lower <= expected);
    REQUIRE(result.nominal_volume_mm3->upper >= expected);
    REQUIRE_THAT(double(expected),WithinAbs(.32,1e-12)); // decimal fixture: 4*.4*.2
    REQUIRE(result.possible_volume_mm3->upper < cap_budget);
    REQUIRE(body_volume+result.possible_volume_mm3->upper < original.volume());
}

TEST_CASE("INT-02 native lower stair support rejects the unchanged upper candidate", "[Nonplanar][A05][INT-02]")
{
    // Two terrace interiors, independently sliced by native Orca. Crossing the
    // seam is outside this single-core experiment, not presumed supported.
    Print high_print,low_print;
    Model high_model,low_model;
    native_body(make_cube(10,8,1),high_print,high_model);
    native_body(make_cube(10,8,.8),low_print,low_model);
    const auto high = native_top_core(high_print), low = native_top_core(low_print);
    REQUIRE_THAT(high_print.objects().front()->layers().back()->print_z,WithinAbs(1,1e-6));
    REQUIRE_THAT(low_print.objects().front()->layers().back()->print_z,WithinAbs(.8,1e-6));
    REQUIRE(assess_first_pass(inside(high,1.18),high,policy()).status == TransitionStatus::Compatible);
    const auto rejected = assess_first_pass(inside(low,1.18),low,policy());
    REQUIRE(rejected.status == TransitionStatus::Rejected);
    REQUIRE(rejected.reason == TransitionReason::GapTooLarge);
    REQUIRE(rejected.gap_mm->lower > .3);
}

TEST_CASE("B04 planar region captures native hierarchy roles and bounded volume", "[Nonplanar][B04][PlanarRegion]")
{
    Print print; Model model; native_body(make_cube(10,8,1),print,model);
    const auto *region=print.objects().front()->layers().back()->regions().front();
    const auto result=capture_planar_region(*region,Position<Frame::BuildPlate>(100,100,0),61);
    INFO(result.reason); REQUIRE(result.snapshot);
    const auto &snapshot=*result.snapshot;
    REQUIRE(snapshot.revision==61);
    REQUIRE(snapshot.native_scale.mm_per_unit()==NativeScale::capture().mm_per_unit());
    REQUIRE(snapshot.native_layer_id==4);
    REQUIRE(snapshot.native_print_z_mm==1);
    REQUIRE(snapshot.entities.size()>snapshot.paths.size());
    REQUIRE_FALSE(snapshot.paths.empty());
    bool perimeter=false, solid=false, loop=false;
    for (const auto &entity : snapshot.entities) if (entity.kind==PlanarEntityKind::Loop) loop=true;
    for (const auto &path : snapshot.paths) {
        REQUIRE(path.native_points.size()==path.points.size());
        REQUIRE(path.entity_id>0);
        REQUIRE(path.entity_id<=snapshot.entities.size());
        REQUIRE(path.coordinate_error_upper_mm>0);
        REQUIRE(path.length_mm.lower>0);
        REQUIRE(path.native_volume_mm3.lower>0);
        for (const auto &point : path.points) REQUIRE(point.z()==1);
        perimeter=perimeter || path.role==erExternalPerimeter;
        solid=solid || path.role==erTopSolidInfill;
    }
    REQUIRE(perimeter); REQUIRE(solid); REQUIRE(loop);
    REQUIRE(snapshot.native_volume_mm3.lower>0);
}
TEST_CASE("B04 planar scaled path preserves the independent three four five oracle", "[Nonplanar][B04][PlanarRegion]")
{
    Print print; Model model; native_body(make_cube(10,8,1),print,model);
    auto *region=print.objects().front()->layers().back()->regions().front();
    region->perimeters.clear(); region->fills.clear();
    ExtrusionPath path(erTopSolidInfill,.125,.8f,.2f);
    path.polyline=Polyline3(Points3{Point3(0,0,0),Point3(3000000,4000000,0)});
    region->fills.append(path);
    PlanarRegionLimits limits;
    limits.cancelled=[&] { region->fills.clear(); region->layer()->print_z=99; return false; };
    const auto result=capture_planar_region(*region,Position<Frame::BuildPlate>(100,200,3),62,limits);
    INFO(result.reason); REQUIRE(result.snapshot);
    REQUIRE(result.snapshot->paths.size()==1);
    const auto &captured=result.snapshot->paths.front();
    contains(captured.length_mm,5.);
    contains(captured.native_volume_mm3,.625);
    REQUIRE(captured.length_mm.upper-captured.length_mm.lower<1e-10);
    REQUIRE(captured.points.front().x()==100);
    REQUIRE(captured.points.back().x()==103);
    REQUIRE(captured.points.back().y()==204);
    REQUIRE(captured.points.back().z()==4);
    REQUIRE(result.snapshot->native_print_z_mm==1);
    REQUIRE(region->fills.empty());
}
TEST_CASE("B04 planar region rejects unsupported roles native Z arc metadata and invalid beads", "[Nonplanar][B04][PlanarRegion]")
{
    Print print; Model model; native_body(make_cube(10,8,1),print,model);
    auto *region=print.objects().front()->layers().back()->regions().front();
    region->perimeters.clear();
    const Position<Frame::BuildPlate> origin(0,0,0);
    for (const auto role : {erBridgeInfill,erInternalBridgeInfill,erOverhangPerimeter,erGapFill,
                           erSupportMaterial,erSupportMaterialInterface,erIroning,erNone}) {
        region->fills.clear(); auto path=straight_bead(); path.set_extrusion_role(role); region->fills.append(path);
        const auto result=capture_planar_region(*region,origin,1);
        CHECK_FALSE(result.snapshot);
        CHECK(result.reason=="UNSUPPORTED_PLANAR_ROLE");
    }
    for (int mutation=0; mutation<8; ++mutation) {
        INFO(mutation); region->fills.clear(); auto path=straight_bead();
        if (mutation==0) path.polyline.points.back().z()=1;
        if (mutation==1) path.z_contoured=true;
        if (mutation==2) path.set_force_no_extrusion(true);
        if (mutation==3) path.polyline.fitting_result.push_back(PathFittingData{0,1,EMovePathType::Arc_move_cw,{}});
        if (mutation==4) path.polyline.points.back()=path.polyline.points.front();
        if (mutation==5) path.height=std::numeric_limits<float>::quiet_NaN();
        if (mutation==6) path.mm3_per_mm=std::numeric_limits<double>::infinity();
        if (mutation==7) path.width=path.height;
        region->fills.append(path);
        CHECK_FALSE(capture_planar_region(*region,origin,1).snapshot);
    }
    region->fills.clear(); auto path=straight_bead();
    ExtrusionPathContoured contoured(Polyline3(path.polyline),path,std::vector<double>{0,0});
    region->fills.append(contoured);
    REQUIRE(capture_planar_region(*region,origin,1).reason=="UNSUPPORTED_PLANAR_ENTITY");
}
TEST_CASE("B04 planar groups preserve hierarchy and reject variable width disconnected and open paths", "[Nonplanar][B04][PlanarRegion]")
{
    Print print; Model model; native_body(make_cube(10,8,1),print,model);
    auto *region=print.objects().front()->layers().back()->regions().front();
    region->perimeters.clear(); region->fills.clear();
    auto path=straight_bead();
    path.polyline=Polyline3(Points3{Point3(0,0,0),Point3(10000000,0,0),Point3(10000000,10000000,0),
                                  Point3(0,10000000,0),Point3(0,0,0)});
    ExtrusionLoop loop(path,elrHole); loop.inset_idx=7;
    ExtrusionEntityCollection group; group.no_sort=true; group.append(loop);
    region->perimeters.append(group);
    const Position<Frame::BuildPlate> origin(0,0,0);
    auto result=capture_planar_region(*region,origin,1);
    INFO(result.reason); REQUIRE(result.snapshot);
    REQUIRE(result.snapshot->entities.size()==5);
    REQUIRE(result.snapshot->entities[2].parent_id==1);
    REQUIRE_FALSE(result.snapshot->entities[2].can_sort);
    REQUIRE(result.snapshot->entities[3].kind==PlanarEntityKind::Loop);
    REQUIRE(result.snapshot->entities[3].parent_id==3);
    REQUIRE(result.snapshot->entities[3].native_inset_index==7);
    REQUIRE(result.snapshot->entities[3].loop_role==elrHole);
    REQUIRE(result.snapshot->paths.front().entity_id==5);
    contains(result.snapshot->paths.front().length_mm,40.);
    region->perimeters.clear();
    region->fills.append(ExtrusionLoop(path,elrSkirt));
    REQUIRE(capture_planar_region(*region,origin,1).reason=="UNSUPPORTED_PLANAR_LOOP_ROLE");
    region->fills.clear();
    region->fills.append(ExtrusionLoop(straight_bead()));
    REQUIRE(capture_planar_region(*region,origin,1).reason=="OPEN_PLANAR_LOOP");
    auto first=straight_bead(), second=first;
    region->fills.clear(); region->fills.append(ExtrusionMultiPath(ExtrusionPaths{first,second}));
    REQUIRE(capture_planar_region(*region,origin,1).reason=="DISCONNECTED_PLANAR_GROUP");
    second.polyline.points.front()=first.polyline.points.back(); second.polyline.points.back().x()+=10000000;
    region->fills.clear(); region->fills.append(ExtrusionMultiPath(ExtrusionPaths{first,second}));
    REQUIRE(capture_planar_region(*region,origin,1).snapshot);
    second.width=std::nextafter(first.width,1.f);
    region->fills.clear(); region->fills.append(ExtrusionMultiPath(ExtrusionPaths{first,second}));
    REQUIRE(capture_planar_region(*region,origin,1).reason=="UNSUPPORTED_VARIABLE_WIDTH_GROUP");
}
TEST_CASE("B04 planar region limits and late callbacks discard the entire candidate", "[Nonplanar][B04][PlanarRegion]")
{
    Print print; Model model; native_body(make_cube(10,8,1),print,model);
    auto *region=print.objects().front()->layers().back()->regions().front();
    const Position<Frame::BuildPlate> origin(0,0,0);
    PlanarRegionLimits limits;
    limits.max_entities=2;
    REQUIRE(capture_planar_region(*region,origin,1,limits).reason=="PLANAR_ENTITY_LIMIT");
    limits={}; limits.max_points=2;
    REQUIRE(capture_planar_region(*region,origin,1,limits).reason=="PLANAR_POINT_LIMIT");
    limits={}; limits.max_depth=1;
    REQUIRE(capture_planar_region(*region,origin,1,limits).reason=="PLANAR_ENTITY_LIMIT");
    region->perimeters.clear(); region->fills.clear(); region->fills.append(straight_bead());
    for (bool stale : {false,true}) {
        size_t calls=0; limits={};
        if (stale) limits.is_current=[&](uint64_t) { return ++calls<4; };
        else limits.cancelled=[&] { return ++calls>=4; };
        const auto result=capture_planar_region(*region,origin,1,limits);
        REQUIRE(calls==4);
        REQUIRE_FALSE(result.snapshot);
        REQUIRE(result.reason==(stale ? "STALE_REVISION" : "CANCELLED"));
    }
    limits={}; limits.timeout=std::chrono::milliseconds(2);
    limits.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(5)); return false; };
    REQUIRE(capture_planar_region(*region,origin,1,limits).reason=="PLANAR_REGION_DEADLINE");
    struct Rounding { int mode=std::fegetround(); ~Rounding() { std::fesetround(mode); } } restore;
    limits={}; limits.cancelled=[] { std::fesetround(FE_UPWARD); return false; };
    const auto rounding=capture_planar_region(*region,origin,1,limits);
    std::fesetround(restore.mode);
    REQUIRE_FALSE(rounding.snapshot);
    REQUIRE(rounding.reason=="PLANAR_REGION_NUMERIC_OR_CAPTURE_FAILURE");
    limits={}; limits.cancelled=[]()->bool { throw std::runtime_error("callback failed"); };
    REQUIRE_FALSE(capture_planar_region(*region,origin,1,limits).snapshot);
}
TEST_CASE("B04 captured region outlives destruction of the entire native print", "[Nonplanar][B04][PlanarRegion]")
{
    auto print=std::make_unique<Print>(); Model model; native_body(make_cube(10,8,1),*print,model);
    const auto *region=print->objects().front()->layers().back()->regions().front();
    PlanarRegionLimits limits;
    limits.cancelled=[&] { print.reset(); limits.max_points=0; return false; };
    const auto result=capture_planar_region(*region,Position<Frame::BuildPlate>(100,100,0),63,limits);
    INFO(result.reason); REQUIRE(result.snapshot);
    REQUIRE_FALSE(print);
    REQUIRE_FALSE(result.snapshot->paths.empty());
    REQUIRE(result.snapshot->native_print_z_mm==1);
}

TEST_CASE("INT-01 affine footprint and volume use all corners", "[Nonplanar][A05][INT-01][VOL-01]")
{
    const auto support = core(straight_bead());
    // h=.16+.01*(x-1)+.1*(y+.2) on [1,9]x[-.2,.2]. Integral = .704 mm3.
    AffineCapCell cell{{1,-.2,9,.2},1.16,1.24,1.20};
    auto result = assess_first_pass(cell,support,policy());
    REQUIRE(result.status == TransitionStatus::Compatible);
    contains(*result.nominal_volume_mm3,.704);
    REQUIRE(result.nominal_volume_mm3->upper-result.nominal_volume_mm3->lower < 1e-12);
    REQUIRE(result.nominal_volume_mm3->upper < .704*std::sqrt(1+.01*.01+.1*.1));
    // Centreline has legal gap; the transverse edge alone is too high.
    cell.z00 = 1.19; cell.z10 = 1.19; cell.z01 = 1.35;
    REQUIRE(assess_first_pass(cell,support,policy()).reason == TransitionReason::GapTooLarge);
}

TEST_CASE("INT-03 offset mutations recheck gap and volume without auto lifting", "[Nonplanar][A05][INT-03]")
{
    const auto support = core(straight_bead());
    for (double offset : {-.15,0.,.15}) {
        DYNAMIC_SECTION("offset " << offset) {
            AffineCapCell cell{{1,-.2,9,.2},1.2+offset,1.2+offset,1.2+offset};
            const auto result = assess_first_pass(cell,support,policy());
            REQUIRE(result.status == (offset == 0 ? TransitionStatus::Compatible : TransitionStatus::Rejected));
            if (offset < 0) REQUIRE(result.reason == TransitionReason::GapTooSmall);
            if (offset > 0) REQUIRE(result.reason == TransitionReason::GapTooLarge);
            contains(*result.nominal_volume_mm3,3.2*(.2+offset));
            REQUIRE(cell.z00 == 1.2+offset);
        }
    }
}

TEST_CASE("INT-02 lower support excludes the bead shoulders endpoints and gaps", "[Nonplanar][A05][INT-02]")
{
    const auto support = core(straight_bead(),1,.02,.01);
    REQUIRE(support.lower_footprint.min_y > -.3);
    REQUIRE(support.lower_footprint.max_y < .3);
    AffineCapCell cell{{1,-.2,9,.2},1.2,1.2,1.2};
    REQUIRE(assess_first_pass(cell,support,policy()).status == TransitionStatus::Compatible);
    cell.footprint.min_y = -.35; // inside outer w=.8, outside guaranteed w-h=.6
    auto shoulder = assess_first_pass(cell,support,policy());
    REQUIRE(shoulder.status == TransitionStatus::Unknown);
    REQUIRE(shoulder.reason == TransitionReason::UnsupportedFootprint);
    cell.footprint = {.01,-.2,1,.2};
    REQUIRE(assess_first_pass(cell,support,policy()).reason == TransitionReason::UnsupportedFootprint);
    cell.footprint = {1,.5,9,.7}; // no material here; CAD is not substituted
    REQUIRE(assess_first_pass(cell,support,policy()).status == TransitionStatus::Unknown);
}

TEST_CASE("INT-03 uncertainty never creates support or increases nominal volume", "[Nonplanar][A05][INT-03][VOL-01]")
{
    const auto path = straight_bead();
    AffineCapCell cell{{1,-.2,9,.2},1.2,1.2,1.2};
    const auto nominal = assess_first_pass(cell,core(path),policy());
    const auto uncertain = assess_first_pass(cell,core(path,1,0,.15),policy());
    REQUIRE(uncertain.status == TransitionStatus::Unknown);
    REQUIRE(uncertain.nominal_volume_mm3->lower == nominal.nominal_volume_mm3->lower);
    REQUIRE(uncertain.nominal_volume_mm3->upper == nominal.nominal_volume_mm3->upper);
    REQUIRE(uncertain.possible_volume_mm3->lower < nominal.possible_volume_mm3->lower);
    cell.z00 = 1.1; cell.z10 = 1.1; cell.z01 = 1.1;
    REQUIRE(assess_first_pass(cell,core(path),policy()).status == TransitionStatus::Unknown);
    cell.z00 = 1.2; cell.z10 = 1.24; cell.z01 = 1.24;
    REQUIRE(assess_first_pass(cell,core(path),policy()).status == TransitionStatus::Compatible);
    REQUIRE(assess_first_pass(cell,core(path),policy(.01)).status == TransitionStatus::Unknown);
}

TEST_CASE("INT-02 unsupported native paths cannot masquerade as planar support", "[Nonplanar][A05][INT-02]")
{
    const auto base = straight_bead();
    for (int mutation = 0; mutation < 7; ++mutation) {
        DYNAMIC_SECTION("mutation " << mutation) {
            auto path = base;
            if (mutation == 0) path.z_contoured = true;
            if (mutation == 1) path.polyline.points[1].z() = 1;
            if (mutation == 2) path.polyline.points[1].y() = 1;
            if (mutation == 3) path.mm3_per_mm *= 2;
            if (mutation == 4) path.set_extrusion_role(erBridgeInfill);
            if (mutation == 5) path.set_extrusion_role(erInternalInfill);
            if (mutation == 6) path.set_force_no_extrusion(true);
            auto result = reconstruct_planar_core(path,0,1,PhysicalPosition(0,0,0),NativeScale::capture(),17,
                Length(.4),Length(0),Length(0));
            REQUIRE_FALSE(result.core.has_value());
            REQUIRE(result.reason == TransitionReason::UnsupportedPath);
        }
    }
}

TEST_CASE("INT-03 invalid cells budgets and stale scales stay unknown", "[Nonplanar][A05][INT-03]")
{
    auto path = straight_bead();
    auto support = core(path);
    AffineCapCell cell{{1,-.2,9,.2},1.2,1.2,1.2};
    auto limits = policy();
    limits.maximum = VerticalGap(.05);
    REQUIRE(assess_first_pass(cell,support,limits).status == TransitionStatus::Unknown);
    cell.z10 = std::numeric_limits<double>::quiet_NaN();
    REQUIRE(assess_first_pass(cell,support,policy()).reason == TransitionReason::InvalidInput);
    for (double nozzle : {0.,1e-300,1e100}) {
        REQUIRE_FALSE(reconstruct_planar_core(path,0,1,PhysicalPosition(0,0,0),NativeScale::capture(),17,
            Length(nozzle),Length(0),Length(0)).core.has_value());
    }
    auto tiny = path;
    tiny.height = std::numeric_limits<float>::denorm_min();
    tiny.width = 2*tiny.height;
    REQUIRE(reconstruct_planar_core(tiny,0,1,PhysicalPosition(0,0,0),NativeScale::capture(),17,
        Length(.4),Length(0),Length(0)).reason == TransitionReason::NumericalFailure);
    cell = {{1,-.2,9,.2},-9999,9999,9999};
    REQUIRE(assess_first_pass(cell,support,policy()).status == TransitionStatus::Unknown);
    const auto scale = NativeScale::capture();
    struct Restore { double value = SCALING_FACTOR; ~Restore() { SCALING_FACTOR = value; } } restore;
    SCALING_FACTOR = SCALING_FACTOR == 1e-6 ? 1e-5 : 1e-6;
    REQUIRE_FALSE(reconstruct_planar_core(path,0,1,PhysicalPosition(0,0,0),scale,17,
        Length(.4),Length(0),Length(0)).core.has_value());
}

TEST_CASE("INT-01 native support is invariant under reversal and explicit translation", "[Nonplanar][A05][INT-01]")
{
    auto path = straight_bead();
    const auto original = core(path,1,.01,.005);
    std::reverse(path.polyline.points.begin(),path.polyline.points.end());
    const auto reversed = core(path,1,.01,.005);
    REQUIRE(original.lower_footprint.min_x == reversed.lower_footprint.min_x);
    REQUIRE(original.lower_footprint.max_y == reversed.lower_footprint.max_y);
    for (auto &point : path.polyline.points) std::swap(point.x(),point.y());
    const auto translated = reconstruct_planar_core(path,0,1,PhysicalPosition(20,30,4),
        NativeScale::capture(),99,Length(.4),Length(.01),Length(.005));
    REQUIRE(translated.core.has_value());
    const auto &r = translated.core->lower_footprint;
    REQUIRE(r.min_x > 19.7);
    REQUIRE(r.max_x < 20.3);
    REQUIRE(r.min_y > 30);
    REQUIRE(r.max_y < 40);
    const auto result = assess_first_pass({{19.8,31,20.2,39},5.2,5.2,5.2},*translated.core,policy());
    REQUIRE(result.status == TransitionStatus::Compatible);
    REQUIRE(result.source_path_id == 99);
    contains(*result.gap_mm,.2);
}
