#include <catch2/catch_test_macros.hpp>
#include "material_join_oracle.hpp"
#include <libslic3r/Nonplanar/DepositionModel.hpp>
#include <libslic3r/Nonplanar/Collision.hpp>
#include <cmath>
#include <cfenv>
#include <thread>
#include <set>
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <nlohmann/json.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>

using namespace Slic3r::nptop;
namespace {
const std::string source_id(64,'a');
MaterialModel model(double outer=.01, double inner=.01)
{ return {17,Length(outer),Length(outer),Length(inner),Length(inner),Length(0)}; }
MaterialRecord bead(uint64_t id, uint64_t index, PhysicalPosition a, PhysicalPosition b,
                    double width, double h0, double h1, BeadSectionKind kind=BeadSectionKind::RoundedRectangle)
{
    const long double dx=static_cast<long double>(b.x())-a.x(), dy=static_cast<long double>(b.y())-a.y();
    const long double length=std::sqrt(dx*dx+dy*dy);
    const long double correction=kind==BeadSectionKind::RoundedRectangle ? 1-std::acos(-1.L)/4 : 0;
    const long double a0=h0,a1=h1;
    const long double midpoint=(a0+a1)/2;
    const long double area=midpoint*(width-correction*midpoint);
    const auto width_at=[&](long double h) { return area/h+correction*h; };
    const ScalarBounds permitted{double(width_at(std::max(a0,a1)))-1e-12,double(width_at(std::min(a0,a1)))+1e-12};
    return {{id,index,33,a,b,Speed(20),Acceleration(100),Deposition{Volume(double(length*area)),WidthXY(width),
        VerticalGap(std::min(h0,h1)),VerticalGap(std::max(h0,h1)),{NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},7,8}},
        BeadSection{kind,h0,h1,permitted}};
}
std::shared_ptr<const MaterialSequenceSnapshot> captured(std::vector<MaterialRecord> rows, MaterialModel params=model())
{
    const auto result=capture_material_sequence(rows,params,23,source_id);
    INFO(result.reason); REQUIRE(result.snapshot); return result.snapshot;
}
void volume_contains(ScalarBounds b, long double expected)
{ REQUIRE(b.lower<=expected); REQUIRE(b.upper>=expected); }
bool stadium_inside(long double s, long double z, long double w, long double h)
{
    const long double horizontal=std::max(std::abs(s)-(w-h)/2,0.L);
    return horizontal*horizontal+z*z<(h/2)*(h/2);
}
}
TEST_CASE("MAT-01 separate rounded material views match independent angled section oracle", "[Nonplanar][B05][MaterialModel][MAT-01]")
{
    const auto input=bead(101,0,{0,0,1},{3,4,2},.8,.2,.4);
    const auto plan=captured({input},model(.02,.01)); const auto all=material_at(plan,1,0); REQUIRE(all.lower.snapshot);
    const auto &geometry=*plan->geometry.front();
    const long double k=1-std::acos(-1.L)/4;
    // Independent constant-flux law from the actual binary64 volume.
    const long double amount=std::get<Deposition>(input.motion.payload).volume.value(), area=amount/5;
    volume_contains(geometry.volume_mm3,amount);
    const long double midpoint=(static_cast<long double>(input.bead->gap_begin_mm)+input.bead->gap_end_mm)/2;
    REQUIRE(std::abs(double(area-midpoint*(.8L-k*midpoint)))<1e-15);
    for (double t : {.13,.37,.79}) for (int n=-18; n<=18; ++n) for (int z=-12; z<=12; ++z) {
        const double transverse=n*.03, vertical=z*.025;
        const double h=.2+t*.2, center=1+t-h/2;
        const PhysicalPosition point(3*t-.8*transverse,4*t+.6*transverse,center+vertical);
        const auto nominal=classify_material(all.nominal,point);
        const long double width=area/h+k*h;
        const bool expected=stadium_inside(transverse,vertical,width,h);
        const long double horizontal=std::max(std::abs(static_cast<long double>(transverse))-(width-h)/2,0.L);
        const long double boundary=horizontal*horizontal+vertical*vertical-(h/2)*(h/2);
        if (std::abs(boundary)<1e-10L) continue; // exact boundary is intentionally UNKNOWN
        REQUIRE(nominal.membership==(expected ? MaterialMembership::Inside : MaterialMembership::Outside));
        const auto lower=classify_material(all.lower,point), upper=classify_material(all.upper,point);
        if (lower.membership==MaterialMembership::Inside) REQUIRE(expected);
        if (expected) REQUIRE(upper.membership==MaterialMembership::Inside);
    }
    const PhysicalPosition inflated(1.5,2.0,1.515);
    REQUIRE(classify_material(all.upper,inflated).membership==MaterialMembership::Inside);
    REQUIRE(classify_material(all.lower,inflated).membership==MaterialMembership::Outside);
    REQUIRE(classify_material(all.nominal,inflated).membership==MaterialMembership::Outside);
}
TEST_CASE("MAT-03 current deposition grows within a segment and future material stays absent", "[Nonplanar][B05][MaterialModel][MAT-03]")
{
    const auto plan=captured({bead(101,0,{0,0,1},{10,0,1},.8,.2,.2)});
    const auto none=material_at(plan,0,0); const auto half=material_at(plan,0,.4); const auto full=material_at(plan,1,0);
    REQUIRE(none.nominal.snapshot); REQUIRE(half.nominal.snapshot); REQUIRE(full.nominal.snapshot);
    REQUIRE(classify_material(none.upper,{0,0,.9}).membership==MaterialMembership::Outside);
    REQUIRE(classify_material(half.lower,{2,0,.9}).membership==MaterialMembership::Inside);
    REQUIRE(classify_material(half.upper,{6,0,.9}).membership==MaterialMembership::Outside);
    REQUIRE(classify_material(full.lower,{6,0,.9}).membership==MaterialMembership::Inside);
    volume_contains(half.nominal.snapshot->nominal_deposited_volume_mm3,static_cast<long double>(plan->geometry.front()->volume_mm3.lower)*.4L);
    REQUIRE(half.nominal.snapshot->nominal_deposited_volume_mm3.upper<full.nominal.snapshot->nominal_deposited_volume_mm3.lower);
    REQUIRE(half.nominal.snapshot->fingerprint()!=full.nominal.snapshot->fingerprint());
    REQUIRE_FALSE(material_at(plan,2,0).nominal.snapshot); REQUIRE_FALSE(material_at(plan,0,1.01).nominal.snapshot);
}
TEST_CASE("MAT-02 GCD-09 neighbour material persists through travel retract and unretract", "[Nonplanar][B05][MaterialModel][MAT-02][GCD-09][MAT-04]")
{
    auto first=bead(101,0,{0,0,1},{10,0,1},.8,.2,.2);
    const auto travel=[](uint64_t id,uint64_t index,PhysicalPosition a,PhysicalPosition b) {
        return MaterialRecord{{id,index,0,a,b,Speed(10),Acceleration(100),Travel{}},{}};
    };
    const auto pressure=[](uint64_t id,uint64_t index,PhysicalPosition p,RetractionState a,RetractionState b) {
        return MaterialRecord{{id,index,0,p,p,Speed(10),Acceleration(100),Retraction{FilamentLength(.8),a,b}},{}};
    };
    const auto second=bead(106,5,{0,2,1},{10,2,1},.8,.2,.2);
    const auto plan=captured({first,travel(102,1,{10,0,1},{10,2,1}),pressure(103,2,{10,2,1},RetractionState::Ready,RetractionState::Retracted),
        travel(104,3,{10,2,1},{0,2,1}),pressure(105,4,{0,2,1},RetractionState::Retracted,RetractionState::Ready),second});
    const auto base=material_at(plan,1,0);
    for (size_t i=1; i<=5; ++i) {
        const auto state=material_at(plan,i,.5);
        REQUIRE(state.upper.snapshot);
        REQUIRE(classify_material(state.upper,{2,0,.9}).membership==MaterialMembership::Inside);
        REQUIRE(classify_material(state.nominal,{2,1,.9}).membership==MaterialMembership::Outside);
        if (i<5) {
            REQUIRE(state.nominal.snapshot->nominal_deposited_volume_mm3.lower==base.nominal.snapshot->nominal_deposited_volume_mm3.lower);
            REQUIRE(state.nominal.snapshot->nominal_deposited_volume_mm3.upper==base.nominal.snapshot->nominal_deposited_volume_mm3.upper);
            REQUIRE(classify_material(state.upper,{2,2,.9}).membership==MaterialMembership::Outside);
        }
    }
    const auto partial=material_at(plan,5,.5);
    REQUIRE(classify_material(partial.lower,{2,2,.9}).membership==MaterialMembership::Inside);
    REQUIRE(classify_material(partial.upper,{7,2,.9}).membership==MaterialMembership::Outside);
}
TEST_CASE("B05 rectangular sloped variable gaps conserve XY volume and reject mutations", "[Nonplanar][B05][MaterialModel]")
{
    const auto input=bead(1,0,{0,0,1},{10,0,2},1,.2,.4,BeadSectionKind::Rectangle);
    const auto plan=captured({input});
    const auto commanded=std::get<Deposition>(input.motion.payload).volume.value();
    volume_contains(plan->geometry.front()->volume_mm3,commanded); REQUIRE(std::abs(commanded-3)<1e-14);
    const auto rejected=[](const MaterialSequenceResult &r) { INFO(r.reason); REQUIRE_FALSE(r.snapshot); };
    auto bad=input; std::get<Deposition>(bad.motion.payload).volume=Volume(3*std::sqrt(1.01));
    rejected(capture_material_sequence({bad},model(),23,source_id));
    bad=input; bad.motion.sequence_index=1; rejected(capture_material_sequence({bad},model(),23,source_id));
    bad=input; bad.bead->gap_end_mm=-1; rejected(capture_material_sequence({bad},model(),23,source_id));
    bad=input; bad.bead->width_mm={1,1}; rejected(capture_material_sequence({bad},model(),23,source_id));
    bad=input; bad.bead->width_mm={.8,.9}; rejected(capture_material_sequence({bad},model(),23,source_id));
    bad=input; bad.motion.start={0,0,2}; bad.motion.end={0,0,3}; rejected(capture_material_sequence({bad},model(),23,source_id));
    rejected(capture_material_sequence({input,input},model(),23,source_id));
    const auto retract=MaterialRecord{{2,1,0,{10,0,2},{10,0,2},Speed(10),Acceleration(100),
        Retraction{FilamentLength(.8),RetractionState::Retracted,RetractionState::Ready}}, {}};
    rejected(capture_material_sequence({input,retract},model(),23,source_id));
    auto moved=bead(2,1,{11,0,2},{15,0,2},1,.2,.2);
    rejected(capture_material_sequence({input,moved},model(),23,source_id));
    REQUIRE(plan->fingerprint().size()==64);
    const auto changed=captured({bead(1,0,{0,0,1},{10,0,2},1,.2,.3,BeadSectionKind::Rectangle)});
    REQUIRE(changed->fingerprint()!=plan->fingerprint());
}
TEST_CASE("B05 material captures owned callbacks and blocks stale late and numerical work", "[Nonplanar][B05][MaterialModel]")
{
    std::vector<MaterialRecord> input{bead(1,0,{0,0,1},{10,0,1},.8,.2,.2)};
    MaterialLimits limits; limits.cancelled=[&] { input.clear(); return false; };
    const auto owned=capture_material_sequence(input,model(),23,source_id,limits); INFO(owned.reason); REQUIRE(owned.snapshot); REQUIRE(input.empty());
    const auto state=material_at(owned.snapshot,1,0); REQUIRE(state.lower.snapshot);
    MaterialQueryLimits query; auto view=state.lower; query.cancelled=[&] { view.snapshot.reset(); return false; };
    const auto kept=classify_material(view,{2,0,.9},query); REQUIRE(kept.membership==MaterialMembership::Inside); REQUIRE_FALSE(view.snapshot);
    input={bead(1,0,{0,0,1},{10,0,1},.8,.2,.2)};
    limits={}; limits.cancelled=[] { return true; }; REQUIRE_FALSE(capture_material_sequence(input,model(),23,source_id,limits).snapshot);
    limits={}; limits.is_current=[](uint64_t){ return false; }; REQUIRE_FALSE(capture_material_sequence(input,model(),23,source_id,limits).snapshot);
    limits={}; limits.timeout=std::chrono::milliseconds(1); limits.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(5));return false; };
    REQUIRE_FALSE(capture_material_sequence(input,model(),23,source_id,limits).snapshot);
    query={}; query.max_evaluations=0; REQUIRE(classify_material(state.lower,{2,0,.9},query).membership==MaterialMembership::Unknown);
    query={}; query.is_current=[](uint64_t){return false;}; REQUIRE(classify_material(state.lower,{2,0,.9},query).membership==MaterialMembership::Unknown);
    REQUIRE(std::fesetround(FE_DOWNWARD)==0);
    const auto rounding=capture_material_sequence(input,model(),23,source_id);
    const auto membership=classify_material(state.lower,{2,0,.9});
    REQUIRE(std::fesetround(FE_TONEAREST)==0); REQUIRE_FALSE(rounding.snapshot); REQUIRE(membership.membership==MaterialMembership::Unknown);
}
TEST_CASE("B05 varying gap respects uniform within-event G1 deposition", "[Nonplanar][B05][MaterialModel][UniformDeposition]")
{
    const auto row=bead(1,0,{0,0,1},{10,0,2},1,.2,.4,BeadSectionKind::Rectangle);
    const auto plan=captured({row}); const auto prefix=material_at(plan,0,.4); REQUIRE(prefix.nominal.snapshot);
    volume_contains(prefix.nominal.snapshot->nominal_deposited_volume_mm3,static_cast<long double>(std::get<Deposition>(row.motion.payload).volume.value())*.4L);
    // At t=.2, h=.24 and constant area=.3 require width=1.25.
    REQUIRE(classify_material(prefix.nominal,{2,.6,1.08}).membership==MaterialMembership::Inside);
    REQUIRE(classify_material(prefix.nominal,{2,.64,1.08}).membership==MaterialMembership::Outside);
    REQUIRE(classify_material(prefix.upper,{6,0,1.5}).membership==MaterialMembership::Outside);
    const auto descending=captured({bead(1,0,{0,0,2},{10,0,1},1,.4,.2,BeadSectionKind::Rectangle)});
    const auto complete=material_at(descending,1,0);
    REQUIRE(classify_material(complete.nominal,{8,.6,1.08}).membership==MaterialMembership::Inside);
    REQUIRE(classify_material(complete.nominal,{8,.64,1.08}).membership==MaterialMembership::Outside);
}
TEST_CASE("B05 material chain and prefix identity match independent byte encoding", "[Nonplanar][B05][MaterialModel][MaterialFingerprint]")
{
    const auto path=boost::filesystem::path(__FILE__).parent_path()/"data/material-fingerprint-v1.json";
    boost::nowide::ifstream input(path.string()); REQUIRE(input.good()); nlohmann::json oracle; input>>oracle;
    const MaterialRecord deposit{{101,0,33,{0,-0.,1},{10,0,2},Speed(20),Acceleration(100),
        Deposition{Volume(3),WidthXY(1),VerticalGap(.2),VerticalGap(.4),
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},7,8},4},
        BeadSection{BeadSectionKind::Rectangle,.2,.4,{.749999,1.500001}}};
    const MaterialRecord travel{{102,1,0,{10,0,2},{20,0,2},Speed(10),Acceleration(100),Travel{}},{}};
    const MaterialRecord retract{{103,2,0,{20,0,2},{20,0,2},Speed(10),Acceleration(100),
        Retraction{FilamentLength(.8),RetractionState::Ready,RetractionState::Retracted}}, {}};
    const MaterialRecord restore{{104,3,0,{20,0,2},{20,0,2},Speed(10),Acceleration(100),
        Retraction{FilamentLength(.8),RetractionState::Retracted,RetractionState::Ready}}, {}};
    // Synthetic geometry interval is byte-encoding data, not a factory proof.
    const auto ledger=std::make_shared<const MaterialSequenceSnapshot>(MaterialSequenceSnapshot{
        23,source_id,model(),{deposit,travel,retract,restore},{DepositedBeadGeometry{{10,10},{3,3}},{},{},{}}});
    REQUIRE(ledger->canonical_context()==oracle.at("context").get<std::string>());
    for (size_t i=0; i<ledger->records.size(); ++i) REQUIRE(ledger->canonical_record(i)==oracle.at("records").at(i).get<std::string>());
    REQUIRE(ledger->fingerprint()==oracle.at("sha256").get<std::string>());
    const MaterialPrefixSnapshot prefix{ledger,0,.4,{1.19,1.21}};
    REQUIRE(prefix.canonical()==oracle.at("prefix").at("canonical").get<std::string>());
    REQUIRE(prefix.fingerprint()==oracle.at("prefix").at("sha256").get<std::string>());
}

TEST_CASE("B05 continuous lower union covers a footprint across multiple deposited rows", "[Nonplanar][B05][MaterialCoverage]")
{
    const auto first=bead(1,0,{0,0,1},{10,0,1},1.2,.2,.2,BeadSectionKind::Rectangle);
    const MaterialRecord connector{{2,1,0,{10,0,1},{0,1,1},Speed(10),Acceleration(100),Travel{}},{}};
    const auto second=bead(3,2,{0,1,1},{10,1,1},1.2,.2,.2,BeadSectionKind::Rectangle);
    const auto ledger=captured({first,connector,second}); const auto all=material_at(ledger,3,0);
    const SceneBox footprint{{1,-.4,.88},{9,1.4,.92}};
    const auto covered=cover_material(all.lower,footprint); INFO(covered.reason);
    REQUIRE(covered.status==MaterialCoverageStatus::Covered); REQUIRE(covered.source==all.lower.snapshot);
    REQUIRE(covered.domain); REQUIRE(covered.domain->max.y()==1.4); REQUIRE(covered.representation==MaterialRepresentation::Lower);
    REQUIRE(covered.cells>1); REQUIRE_FALSE(covered.witness);
    MaterialCoverageLimits exhausted; exhausted.max_cells=1;
    REQUIRE(cover_material(all.lower,footprint,exhausted).status==MaterialCoverageStatus::Unknown);
    exhausted={}; exhausted.max_evaluations=1;
    REQUIRE(cover_material(all.lower,footprint,exhausted).status==MaterialCoverageStatus::Unknown);
    const auto partial=material_at(ledger,0,.4);
    REQUIRE(cover_material(partial.lower,{{1,-.3,.88},{3,.3,.92}}).status==MaterialCoverageStatus::Covered);
    const auto future=cover_material(partial.lower,footprint);
    REQUIRE(future.status==MaterialCoverageStatus::Uncovered); REQUIRE(future.witness);
    REQUIRE(classify_material(partial.lower,*future.witness).membership==MaterialMembership::Outside);
}
TEST_CASE("B05 continuous coverage detects a hole hidden by all footprint corners", "[Nonplanar][B05][MaterialCoverage]")
{
    const auto first=bead(1,0,{0,-1,1},{10,-1,1},.6,.2,.2,BeadSectionKind::Rectangle);
    const MaterialRecord connector{{2,1,0,{10,-1,1},{0,1,1},Speed(10),Acceleration(100),Travel{}},{}};
    const auto ledger=captured({first,connector,bead(3,2,{0,1,1},{10,1,1},.6,.2,.2,BeadSectionKind::Rectangle)});
    const auto all=material_at(ledger,3,0);
    for (double x : {1.,9.}) for (double y : {-1.,1.}) REQUIRE(classify_material(all.lower,{x,y,.9}).membership==MaterialMembership::Inside);
    const auto hole=cover_material(all.lower,{{1,-1,.9},{9,1,.9}}); INFO(hole.reason);
    REQUIRE(hole.status==MaterialCoverageStatus::Uncovered); REQUIRE(hole.witness);
    REQUIRE(classify_material(all.lower,*hole.witness).membership==MaterialMembership::Outside);
    REQUIRE(hole.witness->x()>=1); REQUIRE(hole.witness->x()<=9);
    REQUIRE(hole.witness->y()>=-1); REQUIRE(hole.witness->y()<=1);
}
TEST_CASE("B05 footprint coverage blocks exhausted work stale geometry and uncertain boundary", "[Nonplanar][B05][MaterialCoverage]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},.8,.2,.2)}); const auto all=material_at(ledger,1,0);
    const SceneBox footprint{{1,-.35,.88},{9,.35,.92}};
    REQUIRE(cover_material(all.lower,footprint).status==MaterialCoverageStatus::Covered);
    MaterialCoverageLimits limits; limits.max_evaluations=0;
    REQUIRE(cover_material(all.lower,footprint,limits).status==MaterialCoverageStatus::Unknown);
    limits={}; limits.is_current=[](uint64_t){return false;}; REQUIRE(cover_material(all.lower,footprint,limits).status==MaterialCoverageStatus::Unknown);
    limits={}; limits.cancelled=[] { return true; }; REQUIRE(cover_material(all.lower,footprint,limits).status==MaterialCoverageStatus::Unknown);
    limits={}; limits.timeout=std::chrono::milliseconds(1); limits.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(5)); return false; };
    REQUIRE(cover_material(all.lower,footprint,limits).status==MaterialCoverageStatus::Unknown);
    REQUIRE(cover_material(all.lower,{{2,0,.9},{1,0,.9}}).status==MaterialCoverageStatus::Unknown);
    // Butt boundary is uncertain and can never become covered from refinement.
    limits={}; limits.max_depth=3;
    REQUIRE(cover_material(all.nominal,{{0,0,.9},{0,0,.9}},limits).status==MaterialCoverageStatus::Unknown);
}

TEST_CASE("B06 first pass uses actual stepped material and bounds the missing nominal volume", "[Nonplanar][B06][MaterialTransition]")
{
    const auto first=bead(1,0,{0,0,1},{10,0,1},1.2,.4,.4,BeadSectionKind::Rectangle);
    const MaterialRecord travel{{2,1,0,{10,0,1},{0,1,1.2},Speed(10),Acceleration(100),Travel{}},{}};
    const auto ledger=captured({first,travel,bead(3,2,{0,1,1.2},{10,1,1.2},1.2,.4,.4,BeadSectionKind::Rectangle)});
    const auto state=material_at(ledger,3,0);
    const AffineCapCell cell{{1,-.4,9,1.4},1.4,1.42,1.4};
    const TransitionPolicy policy{VerticalGap(.1),VerticalGap(.6),Length(.00001)};
    const auto result=assess_material_first_pass(state.lower,cell,.9,policy); INFO(result.reason);
    REQUIRE(result.status==TransitionStatus::Compatible);
    REQUIRE(result.source==state.lower.snapshot); REQUIRE(result.cell); REQUIRE(result.policy);
    REQUIRE(result.support.status==MaterialCoverageStatus::Covered);
    REQUIRE(result.support.representation==MaterialRepresentation::Lower);
    REQUIRE(result.nominal_roof_ceiling_mm); REQUIRE(*result.nominal_roof_ceiling_mm>=1.2);
    REQUIRE(*result.nominal_roof_ceiling_mm<1.200000001);
    REQUIRE(result.upper_roof_ceiling_mm); REQUIRE(*result.upper_roof_ceiling_mm>1.21);
    REQUIRE(result.gap_mm); REQUIRE(result.gap_mm->lower>.1); REQUIRE(result.gap_mm->upper<.6);
    // Independent step integral: the higher second row owns y>=.4, including
    // the overlap. Mean affine height is 1.41; commanded bead sums are not used.
    REQUIRE(result.nominal_volume_mm3);
    volume_contains(*result.nominal_volume_mm3,8.L*(.8L*(1.41L-1)+1.L*(1.41L-1.2L)));
    const auto partial=material_at(ledger,0,.4);
    REQUIRE(assess_material_first_pass(partial.lower,cell,.9,policy).status!=TransitionStatus::Compatible);
    const auto low=assess_material_first_pass(state.lower,{{1,-.4,9,1.4},.95,.95,.95},.9,policy);
    REQUIRE(low.status==TransitionStatus::Rejected);
    const auto high=assess_material_first_pass(state.lower,{{1,-.4,9,1.4},2.,2.,2.},.9,policy);
    REQUIRE(high.status==TransitionStatus::Rejected);
}

TEST_CASE("B06 footprint roof excludes a tall neighboring bead by exact oriented clipping", "[Nonplanar][B06][MaterialTransition]")
{
    const auto floor=bead(1,0,{0,0,1},{10,0,1},1.2,.4,.4);
    const MaterialRecord travel{{2,1,0,{10,0,1},{-5,-4,8},Speed(10),Acceleration(100),Travel{}},{}};
    // Its XY bounding box overlaps the target, but the diagonal bead does not.
    const auto tall=bead(3,2,{-5,-4,8},{5,6,9},.8,.2,.4);
    const auto ledger=captured({floor,travel,tall},model(.02,.01)); const auto all=material_at(ledger,3,0);
    const AffineCapCell cell{{2,-.1,8,.1},1.3,1.3,1.3};
    const TransitionPolicy policy{VerticalGap(.1),VerticalGap(.5),Length(0)};
    const auto result=assess_material_first_pass(all.lower,cell,.85,policy); INFO(result.reason);
    REQUIRE(result.status==TransitionStatus::Compatible);
    REQUIRE(result.upper_roof_ceiling_mm); REQUIRE(*result.upper_roof_ceiling_mm<1.05);
    REQUIRE(result.nominal_volume_mm3); volume_contains(*result.nominal_volume_mm3,1.2L*.3L);
    // A previously laid wall really over the footprint must block the pass.
    const MaterialRecord connector{{2,1,0,{10,0,1},{0,0,8},Speed(10),Acceleration(100),Travel{}},{}};
    const auto over=captured({floor,connector,bead(3,2,{0,0,8},{10,0,8},.8,.2,.2)});
    const auto obstacle=assess_material_first_pass(material_at(over,3,0).lower,cell,.85,policy);
    REQUIRE(obstacle.status!=TransitionStatus::Compatible);
    REQUIRE(obstacle.upper_roof_ceiling_mm); REQUIRE(*obstacle.upper_roof_ceiling_mm>8);
}

TEST_CASE("B06 varying gap roof charges transverse growth and clips the current prefix", "[Nonplanar][B06][MaterialTransition]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,2},.8,.2,.4)},model(.02,.01));
    const auto partial=material_at(ledger,0,.4);
    const AffineCapCell cell{{1,-.02,3,.02},1.5,1.7,1.5};
    const TransitionPolicy policy{VerticalGap(.1),VerticalGap(.8),Length(0)};
    // A common plane must actually lie inside the whole varying-height section.
    const auto result=assess_material_first_pass(partial.lower,cell,1.085,policy); INFO(result.reason);
    REQUIRE(result.status!=TransitionStatus::Compatible);
    REQUIRE(result.upper_roof_ceiling_mm);
    const long double ceiling=1.3L+.02L+.02L+(.9L+.1L)*(.02L/10);
    REQUIRE(*result.upper_roof_ceiling_mm>=ceiling); REQUIRE(*result.upper_roof_ceiling_mm<1.343);
    // A small continuously supported footprint is positive, without changing Z.
    const auto narrow=assess_material_first_pass(partial.lower,{{1,-.02,1.2,.02},1.4,1.42,1.4},1.04,policy);
    INFO(narrow.reason); REQUIRE(narrow.status==TransitionStatus::Compatible);
    const auto future=assess_material_first_pass(partial.lower,{{5,-.02,6,.02},1.8,1.9,1.8},1.4,policy);
    REQUIRE(future.status!=TransitionStatus::Compatible);
}

TEST_CASE("B06 transition keeps holes limits revision ownership and uncertain gap blocking", "[Nonplanar][B06][MaterialTransition]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},.8,.2,.2)}); auto state=material_at(ledger,1,0);
    AffineCapCell cell{{1,-.05,3,.05},1.2,1.2,1.2};
    TransitionPolicy policy{VerticalGap(.1),VerticalGap(.4),Length(0)};
    REQUIRE(assess_material_first_pass(state.lower,cell,.9,policy).status==TransitionStatus::Compatible);
    auto uncertain=policy; uncertain.minimum=VerticalGap(.2);
    REQUIRE(assess_material_first_pass(state.lower,cell,.9,uncertain).status==TransitionStatus::Unknown);
    MaterialCoverageLimits limits; limits.max_evaluations=1;
    REQUIRE(assess_material_first_pass(state.lower,cell,.9,policy,limits).status==TransitionStatus::Unknown);
    limits={}; limits.cancelled=[] { return true; };
    REQUIRE(assess_material_first_pass(state.lower,cell,.9,policy,limits).status==TransitionStatus::Unknown);
    limits={}; limits.is_current=[](uint64_t) { return false; };
    REQUIRE(assess_material_first_pass(state.lower,cell,.9,policy,limits).status==TransitionStatus::Unknown);
    limits={}; limits.timeout=std::chrono::milliseconds(1);
    limits.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(5)); return false; };
    REQUIRE(assess_material_first_pass(state.lower,cell,.9,policy,limits).status==TransitionStatus::Unknown);
    limits={}; limits.cancelled=[&] { state.lower.snapshot.reset(); cell.z00=8; policy.minimum=VerticalGap(9); return false; };
    const auto owned=assess_material_first_pass(state.lower,cell,.9,policy,limits); INFO(owned.reason);
    REQUIRE(owned.status==TransitionStatus::Compatible); REQUIRE(owned.source); REQUIRE(owned.cell->z00==1.2);
    state=material_at(ledger,1,0); cell={{1,-.05,3,.05},1.2,1.2,1.2}; policy={VerticalGap(.1),VerticalGap(.4),Length(0)};
    REQUIRE(assess_material_first_pass(state.lower,{{3,-.05,1,.05},1.2,1.2,1.2},.9,policy).status==TransitionStatus::Unknown);
    REQUIRE(assess_material_first_pass(state.lower,cell,10001,policy).status==TransitionStatus::Unknown);
    REQUIRE(assess_material_first_pass({},cell,.9,policy).status==TransitionStatus::Unknown);
    limits={}; const int rounding=std::fegetround();
    limits.cancelled=[] { std::fesetround(FE_UPWARD); return false; };
    const auto numeric=assess_material_first_pass(state.lower,cell,.9,policy,limits);
    std::fesetround(rounding); REQUIRE(numeric.status==TransitionStatus::Unknown);
    const auto hole=assess_material_first_pass(state.lower,{{1,-1,3,1},1.2,1.2,1.2},.9,policy);
    REQUIRE(hole.status==TransitionStatus::Rejected); REQUIRE(hole.support.witness);
}

TEST_CASE("B06 refined volume integrates rounded shoulders with an independent circular primitive", "[Nonplanar][B06][MaterialIntegral]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},.8,.2,.2)}); const auto state=material_at(ledger,1,0);
    const AffineCapCell cell{{1,-.35,9,.35},1.2,1.2,1.2};
    const TransitionPolicy policy{VerticalGap(.1),VerticalGap(.4),Length(0)};
    MaterialIntegralLimits limits; limits.maximum_interval_width=Volume(.0001);
    const auto integral=integrate_material_first_pass(state.lower,cell,.9,policy,limits); INFO(integral.reason);
    REQUIRE(integral.status==MaterialIntegralStatus::Bounded); REQUIRE(integral.nominal_volume_mm3);
    REQUIRE(integral.first_pass.source==state.lower.snapshot);
    const auto bounds=*integral.nominal_volume_mm3;
    // Independent antiderivative of the circular shoulders. Actual binary64
    // commanded area determines the tiny effective-width reconciliation.
    const long double area=std::get<Deposition>(ledger->records.front().motion.payload).volume.value()/10.L;
    const long double h=ledger->records.front().bead->gap_begin_mm;
    const long double width=area/h+(1-std::acos(-1.L)/4)*h, core=(width-h)/2, radius=h/2, shoulder=.35L-core;
    const long double circular=.5L*(shoulder*std::sqrt(radius*radius-shoulder*shoulder)+radius*radius*std::asin(shoulder/radius));
    const long double roof=2*core+2*((1-h/2)*shoulder+circular);
    volume_contains(bounds,8*(.7L*1.2L-roof));
    REQUIRE(bounds.upper-bounds.lower<=limits.maximum_interval_width.value());
    REQUIRE(bounds.lower>1.1233); REQUIRE(bounds.upper<1.1236);
    REQUIRE(integral.cells>1); REQUIRE(integral.evaluations+integral.first_pass.roof_evaluations+
        integral.first_pass.support.evaluations<=limits.max_evaluations);
}

TEST_CASE("B06 refined volume uses the highest overlapping roof and preserves affine cell volume", "[Nonplanar][B06][MaterialIntegral]")
{
    const auto first=bead(1,0,{0,0,1},{10,0,1},1.2,.4,.4,BeadSectionKind::Rectangle);
    const MaterialRecord travel{{2,1,0,{10,0,1},{0,1,1.2},Speed(10),Acceleration(100),Travel{}},{}};
    const auto ledger=captured({first,travel,bead(3,2,{0,1,1.2},{10,1,1.2},1.2,.4,.4,BeadSectionKind::Rectangle)});
    const auto state=material_at(ledger,3,0);
    const TransitionPolicy policy{VerticalGap(.1),VerticalGap(.6),Length(.00001)};
    MaterialIntegralLimits limits; limits.maximum_interval_width=Volume(.000001);
    const auto result=integrate_material_first_pass(state.lower,{{1,-.4,9,1.4},1.4,1.42,1.4},.9,policy,limits);
    INFO(result.reason); REQUIRE(result.status==MaterialIntegralStatus::Bounded); REQUIRE(result.nominal_volume_mm3);
    volume_contains(*result.nominal_volume_mm3,8.L*(.8L*(1.41L-1)+1.L*(1.41L-1.2L)));
    REQUIRE(result.nominal_volume_mm3->upper-result.nominal_volume_mm3->lower<=limits.maximum_interval_width.value());
    REQUIRE(result.nominal_volume_mm3->lower>4.303999); REQUIRE(result.nominal_volume_mm3->upper<4.304001);
    const auto diagonal=captured({bead(1,0,{0,0,1},{3,4,1},.8,.2,.2)}); const auto all=material_at(diagonal,1,0);
    const auto affine=integrate_material_first_pass(all.lower,{{1.44,1.94,1.56,2.06},1.2,1.21,1.19},.9,policy,limits);
    INFO(affine.reason); REQUIRE(affine.status==MaterialIntegralStatus::Bounded); REQUIRE(affine.nominal_volume_mm3);
    volume_contains(*affine.nominal_volume_mm3,.12L*.12L*.2L);
    REQUIRE(affine.nominal_volume_mm3->upper-affine.nominal_volume_mm3->lower<=limits.maximum_interval_width.value());
}

TEST_CASE("B06 refined volume cannot publish broad stale late unsupported or future results", "[Nonplanar][B06][MaterialIntegral]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},.8,.2,.2)}); auto state=material_at(ledger,1,0);
    AffineCapCell cell{{1,-.35,9,.35},1.2,1.2,1.2}; TransitionPolicy policy{VerticalGap(.1),VerticalGap(.4),Length(0)};
    MaterialIntegralLimits limits; limits.max_cells=1;
    REQUIRE(integrate_material_first_pass(state.lower,cell,.9,policy,limits).status==MaterialIntegralStatus::Unknown);
    limits={}; limits.max_evaluations=1;
    REQUIRE(integrate_material_first_pass(state.lower,cell,.9,policy,limits).status==MaterialIntegralStatus::Unknown);
    limits={}; limits.max_depth=1;
    REQUIRE(integrate_material_first_pass(state.lower,cell,.9,policy,limits).status==MaterialIntegralStatus::Unknown);
    limits={}; limits.maximum_interval_width=Volume(0);
    REQUIRE(integrate_material_first_pass(state.lower,cell,.9,policy,limits).status==MaterialIntegralStatus::Unknown);
    limits={}; limits.is_current=[](uint64_t){return false;};
    REQUIRE(integrate_material_first_pass(state.lower,cell,.9,policy,limits).status==MaterialIntegralStatus::Unknown);
    limits={}; limits.cancelled=[] { return true; };
    REQUIRE(integrate_material_first_pass(state.lower,cell,.9,policy,limits).status==MaterialIntegralStatus::Unknown);
    limits={}; limits.timeout=std::chrono::milliseconds(1); limits.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(5));return false; };
    REQUIRE(integrate_material_first_pass(state.lower,cell,.9,policy,limits).status==MaterialIntegralStatus::Unknown);
    limits={}; limits.cancelled=[&] { state.lower.snapshot.reset();cell.z00=8;policy.minimum=VerticalGap(9);return false; };
    const auto owned=integrate_material_first_pass(state.lower,cell,.9,policy,limits); INFO(owned.reason);
    REQUIRE(owned.status==MaterialIntegralStatus::Bounded); REQUIRE(owned.first_pass.cell->z00==1.2);
    cell={{1,-.35,9,.35},1.2,1.2,1.2};policy={VerticalGap(.1),VerticalGap(.4),Length(0)};
    REQUIRE(integrate_material_first_pass(material_at(ledger,0,.4).lower,cell,.9,policy).status==MaterialIntegralStatus::Rejected);
    REQUIRE(integrate_material_first_pass(material_at(ledger,1,0).lower,{{1,-1,9,1},1.2,1.2,1.2},.9,policy).status==MaterialIntegralStatus::Rejected);
}

TEST_CASE("B06 affine pass selection preserves the final surface and bounds nominal volume quotas", "[Nonplanar][B06][PassStack]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},.8,.2,.2)}); const auto all=material_at(ledger,1,0);
    const AffineCapCell target{{1,-.35,9,.35},1.8,1.8,1.8};
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(.00001)},
        VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.001)};
    const auto planned=plan_affine_pass_stack(all.lower,target,.9,policy); INFO(planned.reason); REQUIRE(planned.snapshot);
    const auto &stack=*planned.snapshot; REQUIRE(stack.surfaces.size()==4); REQUIRE(stack.source==all.lower.snapshot);
    REQUIRE(stack.first_pass.status==MaterialIntegralStatus::Bounded);
    REQUIRE(stack.surfaces.back().cell.z00==target.z00); REQUIRE(stack.surfaces.back().cell.z10==target.z10);
    REQUIRE(stack.surfaces.back().cell.z01==target.z01);
    REQUIRE(stack.first_offset_mm>stack.offset_range_mm.lower); REQUIRE(stack.first_offset_mm<stack.offset_range_mm.upper);
    for (size_t i=1; i<stack.surfaces.size(); ++i) {
        const auto &surface=stack.surfaces[i]; REQUIRE(surface.vertical_spacing_mm); REQUIRE(surface.normal_spacing_mm);
        REQUIRE(surface.vertical_spacing_mm->lower>.14); REQUIRE(surface.vertical_spacing_mm->upper<.24);
        REQUIRE(surface.normal_spacing_mm->lower>.14); REQUIRE(surface.normal_spacing_mm->upper<.24);
    }
    REQUIRE(stack.total_allocation_error_mm3<=policy.total_volume_error.value());
    const long double area=std::get<Deposition>(ledger->records.front().motion.payload).volume.value()/10.L;
    const long double h=ledger->records.front().bead->gap_begin_mm;
    const long double width=area/h+(1-std::acos(-1.L)/4)*h, core=(width-h)/2, radius=h/2, s=.35L-core;
    const long double circular=.5L*(s*std::sqrt(radius*radius-s*s)+radius*radius*std::asin(s/radius));
    const long double expected=8*(.7L*static_cast<long double>(target.z00)-(2*core+2*((1-h/2)*s+circular)));
    volume_contains(stack.total_volume_mm3,expected);
    REQUIRE(std::abs(stack.total_allocated_volume.value()-expected)<=policy.total_volume_error.value());
    REQUIRE_FALSE(stack.surfaces.front().vertical_spacing_mm);
}

TEST_CASE("B06 pass selection distinguishes normal spacing on a slope and refuses an impossible stack", "[Nonplanar][B06][PassStack]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},1.2,.4,.4,BeadSectionKind::Rectangle)}); const auto all=material_at(ledger,1,0);
    AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),
        NormalGap(.14),NormalGap(.24),Volume(.001)};
    const AffineCapCell target{{1,-.1,2,.1},1.76,1.81,1.76};
    const auto slope=plan_affine_pass_stack(all.lower,target,.9,policy); INFO(slope.reason); REQUIRE(slope.snapshot);
    const auto &spacing=slope.snapshot->surfaces[1]; REQUIRE(spacing.vertical_spacing_mm); REQUIRE(spacing.normal_spacing_mm);
    REQUIRE(spacing.normal_spacing_mm->upper<spacing.vertical_spacing_mm->lower);
    const long double vertical=spacing.cell.z00-slope.snapshot->surfaces.front().cell.z00;
    const long double normal=vertical/std::sqrt(1+.05L*.05L);
    volume_contains(*spacing.normal_spacing_mm,normal);
    policy.later_normal_minimum=NormalGap(.235);
    REQUIRE_FALSE(plan_affine_pass_stack(all.lower,target,.9,policy).snapshot);
    policy.later_normal_minimum=NormalGap(.14); policy.passes=2;
    REQUIRE_FALSE(plan_affine_pass_stack(all.lower,target,.9,policy).snapshot);
    policy.passes=4;
    REQUIRE_FALSE(plan_affine_pass_stack(all.lower,{{1,-.1,2,.1},3.,3.,3.},.9,policy).snapshot);
    REQUIRE_FALSE(plan_affine_pass_stack(all.lower,{{1,-1,2,1},1.8,1.8,1.8},.9,policy).snapshot);
}

TEST_CASE("B06 pass stack owns policy and rejects stale work exhausted precision and numerical budget", "[Nonplanar][B06][PassStack]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},.8,.2,.2)}); auto state=material_at(ledger,1,0);
    AffineCapCell target{{1,-.35,9,.35},1.8,1.8,1.8};
    AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(.00001)},VerticalGap(.14),VerticalGap(.24),
        NormalGap(.14),NormalGap(.24),Volume(.001)};
    MaterialIntegralLimits limits; limits.cancelled=[&] { state.lower.snapshot.reset();target.z00=8;policy.passes=2;return false; };
    const auto owned=plan_affine_pass_stack(state.lower,target,.9,policy,limits); INFO(owned.reason); REQUIRE(owned.snapshot);
    REQUIRE(owned.snapshot->surfaces.size()==4); REQUIRE(owned.snapshot->final_surface.z00==1.8);
    state=material_at(ledger,1,0); target={{1,-.35,9,.35},1.8,1.8,1.8}; policy.passes=4;
    limits={}; limits.is_current=[](uint64_t){return false;}; REQUIRE_FALSE(plan_affine_pass_stack(state.lower,target,.9,policy,limits).snapshot);
    limits={}; limits.max_evaluations=1; REQUIRE_FALSE(plan_affine_pass_stack(state.lower,target,.9,policy,limits).snapshot);
    limits={}; limits.max_cells=1; REQUIRE_FALSE(plan_affine_pass_stack(state.lower,target,.9,policy,limits).snapshot);
    limits={}; limits.maximum_interval_width=Volume(0); REQUIRE_FALSE(plan_affine_pass_stack(state.lower,target,.9,policy,limits).snapshot);
    policy.first_gap.corner_height_error=Length(.02); REQUIRE_FALSE(plan_affine_pass_stack(state.lower,target,.9,policy).snapshot);
    policy.first_gap.corner_height_error=Length(.00001); policy.total_volume_error=Volume(0);
    REQUIRE_FALSE(plan_affine_pass_stack(state.lower,target,.9,policy).snapshot);
}

TEST_CASE("B06 strip quotas conserve a refined rounded roof integral without repeating material queries", "[Nonplanar][B06][IntegralStrips]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},.8,.2,.2)}); const auto state=material_at(ledger,1,0);
    const AffineCapCell cell{{1,-.35,9,.35},1.2,1.2,1.2};
    MaterialIntegralLimits limits; limits.maximum_interval_width=Volume(.0001);
    const auto amount=integrate_material_first_pass(state.lower,cell,.9,{VerticalGap(.1),VerticalGap(.4),Length(0)},limits);
    REQUIRE(amount.status==MaterialIntegralStatus::Bounded); REQUIRE(amount.proof);
    const auto split=split_material_integral(amount,IntegralSplitAxis::Y,{-.35,-.3,0,.3,.35},limits);
    INFO(split.reason); REQUIRE(split.snapshot); const auto &s=*split.snapshot; REQUIRE(s.strips.size()==4);
    const long double h=ledger->records.front().bead->gap_begin_mm;
    const long double area=std::get<Deposition>(ledger->records.front().motion.payload).volume.value()/10.L;
    const long double width=area/h+(1-std::acos(-1.L)/4)*h,core=(width-h)/2,r=h/2;
    const auto primitive=[&](long double y) {
        const long double edge=std::max(0.L,y-core);
        const long double circle=.5L*(edge*std::sqrt(r*r-edge*edge)+r*r*std::asin(edge/r));
        return std::min(y,core)+(1-h/2)*edge+circle;
    };
    const long double edge=8*(.05L*1.2L-(primitive(.35L)-primitive(.3L)));
    volume_contains(s.strips[0].volume_mm3,edge); volume_contains(s.strips[3].volume_mm3,edge);
    volume_contains(s.strips[1].volume_mm3,8*.3L*.2L); volume_contains(s.strips[2].volume_mm3,8*.3L*.2L);
    REQUIRE(s.source==amount.proof); REQUIRE(s.total_volume_mm3.lower<=amount.nominal_volume_mm3->upper);
    REQUIRE(s.total_volume_mm3.upper>=amount.nominal_volume_mm3->lower);
    REQUIRE(s.total_volume_mm3.upper-s.total_volume_mm3.lower<=limits.maximum_interval_width.value());
    REQUIRE(s.proof_cells>0); REQUIRE(s.evaluations<=limits.max_evaluations);
    const auto along=split_material_integral(amount,IntegralSplitAxis::X,{1,3,6,9},limits); INFO(along.reason); REQUIRE(along.snapshot);
    REQUIRE(along.snapshot->strips.size()==3);
    volume_contains(along.snapshot->strips[0].volume_mm3,edge/2+2*.6L*.2L);
    const auto affine=integrate_material_first_pass(state.lower,{{1,-.35,9,.35},1.2,1.28,1.2},.9,
        {VerticalGap(.1),VerticalGap(.4),Length(0)},limits); REQUIRE(affine.proof);
    const auto varying=split_material_integral(affine,IntegralSplitAxis::X,{1,3,9},limits); REQUIRE(varying.snapshot);
    volume_contains(varying.snapshot->strips[0].volume_mm3,edge/2+2*.6L*.2L+2*.7L*.01L);
}

TEST_CASE("B06 strip partition owns its proof cuts and limits and refuses incomplete or exhausted allocation", "[Nonplanar][B06][IntegralStrips]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},.8,.2,.2)}); const auto state=material_at(ledger,1,0);
    MaterialIntegralLimits limits; limits.maximum_interval_width=Volume(.0001);
    auto amount=integrate_material_first_pass(state.lower,{{1,-.35,9,.35},1.2,1.2,1.2},.9,
        {VerticalGap(.1),VerticalGap(.4),Length(0)},limits); REQUIRE(amount.proof);
    std::vector<double> cuts{-.35,0,.35};
    limits.cancelled=[&] { amount.proof.reset();cuts[1]=.5;limits.max_cells=1;return false; };
    const auto owned=split_material_integral(amount,IntegralSplitAxis::Y,cuts,limits); INFO(owned.reason); REQUIRE(owned.snapshot);
    REQUIRE(owned.snapshot->cuts[1]==0); REQUIRE(owned.snapshot->source);
    amount=integrate_material_first_pass(state.lower,{{1,-.35,9,.35},1.2,1.2,1.2},.9,
        {VerticalGap(.1),VerticalGap(.4),Length(0)});
    limits={}; limits.maximum_interval_width=Volume(.001);
    REQUIRE_FALSE(split_material_integral(amount,IntegralSplitAxis::Y,{-.3,0,.35},limits).snapshot);
    REQUIRE_FALSE(split_material_integral(amount,IntegralSplitAxis::Y,{-.35,.2,.1,.35},limits).snapshot);
    REQUIRE_FALSE(split_material_integral(amount,IntegralSplitAxis::Y,{-.35,0,0,.35},limits).snapshot);
    REQUIRE_FALSE(split_material_integral({},IntegralSplitAxis::Y,{-.35,0,.35},limits).snapshot);
    limits.max_cells=1; REQUIRE_FALSE(split_material_integral(amount,IntegralSplitAxis::Y,{-.35,0,.35},limits).snapshot);
    limits={}; limits.max_evaluations=1; REQUIRE_FALSE(split_material_integral(amount,IntegralSplitAxis::Y,{-.35,0,.35},limits).snapshot);
    limits={}; limits.is_current=[](uint64_t){return false;}; REQUIRE_FALSE(split_material_integral(amount,IntegralSplitAxis::Y,{-.35,0,.35},limits).snapshot);
    limits={}; limits.maximum_interval_width=Volume(.000000001);
    REQUIRE_FALSE(split_material_integral(amount,IntegralSplitAxis::Y,{-.35,0,.35},limits).snapshot);
    limits={}; limits.cancelled=[] { return true; };
    REQUIRE_FALSE(split_material_integral(amount,IntegralSplitAxis::Y,{-.35,0,.35},limits).snapshot);
    limits={}; limits.timeout=std::chrono::milliseconds(1);
    limits.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(3));return false; };
    REQUIRE_FALSE(split_material_integral(amount,IntegralSplitAxis::Y,{-.35,0,.35},limits).snapshot);
}

TEST_CASE("B07 finite fixed-width hatch candidates retain affine Z strip volumes and both direction choices", "[Nonplanar][B07][AffineHatches]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},1.2,.4,.4,BeadSectionKind::Rectangle)});
    const auto present=material_at(ledger,1,0);
    const AffinePassPolicy passes{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),
        NormalGap(.14),NormalGap(.24),Volume(.001)};
    const auto stack=plan_affine_pass_stack(present.lower,{{1,-.4,3,.4},1.76,1.8,1.76},.9,passes); REQUIRE(stack.snapshot);
    const AffineHatchPolicy policy{WidthXY(.45),Length(.4),Length(.05),HatchDirection::AlongX};
    AffineHatchLimits limits; limits.volumes.maximum_interval_width=Volume(.001);
    const auto result=plan_affine_hatches(stack,policy,limits); INFO(result.reason); REQUIRE(result.snapshot);
    const auto &layout=*result.snapshot; REQUIRE(layout.source==stack.snapshot); REQUIRE(layout.passes.size()==4);
    size_t count=0;
    for (size_t p=0; p<layout.passes.size(); ++p) {
        const auto &pass=layout.passes[p]; REQUIRE(pass.direction==(p%2 ? HatchDirection::AlongY : HatchDirection::AlongX));
        REQUIRE(pass.boundary_regions.size()==4); REQUIRE(pass.lines.size()>=2);
        REQUIRE(pass.pitch_mm.lower>0); REQUIRE(pass.pitch_mm.upper<=policy.maximum_pitch.value());
        for (const auto &line : pass.lines) {
            ++count; REQUIRE(line.width.value()==.45); REQUIRE(line.prospective_cell_volume_mm3.lower>0);
            const auto &cell=stack.snapshot->surfaces[p].cell;
            for (const auto &point : {line.start,line.end}) {
                REQUIRE(point.x()-.225>=cell.footprint.min_x); REQUIRE(point.x()+.225<=cell.footprint.max_x);
                REQUIRE(point.y()-.225>=cell.footprint.min_y); REQUIRE(point.y()+.225<=cell.footprint.max_y);
                const long double z=static_cast<long double>(cell.z00)+
                    (static_cast<long double>(cell.z10)-cell.z00)*(point.x()-1)/2;
                REQUIRE(std::abs(point.z()-z)<=line.coordinate_error_upper_mm);
            }
            REQUIRE(line.reverse_start.x()==line.end.x()); REQUIRE(line.reverse_end.z()==line.start.z());
            REQUIRE(line.projected_length_mm.lower>0);
        }
    }
    REQUIRE(count==layout.line_count);
    // Independent affine mean-height volume above the rectangular source top.
    volume_contains(layout.total_prospective_volume_mm3,2.L*.8L*((static_cast<long double>(double(1.76))+double(1.8))/2-1));
    REQUIRE(layout.total_prospective_volume_mm3.upper-layout.total_prospective_volume_mm3.lower<=.001);
    REQUIRE(layout.passes.front().lines.front().start.z()!=layout.passes.front().lines.front().end.z());
    REQUIRE(layout.passes.back().lines.front().start.z()!=layout.passes.back().lines.back().start.z());
    auto alternate=policy; alternate.first_direction=HatchDirection::AlongY;
    const auto other=plan_affine_hatches(stack,alternate,limits); INFO(other.reason); REQUIRE(other.snapshot);
    for (size_t p=0; p<4; ++p)
        REQUIRE(other.snapshot->passes[p].direction==(p%2 ? HatchDirection::AlongX : HatchDirection::AlongY));
    volume_contains(other.snapshot->total_prospective_volume_mm3,
        2.L*.8L*((static_cast<long double>(double(1.76))+double(1.8))/2-1));
}

TEST_CASE("B07 finite hatch capture refuses thin sparse stale cancelled and exhausted candidates", "[Nonplanar][B07][AffineHatches]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},1.2,.4,.4,BeadSectionKind::Rectangle)});
    const auto present=material_at(ledger,1,0);
    const AffinePassPolicy passes{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),
        NormalGap(.14),NormalGap(.24),Volume(.001)};
    auto stack=plan_affine_pass_stack(present.lower,{{1,-.4,3,.4},1.76,1.8,1.76},.9,passes); REQUIRE(stack.snapshot);
    AffineHatchPolicy policy{WidthXY(.45),Length(.4),Length(.05),HatchDirection::AlongX};
    AffineHatchLimits limits; limits.cancelled=[&] { stack.snapshot.reset();policy.width=WidthXY(3);limits.max_lines=1;return false; };
    const auto retained=plan_affine_hatches(stack,policy,limits); INFO(retained.reason); REQUIRE(retained.snapshot);
    REQUIRE(retained.snapshot->policy.width.value()==.45); REQUIRE(retained.snapshot->line_count>1);
    stack.snapshot=retained.snapshot->source; policy=retained.snapshot->policy; limits={};
    REQUIRE_FALSE(plan_affine_hatches({},policy,limits).snapshot);
    policy.boundary_band=Length(0); REQUIRE_FALSE(plan_affine_hatches(stack,policy,limits).snapshot);
    policy.boundary_band=Length(.05);
    policy.first_direction=static_cast<HatchDirection>(2); REQUIRE_FALSE(plan_affine_hatches(stack,policy,limits).snapshot);
    policy.first_direction=HatchDirection::AlongX;
    policy.maximum_pitch=Length(.6); REQUIRE_FALSE(plan_affine_hatches(stack,policy,limits).snapshot);
    policy.maximum_pitch=Length(.4); policy.width=WidthXY(1); REQUIRE_FALSE(plan_affine_hatches(stack,policy,limits).snapshot);
    policy.width=WidthXY(.45); limits.max_lines=1; REQUIRE_FALSE(plan_affine_hatches(stack,policy,limits).snapshot);
    limits={}; limits.is_current=[](uint64_t){return false;}; REQUIRE_FALSE(plan_affine_hatches(stack,policy,limits).snapshot);
    limits={}; limits.cancelled=[] { return true; }; REQUIRE_FALSE(plan_affine_hatches(stack,policy,limits).snapshot);
    limits={}; limits.volumes.max_evaluations=1; REQUIRE_FALSE(plan_affine_hatches(stack,policy,limits).snapshot);
    limits={}; limits.timeout=std::chrono::milliseconds(1);
    limits.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(3));return false; };
    REQUIRE_FALSE(plan_affine_hatches(stack,policy,limits).snapshot);
}

TEST_CASE("B07 finite hatch cells conserve interior and boundary volumes without charging ends to a bead", "[Nonplanar][B07][HatchCells]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},1.2,.4,.4,BeadSectionKind::Rectangle)});
    const auto present=material_at(ledger,1,0);
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),
        NormalGap(.14),NormalGap(.24),Volume(.001)};
    const auto stack=plan_affine_pass_stack(present.lower,{{1,-.4,3,.4},1.76,1.8,1.76},.9,policy); REQUIRE(stack.snapshot);
    const auto hatch=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),HatchDirection::AlongX}); REQUIRE(hatch.snapshot);
    const auto result=allocate_affine_hatch_cells(hatch); INFO(result.reason); REQUIRE(result.snapshot);
    const auto &allocation=*result.snapshot; REQUIRE(allocation.source==hatch.snapshot); REQUIRE(allocation.passes.size()==4);
    long double expected_inside=0,expected_remainder=0;
    for (size_t p=0; p<4; ++p) {
        const auto &pass=allocation.passes[p]; const auto &lines=hatch.snapshot->passes[p].lines;
        REQUIRE(pass.finite_cells.size()==lines.size()); REQUIRE(pass.remainder_cells.size()==4);
        const auto &r=pass.finite_footprint; long double before=p ? stack.snapshot->surfaces[p-1].cell.z00 : 1;
        const auto &above=stack.snapshot->surfaces[p].cell;
        const long double lower_gradient=p ?
            (static_cast<long double>(stack.snapshot->surfaces[p-1].cell.z10)-before)/2 : 0;
        const long double slope=(static_cast<long double>(above.z10)-above.z00)/2-lower_gradient;
        const long double base=static_cast<long double>(above.z00)-before;
        const auto oracle=[&](const RectangleXY &box) {
            return (static_cast<long double>(box.max_x)-box.min_x)*(static_cast<long double>(box.max_y)-box.min_y)*
                (base+slope*((static_cast<long double>(box.min_x)+box.max_x)/2-1));
        };
        for (size_t i=0; i<lines.size(); ++i) {
            const auto &cell=pass.finite_cells[i]; REQUIRE(cell.volume_mm3.lower>0);
            REQUIRE(cell.footprint.min_x>=r.min_x); REQUIRE(cell.footprint.max_x<=r.max_x);
            REQUIRE(cell.footprint.min_y>=r.min_y); REQUIRE(cell.footprint.max_y<=r.max_y);
            if (hatch.snapshot->passes[p].direction==HatchDirection::AlongX) {
                REQUIRE(cell.footprint.min_x>=lines[i].start.x()); REQUIRE(cell.footprint.max_x<=lines[i].end.x());
                REQUIRE(cell.footprint.min_y>=lines[i].start.y()-.225); REQUIRE(cell.footprint.max_y<=lines[i].start.y()+.225);
            } else {
                REQUIRE(cell.footprint.min_y>=lines[i].start.y()); REQUIRE(cell.footprint.max_y<=lines[i].end.y());
                REQUIRE(cell.footprint.min_x>=lines[i].start.x()-.225); REQUIRE(cell.footprint.max_x<=lines[i].start.x()+.225);
            }
            volume_contains(cell.volume_mm3,oracle(cell.footprint));
        }
        volume_contains(pass.finite_volume_mm3,oracle(r)); expected_inside+=oracle(r);
        const RectangleXY whole{1,-.4,3,.4}; const auto remainder=oracle(whole)-oracle(r);
        volume_contains(pass.remainder_volume_mm3,remainder); expected_remainder+=remainder;
        REQUIRE(pass.remainder_volume_mm3.lower>0); REQUIRE(pass.finite_volume_mm3.upper<pass.total_volume_mm3.lower);
        for (const auto &cell : pass.remainder_cells) volume_contains(cell.volume_mm3,oracle(cell.footprint));
    }
    volume_contains(allocation.finite_volume_mm3,expected_inside);
    volume_contains(allocation.remainder_volume_mm3,expected_remainder);
    volume_contains(allocation.total_volume_mm3,expected_inside+expected_remainder);
    REQUIRE(allocation.total_volume_mm3.upper-allocation.total_volume_mm3.lower<=.001);
}

TEST_CASE("B07 finite cell allocation owns its parent and rejects stale cancelled or exhausted proofs", "[Nonplanar][B07][HatchCells]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},1.2,.4,.4,BeadSectionKind::Rectangle)});
    const auto present=material_at(ledger,1,0);
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),
        NormalGap(.14),NormalGap(.24),Volume(.001)};
    const auto stack=plan_affine_pass_stack(present.lower,{{1,-.4,3,.4},1.76,1.8,1.76},.9,policy);
    auto hatch=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),HatchDirection::AlongX}); REQUIRE(hatch.snapshot);
    MaterialIntegralLimits limits; limits.cancelled=[&] { hatch.snapshot.reset();limits.max_cells=1;return false; };
    const auto owned=allocate_affine_hatch_cells(hatch,limits); INFO(owned.reason); REQUIRE(owned.snapshot);
    REQUIRE(owned.snapshot->source); hatch.snapshot=owned.snapshot->source; limits={};
    REQUIRE_FALSE(allocate_affine_hatch_cells({}).snapshot);
    limits.max_cells=1; REQUIRE_FALSE(allocate_affine_hatch_cells(hatch,limits).snapshot);
    limits={}; limits.max_evaluations=1; REQUIRE_FALSE(allocate_affine_hatch_cells(hatch,limits).snapshot);
    limits={}; limits.maximum_interval_width=Volume(1e-17); REQUIRE_FALSE(allocate_affine_hatch_cells(hatch,limits).snapshot);
    limits={}; limits.is_current=[](uint64_t){return false;}; REQUIRE_FALSE(allocate_affine_hatch_cells(hatch,limits).snapshot);
    limits={}; limits.cancelled=[] { return true; }; REQUIRE_FALSE(allocate_affine_hatch_cells(hatch,limits).snapshot);
    limits={}; limits.timeout=std::chrono::milliseconds(1);
    limits.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(3));return false; };
    REQUIRE_FALSE(allocate_affine_hatch_cells(hatch,limits).snapshot);
}

TEST_CASE("B07 finite cells retain rounded source shoulders in the boundary remainder", "[Nonplanar][B07][HatchCells]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},.8,.2,.2)}); const auto present=material_at(ledger,1,0);
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),
        NormalGap(.14),NormalGap(.24),Volume(.001)};
    const auto stack=plan_affine_pass_stack(present.lower,{{1,-.35,3,.35},1.76,1.8,1.76},.9,policy); REQUIRE(stack.snapshot);
    const auto hatch=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),HatchDirection::AlongX}); REQUIRE(hatch.snapshot);
    const auto cells=allocate_affine_hatch_cells(hatch); INFO(cells.reason); REQUIRE(cells.snapshot);
    const auto &core=cells.snapshot->passes.front().finite_footprint;
    REQUIRE(core.min_y>-.3); REQUIRE(core.max_y<.3);
    // Independent circular primitive for the two 0.05 mm shoulders. The
    // finite first-pass core lies wholly over the flat nominal section.
    const long double radius=.1L,u=.05L;
    const long double roof=.6L+.1L*.9L+u*std::sqrt(radius*radius-u*u)+radius*radius*std::asin(u/radius);
    const long double final_mean=(static_cast<long double>(double(1.76))+double(1.8))/2;
    const long double whole=2*(.7L*final_mean-roof);
    volume_contains(cells.snapshot->total_volume_mm3,whole);
    const auto &first=stack.snapshot->surfaces.front().cell;
    const long double mean=static_cast<long double>(first.z00)+(static_cast<long double>(first.z10)-first.z00)*
        ((static_cast<long double>(core.min_x)+core.max_x)/2-1)/2;
    const long double finite=(static_cast<long double>(core.max_x)-core.min_x)*(static_cast<long double>(core.max_y)-core.min_y)*(mean-1);
    const long double first_whole=2*(.7L*(static_cast<long double>(first.z00)+first.z10)/2-roof);
    volume_contains(cells.snapshot->passes.front().finite_volume_mm3,finite);
    volume_contains(cells.snapshot->passes.front().remainder_volume_mm3,first_whole-finite);
}

TEST_CASE("B07 fixed nominal width uses adaptive constant-flux packets with bounded volume", "[Nonplanar][B07][FixedWidthBeads]")
{
    for (auto kind : {BeadSectionKind::Rectangle,BeadSectionKind::RoundedRectangle}) {
        const FixedWidthBeadRequest request{{0,0,1},{3,4,2},WidthXY(.8),VerticalGap(.2),VerticalGap(.4),kind,23,source_id};
        FixedWidthBeadLimits limits; limits.maximum_width_error=Length(.002);limits.maximum_volume_error=Volume(.00001);
        const auto result=plan_fixed_width_bead(request,limits); INFO(result.reason); REQUIRE(result.snapshot);
        const auto &plan=*result.snapshot; REQUIRE(plan.pieces.size()>1); REQUIRE(plan.pieces.size()<=limits.max_segments);
        const long double h0=.2L,h1=.4L,k=kind==BeadSectionKind::RoundedRectangle ? 1-std::acos(-1.L)/4 : 0;
        const long double expected=5*(.8L*(h0+h1)/2-k*(h0*h0+h0*h1+h1*h1)/3);
        volume_contains(plan.target_volume_mm3,expected);
        REQUIRE(plan.total_volume_error_mm3<=limits.maximum_volume_error.value());
        REQUIRE(plan.maximum_width_error_mm<=limits.maximum_width_error.value());
        long double delivered=0; PhysicalPosition previous=request.start;
        for (size_t i=0; i<plan.pieces.size(); ++i) {
            const auto &piece=plan.pieces[i]; REQUIRE(piece.start.x()==previous.x()); REQUIRE(piece.start.z()==previous.z());
            REQUIRE(piece.nominal_width.value()==.8); REQUIRE(piece.section.kind==kind);
            const long double dx=static_cast<long double>(piece.end.x())-piece.start.x(),dy=static_cast<long double>(piece.end.y())-piece.start.y();
            const long double area=piece.volume.value()/std::sqrt(dx*dx+dy*dy);
            for (long double t : {0.L,.23L,.71L,1.L}) {
                const long double h=piece.section.gap_begin_mm+t*(static_cast<long double>(piece.section.gap_end_mm)-piece.section.gap_begin_mm);
                const long double width=area/h+k*h;
                REQUIRE(std::abs(width-.8L)<=limits.maximum_width_error.value());
                REQUIRE(width>=piece.section.width_mm.lower); REQUIRE(width<=piece.section.width_mm.upper);
            }
            const auto record=MaterialRecord{{1,0,33,piece.start,piece.end,Speed(20),Acceleration(100),Deposition{piece.volume,piece.nominal_width,
                VerticalGap(std::min(piece.section.gap_begin_mm,piece.section.gap_end_mm)),VerticalGap(std::max(piece.section.gap_begin_mm,piece.section.gap_end_mm)),
                {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},7,8}},piece.section};
            const auto accepted=capture_material_sequence({record},model(),23,source_id);INFO(accepted.reason);REQUIRE(accepted.snapshot);
            delivered+=piece.volume.value(); previous=piece.end;
        }
        REQUIRE(previous.x()==request.end.x()); REQUIRE(previous.y()==request.end.y()); REQUIRE(previous.z()==request.end.z());
        REQUIRE(std::abs(delivered-expected)<=limits.maximum_volume_error.value());
        auto reversed=request;reversed.start=request.end;reversed.end=request.start;
        reversed.gap_begin=request.gap_end;reversed.gap_end=request.gap_begin;
        const auto reverse_plan=plan_fixed_width_bead(reversed,limits);INFO(reverse_plan.reason);REQUIRE(reverse_plan.snapshot);
        volume_contains(reverse_plan.snapshot->target_volume_mm3,expected);
        REQUIRE(reverse_plan.snapshot->pieces.front().section.gap_begin_mm==request.gap_end.value());
        REQUIRE(reverse_plan.snapshot->pieces.back().section.gap_end_mm==request.gap_begin.value());
        REQUIRE(reverse_plan.snapshot->total_volume_error_mm3<=limits.maximum_volume_error.value());
    }
}

TEST_CASE("B07 constant gap keeps one packet and affine slope never multiplies XY deposition volume", "[Nonplanar][B07][FixedWidthBeads]")
{
    const FixedWidthBeadRequest request{{1,2,1},{11,2,5},WidthXY(.45),VerticalGap(.2),VerticalGap(.2),BeadSectionKind::RoundedRectangle,23,source_id};
    const auto plan=plan_fixed_width_bead(request); INFO(plan.reason); REQUIRE(plan.snapshot); REQUIRE(plan.snapshot->pieces.size()==1);
    const long double expected=10*.2L*(.45L-(1-std::acos(-1.L)/4)*.2L);
    volume_contains(plan.snapshot->target_volume_mm3,expected);
    REQUIRE(std::abs(plan.snapshot->pieces.front().volume.value()-expected)<1e-12L);
    auto reversed=request; reversed.start=request.end;reversed.end=request.start;
    const auto other=plan_fixed_width_bead(reversed); REQUIRE(other.snapshot);
    volume_contains(other.snapshot->target_volume_mm3,expected);
    REQUIRE(other.snapshot->pieces.front().start.z()==5); REQUIRE(other.snapshot->pieces.front().end.z()==1);
}

TEST_CASE("B07 fixed-width packet planning captures inputs and refuses stale invalid and exhausted requests", "[Nonplanar][B07][FixedWidthBeads]")
{
    FixedWidthBeadRequest request{{0,0,1},{10,0,2},WidthXY(.8),VerticalGap(.2),VerticalGap(.4),BeadSectionKind::RoundedRectangle,23,source_id};
    FixedWidthBeadLimits limits;limits.cancelled=[&] { request.end={0,0,1};request.source_fingerprint.clear();limits.max_segments=1;return false; };
    const auto owned=plan_fixed_width_bead(request,limits);INFO(owned.reason);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->request.end.x()==10);
    request=owned.snapshot->request;limits={};limits.max_segments=1;REQUIRE_FALSE(plan_fixed_width_bead(request,limits).snapshot);
    limits={};limits.max_depth=1;limits.maximum_width_error=Length(1e-7);REQUIRE_FALSE(plan_fixed_width_bead(request,limits).snapshot);
    limits={};limits.maximum_width_error=Length(0);REQUIRE_FALSE(plan_fixed_width_bead(request,limits).snapshot);
    limits={};limits.maximum_volume_error=Volume(1e-20);REQUIRE_FALSE(plan_fixed_width_bead(request,limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(plan_fixed_width_bead(request,limits).snapshot);
    limits={};limits.cancelled=[] { return true; };REQUIRE_FALSE(plan_fixed_width_bead(request,limits).snapshot);
    limits={};limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};
    REQUIRE_FALSE(plan_fixed_width_bead(request,limits).snapshot);
    limits={}; auto invalid=request;invalid.end=invalid.start;REQUIRE_FALSE(plan_fixed_width_bead(invalid,limits).snapshot);
    invalid=request;invalid.width=WidthXY(.3);REQUIRE_FALSE(plan_fixed_width_bead(invalid,limits).snapshot);
    invalid=request;invalid.source_fingerprint="other";REQUIRE_FALSE(plan_fixed_width_bead(invalid,limits).snapshot);
    invalid=request;invalid.kind=static_cast<BeadSectionKind>(7);REQUIRE_FALSE(plan_fixed_width_bead(invalid,limits).snapshot);
    limits.cancelled=[] { std::fesetround(FE_DOWNWARD);return false; };
    const auto rounding=plan_fixed_width_bead(request,limits);REQUIRE(std::fesetround(FE_TONEAREST)==0);REQUIRE_FALSE(rounding.snapshot);
}

TEST_CASE("B07 first hatch bead uses the laid roof rather than the lower support plane", "[Nonplanar][B07][FirstHatchBeads]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},1.2,.4,.4,BeadSectionKind::Rectangle)});const auto present=material_at(ledger,1,0);
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.001)};
    const auto stack=plan_affine_pass_stack(present.lower,{{1,-.4,3,.4},1.76,1.8,1.76},.9,policy);REQUIRE(stack.snapshot);
    const auto hatch=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),HatchDirection::AlongX});REQUIRE(hatch.snapshot);
    FirstHatchBeadLimits limits;limits.maximum_gap_error=Length(.0001);limits.packets.maximum_width_error=Length(.002);
    limits.packets.maximum_volume_error=Volume(.00001);
    const auto result=plan_first_hatch_bead(hatch,0,limits);INFO(result.reason);REQUIRE(result.snapshot);
    const auto &plan=*result.snapshot;REQUIRE(plan.source==hatch.snapshot);REQUIRE(plan.line_index==0);REQUIRE(plan.pieces.size()>1);
    const auto &line=hatch.snapshot->passes.front().lines.front();
    const long double h0=static_cast<long double>(line.start.z())-1,h1=static_cast<long double>(line.end.z())-1;
    const long double length=static_cast<long double>(line.end.x())-line.start.x(),k=1-std::acos(-1.L)/4;
    const long double target=length*(.45L*(h0+h1)/2-k*(h0*h0+h0*h1+h1*h1)/3);
    volume_contains(plan.actual_target_volume_mm3,target);
    REQUIRE(plan.maximum_gap_error_mm<=limits.maximum_gap_error.value());
    REQUIRE(plan.maximum_width_error_mm<=limits.packets.maximum_width_error.value());
    REQUIRE(plan.total_volume_error_mm3<=limits.packets.maximum_volume_error.value());
    for (const auto &piece : plan.pieces) {
        REQUIRE(std::abs(piece.start.z()-piece.section.gap_begin_mm-1)<=plan.maximum_gap_error_mm);
        REQUIRE(std::abs(piece.end.z()-piece.section.gap_end_mm-1)<=plan.maximum_gap_error_mm);
    }
}

TEST_CASE("B07 first hatch bead bounds the continuous rounded roof and owns revision callbacks", "[Nonplanar][B07][FirstHatchBeads]")
{
    const auto row=bead(1,0,{0,0,1},{10,0,1},1.6,.6,.6);const auto ledger=captured({row});const auto present=material_at(ledger,1,0);
    const AffinePassPolicy policy{4,{VerticalGap(.05),VerticalGap(.28),Length(0)},VerticalGap(.08),VerticalGap(.13),NormalGap(.08),NormalGap(.13),Volume(.001)};
    const auto stack=plan_affine_pass_stack(present.lower,{{1,-.75,3,.75},1.43,1.43,1.43},.82,policy);INFO(stack.reason);REQUIRE(stack.snapshot);
    auto hatch=plan_affine_hatches(stack,{WidthXY(.3),Length(.25),Length(.05),HatchDirection::AlongY});INFO(hatch.reason);REQUIRE(hatch.snapshot);
    FirstHatchBeadLimits limits;limits.maximum_gap_error=Length(.0001);limits.packets.maximum_width_error=Length(.002);
    limits.packets.maximum_volume_error=Volume(.0001);
    const auto result=plan_first_hatch_bead(hatch,0,limits);INFO(result.reason);REQUIRE(result.snapshot);
    const auto &plan=*result.snapshot;const auto &line=hatch.snapshot->passes.front().lines.front();REQUIRE(plan.roof_segments>1);
    // Independent monotone shoulder integration. Actual binary64 source amount
    // defines its core; production accepts continuous interval bounds, not samples.
    const long double k=1-std::acos(-1.L)/4,h=row.bead->gap_begin_mm;
    const long double width=std::get<Deposition>(row.motion.payload).volume.value()/10/h+k*h,core=(width-h)/2,radius=h/2;
    const auto area=[&](long double y) {const long double d=std::max(std::abs(y)-core,0.L);
        const long double roof=1-h/2+std::sqrt(radius*radius-d*d),gap=line.start.z()-roof;return gap*(.3L-k*gap);};
    const long double dy=(static_cast<long double>(line.end.y())-line.start.y())/4096;long double lower=0,upper=0;
    for (size_t i=0;i<4096;++i) {const long double a=area(line.start.y()+dy*i),b=area(line.start.y()+dy*(i+1));
        lower+=std::min(a,b)*dy;upper+=std::max(a,b)*dy;}
    REQUIRE(plan.actual_target_volume_mm3.lower<=lower-1e-10L);REQUIRE(plan.actual_target_volume_mm3.upper>=upper+1e-10L);
    REQUIRE(plan.total_volume_error_mm3<=limits.packets.maximum_volume_error.value());
    limits.cancelled=[&] {hatch.snapshot.reset();limits.max_roof_segments=1;return false;};
    const auto owned=plan_first_hatch_bead(hatch,0,limits);INFO(owned.reason);REQUIRE(owned.snapshot);hatch.snapshot=owned.snapshot->source;limits={};
    REQUIRE_FALSE(plan_first_hatch_bead({},0,limits).snapshot);REQUIRE_FALSE(plan_first_hatch_bead(hatch,10000,limits).snapshot);
    limits.max_evaluations=1;REQUIRE_FALSE(plan_first_hatch_bead(hatch,0,limits).snapshot);
    limits={};limits.maximum_gap_error=Length(1e-8);limits.max_roof_segments=1;REQUIRE_FALSE(plan_first_hatch_bead(hatch,0,limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(plan_first_hatch_bead(hatch,0,limits).snapshot);
    limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(plan_first_hatch_bead(hatch,0,limits).snapshot);
    limits={};limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};
    REQUIRE_FALSE(plan_first_hatch_bead(hatch,0,limits).snapshot);
}

TEST_CASE("B07 first bead excludes future roof and refuses packet and numerical limits", "[Nonplanar][B07][FirstHatchBeads]")
{
    const auto low=bead(1,0,{0,0,1},{10,0,1},1.2,.4,.4,BeadSectionKind::Rectangle);
    const MaterialRecord travel{{2,1,0,{10,0,1},{0,0,2},Speed(10),Acceleration(100),Travel{}},{}};
    const auto future=bead(3,2,{0,0,2},{10,0,2},1.2,.4,.4,BeadSectionKind::Rectangle);
    const auto ledger=captured({low,travel,future});const auto present=material_at(ledger,1,0);
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.001)};
    const auto stack=plan_affine_pass_stack(present.lower,{{1,-.4,3,.4},1.76,1.8,1.76},.9,policy);REQUIRE(stack.snapshot);
    const auto hatch=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),HatchDirection::AlongX});REQUIRE(hatch.snapshot);
    const auto result=plan_first_hatch_bead(hatch,0);INFO(result.reason);REQUIRE(result.snapshot);
    REQUIRE(result.snapshot->source->source->source->completed_records==1);
    for (const auto &piece : result.snapshot->pieces) {
        REQUIRE(std::abs(piece.start.z()-piece.section.gap_begin_mm-1)<=result.snapshot->maximum_gap_error_mm);
        REQUIRE(std::abs(piece.end.z()-piece.section.gap_end_mm-1)<=result.snapshot->maximum_gap_error_mm);
    }
    FirstHatchBeadLimits limits;limits.packets.max_segments=1;REQUIRE_FALSE(plan_first_hatch_bead(hatch,0,limits).snapshot);
    limits={};limits.packets.maximum_volume_error=Volume(1e-20);REQUIRE_FALSE(plan_first_hatch_bead(hatch,0,limits).snapshot);
    limits={};limits.packets.cancelled=[] {return true;};REQUIRE_FALSE(plan_first_hatch_bead(hatch,0,limits).snapshot);
    limits={};limits.packets.is_current=[](uint64_t){return false;};REQUIRE_FALSE(plan_first_hatch_bead(hatch,0,limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
    const auto rounding=plan_first_hatch_bead(hatch,0,limits);REQUIRE(std::fesetround(FE_TONEAREST)==0);REQUIRE_FALSE(rounding.snapshot);
}

TEST_CASE("B07 first footprint plans amounts over the whole finite width on a flat laid roof", "[Nonplanar][B07][FirstHatchFootprint]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle)});
    const auto present=material_at(ledger,1,0);
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.001)};
    const auto stack=plan_affine_pass_stack(present.lower,{{1,-.4,3,.4},1.76,1.8,1.76},.9,policy);REQUIRE(stack.snapshot);
    for (const auto direction : {HatchDirection::AlongX,HatchDirection::AlongY}) {
        auto hatch=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),direction});REQUIRE(hatch.snapshot);
        FirstHatchBeadLimits limits;limits.maximum_gap_error=Length(.0001);limits.packets.maximum_width_error=Length(.002);
        limits.packets.maximum_volume_error=Volume(.00001);
        const auto plan=plan_first_hatch_footprint_bead(hatch,0,limits);INFO(plan.reason);REQUIRE(plan.snapshot);
        REQUIRE(plan.snapshot->roof_domain==FirstHatchRoofDomain::FiniteWidth);
        const auto &line=hatch.snapshot->passes.front().lines.front();
        const long double h0=static_cast<long double>(line.start.z())-1,h1=static_cast<long double>(line.end.z())-1;
        const long double dx=static_cast<long double>(line.end.x())-line.start.x(),dy=static_cast<long double>(line.end.y())-line.start.y();
        const long double k=1-std::acos(-1.L)/4;
        volume_contains(plan.snapshot->actual_target_volume_mm3,std::sqrt(dx*dx+dy*dy)*(.45L*(h0+h1)/2-k*(h0*h0+h0*h1+h1*h1)/3));
        REQUIRE(plan.snapshot->maximum_gap_error_mm<=limits.maximum_gap_error.value());
        REQUIRE(plan.snapshot->maximum_width_error_mm<=limits.packets.maximum_width_error.value());
        const auto centerline=plan_first_hatch_bead(hatch,0,limits);REQUIRE(centerline.snapshot);
        REQUIRE(centerline.snapshot->roof_domain==FirstHatchRoofDomain::Centerline);
        limits.cancelled=[&] {hatch.snapshot.reset();limits.max_roof_segments=1;return false;};
        const auto owned=plan_first_hatch_footprint_bead(hatch,0,limits);INFO(owned.reason);REQUIRE(owned.snapshot);
        REQUIRE(owned.snapshot->source==plan.snapshot->source);
    }
}

TEST_CASE("B07 first footprint retains a transverse ridge that its centerline misses", "[Nonplanar][B07][FirstHatchFootprint]")
{
    const auto floor=bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle);
    const auto ridge=bead(3,2,{0,.075,1.06},{10,.075,1.06},.06,.05,.05,BeadSectionKind::Rectangle);
    const MaterialRecord travel{{2,1,0,floor.motion.end,ridge.motion.start,Speed(10),Acceleration(100),Travel{}},{}};
    const auto ledger=captured({floor,travel,ridge});
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.001)};
    for (const double progress : {0.,.1,.2,.3,1.}) {
        const auto present=material_at(ledger,2,progress);
        const auto stack=plan_affine_pass_stack(present.lower,{{1,-.4,3,.4},1.76,1.8,1.76},.9,policy);INFO(stack.reason);REQUIRE(stack.snapshot);
        const auto hatch=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),HatchDirection::AlongX});REQUIRE(hatch.snapshot);
        const auto &line=hatch.snapshot->passes.front().lines.front();REQUIRE(std::abs(line.start.y()+.125)<1e-12);
        FirstHatchBeadLimits limits;limits.max_roof_segments=32;
        const auto centerline=plan_first_hatch_bead(hatch,0,limits);INFO(centerline.reason);REQUIRE(centerline.snapshot);
        const auto footprint=plan_first_hatch_footprint_bead(hatch,0,limits);INFO(footprint.reason);
        // At .1 the true finite butt is X=1, before this candidate. Future
        // material and the remainder of the current event are absent.
        if (progress<=.1) REQUIRE(footprint.snapshot);
        else REQUIRE_FALSE(footprint.snapshot);
    }
}

TEST_CASE("B07 first footprint refuses unsupported source and exhausted continuous proof", "[Nonplanar][B07][FirstHatchFootprint]")
{
    const auto floor=bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle);
    const MaterialRecord travel{{2,1,0,floor.motion.end,floor.motion.start,Speed(10),Acceleration(100),Travel{}},{}};
    const auto ledger=captured({floor,travel,bead(3,2,floor.motion.start,floor.motion.end,2,.4,.4,BeadSectionKind::Rectangle)});
    const auto present=material_at(ledger,3,0);
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.001)};
    const auto stack=plan_affine_pass_stack(present.lower,{{1,-.4,3,.4},1.76,1.8,1.76},.9,policy);REQUIRE(stack.snapshot);
    const auto hatch=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),HatchDirection::AlongX});REQUIRE(hatch.snapshot);
    REQUIRE(plan_first_hatch_footprint_bead(hatch,0).snapshot);
    const auto boundary=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.0001),HatchDirection::AlongX});REQUIRE(boundary.snapshot);
    REQUIRE_FALSE(plan_first_hatch_footprint_bead(boundary,0).snapshot);
    FirstHatchBeadLimits limits;
    REQUIRE_FALSE(plan_first_hatch_footprint_bead({},0,limits).snapshot);
    REQUIRE_FALSE(plan_first_hatch_footprint_bead(hatch,10000,limits).snapshot);
    limits.max_evaluations=1;REQUIRE_FALSE(plan_first_hatch_footprint_bead(hatch,0,limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(plan_first_hatch_footprint_bead(hatch,0,limits).snapshot);
    limits={};limits.packets.is_current=[](uint64_t){return false;};REQUIRE_FALSE(plan_first_hatch_footprint_bead(hatch,0,limits).snapshot);
    limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(plan_first_hatch_footprint_bead(hatch,0,limits).snapshot);
    limits={};limits.packets.max_segments=1;REQUIRE_FALSE(plan_first_hatch_footprint_bead(hatch,0,limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
    const auto rounding=plan_first_hatch_footprint_bead(hatch,0,limits);REQUIRE(std::fesetround(FE_TONEAREST)==0);REQUIRE_FALSE(rounding.snapshot);
}

TEST_CASE("B07 first footprint refines continuous rounded shoulders without transverse sampling", "[Nonplanar][B07][FirstHatchFootprint]")
{
    const auto row=bead(1,0,{0,0,1},{10,0,1},1.6,.6,.6);const auto ledger=captured({row});const auto present=material_at(ledger,1,0);
    const AffinePassPolicy policy{4,{VerticalGap(.05),VerticalGap(.28),Length(0)},VerticalGap(.08),VerticalGap(.13),NormalGap(.08),NormalGap(.13),Volume(.001)};
    const auto stack=plan_affine_pass_stack(present.lower,{{1,-.75,3,.75},1.43,1.43,1.43},.82,policy);REQUIRE(stack.snapshot);
    const auto hatch=plan_affine_hatches(stack,{WidthXY(.3),Length(.25),Length(.05),HatchDirection::AlongY});REQUIRE(hatch.snapshot);
    FirstHatchBeadLimits limits;limits.maximum_gap_error=Length(.0001);limits.packets.maximum_width_error=Length(.002);
    limits.packets.maximum_volume_error=Volume(.0001);
    const auto plan=plan_first_hatch_footprint_bead(hatch,0,limits);INFO(plan.reason);REQUIRE(plan.snapshot);
    REQUIRE(plan.snapshot->roof_domain==FirstHatchRoofDomain::FiniteWidth);REQUIRE(plan.snapshot->roof_segments>1);
    const auto &line=hatch.snapshot->passes.front().lines.front();
    const long double k=1-std::acos(-1.L)/4,h=row.bead->gap_begin_mm;
    const long double width=std::get<Deposition>(row.motion.payload).volume.value()/10/h+k*h,core=(width-h)/2,radius=h/2;
    const auto area=[&](long double y) {const long double d=std::max(std::abs(y)-core,0.L);
        const long double gap=line.start.z()-(1-h/2+std::sqrt(radius*radius-d*d));return gap*(.3L-k*gap);};
    const long double dy=(static_cast<long double>(line.end.y())-line.start.y())/4096;long double lower=0,upper=0;
    for (size_t i=0;i<4096;++i) {const long double a=area(line.start.y()+dy*i),b=area(line.start.y()+dy*(i+1));
        lower+=std::min(a,b)*dy;upper+=std::max(a,b)*dy;}
    REQUIRE(plan.snapshot->actual_target_volume_mm3.lower<=lower);REQUIRE(plan.snapshot->actual_target_volume_mm3.upper>=upper);
    limits.max_roof_segments=1;REQUIRE_FALSE(plan_first_hatch_footprint_bead(hatch,0,limits).snapshot);
}

TEST_CASE("B07 finite material union distinguishes repeated amount from geometric occupancy", "[Nonplanar][B07][MaterialUnion]")
{
    for (const auto kind : {BeadSectionKind::Rectangle,BeadSectionKind::RoundedRectangle}) {
        const auto first=bead(1,0,{0,0,1},{10,0,1},.8,.2,.2,kind);
        const MaterialRecord travel{{2,1,0,{10,0,1},{0,0,1},Speed(10),Acceleration(100),Travel{}},{}};
        auto second=first;second.motion.event_id=3;second.motion.sequence_index=2;
        const auto ledger=captured({first,travel,second});const SceneBox region{{-1,-1,.7},{11,1,1.1}};
        const auto present=material_at(ledger,3,0);MaterialUnionLimits limits;limits.maximum_interval_width=Volume(.005);
        const auto result=integrate_material_union(present.nominal,region,limits);INFO(result.reason);REQUIRE(result.snapshot);
        const long double expected=std::get<Deposition>(first.motion.payload).volume.value();
        volume_contains(result.snapshot->union_volume_mm3,expected);
        volume_contains(result.snapshot->individual_volume_mm3,2*expected);
        volume_contains(result.snapshot->repeated_volume_mm3,expected);
        REQUIRE(result.snapshot->source==present.nominal.snapshot);REQUIRE(result.snapshot->union_volume_mm3.lower>0);
        REQUIRE(result.snapshot->union_volume_mm3.upper-result.snapshot->union_volume_mm3.lower<=.005);
        const auto prefix=material_at(ledger,1,0);const auto once=integrate_material_union(prefix.nominal,region,limits);REQUIRE(once.snapshot);
        volume_contains(once.snapshot->union_volume_mm3,expected);volume_contains(once.snapshot->individual_volume_mm3,expected);
        REQUIRE(once.snapshot->repeated_volume_mm3.lower==0);REQUIRE(once.snapshot->repeated_volume_mm3.upper<=.005);
    }
}

TEST_CASE("B07 rounded neighboring finite beads match an independent circular lens overlap", "[Nonplanar][B07][MaterialUnion]")
{
    const double pitch=.4;
    const auto first=bead(1,0,{0,0,1},{2,0,1},.45,.2,.2);
    const MaterialRecord travel{{2,1,0,{2,0,1},{0,pitch,1},Speed(10),Acceleration(100),Travel{}},{}};
    const auto second=bead(3,2,{0,pitch,1},{2,pitch,1},.45,.2,.2);
    const auto ledger=captured({first,travel,second});const auto present=material_at(ledger,3,0);
    MaterialUnionLimits limits;limits.maximum_interval_width=Volume(.001);limits.max_cells=65535;
    const auto result=integrate_material_union(present.nominal,{{0,-.3,.7},{2,.7,1.1}},limits);INFO(result.reason);REQUIRE(result.snapshot);
    const long double r=.1L,h=first.bead->gap_begin_mm,k=1-std::acos(-1.L)/4;
    const long double amount=std::get<Deposition>(first.motion.payload).volume.value(),w=amount/2/h+k*h,d=pitch-(w-h);
    const long double repeated=2*(2*r*r*std::acos(d/(2*r))-d/2*std::sqrt(4*r*r-d*d));
    volume_contains(result.snapshot->individual_volume_mm3,2*amount);
    volume_contains(result.snapshot->repeated_volume_mm3,repeated);
    volume_contains(result.snapshot->union_volume_mm3,2*amount-repeated);
    REQUIRE(result.snapshot->repeated_volume_mm3.lower>0);
    REQUIRE(result.snapshot->union_volume_mm3.upper<result.snapshot->individual_volume_mm3.lower);
    // Clip half the finite length and a vertical half-section. Rounded symmetry
    // gives one quarter of every measure, without extending butt ends.
    const auto clipped=integrate_material_union(present.nominal,{{.5,-.3,.9},{1.5,.7,1.1}},limits);INFO(clipped.reason);REQUIRE(clipped.snapshot);
    volume_contains(clipped.snapshot->union_volume_mm3,(2*amount-repeated)/4);
    volume_contains(clipped.snapshot->individual_volume_mm3,amount/2);
    volume_contains(clipped.snapshot->repeated_volume_mm3,repeated/4);
}

TEST_CASE("B07 union volume captures its domain and refuses stale cancelled or exhausted bounds", "[Nonplanar][B07][MaterialUnion]")
{
    const auto ledger=captured({bead(1,0,{0,0,1},{10,0,1},.8,.2,.2)});auto present=material_at(ledger,1,0);
    SceneBox region{{0,-.35,.7},{10,.35,1.1}};MaterialUnionLimits limits;limits.maximum_interval_width=Volume(.005);
    limits.cancelled=[&] {present.nominal.snapshot.reset();region={{0,0,0},{0,0,0}};limits.max_cells=1;return false;};
    const auto owned=integrate_material_union(present.nominal,region,limits);INFO(owned.reason);REQUIRE(owned.snapshot);
    present.nominal.snapshot=owned.snapshot->source;region=owned.snapshot->domain;limits={};
    REQUIRE_FALSE(integrate_material_union({},region,limits).snapshot);
    limits.max_cells=1;REQUIRE_FALSE(integrate_material_union(present.nominal,region,limits).snapshot);
    limits={};limits.max_evaluations=1;REQUIRE_FALSE(integrate_material_union(present.nominal,region,limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(integrate_material_union(present.nominal,region,limits).snapshot);
    limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(integrate_material_union(present.nominal,region,limits).snapshot);
    limits={};limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};
    REQUIRE_FALSE(integrate_material_union(present.nominal,region,limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
    const auto rounding=integrate_material_union(present.nominal,region,limits);REQUIRE(std::fesetround(FE_TONEAREST)==0);REQUIRE_FALSE(rounding.snapshot);
}

TEST_CASE("B07 stitched nominal strips prove finite continuity without filling a missing packet", "[Nonplanar][B07][MaterialUnion]")
{
    for (bool gap : {false,true}) {
        const auto a=bead(1,0,{0,0,1},{1,0,1},.8,.2,.2);
        const MaterialRecord travel{{2,1,0,{1,0,1},{gap ? 2. : 1.,0,1},Speed(10),Acceleration(100),Travel{}},{}};
        const auto b=bead(3,2,{gap ? 2. : 1.,0,1},{gap ? 3. : 2.,0,1},.8,.2,.2);
        const auto ledger=captured({a,travel,b});const auto present=material_at(ledger,3,0);
        MaterialUnionLimits limits;limits.maximum_interval_width=Volume(.002);limits.max_cells=65535;
        const auto result=integrate_material_union(present.nominal,{{0,-1,.7},{3,1,1.1}},limits);INFO(result.reason);REQUIRE(result.snapshot);
        const long double expected=std::get<Deposition>(a.motion.payload).volume.value()+std::get<Deposition>(b.motion.payload).volume.value();
        volume_contains(result.snapshot->union_volume_mm3,expected);volume_contains(result.snapshot->individual_volume_mm3,expected);
        REQUIRE(result.snapshot->repeated_volume_mm3.lower==0);REQUIRE(result.snapshot->repeated_volume_mm3.upper<=.002);
        if (gap) {
            const auto empty=integrate_material_union(present.nominal,{{1,-1,.7},{2,1,1.1}},limits);INFO(empty.reason);REQUIRE(empty.snapshot);
            REQUIRE(empty.snapshot->union_volume_mm3.upper==0);REQUIRE(empty.snapshot->individual_volume_mm3.upper==0);
        }
    }
}

TEST_CASE("B07 sheared integration preserves sloped XY volumes and clips the original XYZ window", "[Nonplanar][B07][MaterialUnion]")
{
    for (auto kind : {BeadSectionKind::Rectangle,BeadSectionKind::RoundedRectangle}) {
        const auto row=bead(1,0,{0,0,1},{10,0,2},.8,.2,.2,kind);const auto ledger=captured({row});const auto present=material_at(ledger,1,0);
        MaterialUnionLimits limits;limits.maximum_interval_width=Volume(.001);limits.max_cells=65535;
        const auto whole=integrate_material_union(present.nominal,{{0,-1,.5},{10,1,2.5}},limits);INFO(whole.reason);REQUIRE(whole.snapshot);
        const long double expected=std::get<Deposition>(row.motion.payload).volume.value();
        volume_contains(whole.snapshot->union_volume_mm3,expected);volume_contains(whole.snapshot->individual_volume_mm3,expected);
        REQUIRE(whole.snapshot->repeated_volume_mm3.upper<=.001);
        if (kind==BeadSectionKind::Rectangle) {
            // Independent trapezoids: vertical overlaps are .1*x on [0,2],
            // .2 on [2,5], and .7-.1*x on [5,7]. Integral is 1 mm2 times width.
            const auto clipped=integrate_material_union(present.nominal,{{0,-1,1},{10,1,1.5}},limits);INFO(clipped.reason);REQUIRE(clipped.snapshot);
            volume_contains(clipped.snapshot->union_volume_mm3,expected/2);
            volume_contains(clipped.snapshot->individual_volume_mm3,expected/2);
        }
    }
}

TEST_CASE("B07 nominal union retains rotated reversed and current finite XY geometry", "[Nonplanar][B07][MaterialUnion]")
{
    for (const auto kind : {BeadSectionKind::Rectangle,BeadSectionKind::RoundedRectangle})
        for (const auto end : {PhysicalPosition(3,4,2),PhysicalPosition(0,-5,2)}) {
            const auto row=bead(1,0,{0,0,1},end,.8,.2,.2,kind);const auto ledger=captured({row});
            const long double amount=std::get<Deposition>(row.motion.payload).volume.value();
            for (double fraction : {.5,1.}) {
                const auto present=material_at(ledger,fraction==1 ? 1 : 0,fraction==1 ? 0 : fraction);
                const auto result=integrate_material_union(present.nominal,{{-1,-6,.5},{4,5,2.5}});INFO(result.reason);REQUIRE(result.snapshot);
                volume_contains(result.snapshot->union_volume_mm3,amount*fraction);
                volume_contains(result.snapshot->individual_volume_mm3,amount*fraction);
                REQUIRE(result.snapshot->repeated_volume_mm3.upper==0);
            }
        }
}

TEST_CASE("B07 triple occupancy counts multiplicity excess without pairwise double counting", "[Nonplanar][B07][MaterialUnion]")
{
    const auto first=bead(1,0,{0,0,1},{1,0,1},.8,.2,.2);
    const MaterialRecord travel{{2,1,0,{1,0,1},{0,0,1},Speed(10),Acceleration(100),Travel{}},{}};
    auto second=first;second.motion.event_id=3;second.motion.sequence_index=2;
    auto back=travel;back.motion.event_id=4;back.motion.sequence_index=3;
    auto third=first;third.motion.event_id=5;third.motion.sequence_index=4;
    const auto ledger=captured({first,travel,second,back,third});
    const long double amount=std::get<Deposition>(first.motion.payload).volume.value();
    MaterialUnionLimits limits;limits.maximum_interval_width=Volume(.002);
    for (double fraction : {.5,1.}) {
        const auto present=material_at(ledger,fraction==1 ? 5 : 4,fraction==1 ? 0 : fraction);
        const auto result=integrate_material_union(present.nominal,{{0,-1,.5},{1,1,1.5}},limits);INFO(result.reason);REQUIRE(result.snapshot);
        volume_contains(result.snapshot->union_volume_mm3,amount);
        volume_contains(result.snapshot->individual_volume_mm3,(2+fraction)*amount);
        volume_contains(result.snapshot->repeated_volume_mm3,(1+fraction)*amount);
        REQUIRE(result.snapshot->repeated_volume_mm3.upper<3*amount);
    }
}

namespace {
MaterialIntegralResult flat_fill_target()
{
    const auto body=captured({bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle)});
    const auto state=material_at(body,1,0);
    auto target=integrate_material_first_pass(state.lower,{{1,-.4,3,.4},1.2,1.2,1.2},.9,{VerticalGap(.1),VerticalGap(.4),Length(0)});
    INFO(target.reason);REQUIRE(target.proof);return target;
}
MaterialUnionResult flat_fill_union(double top,double fraction=1)
{
    const auto row=bead(1,0,{1,0,top},{3,0,top},.8,.2,.2,BeadSectionKind::Rectangle);
    const auto state=material_at(captured({row}),fraction==1 ? 1 : 0,fraction==1 ? 0 : fraction);
    auto result=integrate_material_union(state.nominal,{{1,-.4,.7},{3,.4,1.6}});
    INFO(result.reason);REQUIRE(result.snapshot);return result;
}
}
TEST_CASE("B07 target fill separates equal-total underfill from material outside the cap", "[Nonplanar][B07][MaterialFill]")
{
    const auto target=flat_fill_target();const auto material=flat_fill_union(1.3);
    const auto fit=reconcile_material_fill(target,material);INFO(fit.reason);REQUIRE(fit.snapshot);
    volume_contains(fit.snapshot->target_volume_mm3,2*.8L*.2L);
    volume_contains(fit.snapshot->covered_target_mm3,2*.8L*.1L);
    volume_contains(fit.snapshot->missing_target_mm3,2*.8L*.1L);
    volume_contains(fit.snapshot->outside_target_mm3,2*.8L*.1L);
    volume_contains(fit.snapshot->above_surface_mm3,2*.8L*.1L);
    REQUIRE(fit.snapshot->below_roof_mm3.upper==0);
    REQUIRE(fit.snapshot->missing_target_mm3.lower>.15);REQUIRE(fit.snapshot->outside_target_mm3.lower>.15);
    REQUIRE(fit.snapshot->target==target.proof);REQUIRE(fit.snapshot->occupied==material.snapshot);
}
TEST_CASE("B07 exact fill current fraction and below-roof spill retain separate geometric measures", "[Nonplanar][B07][MaterialFill]")
{
    const auto target=flat_fill_target();
    for (double fraction : {0.,.3,.5,1.}) {
        const auto material=flat_fill_union(1.2,fraction);const auto fit=reconcile_material_fill(target,material);INFO(fit.reason);REQUIRE(fit.snapshot);
        volume_contains(fit.snapshot->covered_target_mm3,2*.8L*.2L*fraction);
        volume_contains(fit.snapshot->missing_target_mm3,2*.8L*.2L*(1-fraction));
        REQUIRE(fit.snapshot->outside_target_mm3.upper<=.001);
    }
    const auto low=flat_fill_union(1.1);const auto spill=reconcile_material_fill(target,low);INFO(spill.reason);REQUIRE(spill.snapshot);
    volume_contains(spill.snapshot->below_roof_mm3,2*.8L*.1L);
    volume_contains(spill.snapshot->missing_target_mm3,2*.8L*.1L);
    REQUIRE(spill.snapshot->above_surface_mm3.upper==0);
}
TEST_CASE("B07 fill reconciliation owns protected inputs and refuses mismatch precision and limits", "[Nonplanar][B07][MaterialFill]")
{
    auto target=flat_fill_target();auto material=flat_fill_union(1.3);MaterialFillLimits limits;
    const auto roof=target.proof;const auto occupied=material.snapshot;
    target.status=MaterialIntegralStatus::Unknown;target.nominal_volume_mm3=ScalarBounds{100,100};
    material.reason="FORGED";material.provisional_union_mm3=ScalarBounds{100,100};
    limits.cancelled=[&] {target.proof.reset();material.snapshot.reset();limits.max_cells=1;return false;};
    const auto owned=reconcile_material_fill(target,material,limits);INFO(owned.reason);REQUIRE(owned.snapshot);
    REQUIRE(owned.snapshot->target==roof);REQUIRE(owned.snapshot->occupied==occupied);
    target.proof=roof;material.snapshot=occupied;limits={};
    REQUIRE_FALSE(reconcile_material_fill({},material,limits).snapshot);REQUIRE_FALSE(reconcile_material_fill(target,{},limits).snapshot);
    limits.max_cells=1;REQUIRE_FALSE(reconcile_material_fill(target,material,limits).snapshot);
    limits={};limits.max_evaluations=1;REQUIRE_FALSE(reconcile_material_fill(target,material,limits).snapshot);
    limits={};limits.maximum_interval_width=Volume(1e-20);REQUIRE_FALSE(reconcile_material_fill(target,material,limits).snapshot);
    limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(reconcile_material_fill(target,material,limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(reconcile_material_fill(target,material,limits).snapshot);
    limits={};limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};
    REQUIRE_FALSE(reconcile_material_fill(target,material,limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};const auto rounding=reconcile_material_fill(target,material,limits);
    REQUIRE(std::fesetround(FE_TONEAREST)==0);REQUIRE_FALSE(rounding.snapshot);
    limits={};const auto row=bead(1,0,{1,0,1.3},{3,0,1.3},.8,.2,.2,BeadSectionKind::Rectangle);
    const auto other=capture_material_sequence({row},model(),24,source_id);REQUIRE(other.snapshot);
    const auto mismatch=integrate_material_union(material_at(other.snapshot,1,0).nominal,occupied->domain);REQUIRE(mismatch.snapshot);
    REQUIRE_FALSE(reconcile_material_fill(target,mismatch,limits).snapshot);
    const auto foreign=capture_material_sequence({row},model(),23,std::string(64,'b'));REQUIRE(foreign.snapshot);
    const auto foreign_union=integrate_material_union(material_at(foreign.snapshot,1,0).nominal,occupied->domain);REQUIRE(foreign_union.snapshot);
    REQUIRE_FALSE(reconcile_material_fill(target,foreign_union,limits).snapshot);
    const auto wrong=integrate_material_union({occupied->source},{{0,-.4,.7},{3,.4,1.6}});REQUIRE(wrong.snapshot);
    REQUIRE_FALSE(reconcile_material_fill(target,wrong,limits).snapshot);
    const auto shallow=integrate_material_union({occupied->source},{{1,-.4,.7},{3,.4,1.1}});REQUIRE(shallow.snapshot);
    REQUIRE_FALSE(reconcile_material_fill(target,shallow,limits).snapshot);
    const auto body=captured({bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle)});
    const auto partial=material_at(body,0,.5);
    const auto partial_target=integrate_material_first_pass(partial.lower,{{1,-.4,3,.4},1.2,1.2,1.2},.9,{VerticalGap(.1),VerticalGap(.4),Length(0)});
    REQUIRE(partial_target.proof);
    const auto derived=capture_material_sequence({row},model(),23,body->fingerprint());REQUIRE(derived.snapshot);
    const auto derived_union=integrate_material_union(material_at(derived.snapshot,1,0).nominal,occupied->domain);REQUIRE(derived_union.snapshot);
    REQUIRE_FALSE(reconcile_material_fill(partial_target,derived_union,limits).snapshot);
}

TEST_CASE("B07 transverse affine cap slope exposes balanced underfill and excess", "[Nonplanar][B07][MaterialFill]")
{
    const auto body=captured({bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle)});
    const auto target=integrate_material_first_pass(material_at(body,1,0).lower,{{1,-.4,3,.4},1.1,1.3,1.1},.9,
        {VerticalGap(.05),VerticalGap(.5),Length(0)});INFO(target.reason);REQUIRE(target.proof);
    // Infill is AlongY; its flat transverse top cannot equal the cap's X slope.
    const auto row=bead(1,0,{2,-.4,1.2},{2,.4,1.2},2,.2,.2,BeadSectionKind::Rectangle);
    const auto state=material_at(captured({row}),1,0);
    const auto occupied=integrate_material_union(state.nominal,{{1,-.4,.7},{3,.4,1.6}});REQUIRE(occupied.snapshot);
    const auto fit=reconcile_material_fill(target,occupied);INFO(fit.reason);REQUIRE(fit.snapshot);
    // Independent triangles: .8 * integral_0^1(.1*x) dx = .04 mm3 on each side.
    volume_contains(fit.snapshot->target_volume_mm3,.32L);
    volume_contains(occupied.snapshot->union_volume_mm3,.32L);
    volume_contains(fit.snapshot->above_surface_mm3,.04L);
    volume_contains(fit.snapshot->missing_target_mm3,.04L);
    volume_contains(fit.snapshot->covered_target_mm3,.28L);
    REQUIRE(fit.snapshot->outside_target_mm3.lower>.039);REQUIRE(fit.snapshot->missing_target_mm3.lower>.039);
}

TEST_CASE("B07 rounded actual roof spill matches an independent circular integral", "[Nonplanar][B07][MaterialFill]")
{
    const auto body=captured({bead(1,0,{0,0,1},{1,0,1},.6,.4,.4)});
    MaterialIntegralLimits accuracy;accuracy.maximum_interval_width=Volume(.0001);
    const auto target=integrate_material_first_pass(material_at(body,1,0).lower,{{.2,.1,.8,.2},1.2,1.2,1.2},.7,
        {VerticalGap(.1),VerticalGap(.6),Length(0)},accuracy);INFO(target.reason);REQUIRE(target.proof);
    const auto row=bead(1,0,{.2,.15,1.1},{.8,.15,1.1},.1,.2,.2,BeadSectionKind::Rectangle);
    const auto occupied=integrate_material_union(material_at(captured({row}),1,0).nominal,{{.2,.1,.7},{.8,.2,1.3}});
    REQUIRE(occupied.snapshot);const auto fit=reconcile_material_fill(target,occupied);INFO(fit.reason);REQUIRE(fit.snapshot);
    const long double r=.2L,y=.1L;
    const long double circle=.5L*(y*std::sqrt(r*r-y*y)+r*r*std::asin(y/r));
    // Roof is .8 + sqrt(r*r-(y-.1)^2). The bead bottom is .9 and top 1.1.
    volume_contains(fit.snapshot->target_volume_mm3,.6L*(.1L*.4L-circle));
    volume_contains(fit.snapshot->below_roof_mm3,.6L*(circle-.1L*.1L));
    volume_contains(fit.snapshot->covered_target_mm3,.6L*(.1L*.3L-circle));
    volume_contains(fit.snapshot->missing_target_mm3,.006L);
    REQUIRE(fit.snapshot->below_roof_mm3.lower>0);REQUIRE(fit.snapshot->above_surface_mm3.upper==0);
}

TEST_CASE("B07 deficit cells locate the unprinted finite prefix without future material", "[Nonplanar][B07][MaterialDeficit]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<MaterialDeficitSnapshot>::value);
    const auto target=flat_fill_target();
    for (double fraction : {0.,.3,.5,1.}) {
        const auto fill=reconcile_material_fill(target,flat_fill_union(1.2,fraction));REQUIRE(fill.snapshot);
        const auto map=locate_material_deficit(fill,{1,2,3},{-.4,0,.4});INFO(map.reason);REQUIRE(map.snapshot);
        REQUIRE(map.snapshot->source==fill.snapshot);REQUIRE(map.snapshot->cells.size()==4);
        volume_contains(map.snapshot->target_volume_mm3,.32L);
        // If x>=2 has no laid material, both cells there must have a positive
        // volume witness. Entire amounts counted at intersections may overstate
        // coverage elsewhere, which is allowed only for a lower-deficit witness.
        for (const auto &cell : map.snapshot->cells) {
            const long double printed=std::max(0.L,std::min(static_cast<long double>(cell.footprint.max_x),1+2*static_cast<long double>(fraction))-cell.footprint.min_x);
            const long double missing=(cell.footprint.max_x-cell.footprint.min_x-printed)*.4L*.2L;
            REQUIRE(cell.missing_lower_mm3>=0);REQUIRE(cell.missing_lower_mm3<=missing+1e-14L);
            if (fraction<=.5 && cell.footprint.min_x==2) REQUIRE(cell.missing_lower_mm3>.031);
            if (fraction==1) REQUIRE(cell.missing_lower_mm3==0);
        }
        REQUIRE(map.snapshot->localized_missing_lower_mm3<=fill.snapshot->missing_target_mm3.upper);
        if (fraction==0) REQUIRE(map.snapshot->localized_missing_lower_mm3>.319);
        if (fraction==1) REQUIRE(map.snapshot->localized_missing_lower_mm3==0);
    }
}
TEST_CASE("B07 deficit witnesses retain protected ownership and reject incomplete grids and exhausted budgets", "[Nonplanar][B07][MaterialDeficit]")
{
    auto fill=reconcile_material_fill(flat_fill_target(),flat_fill_union(1.2,.5));REQUIRE(fill.snapshot);
    const auto source=fill.snapshot;std::vector<double> x{1,2,3},y{-.4,0,.4};MaterialDeficitLimits limits;
    fill.reason="FORGED";fill.provisional_below_roof_mm3=ScalarBounds{100,100};
    limits.cancelled=[&] {fill.snapshot.reset();x.clear();y.clear();limits.max_regions=1;return false;};
    const auto owned=locate_material_deficit(fill,x,y,limits);INFO(owned.reason);REQUIRE(owned.snapshot);
    REQUIRE(owned.snapshot->source==source);REQUIRE(owned.snapshot->cells.size()==4);
    fill.snapshot=source;x={1,2,3};y={-.4,0,.4};limits={};
    REQUIRE_FALSE(locate_material_deficit({},x,y,limits).snapshot);
    REQUIRE_FALSE(locate_material_deficit(fill,{1,2},{-.4,.4},limits).snapshot);
    REQUIRE_FALSE(locate_material_deficit(fill,{1,2,2,3},y,limits).snapshot);
    REQUIRE_FALSE(locate_material_deficit(fill,{1,2,3},{-.5,0,.4},limits).snapshot);
    REQUIRE_FALSE(locate_material_deficit(fill,{1,2,3},{-.4,NAN,.4},limits).snapshot);
    limits.max_regions=1;REQUIRE_FALSE(locate_material_deficit(fill,x,y,limits).snapshot);
    limits={};limits.max_evaluations=1;REQUIRE_FALSE(locate_material_deficit(fill,x,y,limits).snapshot);
    limits={};limits.max_cells=1;REQUIRE_FALSE(locate_material_deficit(fill,x,y,limits).snapshot);
    limits={};limits.maximum_interval_width=Volume(1e-20);REQUIRE_FALSE(locate_material_deficit(fill,x,y,limits).snapshot);
    limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(locate_material_deficit(fill,x,y,limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(locate_material_deficit(fill,x,y,limits).snapshot);
    limits={};limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};
    REQUIRE_FALSE(locate_material_deficit(fill,x,y,limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};const auto rounding=locate_material_deficit(fill,x,y,limits);
    REQUIRE(std::fesetround(FE_TONEAREST)==0);REQUIRE_FALSE(rounding.snapshot);
}

TEST_CASE("B07 deficit localization keeps a rotated current butt and future ledgers separate", "[Nonplanar][B07][MaterialDeficit]")
{
    const auto row=bead(1,0,{1,-.4,1.2},{3,.4,1.2},.15,.2,.2,BeadSectionKind::Rectangle);
    const MaterialRecord travel{{2,1,0,row.motion.end,row.motion.start,Speed(10),Acceleration(100),Travel{}},{}};
    auto future=row;future.motion.event_id=3;future.motion.sequence_index=2;
    const auto prefix=material_at(captured({row,travel,future}),0,.5);
    const auto occupied=integrate_material_union(prefix.nominal,{{1,-.4,.7},{3,.4,1.6}});INFO(occupied.reason);REQUIRE(occupied.snapshot);
    const auto fill=reconcile_material_fill(flat_fill_target(),occupied);INFO(fill.reason);REQUIRE(fill.snapshot);
    const auto map=locate_material_deficit(fill,{1,2,3},{-.4,0,.4});INFO(map.reason);REQUIRE(map.snapshot);
    // The end plane of this half diagonal is 2*(x-2)+.8*y=0.
    // The upper-right quadrant lies beyond it, except for a zero-volume corner.
    const auto &cell=map.snapshot->cells.back();REQUIRE(cell.footprint.min_x==2);REQUIRE(cell.footprint.min_y==0);
    REQUIRE(cell.candidate_amount_upper_mm3==0);REQUIRE(cell.covered_upper_mm3==0);
    REQUIRE(cell.missing_lower_mm3>.079);REQUIRE(cell.missing_lower_mm3<=.080000000001);
    REQUIRE(map.snapshot->localized_missing_lower_mm3<=fill.snapshot->missing_target_mm3.upper);
}

namespace {
struct RemainderFixture {AffineHatchResult hatches;MaterialFillResult fill;};
RemainderFixture remainder_fixture(double progress,HatchDirection direction=HatchDirection::AlongX,bool transverse=false,bool sloped=false)
{
    const auto body=captured({bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle)});
    const auto present=material_at(body,1,0);
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.001)};
    const auto stack=plan_affine_pass_stack(present.lower,{{1,-.4,3,.4},1.8,sloped ? 1.84 : 1.8,1.8},.9,policy);REQUIRE(stack.snapshot);
    const auto hatches=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),direction});REQUIRE(hatches.snapshot);
    const auto &line=hatches.snapshot->passes.front().lines.front();
    const double h=line.start.z()-1;
    auto row=bead(1,0,line.start,line.end,.45,h,h);
    if (transverse) row=bead(1,0,{line.start.x(),line.start.y()+.24,line.start.z()-.1},
        {line.end.x(),line.end.y()+.24,line.end.z()-.1},.02,.02,.02,BeadSectionKind::Rectangle);
    const auto cap=captured({row});const auto prefix=material_at(cap,0,progress);
    MaterialUnionLimits volume;volume.maximum_interval_width=Volume(.0001);
    const auto occupied=integrate_material_union(prefix.nominal,{{1,-.4,.7},{3,.4,1.8}},volume);INFO(occupied.reason);REQUIRE(occupied.snapshot);
    const auto fill=reconcile_material_fill(stack.snapshot->first_pass,occupied);INFO(fill.reason);REQUIRE(fill.snapshot);
    return {hatches,fill};
}
}
TEST_CASE("B07 remainder hatch constructs finite paths after the actual current butt and proves added fill", "[Nonplanar][B07][RemainderHatch]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<RemainingHatchSnapshot>::value);
    for (const auto direction : {HatchDirection::AlongX,HatchDirection::AlongY}) for (double progress : {0.,.3,.5}) {
        const auto f=remainder_fixture(progress,direction);
        const auto result=plan_remaining_first_hatch(f.hatches,0,f.fill);INFO(result.reason);REQUIRE(result.snapshot);
        const auto &r=*result.snapshot;REQUIRE(r.before==f.fill.snapshot);REQUIRE(r.paths.size()==1);
        const auto &path=*r.paths.front();const auto &line=f.hatches.snapshot->passes.front().lines.front();
        REQUIRE(path.roof_domain==FirstHatchRoofDomain::FiniteWidth);
        const bool x=direction==HatchDirection::AlongX;
        const double first=x ? line.start.x() : line.start.y(),last=x ? line.end.x() : line.end.y();
        const double a=x ? path.path_start.x() : path.path_start.y(),b=x ? path.path_end.x() : path.path_end.y();
        if (progress==0) REQUIRE(a>=first);
        else REQUIRE(a>=first+(last-first)*progress+.02-1e-12); // .01 outer growth + .01 separation.
        REQUIRE(b<=last);REQUIRE(r.covered_target_mm3.lower>r.before->covered_target_mm3.upper);
        REQUIRE(r.missing_target_mm3.upper<r.before->missing_target_mm3.lower);
        REQUIRE(r.covered_gain_lower_mm3>0);REQUIRE(r.outside_target_mm3.upper<=.001);
        REQUIRE(r.before->occupied->source->current_progress==progress);
        // Independent constant-gap rounded area over the accepted exact endpoints.
        const long double h=static_cast<long double>(line.start.z())-1,k=1-std::acos(-1.L)/4;
        const long double amount=(static_cast<long double>(b)-a)*h*(.45L-k*h);
        REQUIRE(r.added->covered_target_mm3.lower<=amount);REQUIRE(r.added->covered_target_mm3.upper>=amount);
    }
}
TEST_CASE("B07 remainder hatch retains upper transverse neighbours and refuses exhausted or incompatible proofs", "[Nonplanar][B07][RemainderHatch]")
{
    const auto transverse=remainder_fixture(1,HatchDirection::AlongX,true);
    REQUIRE_FALSE(plan_remaining_first_hatch(transverse.hatches,0,transverse.fill).snapshot);
    const auto full=remainder_fixture(1);REQUIRE_FALSE(plan_remaining_first_hatch(full.hatches,0,full.fill).snapshot);
    auto f=remainder_fixture(.3);RemainingHatchLimits limits;
    const auto source=f.fill.snapshot;const auto hatch=f.hatches.snapshot;
    limits.cancelled=[&] {f.fill.snapshot.reset();f.hatches.snapshot.reset();limits.max_paths=0;return false;};
    const auto owned=plan_remaining_first_hatch(f.hatches,0,f.fill,{},limits);INFO(owned.reason);REQUIRE(owned.snapshot);
    REQUIRE(owned.snapshot->before==source);f.fill.snapshot=source;f.hatches.snapshot=hatch;limits={};
    REQUIRE_FALSE(plan_remaining_first_hatch({},0,f.fill).snapshot);
    REQUIRE_FALSE(plan_remaining_first_hatch(f.hatches,0,{}).snapshot);
    REQUIRE_FALSE(plan_remaining_first_hatch(f.hatches,10000,f.fill).snapshot);
    limits.max_evaluations=1;REQUIRE_FALSE(plan_remaining_first_hatch(f.hatches,0,f.fill,{},limits).snapshot);
    limits={};limits.volumes.max_cells=1;REQUIRE_FALSE(plan_remaining_first_hatch(f.hatches,0,f.fill,{},limits).snapshot);
    limits={};limits.beads.packets.max_segments=0;REQUIRE_FALSE(plan_remaining_first_hatch(f.hatches,0,f.fill,{},limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(plan_remaining_first_hatch(f.hatches,0,f.fill,{},limits).snapshot);
    limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(plan_remaining_first_hatch(f.hatches,0,f.fill,{},limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
    const auto rounding=plan_remaining_first_hatch(f.hatches,0,f.fill,{},limits);REQUIRE(std::fesetround(FE_TONEAREST)==0);REQUIRE_FALSE(rounding.snapshot);
    REQUIRE_FALSE(plan_remaining_first_hatch(f.hatches,0,f.fill,{Length(.01),Volume(.001),Volume(1)}).snapshot);
    const auto other=remainder_fixture(.3);REQUIRE_FALSE(plan_remaining_first_hatch(f.hatches,0,other.fill).snapshot);
    const auto sloped=remainder_fixture(0,HatchDirection::AlongY,false,true);
    REQUIRE_FALSE(plan_remaining_first_hatch(sloped.hatches,0,sloped.fill,{Length(.01),Volume(.000001),Volume(.001)}).snapshot);
}

TEST_CASE("B07 remainder hatch preserves middle obstacles and shares packet and path limits", "[Nonplanar][B07][RemainderHatch]")
{
    const auto f=remainder_fixture(0);const auto &line=f.hatches.snapshot->passes.front().lines.front();
    const double length=line.end.x()-line.start.x(),z=line.start.z();
    const auto cap=captured({bead(1,0,{line.start.x()+.4*length,line.start.y(),z},{line.start.x()+.6*length,line.start.y(),z},.45,z-1,z-1)});
    const auto prefix=material_at(cap,1,0);const auto occupied=integrate_material_union(prefix.nominal,{{1,-.4,.7},{3,.4,1.8}});REQUIRE(occupied.snapshot);
    const auto fit=reconcile_material_fill(f.hatches.snapshot->source->first_pass,occupied);REQUIRE(fit.snapshot);
    const auto planned=plan_remaining_first_hatch(f.hatches,0,fit);INFO(planned.reason);REQUIRE(planned.snapshot);
    REQUIRE(planned.snapshot->paths.size()==2);REQUIRE(planned.snapshot->covered_gain_lower_mm3>0);
    REQUIRE(planned.snapshot->paths.front()->path_end.x()<line.start.x()+.4*length);
    REQUIRE(planned.snapshot->paths.back()->path_start.x()>line.start.x()+.6*length);
    RemainingHatchLimits limits;limits.max_paths=1;REQUIRE_FALSE(plan_remaining_first_hatch(f.hatches,0,fit,{},limits).snapshot);
    limits={};limits.beads.packets.max_segments=1;REQUIRE_FALSE(plan_remaining_first_hatch(f.hatches,0,fit,{},limits).snapshot);
}

namespace {
MaterialIntegralResult stadium_fill_target(double end_z)
{
    const auto body=captured({bead(1,0,{0,0,1},{10,0,1},4,.4,.4,BeadSectionKind::Rectangle)});
    const auto state=material_at(body,1,0);
    const auto target=integrate_material_first_pass(state.lower,{{1,-1,3,1},1.2,end_z,1.2},.9,
        {VerticalGap(.1),VerticalGap(.6),Length(0)});
    INFO(target.reason);REQUIRE(target.proof);return target;
}
}
TEST_CASE("B07 variable stadium depth cannot create material above its affine top or below its flat floor", "[Nonplanar][B07][StadiumDepthBounds]")
{
    const auto target=stadium_fill_target(1.4);
    const auto row=bead(1,0,{1,0,1.2},{3,0,1.4},.8,.2,.4);
    const auto cap=captured({row});
    for (double fraction : {.3,1.}) {
        const auto present=material_at(cap,fraction==1 ? 1 : 0,fraction==1 ? 0 : fraction);
        const auto occupied=integrate_material_union(present.nominal,{{1,-1,.7},{3,1,1.6}});
        INFO(occupied.reason);REQUIRE(occupied.snapshot);
        MaterialFillLimits limits;limits.maximum_interval_width=Volume(.00002);limits.max_cells=2;
        const auto fill=reconcile_material_fill(target,occupied,limits);INFO(fill.reason);REQUIRE(fill.snapshot);
        REQUIRE(fill.snapshot->below_roof_mm3.upper<1e-10);
        REQUIRE(fill.snapshot->above_surface_mm3.upper<1e-10);
        // Every real stadium section has nonnegative depth below its axis top.
        // Its floor is 1 + depth, so the complete current/full amount lies in T.
        const long double amount=static_cast<long double>(std::get<Deposition>(row.motion.payload).volume.value())*fraction;
        volume_contains(fill.snapshot->covered_target_mm3,amount);
        REQUIRE(fill.snapshot->cells==2);
    }
}
TEST_CASE("B07 stadium depth tightening retains genuine rounded material above the target", "[Nonplanar][B07][StadiumDepthBounds]")
{
    const auto target=stadium_fill_target(1.2);
    const auto row=bead(1,0,{1,0,1.21},{3,0,1.21},.8,.2,.2);
    const auto present=material_at(captured({row}),1,0);
    const auto occupied=integrate_material_union(present.nominal,{{1,-1,.7},{3,1,1.6}});REQUIRE(occupied.snapshot);
    MaterialFillLimits limits;limits.maximum_interval_width=Volume(.0001);
    const auto fill=reconcile_material_fill(target,occupied,limits);INFO(fill.reason);REQUIRE(fill.snapshot);
    // Independent bounds: 2 mm length times .01 mm spill height, between
    // the .6 mm flat core and the full .8 mm width (strictly rounded sides).
    REQUIRE(fill.snapshot->above_surface_mm3.lower>.012);
    REQUIRE(fill.snapshot->above_surface_mm3.upper<.016);
    REQUIRE(fill.snapshot->below_roof_mm3.upper<1e-10);
    REQUIRE(fill.snapshot->covered_target_mm3.upper<occupied.snapshot->union_volume_mm3.lower);
}

namespace {
struct FlatRoofFixture {MaterialIntegralResult target;MaterialUnionResult occupied;long double amount,below,above;};
FlatRoofFixture flat_roof_fixture(double fraction,bool raised=false)
{
    const auto body=bead(1,0,{0,0,1},{10,0,1},.8,.2,.2);
    const MaterialRecord travel{{2,1,0,body.motion.end,{0,0,1.1},Speed(20),Acceleration(100),Travel{}},{}};
    const auto future=bead(3,2,{0,0,1.1},{10,0,1.1},.8,.2,.2);
    const auto laid=material_at(captured({body,travel,future}),1,0);
    MaterialIntegralLimits target_limits;target_limits.maximum_interval_width=Volume(.0001);
    target_limits.max_cells=65535;target_limits.timeout=std::chrono::seconds(5);
    const auto target=integrate_material_first_pass(laid.lower,{{1,-.34,3,.34},1.2,1.2,1.2},.85,
        {VerticalGap(.1),VerticalGap(.4),Length(0)},target_limits);
    INFO(target.reason);REQUIRE(target.proof);REQUIRE(target.cells>1);
    const double top=raised ? 1.205 : 1.195;
    const auto row=bead(1,0,{1,0,top},{3,0,top},.66,.2,.2,BeadSectionKind::Rectangle);
    const auto cap=material_at(captured({row}),fraction==1 ? 1 : 0,fraction==1 ? 0 : fraction);
    const auto occupied=integrate_material_union(cap.nominal,{{1,-.34,.8},{3,.34,1.4}});REQUIRE(occupied.snapshot);
    // Independent circle-segment integral over the actual rounded body's
    // shoulders. Its future raised row is absent from this target prefix.
    const long double h=body.bead->gap_begin_mm,r=h/2,k=1-std::acos(-1.L)/4;
    const long double area=std::get<Deposition>(body.motion.payload).volume.value()/10.L,core=(area/h+k*h-h)/2;
    const long double cap_area=std::get<Deposition>(row.motion.payload).volume.value()/2.L;
    const long double width=cap_area/row.bead->gap_begin_mm,floor=static_cast<long double>(top)-row.bead->gap_begin_mm;
    const long double offset=floor-(1-r),length=2.L*fraction;
    long double below=0;
    if (floor<1) {
        const long double u=std::min(width/2-core,std::sqrt(r*r-offset*offset));
        const long double shoulder=(u*std::sqrt(r*r-u*u)+r*r*std::asin(u/r))/2-offset*u;
        below=length*(2*core*(1-floor)+2*shoulder);
    }
    return {target,occupied,length*cap_area,below,raised ? length*width*(static_cast<long double>(top)-1.2) : 0};
}
}
TEST_CASE("B07 flat roof queries retain curved shoulders for current material at both precisions", "[Nonplanar][B07][FlatRoofQueries]")
{
    for (double fraction : {.3,1.}) for (bool raised : {false,true}) {
        auto f=flat_roof_fixture(fraction,raised);
        for (double precision : {.01,.0004}) {
            MaterialFillLimits limits;limits.maximum_interval_width=Volume(precision);limits.max_cells=65535;
            const auto fill=reconcile_material_fill(f.target,f.occupied,limits);INFO(fill.reason);REQUIRE(fill.snapshot);
            volume_contains(fill.snapshot->below_roof_mm3,f.below);volume_contains(fill.snapshot->above_surface_mm3,f.above);
            volume_contains(fill.snapshot->covered_target_mm3,f.amount-f.below-f.above);
            REQUIRE(fill.snapshot->occupied==f.occupied.snapshot);REQUIRE(fill.snapshot->target==f.target.proof);
        }
    }
}
TEST_CASE("B07 flat roof queries preserve owned sources and shared interruption budgets", "[Nonplanar][B07][FlatRoofQueries]")
{
    auto f=flat_roof_fixture(.3);const auto source=f.target.proof;const auto occupied=f.occupied.snapshot;
    MaterialFillLimits limits;
    limits.cancelled=[&] {f.target.proof.reset();f.target.nominal_volume_mm3=ScalarBounds{0,0};f.occupied.snapshot.reset();return false;};
    const auto owned=reconcile_material_fill(f.target,f.occupied,limits);INFO(owned.reason);REQUIRE(owned.snapshot);
    REQUIRE(owned.snapshot->target==source);REQUIRE(owned.snapshot->occupied==occupied);
    volume_contains(owned.snapshot->below_roof_mm3,f.below);
    f.target.proof=source;f.occupied.snapshot=occupied;
    limits={};limits.max_evaluations=1;REQUIRE_FALSE(reconcile_material_fill(f.target,f.occupied,limits).snapshot);
    limits={};limits.max_cells=1;REQUIRE_FALSE(reconcile_material_fill(f.target,f.occupied,limits).snapshot);
    size_t polls=0;limits={};limits.cancelled=[&] {return ++polls==8;};
    REQUIRE_FALSE(reconcile_material_fill(f.target,f.occupied,limits).snapshot);REQUIRE(polls==8);
    limits={};limits.is_current=[](uint64_t) {return false;};REQUIRE_FALSE(reconcile_material_fill(f.target,f.occupied,limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_UPWARD);return false;};
    const auto rounding=reconcile_material_fill(f.target,f.occupied,limits);
    REQUIRE(std::fesetround(FE_TONEAREST)==0);REQUIRE_FALSE(rounding.snapshot);
}

TEST_CASE("B07 flat roof queries retain reverse diagonal current support for both cap axes", "[Nonplanar][B07][FlatRoofQueries]")
{
    for (bool reverse : {false,true}) for (bool x_axis : {false,true}) {
        const auto body=bead(1,0,reverse ? PhysicalPosition(10,10,1) : PhysicalPosition(0,0,1),
            reverse ? PhysicalPosition(0,0,1) : PhysicalPosition(10,10,1),.8,.2,.2);
        const auto laid=material_at(captured({body}),0,.55);
        MaterialIntegralLimits target_limits;target_limits.maximum_interval_width=Volume(.0001);
        const auto target=integrate_material_first_pass(laid.lower,{{4.76,4.76,5.24,5.24},1.2,1.2,1.2},.85,
            {VerticalGap(.1),VerticalGap(.4),Length(0)},target_limits);INFO(target.reason);REQUIRE(target.proof);REQUIRE(target.cells>1);
        const auto cap=bead(1,0,x_axis ? PhysicalPosition(4.8,5,1.195) : PhysicalPosition(5,4.8,1.195),
            x_axis ? PhysicalPosition(5.2,5,1.195) : PhysicalPosition(5,5.2,1.195),.44,.2,.2,BeadSectionKind::Rectangle);
        const auto present=material_at(captured({cap}),0,.4);
        const auto occupied=integrate_material_union(present.nominal,{{4.76,4.76,.8},{5.24,5.24,1.4}});REQUIRE(occupied.snapshot);
        const auto fill=reconcile_material_fill(target,occupied);INFO(fill.reason);REQUIRE(fill.snapshot);
        // The complete cap footprint has |normal| <= .42/sqrt(2) < .3,
        // inside the diagonal body's flat core despite curved target corners.
        const long double amount=static_cast<long double>(std::get<Deposition>(cap.motion.payload).volume.value())*.4;
        const long double floor=static_cast<long double>(cap.motion.start.z())-cap.bead->gap_begin_mm;
        const long double below=amount*(1-floor)/cap.bead->gap_begin_mm;
        volume_contains(fill.snapshot->below_roof_mm3,below);volume_contains(fill.snapshot->covered_target_mm3,amount-below);
        REQUIRE(fill.snapshot->above_surface_mm3.upper<1e-10);
    }
}

TEST_CASE("B07 flat roof queries exclude a higher rectangular neighbour outside the complete footprint", "[Nonplanar][B07][FlatRoofQueries]")
{
    const auto body=bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle);
    const MaterialRecord travel{{2,1,0,body.motion.end,{0,.8,1.15},Speed(20),Acceleration(100),Travel{}},{}};
    const auto ridge=bead(3,2,{0,.8,1.15},{10,.8,1.15},.1,.05,.05,BeadSectionKind::Rectangle);
    const auto present=material_at(captured({body,travel,ridge}),3,0);
    const auto target=integrate_material_first_pass(present.lower,{{1,-.4,3,.4},1.2,1.2,1.2},.9,
        {VerticalGap(.1),VerticalGap(.4),Length(0)});INFO(target.reason);REQUIRE(target.proof);
    volume_contains(*target.nominal_volume_mm3,2.L*(2.L*.4)*(static_cast<long double>(double(1.2))-1));
}

TEST_CASE("B07 complete first hatch layer measures rounded overlap rather than summing line coverage", "[Nonplanar][B07][FirstHatchLayer]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<FirstHatchLayerSnapshot>::value);
    for (auto direction : {HatchDirection::AlongX,HatchDirection::AlongY}) {
        const auto f=remainder_fixture(0,direction);
        const auto result=plan_first_hatch_layer(f.hatches,{{1,-.4,.7},{3,.4,1.8}});INFO(result.reason);REQUIRE(result.snapshot);
        const auto &layer=*result.snapshot;const auto &lines=f.hatches.snapshot->passes.front().lines;
        REQUIRE(layer.source==f.hatches.snapshot);REQUIRE(layer.paths.size()==lines.size());REQUIRE(layer.paths.size()>1);
        const bool x=direction==HatchDirection::AlongX;
        long double sum=0,overlap=0,previous=0,first_width=0,first_height=0,length=0;
        for (size_t i=0;i<layer.paths.size();++i) {
            const auto &path=*layer.paths[i];REQUIRE(path.line_index==i);REQUIRE(path.source==layer.source);
            REQUIRE(path.roof_domain==FirstHatchRoofDomain::FiniteWidth);REQUIRE(path.pieces.size()==1);
            const auto &piece=path.pieces.front();const long double h=piece.section.gap_begin_mm;
            REQUIRE(piece.section.gap_end_mm==h);
            const long double l=x ? static_cast<long double>(piece.end.x())-piece.start.x() : static_cast<long double>(piece.end.y())-piece.start.y();
            const long double area=piece.volume.value()/l,k=1-std::acos(-1.L)/4,w=area/h+k*h;
            const long double center=x ? piece.start.y() : piece.start.x();sum+=piece.volume.value();
            if (i==0) {first_width=w;first_height=h;length=l;}
            else {
                REQUIRE(w==first_width);REQUIRE(h==first_height);REQUIRE(l==length);
                const long double d=center-previous,core=w-h,r=h/2,q=d-core;
                REQUIRE(d>0);if (i>1) REQUIRE(2*d>w); // No triple intersection in this fixture.
                const long double shared=q<=0 ? area-h*d : q>=h ? 0 :
                    2*r*r*std::acos(q/(2*r))-q*std::sqrt(4*r*r-q*q)/2;
                overlap+=l*shared;
            }
            previous=center;
        }
        const auto &fill=*layer.fill;const auto &occupied=*fill.occupied;
        volume_contains(occupied.individual_volume_mm3,sum);volume_contains(occupied.union_volume_mm3,sum-overlap);
        volume_contains(occupied.repeated_volume_mm3,overlap);volume_contains(fill.covered_target_mm3,sum-overlap);
        REQUIRE(occupied.repeated_volume_mm3.lower>0);REQUIRE(fill.missing_target_mm3.lower>0);
        REQUIRE(fill.outside_target_mm3.upper<=.001);REQUIRE(layer.global_volume_error_mm3<=.001);
        REQUIRE(fill.occupied->source->completed_records==fill.occupied->source->sequence->records.size());
    }
}
TEST_CASE("B07 complete first hatch layer captures shared budgets and refuses partial or clipped candidates", "[Nonplanar][B07][FirstHatchLayer]")
{
    auto f=remainder_fixture(0);const auto source=f.hatches.snapshot;const SceneBox box{{1,-.4,.7},{3,.4,1.8}};
    FirstHatchLayerLimits limits;
    limits.cancelled=[&] {f.hatches.snapshot.reset();limits.max_paths=0;return false;};
    const auto owned=plan_first_hatch_layer(f.hatches,box,limits);INFO(owned.reason);REQUIRE(owned.snapshot);
    REQUIRE(owned.snapshot->source==source);f.hatches.snapshot=source;
    REQUIRE_FALSE(plan_first_hatch_layer({},box).snapshot);
    REQUIRE_FALSE(plan_first_hatch_layer(f.hatches,{{1,-.4,1.1},{3,.4,1.8}}).snapshot);
    REQUIRE_FALSE(plan_first_hatch_layer(f.hatches,{{1.1,-.4,.7},{3,.4,1.8}}).snapshot);
    limits={};limits.max_paths=1;REQUIRE_FALSE(plan_first_hatch_layer(f.hatches,box,limits).snapshot);
    limits={};limits.max_records=1;REQUIRE_FALSE(plan_first_hatch_layer(f.hatches,box,limits).snapshot);
    limits={};limits.beads.packets.max_segments=1;REQUIRE_FALSE(plan_first_hatch_layer(f.hatches,box,limits).snapshot);
    limits={};limits.beads.max_roof_segments=1;REQUIRE_FALSE(plan_first_hatch_layer(f.hatches,box,limits).snapshot);
    limits={};limits.max_evaluations=1;REQUIRE_FALSE(plan_first_hatch_layer(f.hatches,box,limits).snapshot);
    limits={};limits.max_cells=1;REQUIRE_FALSE(plan_first_hatch_layer(f.hatches,box,limits).snapshot);
    limits={};limits.volumes.max_depth=0;REQUIRE_FALSE(plan_first_hatch_layer(f.hatches,box,limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(plan_first_hatch_layer(f.hatches,box,limits).snapshot);
    size_t polls=0;limits={};limits.cancelled=[&] {return ++polls==8;};
    REQUIRE_FALSE(plan_first_hatch_layer(f.hatches,box,limits).snapshot);REQUIRE(polls==8);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
    const auto rounding=plan_first_hatch_layer(f.hatches,box,limits);
    REQUIRE(std::fesetround(FE_TONEAREST)==0);REQUIRE_FALSE(rounding.snapshot);
    const auto floor=bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle);
    const auto ridge=bead(3,2,{0,.3,1.06},{10,.3,1.06},.06,.05,.05,BeadSectionKind::Rectangle);
    const MaterialRecord travel{{2,1,0,floor.motion.end,ridge.motion.start,Speed(10),Acceleration(100),Travel{}},{}};
    const auto ledger=captured({floor,travel,ridge});
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.0001)};
    for (double progress : {0.,.1,.3,1.}) {
        const auto present=material_at(ledger,2,progress);
        const auto stack=plan_affine_pass_stack(present.lower,{{1,-.4,3,.4},1.8,1.8,1.8},.9,policy);REQUIRE(stack.snapshot);
        const auto hatch=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),HatchDirection::AlongX});REQUIRE(hatch.snapshot);
        limits={};limits.beads.max_roof_segments=32;
        REQUIRE(plan_first_hatch_footprint_bead(hatch,0,limits.beads).snapshot); // The first line misses the ridge.
        const auto layer=plan_first_hatch_layer(hatch,box,limits);INFO(layer.reason);
        // Future material and the current event after its true butt are absent.
        // Once the ridge intersects the second line, refuse the whole candidate.
        if (progress<=.1) REQUIRE(layer.snapshot);
        else REQUIRE_FALSE(layer.snapshot);
    }
}

TEST_CASE("B07 congruent rounded packet pairs integrate affine gaps and preserve missing longitudinal space", "[Nonplanar][B07][MaterialUnion][CongruentUnion]")
{
    using Amount=boost::multiprecision::cpp_bin_float_quad;
    for (bool x : {false,true}) for (bool reverse : {false,true}) {
        std::vector<MaterialRecord> rows;Amount one=0,extra=0;
        const auto point=[&](double along,double normal,double z) {return x ? PhysicalPosition(along,normal,z) : PhysicalPosition(normal,along,z);};
        for (double center : {0.,.125}) for (size_t n=0;n<2;++n) {
            const double a=n ? 3. : 0.,b=n ? 5. : 1.,h0=n ? .25 : .2,h1=n ? .3 : .25,z0=n ? 1.8 : 1.,z1=n ? 2.4 : 1.5;
            const auto start=point(reverse ? b : a,center,reverse ? z1 : z0),end=point(reverse ? a : b,center,reverse ? z0 : z1);
            if (!rows.empty()) {const size_t i=rows.size();rows.push_back({{i+1,i,0,rows.back().motion.end,start,Speed(10),Acceleration(100),Travel{}},{}});}
            const size_t i=rows.size();rows.push_back(bead(i+1,i,start,end,.9,reverse ? h1 : h0,reverse ? h0 : h1));
            if (center==0) {one+=std::get<Deposition>(rows.back().motion.payload).volume.value();extra+=Amount(.125)*(Amount(b)-a)*(Amount(h0)+h1)/2;}
        }
        const auto ledger=captured(rows);const auto prefix=material_at(ledger,rows.size(),0);
        MaterialUnionLimits limits;limits.max_cells=2;limits.maximum_interval_width=Volume(1e-12);
        const SceneBox box{{-2,-2,-1},{6,6,4}};
        const auto result=integrate_material_union(prefix.nominal,box,limits);INFO(result.reason);REQUIRE(result.snapshot);
        volume_contains(result.snapshot->individual_volume_mm3,(2*one).convert_to<long double>());
        volume_contains(result.snapshot->union_volume_mm3,(one+extra).convert_to<long double>());
        volume_contains(result.snapshot->repeated_volume_mm3,(one-extra).convert_to<long double>());REQUIRE(result.cells==2);
        limits.max_cells=1;REQUIRE_FALSE(integrate_material_union(prefix.nominal,box,limits).snapshot);
        limits.max_cells=2;
        const auto empty=integrate_material_union(prefix.nominal,x ? SceneBox{{1,-2,-1},{3,2,4}} : SceneBox{{-2,1,-1},{2,3,4}},limits);
        REQUIRE(empty.snapshot);REQUIRE(empty.snapshot->union_volume_mm3.upper==0);
        // Different profiles require the general solver; never reuse the exact identity.
        const auto &last=rows.back();
        rows.back()=bead(last.motion.event_id,last.motion.sequence_index,last.motion.start,last.motion.end,.91,last.bead->gap_begin_mm,last.bead->gap_end_mm);
        const auto changed=captured(rows);const auto changed_prefix=material_at(changed,rows.size(),0);
        REQUIRE_FALSE(integrate_material_union(changed_prefix.nominal,box,limits).snapshot);
    }
}

TEST_CASE("B07 whole first-hatch end replan reduces measured deficit without appending old future material", "[Nonplanar][B07][FirstHatchEndReplan]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<FirstHatchReplanSnapshot>::value);
    for (auto direction : {HatchDirection::AlongX,HatchDirection::AlongY}) {
        const auto f=remainder_fixture(0,direction);const SceneBox box{{1,-.4,.7},{3,.4,1.8}};
        const auto before=plan_first_hatch_layer(f.hatches,box);REQUIRE(before.snapshot);
        const auto result=replan_first_hatch_ends(before);INFO(result.reason);REQUIRE(result.snapshot);
        const auto &r=*result.snapshot;REQUIRE(r.before==before.snapshot);REQUIRE(r.after->source!=r.before->source);
        REQUIRE(r.after->source->source==r.before->source->source);REQUIRE(r.after->fill->target==r.before->fill->target);
        REQUIRE(r.before->source->first_pass_extent==FirstHatchExtent::CapsuleInset);
        REQUIRE(r.after->source->first_pass_extent==FirstHatchExtent::FiniteButtInset);
        REQUIRE(r.after->source->policy.boundary_band.value()==r.before->source->policy.boundary_band.value());
        REQUIRE(r.after->paths.size()==r.before->paths.size());
        REQUIRE(r.covered_gain_mm3.lower>.001);REQUIRE(r.missing_reduction_mm3.lower>.001);
        REQUIRE(r.after->fill->outside_target_mm3.upper<=.001);REQUIRE(r.after->fill->missing_target_mm3.lower>0);
        REQUIRE(r.after->fill->occupied->source->sequence!=r.before->fill->occupied->source->sequence);
        const bool x=direction==HatchDirection::AlongX;
        const auto coordinates=[](PhysicalPosition p) {return std::array<double,3>{p.x(),p.y(),p.z()};};
        const auto axis=[&](PhysicalPosition p) {return x ? p.x() : p.y();};
        const auto center=[&](PhysicalPosition p) {return x ? p.y() : p.x();};
        const auto old_length=axis(r.before->paths[0]->path_end)-axis(r.before->paths[0]->path_start);
        const auto new_length=axis(r.after->paths[0]->path_end)-axis(r.after->paths[0]->path_start);
        REQUIRE(new_length>old_length);
        for (size_t i=0;i<r.after->paths.size();++i) {
            const auto &old=*r.before->paths[i],&next=*r.after->paths[i];
            REQUIRE(center(next.path_start)==center(old.path_start));REQUIRE(center(next.path_end)==center(old.path_end));
            REQUIRE(axis(next.path_start)<axis(old.path_start));REQUIRE(axis(next.path_end)>axis(old.path_end));
            REQUIRE(next.source==r.after->source);REQUIRE(coordinates(next.path_start)==coordinates(r.after->source->passes.front().lines[i].start));
            REQUIRE(coordinates(next.path_end)==coordinates(r.after->source->passes.front().lines[i].end));
        }
        // Independent stadium measures use actual new binary amounts; ideal
        // area scaling would incorrectly assume identical packet rounding.
        using Amount=boost::multiprecision::cpp_bin_float_quad;
        Amount sum=0,repeated=0,previous=0,area=0,h=0,w=0,length=0;
        for (size_t i=0;i<r.after->paths.size();++i) {
            const auto &path=*r.after->paths[i];REQUIRE(path.pieces.size()==1);const auto &piece=path.pieces[0];
            const Amount next_length=Amount(axis(piece.end))-axis(piece.start),next_h=piece.section.gap_begin_mm;
            const Amount next_area=Amount(piece.volume.value())/next_length,next_width=next_area/next_h+(1-acos(Amount(-1))/4)*next_h;
            sum+=piece.volume.value();
            if (i==0) {area=next_area;h=next_h;w=next_width;length=next_length;}
            else {
                REQUIRE(area==next_area);REQUIRE(h==next_h);REQUIRE(w==next_width);REQUIRE(length==next_length);
                const Amount d=Amount(center(piece.start))-previous,q=d-(w-h),radius=h/2;
                REQUIRE(d>0);if (i>1) REQUIRE(2*d>w);
                const Amount overlap=q<=0 ? area-h*d : q>=h ? Amount(0) :
                    2*radius*radius*acos(q/(2*radius))-q*sqrt(4*radius*radius-q*q)/2;
                repeated+=length*overlap;
            }
            previous=center(piece.start);
        }
        const auto contains=[&](ScalarBounds measured,Amount amount) {REQUIRE(measured.lower<=amount);REQUIRE(measured.upper>=amount);};
        contains(r.after->fill->occupied->individual_volume_mm3,sum);contains(r.after->fill->occupied->union_volume_mm3,sum-repeated);
        contains(r.after->fill->occupied->repeated_volume_mm3,repeated);
        REQUIRE_FALSE(replan_first_hatch_ends({"",r.after}).snapshot); // Already replaced this extent.
    }
}
TEST_CASE("B07 first-hatch end replan captures sources and refuses invalid or exhausted global proofs", "[Nonplanar][B07][FirstHatchEndReplan]")
{
    const auto f=remainder_fixture(0);const SceneBox box{{1,-.4,.7},{3,.4,1.8}};
    auto before=plan_first_hatch_layer(f.hatches,box);REQUIRE(before.snapshot);const auto saved=before.snapshot;
    FirstHatchLayerLimits limits;FirstHatchReplanPolicy policy;
    limits.cancelled=[&] {before.snapshot.reset();limits.max_evaluations=0;policy.minimum_covered_gain=Volume(100);return false;};
    const auto captured=replan_first_hatch_ends(before,policy,limits);INFO(captured.reason);REQUIRE(captured.snapshot);
    REQUIRE(captured.snapshot->before==saved);before.snapshot=saved;policy={};limits={};
    REQUIRE_FALSE(replan_first_hatch_ends({}).snapshot);
    policy.minimum_covered_gain=Volume(1);REQUIRE_FALSE(replan_first_hatch_ends(before,policy).snapshot);
    policy.minimum_covered_gain=Volume(0);REQUIRE_FALSE(replan_first_hatch_ends(before,policy).snapshot);policy={};
    limits.max_evaluations=1;REQUIRE_FALSE(replan_first_hatch_ends(before,policy,limits).snapshot);
    limits={};limits.max_paths=1;REQUIRE_FALSE(replan_first_hatch_ends(before,policy,limits).snapshot);
    limits={};limits.max_cells=1;REQUIRE_FALSE(replan_first_hatch_ends(before,policy,limits).snapshot);
    limits={};limits.max_records=1;REQUIRE_FALSE(replan_first_hatch_ends(before,policy,limits).snapshot);
    limits={};limits.beads.packets.max_segments=1;REQUIRE_FALSE(replan_first_hatch_ends(before,policy,limits).snapshot);
    limits={};limits.volumes.max_depth=0;REQUIRE_FALSE(replan_first_hatch_ends(before,policy,limits).snapshot);
    limits={};limits.max_evaluations=2000001;REQUIRE_FALSE(replan_first_hatch_ends(before,policy,limits).snapshot);
    limits={};limits.is_current=[](uint64_t) {return false;};REQUIRE_FALSE(replan_first_hatch_ends(before,policy,limits).snapshot);
    limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(replan_first_hatch_ends(before,policy,limits).snapshot);
    limits={};limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};
    REQUIRE_FALSE(replan_first_hatch_ends(before,policy,limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
    const auto rounding=replan_first_hatch_ends(before,policy,limits);REQUIRE(std::fesetround(FE_TONEAREST)==0);REQUIRE_FALSE(rounding.snapshot);
}
TEST_CASE("B07 end replan retains current material only in the newly extended roof domain", "[Nonplanar][B07][FirstHatchEndReplan]")
{
    const auto floor=bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle);
    const auto ridge=bead(3,2,{0,-.125,1.06},{10,-.125,1.06},.06,.05,.05,BeadSectionKind::Rectangle);
    const MaterialRecord travel{{2,1,0,floor.motion.end,ridge.motion.start,Speed(10),Acceleration(100),Travel{}},{}};
    const auto ledger=captured({floor,travel,ridge});
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.0001)};
    const SceneBox box{{1,-.4,.7},{3,.4,1.8}};
    FirstHatchLayerLimits limits;limits.beads.max_roof_segments=32;
    for (double progress : {0.,.1,.12}) {
        const auto present=material_at(ledger,2,progress);
        const auto stack=plan_affine_pass_stack(present.lower,{{1,-.4,3,.4},1.8,1.8,1.8},.9,policy);REQUIRE(stack.snapshot);
        const auto hatches=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),HatchDirection::AlongX});REQUIRE(hatches.snapshot);
        const auto before=plan_first_hatch_layer(hatches,box,limits);INFO(before.reason);REQUIRE(before.snapshot);
        const auto next=replan_first_hatch_ends(before,{},limits);INFO(next.reason);
        if (progress<=.1) REQUIRE(next.snapshot); // The actual butt is before the new start.
        else REQUIRE_FALSE(next.snapshot); // The narrow ridge is inside only the newly extended footprint.
        REQUIRE(before.snapshot->source->source->source==present.nominal.snapshot);
    }
}

TEST_CASE("B07 first-width replan reduces actual overlap while retaining measured coverage", "[Nonplanar][B07][FirstHatchWidthReplan]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<FirstHatchWidthReplanSnapshot>::value);
    using Amount=boost::multiprecision::cpp_bin_float_quad;
    for (bool x : {false,true}) {
        const auto point=[&](double along,double normal,double z) {return x ? PhysicalPosition(along,normal,z) : PhysicalPosition(normal,along,z);};
        const auto floor=bead(1,0,point(0,0,1),point(10,0,1),2,.4,.4,BeadSectionKind::Rectangle);
        const auto ledger=captured({floor});const auto present=material_at(ledger,1,0);
        const RectangleXY roi=x ? RectangleXY{1,-.3,3,.3} : RectangleXY{-.3,1,.3,3};
        const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.0001)};
        const auto stack=plan_affine_pass_stack(present.lower,{roi,1.8,1.8,1.8},.9,policy);REQUIRE(stack.snapshot);
        const auto hatch=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),x ? HatchDirection::AlongX : HatchDirection::AlongY});REQUIRE(hatch.snapshot);
        const SceneBox box{{roi.min_x,roi.min_y,.7},{roi.max_x,roi.max_y,1.8}};
        const auto initial=plan_first_hatch_layer(hatch,box);REQUIRE(initial.snapshot);
        const auto ends=replan_first_hatch_ends(initial);REQUIRE(ends.snapshot);
        const FirstHatchLayerResult before{"",ends.snapshot->after};
        const auto result=replan_first_hatch_width(before,WidthXY(.4));INFO(result.reason);REQUIRE(result.snapshot);
        const auto &r=*result.snapshot;const auto &next=*r.after;const auto &old=*r.before;
        REQUIRE(r.before==before.snapshot);REQUIRE(next.source!=old.source);REQUIRE(next.source->source==old.source->source);
        REQUIRE(next.fill->target==old.fill->target);REQUIRE(next.fill->occupied->source->sequence!=old.fill->occupied->source->sequence);
        REQUIRE(next.source->policy.width.value()==.45); // Original generation policy is retained.
        REQUIRE(next.source->first_pass_extent==old.source->first_pass_extent);
        REQUIRE(next.paths.size()==2);REQUIRE(r.repeated_reduction_mm3.lower>.02);REQUIRE(r.commanded_reduction_mm3.lower>.02);
        REQUIRE(r.covered_change_mm3.lower>=-.001);REQUIRE(next.fill->outside_target_mm3.upper<=.001);
        const auto along=[&](PhysicalPosition p) {return x ? p.x() : p.y();};
        const auto normal=[&](PhysicalPosition p) {return x ? p.y() : p.x();};
        Amount sum=0,one=0,extra=0;
        const Amount d=Amount(normal(next.paths[1]->path_start))-normal(next.paths[0]->path_start);
        for (size_t i=0;i<next.paths.size();++i) {
            REQUIRE(next.paths[i]->source==next.source);REQUIRE(next.source->passes.front().lines[i].width.value()==.4);
            REQUIRE(along(next.paths[i]->path_start)==along(old.paths[i]->path_start));
            REQUIRE(along(next.paths[i]->path_end)==along(old.paths[i]->path_end));
            REQUIRE(normal(next.paths[i]->path_start)!=normal(old.paths[i]->path_start));
            for (const auto &piece : next.paths[i]->pieces) {
                REQUIRE(piece.nominal_width.value()==.4);sum+=piece.volume.value();
                if (!i) {one+=piece.volume.value();extra+=d*abs(Amount(along(piece.end))-along(piece.start))*(Amount(piece.section.gap_begin_mm)+piece.section.gap_end_mm)/2;}
            }
        }
        const auto contains=[&](ScalarBounds v,Amount amount) {REQUIRE(v.lower<=amount);REQUIRE(v.upper>=amount);};
        contains(next.fill->occupied->individual_volume_mm3,sum);contains(next.fill->occupied->union_volume_mm3,one+extra);
        contains(next.fill->occupied->repeated_volume_mm3,one-extra);
        REQUIRE(next.fill->missing_target_mm3.lower>0);REQUIRE(next.global_volume_error_mm3<=.001);
        for (size_t p=1;p<next.source->passes.size();++p) {
            REQUIRE(next.source->passes[p].lines.size()==old.source->passes[p].lines.size());
            for (size_t i=0;i<next.source->passes[p].lines.size();++i) {
                const auto &a=next.source->passes[p].lines[i],&b=old.source->passes[p].lines[i];
                REQUIRE(a.width.value()==b.width.value());REQUIRE(a.start.x()==b.start.x());REQUIRE(a.start.y()==b.start.y());REQUIRE(a.start.z()==b.start.z());
            }
        }
        const auto cells=allocate_affine_hatch_cells({"",next.source});INFO(cells.reason);REQUIRE(cells.snapshot);
        const auto &first=cells.snapshot->passes.front().finite_footprint;
        const double low=x ? first.min_y : first.min_x,high=x ? first.max_y : first.max_x;
        REQUIRE(low>=normal(next.paths[0]->path_start)-.2);REQUIRE(high<=normal(next.paths[1]->path_start)+.2);
        const auto empty=material_at(next.fill->occupied->source->sequence,0,0);REQUIRE(empty.nominal.snapshot);
        const auto empty_union=integrate_material_union(empty.nominal,box);REQUIRE(empty_union.snapshot);
        const auto empty_fill=reconcile_material_fill(stack.snapshot->first_pass,empty_union);REQUIRE(empty_fill.snapshot);
        const auto remainder=plan_remaining_first_hatch({"",next.source},0,empty_fill);INFO(remainder.reason);REQUIRE(remainder.snapshot);
        REQUIRE(remainder.snapshot->paths.size()==1);
        for (const auto &piece : remainder.snapshot->paths.front()->pieces) REQUIRE(piece.nominal_width.value()==.4);
        const auto narrow_initial=replan_first_hatch_width(initial,WidthXY(.4));REQUIRE(narrow_initial.snapshot);
        const auto narrow_ends=replan_first_hatch_ends({"",narrow_initial.snapshot->after});INFO(narrow_ends.reason);REQUIRE(narrow_ends.snapshot);
        REQUIRE(narrow_ends.snapshot->after->source->policy.width.value()==.45);
        for (const auto &path : narrow_ends.snapshot->after->paths)
            for (const auto &piece : path->pieces) REQUIRE(piece.nominal_width.value()==.4);
        REQUIRE_FALSE(replan_first_hatch_width({"",result.snapshot->after},WidthXY(.4)).snapshot);
    }
}

TEST_CASE("B07 first-width replan captures immutable input and refuses loss or exhausted proofs", "[Nonplanar][B07][FirstHatchWidthReplan]")
{
    const auto f=remainder_fixture(0);const SceneBox box{{1,-.4,.7},{3,.4,1.8}};
    auto before=plan_first_hatch_layer(f.hatches,box);REQUIRE(before.snapshot);const auto saved=before.snapshot;
    FirstHatchLayerLimits limits;FirstHatchWidthReplanPolicy policy;policy.maximum_covered_loss=Volume(.1);
    limits.cancelled=[&] {before.snapshot.reset();limits.max_paths=0;policy.minimum_repeated_reduction=Volume(100);return false;};
    const auto retained=replan_first_hatch_width(before,WidthXY(.44),policy,limits);INFO(retained.reason);REQUIRE(retained.snapshot);
    REQUIRE(retained.snapshot->before==saved);before.snapshot=saved;
    // Narrow sections on this wider ROI create real new gaps; default coverage gate refuses them.
    const auto loss=replan_first_hatch_width(before,WidthXY(.36));INFO(loss.reason);
    REQUIRE_FALSE(loss.snapshot);REQUIRE(loss.reason=="FIRST_HATCH_WIDTH_REPLAN_COVERAGE_LOSS");
    REQUIRE_FALSE(replan_first_hatch_width({},WidthXY(.4)).snapshot);
    REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.45)).snapshot);
    REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.5)).snapshot);
    REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.1)).snapshot);
    policy={};policy.minimum_repeated_reduction=Volume(1);REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.44),policy).snapshot);
    policy.minimum_repeated_reduction=Volume(0);REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.44),policy).snapshot);
    policy={};policy.maximum_covered_loss=Volume(.1);
    limits={};limits.max_evaluations=1;REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.44),policy,limits).snapshot);
    limits={};limits.max_paths=1;REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.44),policy,limits).snapshot);
    limits={};limits.max_cells=1;REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.44),policy,limits).snapshot);
    limits={};limits.max_records=1;REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.44),policy,limits).snapshot);
    limits={};limits.beads.packets.max_segments=1;REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.44),policy,limits).snapshot);
    limits={};limits.volumes.max_depth=0;REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.44),policy,limits).snapshot);
    limits={};limits.max_evaluations=2000001;REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.44),policy,limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.44),policy,limits).snapshot);
    limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.44),policy,limits).snapshot);
    limits={};limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};
    REQUIRE_FALSE(replan_first_hatch_width(before,WidthXY(.44),policy,limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
    const auto rounding=replan_first_hatch_width(before,WidthXY(.44),policy,limits);REQUIRE(std::fesetround(FE_TONEAREST)==0);REQUIRE_FALSE(rounding.snapshot);
}

TEST_CASE("B07 first-width replan repartitions displaced interior strip owners", "[Nonplanar][B07][FirstHatchWidthReplan]")
{
    const auto floor=bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle);
    const auto ledger=captured({floor});const auto present=material_at(ledger,1,0);
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.0001)};
    const auto stack=plan_affine_pass_stack(present.lower,{{1,-.7,3,.7},1.8,1.8,1.8},.9,policy);REQUIRE(stack.snapshot);
    const auto hatch=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),HatchDirection::AlongX});REQUIRE(hatch.snapshot);
    const auto before=plan_first_hatch_layer(hatch,{{1,-.7,.7},{3,.7,1.8}});REQUIRE(before.snapshot);
    const auto result=replan_first_hatch_width(before,WidthXY(.44));INFO(result.reason);REQUIRE(result.snapshot);
    const auto &old=before.snapshot->source->passes.front().lines,&next=result.snapshot->after->source->passes.front().lines;
    REQUIRE(next.size()==4);REQUIRE(next[0].volume_cell.max_y!=old[0].volume_cell.max_y);
    using Amount=boost::multiprecision::cpp_bin_float_quad;Amount sum=0,repeat=0,previous=0;
    for (size_t i=0;i<next.size();++i) {
        if (i) {
            REQUIRE(next[i].volume_cell.min_y==next[i-1].volume_cell.max_y);
            const Amount middle=(Amount(next[i-1].start.y())+next[i].start.y())/2;
            REQUIRE(abs(Amount(next[i].volume_cell.min_y)-middle)<1e-15);
        }
        REQUIRE(next[i].prospective_cell_volume_mm3.lower>0);
        const auto &path=*result.snapshot->after->paths[i];REQUIRE(path.pieces.size()==1);
        const auto &p=path.pieces[0];sum+=p.volume.value();
        const Amount length=Amount(p.end.x())-p.start.x(),h=p.section.gap_begin_mm,A=Amount(p.volume.value())/length;
        const Amount w=A/h+(1-acos(Amount(-1))/4)*h;
        if (i) {
            const Amount d=Amount(p.start.y())-previous,q=d-(w-h),r=h/2;
            REQUIRE(q>0);REQUIRE(q<h);REQUIRE(2*d>w); // Adjacent circular lenses only.
            repeat+=length*(2*r*r*acos(q/(2*r))-q*sqrt(4*r*r-q*q)/2);
        }
        previous=p.start.y();
    }
    const auto contains=[&](ScalarBounds v,Amount amount) {REQUIRE(v.lower<=amount);REQUIRE(v.upper>=amount);};
    const auto &fill=*result.snapshot->after->fill;
    contains(fill.occupied->individual_volume_mm3,sum);contains(fill.occupied->union_volume_mm3,sum-repeat);contains(fill.occupied->repeated_volume_mm3,repeat);
    const auto cells=allocate_affine_hatch_cells({"",result.snapshot->after->source});INFO(cells.reason);REQUIRE(cells.snapshot);
    REQUIRE(cells.snapshot->passes.front().finite_cells.size()==4);
    REQUIRE(fill.covered_target_mm3.upper<before.snapshot->fill->covered_target_mm3.lower); // Small real loss is explicitly bounded, not called gain.
    REQUIRE(result.snapshot->covered_change_mm3.lower>=-.001);REQUIRE(result.snapshot->repeated_reduction_mm3.lower>.005);
}

TEST_CASE("B07 first contour owns a closed finite-width loop and measures corner overlap", "[Nonplanar][B07][FirstContour]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<FirstContourSnapshot>::value);
    const auto f=remainder_fixture(0);const SceneBox box{{1,-.4,.7},{3,.4,1.8}};
    for (bool clockwise : {false,true}) for (size_t seam=0;seam<4;++seam) {
        const auto result=plan_first_contour(f.hatches,{WidthXY(.45),seam,clockwise,Volume(.001)},box);INFO(result.reason);REQUIRE(result.snapshot);
        const auto &loop=*result.snapshot;REQUIRE(loop.source==f.hatches.snapshot);REQUIRE(loop.edges.size()==4);
        REQUIRE(loop.policy.seam_corner==seam);REQUIRE(loop.policy.clockwise==clockwise);
        const auto same=[](PhysicalPosition a,PhysicalPosition b) {return a.x()==b.x() && a.y()==b.y() && a.z()==b.z();};
        using Amount=boost::multiprecision::cpp_bin_float_quad;Amount sum=0;double orientation=0;
        for (size_t i=0;i<4;++i) {
            const auto &edge=*loop.edges[i];const auto &next=*loop.edges[(i+1)%4];
            REQUIRE_FALSE(edge.line_index.has_value());REQUIRE(edge.source==loop.source);
            REQUIRE(edge.roof_domain==FirstHatchRoofDomain::FiniteWidth);REQUIRE(same(edge.path_end,next.path_start));
            REQUIRE(edge.pieces.size()==1);REQUIRE(same(edge.pieces.front().start,edge.path_start));REQUIRE(same(edge.pieces.back().end,edge.path_end));
            orientation+=edge.path_start.x()*edge.path_end.y()-edge.path_end.x()*edge.path_start.y();
            const auto &p=edge.pieces[0];const Amount length=sqrt(pow(Amount(p.end.x())-p.start.x(),2)+pow(Amount(p.end.y())-p.start.y(),2));
            const Amount h=p.section.gap_begin_mm,k=1-acos(Amount(-1))/4;
            REQUIRE(p.section.gap_end_mm==p.section.gap_begin_mm);
            const Amount ideal=length*h*(Amount(.45)-k*h);
            REQUIRE(abs(Amount(p.volume.value())-ideal)<=edge.total_volume_error_mm3);sum+=p.volume.value();
        }
        REQUIRE((clockwise ? orientation<0 : orientation>0));
        REQUIRE(loop.fill->occupied->individual_volume_mm3.lower<=sum);REQUIRE(loop.fill->occupied->individual_volume_mm3.upper>=sum);
        REQUIRE(loop.fill->occupied->repeated_volume_mm3.lower>0); // Corners and the nearby long sides overlap.
        REQUIRE(loop.fill->covered_target_mm3.lower>0);REQUIRE(loop.fill->missing_target_mm3.lower>0);
        REQUIRE(loop.fill->outside_target_mm3.upper<=.001);REQUIRE(loop.global_volume_error_mm3<=.001);
        // Independent exact union: the two long horizontal flat cores overlap
        // across the entire short span. Vertical sides contribute their outer
        // half-sections; all inside quarters are already in that merged strip.
        std::optional<Amount> horizontal,vertical,long_span,short_span,height;
        for (const auto &edge : loop.edges) {
            const auto &p=edge->pieces[0];const Amount h=p.section.gap_begin_mm;
            const bool x=p.start.y()==p.end.y();const Amount length=x ? abs(Amount(p.end.x())-p.start.x()) : abs(Amount(p.end.y())-p.start.y());
            auto &volume=x ? horizontal : vertical;auto &span=x ? long_span : short_span;
            if (volume) {REQUIRE(*volume==p.volume.value());REQUIRE(*span==length);REQUIRE(*height==h);}
            else {volume=p.volume.value();span=length;height=h;}
        }
        const Amount k=1-acos(Amount(-1))/4,h=*height;
        const Amount width_h=*horizontal / *long_span/h+k*h,width_v=*vertical / *short_span/h+k*h;
        REQUIRE(*short_span<=width_h-h);REQUIRE(*long_span>width_v);
        const Amount united=*horizontal + *vertical + *long_span * *short_span*h;
        const auto contains=[&](ScalarBounds v,Amount amount) {REQUIRE(v.lower<=amount);REQUIRE(v.upper>=amount);};
        contains(loop.fill->occupied->union_volume_mm3,united);contains(loop.fill->occupied->repeated_volume_mm3,sum-united);
        contains(loop.fill->covered_target_mm3,united);
        const auto &rows=loop.fill->occupied->source->sequence->records;
        REQUIRE(rows.size()==4); // Connected deposition edges need no zero-length travel.
    }
}

TEST_CASE("B07 first contour rejects partial roofs and shares all construction and fill budgets", "[Nonplanar][B07][FirstContour]")
{
    auto f=remainder_fixture(0);const auto saved=f.hatches.snapshot;const SceneBox box{{1,-.4,.7},{3,.4,1.8}};
    FirstContourPolicy policy{WidthXY(.45),0,false,Volume(.001)};FirstHatchLayerLimits limits;
    limits.cancelled=[&] {f.hatches.snapshot.reset();policy.seam_corner=9;limits.max_paths=0;return false;};
    const auto owned=plan_first_contour(f.hatches,policy,box,limits);INFO(owned.reason);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->source==saved);
    f.hatches.snapshot=saved;policy={WidthXY(.45),0,false,Volume(.001)};limits={};
    REQUIRE_FALSE(plan_first_contour({},policy,box).snapshot);
    policy.seam_corner=4;REQUIRE_FALSE(plan_first_contour(f.hatches,policy,box).snapshot);policy.seam_corner=0;
    policy.width=WidthXY(.1);REQUIRE_FALSE(plan_first_contour(f.hatches,policy,box).snapshot);policy.width=WidthXY(.9);
    REQUIRE_FALSE(plan_first_contour(f.hatches,policy,box).snapshot);policy.width=WidthXY(.45);
    REQUIRE_FALSE(plan_first_contour(f.hatches,policy,{{1.1,-.4,.7},{3,.4,1.8}}).snapshot);
    REQUIRE_FALSE(plan_first_contour(f.hatches,policy,{{1,-.4,1.1},{3,.4,1.8}}).snapshot);
    limits.max_paths=3;REQUIRE_FALSE(plan_first_contour(f.hatches,policy,box,limits).snapshot);
    limits={};limits.max_records=3;REQUIRE_FALSE(plan_first_contour(f.hatches,policy,box,limits).snapshot);
    limits={};limits.max_cells=1;REQUIRE_FALSE(plan_first_contour(f.hatches,policy,box,limits).snapshot);
    limits={};limits.max_evaluations=1;REQUIRE_FALSE(plan_first_contour(f.hatches,policy,box,limits).snapshot);
    limits={};limits.beads.packets.max_segments=3;REQUIRE_FALSE(plan_first_contour(f.hatches,policy,box,limits).snapshot);
    limits={};limits.beads.max_roof_segments=3;REQUIRE_FALSE(plan_first_contour(f.hatches,policy,box,limits).snapshot);
    limits={};limits.volumes.max_depth=0;REQUIRE_FALSE(plan_first_contour(f.hatches,policy,box,limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(plan_first_contour(f.hatches,policy,box,limits).snapshot);
    limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(plan_first_contour(f.hatches,policy,box,limits).snapshot);
    limits={};limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};
    REQUIRE_FALSE(plan_first_contour(f.hatches,policy,box,limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
    const auto rounding=plan_first_contour(f.hatches,policy,box,limits);REQUIRE(std::fesetround(FE_TONEAREST)==0);REQUIRE_FALSE(rounding.snapshot);
    const auto floor=bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle);
    const auto ridge=bead(3,2,{0,.3,1.06},{10,.3,1.06},.06,.05,.05,BeadSectionKind::Rectangle);
    const MaterialRecord travel{{2,1,0,floor.motion.end,ridge.motion.start,Speed(10),Acceleration(100),Travel{}},{}};
    const auto ledger=captured({floor,travel,ridge});
    const AffinePassPolicy passes{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.0001)};
    for (double progress : {0.,.1,.3}) {
        const auto present=material_at(ledger,2,progress);
        const auto stack=plan_affine_pass_stack(present.lower,{{1,-.4,3,.4},1.8,1.8,1.8},.9,passes);REQUIRE(stack.snapshot);
        const auto hatch=plan_affine_hatches(stack,{WidthXY(.45),Length(.4),Length(.05),HatchDirection::AlongX});REQUIRE(hatch.snapshot);
        limits={};limits.beads.max_roof_segments=32;
        REQUIRE(plan_first_hatch_footprint_bead(hatch,0,limits.beads).snapshot); // First edge misses the ridge.
        const auto contour=plan_first_contour(hatch,policy,box,limits);INFO(contour.reason);
        if (progress<=.1) REQUIRE(contour.snapshot);else REQUIRE_FALSE(contour.snapshot);
    }
}

TEST_CASE("B07 closed-loop unions enclose independent circular section moments", "[Nonplanar][B07][MaterialUnion][ClosedLoopUnion]")
{
    using Amount=boost::multiprecision::cpp_bin_float_quad;
    for (bool rotate : {false,true}) for (double short_span : {.1,.3,1.}) {
        const auto point=[&](double a,double b) {return rotate ? PhysicalPosition(b,a,1.2) : PhysicalPosition(a,b,1.2);};
        const std::array<PhysicalPosition,4> points{point(0,0),point(2,0),point(2,short_span),point(0,short_span)};
        std::vector<MaterialRecord> rows;
        for (size_t i=0;i<4;++i) rows.push_back(bead(i+1,i,points[i],points[(i+1)%4],i%2 ? .55 : .45,.2,.2));
        const auto ledger=captured(rows);const auto present=material_at(ledger,4,0);
        MaterialUnionLimits limits;limits.max_cells=511;limits.maximum_interval_width=Volume(.0001);
        const SceneBox box{{-1,-1,.7},{3,3,1.5}};
        const auto result=integrate_material_union(present.nominal,box,limits);INFO(result.reason);REQUIRE(result.snapshot);
        const Amount A=2,B=short_span,h=.2,r=h/2,pi=acos(Amount(-1)),k=1-pi/4;
        const Amount vh=std::get<Deposition>(rows[0].motion.payload).volume.value(),vv=std::get<Deposition>(rows[1].motion.payload).volume.value();
        const Amount c=(vh/A/h+k*h-h)/2,e=(vv/B/h+k*h-h)/2,sum=2*(vh+vv);
        Amount united;
        if (B<=2*c) united=vh+vv+A*B*h;
        else if (B>=2*(c+r)) united=sum-4*(c*e*h+(c+e)*pi*h*h/8+h*h*h/6);
        else {
            const Amount t=B/2-c,d=r-sqrt(r*r-t*t),q=d-r;
            const Amount circle=(q*sqrt(r*r-q*q)+r*r*asin(q/r))/2+pi*r*r/4;
            const Amount square=h*d*d/2-d*d*d/3;
            const Amount a0=4*(c*A+e*B-c*e),a1=4*(A+B-c-e),b0=A*B+2*A*c+2*B*e,b1=2*(A+B);
            united=2*(a0*d+a1*circle-4*square+b0*(r-d)+b1*(pi*r*r/4-circle));
        }
        const auto contains=[&](ScalarBounds v,Amount amount) {REQUIRE(v.lower<=amount);REQUIRE(v.upper>=amount);};
        contains(result.snapshot->individual_volume_mm3,sum);contains(result.snapshot->union_volume_mm3,united);contains(result.snapshot->repeated_volume_mm3,sum-united);
        limits.max_cells=1;REQUIRE_FALSE(integrate_material_union(present.nominal,box,limits).snapshot);
        limits.max_cells=4;limits.maximum_interval_width=Volume(1e-10);
        const auto partial=material_at(ledger,3,.4);REQUIRE_FALSE(integrate_material_union(partial.nominal,box,limits).snapshot);
        auto changed=rows;changed[2]=bead(3,2,points[2],points[3],.46,.2,.2);
        const auto unequal=captured(changed);REQUIRE_FALSE(integrate_material_union(material_at(unequal,4,0).nominal,box,limits).snapshot);
        changed=rows;changed[3]=bead(4,3,points[3],points[0],.55,.2,.21);
        const auto variable=captured(changed);REQUIRE_FALSE(integrate_material_union(material_at(variable,4,0).nominal,box,limits).snapshot);
        REQUIRE_FALSE(integrate_material_union(present.nominal,{{-1,-1,1.1},{3,3,1.5}},limits).snapshot); // A clipped box cannot use full-loop volume.
    }
}

TEST_CASE("B07 sloped loop union matches independent circle-line spill and preserves packet gaps", "[Nonplanar][B07][MaterialUnion][LoftLoopUnion]")
{
    using Amount=boost::multiprecision::cpp_bin_float_quad;
    for (bool rotate : {false,true}) {
        const auto point=[&](double a,double b,double z) {return rotate ? PhysicalPosition(b,a,z) : PhysicalPosition(a,b,z);};
        std::vector<MaterialRecord> rows;
        const auto add=[&](PhysicalPosition a,PhysicalPosition b,double h0,double h1) {const size_t i=rows.size();rows.push_back(bead(i+1,i,a,b,.9,h0,h1));};
        add(point(0,0,1.25),point(1,0,1.3125),.25,.3125);
        add(point(1,0,1.3125),point(2,0,1.375),.3125,.375);
        add(point(2,0,1.375),point(2,.1,1.375),.375,.375);
        add(point(2,.1,1.375),point(1,.1,1.3125),.375,.3125);
        add(point(1,.1,1.3125),point(0,.1,1.25),.3125,.25);
        add(point(0,.1,1.25),point(0,0,1.25),.25,.25);
        const auto ledger=captured(rows);const auto present=material_at(ledger,rows.size(),0);
        MaterialUnionLimits limits;limits.max_cells=511;limits.maximum_interval_width=Volume(.0001);
        const SceneBox box{{-1,-1,.7},{3,3,1.6}};
        const auto result=integrate_material_union(present.nominal,box,limits);INFO(result.reason);REQUIRE(result.snapshot);
        const auto amount=[&](size_t i) {return Amount(std::get<Deposition>(rows[i].motion.payload).volume.value());};
        const Amount main=amount(0)+amount(1),left=amount(5),right=amount(2),B=.1,h=.375,r=h/2,s=.0625,pi=acos(Amount(-1));
        const Amount c=(right/B/h+(1-pi/4)*h-h)/2;
        // Independent positive circle/line intersection: the high transverse
        // end protrudes above the primary affine top on its inner half-section.
        const Amount qa=1+s*s,qb=2*s*s*c-2*r*s,qc=s*s*c*c-2*r*s*c;
        const Amount q=(-qb+sqrt(qb*qb-4*qa*qc))/(2*qa);
        REQUIRE(q>0);REQUIRE(q<r);REQUIRE(s*(c+q)<r);
        const Amount spill=s*c*c/2+(s*c-r)*q+s*q*q/2+(q*sqrt(r*r-q*q)+r*r*asin(q/r))/2;
        const Amount united=main+B*2*(Amount(.25)+.375)/2+(left+right)/2+B*spill,sum=2*main+left+right;
        const auto contains=[&](ScalarBounds v,Amount value) {REQUIRE(v.lower<=value);REQUIRE(v.upper>=value);};
        contains(result.snapshot->individual_volume_mm3,sum);contains(result.snapshot->union_volume_mm3,united);contains(result.snapshot->repeated_volume_mm3,sum-united);
        limits.max_cells=1;REQUIRE_FALSE(integrate_material_union(present.nominal,box,limits).snapshot);
        limits.max_cells=4;limits.maximum_interval_width=Volume(1e-10);
        REQUIRE_FALSE(integrate_material_union(material_at(ledger,5,.4).nominal,box,limits).snapshot);
        auto changed=rows;changed[3]=bead(4,3,changed[3].motion.start,changed[3].motion.end,.91,.375,.3125);
        const auto unequal=captured(changed);REQUIRE_FALSE(integrate_material_union(material_at(unequal,6,0).nominal,box,limits).snapshot);
        // A real gap in both paired long sides must not become a continuous slab.
        changed[3]=rows[3];changed[1]=bead(2,1,point(1.125,0,1.3125),point(2,0,1.375),.9,.3125,.375);
        changed[3]=bead(4,3,point(2,.1,1.375),point(1.125,.1,1.3125),.9,.375,.3125);
        std::vector<MaterialRecord> gapped;
        for (auto row : changed) {
            if (!gapped.empty() && (gapped.back().motion.end.x()!=row.motion.start.x() || gapped.back().motion.end.y()!=row.motion.start.y())) {
                const size_t i=gapped.size();gapped.push_back({{i+1,i,0,gapped.back().motion.end,row.motion.start,Speed(10),Acceleration(100),Travel{}},{}});
            }
            row.motion.event_id=gapped.size()+1;row.motion.sequence_index=gapped.size();gapped.push_back(std::move(row));
        }
        const auto holes=captured(gapped);REQUIRE_FALSE(integrate_material_union(material_at(holes,gapped.size(),0).nominal,box,limits).snapshot);
    }
}

namespace {
AffineHatchResult first_cap_fixture(HatchDirection direction,bool sloped=false,double inner_loss=.01)
{
    const auto body=captured({bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle)},model(.01,inner_loss));
    const auto state=material_at(body,1,0);
    const AffinePassPolicy policy{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.001)};
    const bool x=direction==HatchDirection::AlongX;
    const auto stack=plan_affine_pass_stack(state.lower,{{1,-.8,3,.8},1.8,sloped && x ? 1.84 : 1.8,sloped && !x ? 1.84 : 1.8},.9,policy);REQUIRE(stack.snapshot);
    const auto hatches=plan_affine_hatches(stack,{WidthXY(.45),Length(.2),Length(.05),direction});REQUIRE(hatches.snapshot);return hatches;
}
}
TEST_CASE("B07 first cap replaces boundary hatches and measures contour plus interior in one ordered ledger", "[Nonplanar][B07][FirstCap]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<FirstCapSnapshot>::value);
    const SceneBox box{{1,-.8,.7},{3,.8,1.84}};
    for (auto direction : {HatchDirection::AlongX,HatchDirection::AlongY}) for (bool sloped : {false,true}) {
        const auto hatches=first_cap_fixture(direction,sloped);const auto source=hatches.snapshot;
        const auto result=plan_first_cap(hatches,{WidthXY(.45),1,true,Volume(.001)},box);INFO(result.reason);REQUIRE(result.snapshot);
        const auto &cap=*result.snapshot;const auto &lines=source->passes.front().lines;
        REQUIRE(cap.source==source);REQUIRE(cap.paths.size()==lines.size()+2);REQUIRE((cap.replaced_boundary_lines==std::vector<size_t>{0,lines.size()-1}));
        REQUIRE(cap.fill->target==source->source->first_pass.proof);REQUIRE(cap.global_volume_error_mm3<=.001);
        REQUIRE(cap.fill->covered_target_mm3.lower>0);REQUIRE(cap.fill->missing_target_mm3.lower>0);REQUIRE(cap.fill->outside_target_mm3.upper<=.001);
        using Amount=boost::multiprecision::cpp_bin_float_quad;Amount sum=0;
        const bool x=direction==HatchDirection::AlongX;
        const auto axis=[&](PhysicalPosition p) {return x ? p.x() : p.y();};
        const auto normal=[&](PhysicalPosition p) {return x ? p.y() : p.x();};
        double lo=1e10,hi=-lo,first=lo,last=hi;
        for (size_t i=0;i<4;++i) {
            const auto &edge=*cap.paths[i],&next=*cap.paths[(i+1)%4];REQUIRE_FALSE(edge.line_index.has_value());
            REQUIRE(edge.path_end.x()==next.path_start.x());REQUIRE(edge.path_end.y()==next.path_start.y());REQUIRE(edge.path_end.z()==next.path_start.z());
            lo=std::min(lo,axis(edge.path_start));hi=std::max(hi,axis(edge.path_start));first=std::min(first,normal(edge.path_start));last=std::max(last,normal(edge.path_start));
        }
        for (size_t i=4;i<cap.paths.size();++i) {
            const auto &path=*cap.paths[i];REQUIRE(path.line_index==i-3);REQUIRE(path.source==source);REQUIRE(path.roof_domain==FirstHatchRoofDomain::FiniteWidth);
            const auto &line=lines[*path.line_index];REQUIRE(normal(path.path_start)==normal(line.start));REQUIRE(normal(path.path_end)==normal(line.end));
            REQUIRE(axis(path.path_start)==lo);REQUIRE(axis(path.path_end)==hi);REQUIRE(lo>=axis(line.start));REQUIRE(hi<=axis(line.end));
            REQUIRE(normal(path.path_start)>first);REQUIRE(normal(path.path_start)<last);
        }
        for (const auto &path : cap.paths) for (const auto &piece : path->pieces) {REQUIRE(piece.nominal_width.value()==.45);sum+=piece.volume.value();}
        const auto contains=[&](ScalarBounds v,Amount value) {REQUIRE(v.lower<=value);REQUIRE(v.upper>=value);};
        contains(cap.fill->occupied->individual_volume_mm3,sum);contains(cap.deposited_volume_mm3,sum);
        // The fresh ledger owns the declared path order, including connectors.
        const auto &rows=cap.fill->occupied->source->sequence->records;size_t row=0;
        for (const auto &path : cap.paths) {
            if (row<rows.size() && !rows[row].bead) {REQUIRE(std::holds_alternative<Travel>(rows[row].motion.payload));++row;}
            for (const auto &piece : path->pieces) {REQUIRE(row<rows.size());REQUIRE(rows[row].motion.start.x()==piece.start.x());REQUIRE(rows[row].motion.start.y()==piece.start.y());
                REQUIRE(rows[row].motion.end.z()==piece.end.z());REQUIRE(std::get<Deposition>(rows[row].motion.payload).volume.value()==piece.volume.value());++row;}
        }
        REQUIRE(row==rows.size());REQUIRE(cap.fill->occupied->source->completed_records==rows.size());
        if (!sloped) {
            Amount primary=0,transverse=0;std::vector<Amount> centres;std::optional<Amount> h;
            for (const auto &path : cap.paths) {
                REQUIRE(path->pieces.size()==1);const auto &p=path->pieces[0];h=p.section.gap_begin_mm;REQUIRE(p.section.gap_end_mm==*h);
                if (normal(p.start)==normal(p.end)) {primary=p.volume.value();centres.emplace_back(normal(p.start));}
                else transverse=p.volume.value();
            }
            std::sort(centres.begin(),centres.end());const Amount a=Amount(hi)-lo,b=Amount(last)-first,k=1-acos(Amount(-1))/4;
            const Amount core=primary/a/ *h+k* *h- *h;
            for (size_t i=1;i<centres.size();++i) REQUIRE(centres[i]-centres[i-1]<=core);
            const Amount united=primary+transverse+a*b* *h;
            contains(cap.fill->occupied->union_volume_mm3,united);contains(cap.fill->occupied->repeated_volume_mm3,sum-united);contains(cap.fill->covered_target_mm3,united);
            const auto contour=plan_first_contour(hatches,{WidthXY(.45),1,true,Volume(.001)},box);INFO(contour.reason);REQUIRE(contour.snapshot);
            REQUIRE(cap.fill->covered_target_mm3.lower>contour.snapshot->fill->covered_target_mm3.upper);
            REQUIRE(cap.fill->missing_target_mm3.upper<contour.snapshot->fill->missing_target_mm3.lower);
        }
    }
}

TEST_CASE("B07 first cap refuses absent interiors later infill roofs and shared construction limits", "[Nonplanar][B07][FirstCap]")
{
    auto hatches=first_cap_fixture(HatchDirection::AlongX);const auto source=hatches.snapshot;const SceneBox box{{1,-.8,.7},{3,.8,1.84}};
    FirstContourPolicy policy{WidthXY(.45),0,false,Volume(.001)};FirstHatchLayerLimits limits;
    limits.cancelled=[&] {hatches.snapshot.reset();policy.width=WidthXY(.1);limits.max_paths=0;return false;};
    const auto owned=plan_first_cap(hatches,policy,box,limits);INFO(owned.reason);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->source==source);
    hatches.snapshot=source;policy={WidthXY(.45),0,false,Volume(.001)};limits={};
    REQUIRE_FALSE(plan_first_cap({},policy,box).snapshot);policy.width=WidthXY(.4);REQUIRE_FALSE(plan_first_cap(hatches,policy,box).snapshot);policy.width=WidthXY(.45);
    const auto thin=remainder_fixture(0);const auto no_interior=plan_first_cap(thin.hatches,policy,{{1,-.4,.7},{3,.4,1.8}});
    REQUIRE_FALSE(no_interior.snapshot);REQUIRE(no_interior.reason=="FIRST_CAP_NO_INTERIOR_HATCH");
    REQUIRE_FALSE(plan_first_cap(hatches,policy,{{1.1,-.8,.7},{3,.8,1.84}}).snapshot);
    REQUIRE_FALSE(plan_first_cap(hatches,policy,{{1,-.8,1.1},{3,.8,1.84}}).snapshot);
    limits.max_paths=4;REQUIRE_FALSE(plan_first_cap(hatches,policy,box,limits).snapshot);
    limits={};limits.max_records=4;REQUIRE_FALSE(plan_first_cap(hatches,policy,box,limits).snapshot);
    limits={};limits.max_cells=1;REQUIRE_FALSE(plan_first_cap(hatches,policy,box,limits).snapshot);
    limits={};limits.max_evaluations=1;REQUIRE_FALSE(plan_first_cap(hatches,policy,box,limits).snapshot);
    limits={};limits.beads.packets.max_segments=4;REQUIRE_FALSE(plan_first_cap(hatches,policy,box,limits).snapshot);
    limits={};limits.beads.max_roof_segments=4;REQUIRE_FALSE(plan_first_cap(hatches,policy,box,limits).snapshot);
    limits={};limits.volumes.max_depth=0;REQUIRE_FALSE(plan_first_cap(hatches,policy,box,limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(plan_first_cap(hatches,policy,box,limits).snapshot);
    limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(plan_first_cap(hatches,policy,box,limits).snapshot);
    limits={};limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};
    REQUIRE_FALSE(plan_first_cap(hatches,policy,box,limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};const auto rounding=plan_first_cap(hatches,policy,box,limits);
    REQUIRE(std::fesetround(FE_TONEAREST)==0);REQUIRE_FALSE(rounding.snapshot);
    const auto floor=bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle);
    const auto ridge=bead(3,2,{1.7,0,1.06},{2.3,0,1.06},.06,.05,.05,BeadSectionKind::Rectangle);
    const MaterialRecord travel{{2,1,0,floor.motion.end,ridge.motion.start,Speed(10),Acceleration(100),Travel{}},{}};
    const auto body=captured({floor,travel,ridge});
    const AffinePassPolicy passes{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.0001)};
    for (double progress : {0.,.5,1.}) {
        const auto stack=plan_affine_pass_stack(material_at(body,2,progress).lower,{{1,-.8,3,.8},1.8,1.8,1.8},.9,passes);REQUIRE(stack.snapshot);
        const auto hatch=plan_affine_hatches(stack,{WidthXY(.45),Length(.2),Length(.05),HatchDirection::AlongX});REQUIRE(hatch.snapshot);
        const auto contour=plan_first_contour(hatch,policy,box);INFO(contour.reason);INFO("ridge progress=" << progress);
        REQUIRE(contour.snapshot); // The local ridge is outside all contour footprints.
        const auto cap=plan_first_cap(hatch,policy,box);INFO(cap.reason);if (progress==0) REQUIRE(cap.snapshot);else REQUIRE_FALSE(cap.snapshot);
    }
}

TEST_CASE("B07 merged infill-loop union encloses total multiplicity and refuses an unmerged or unequal interior", "[Nonplanar][B07][MaterialUnion][InfillLoopUnion]")
{
    using Amount=boost::multiprecision::cpp_bin_float_quad;
    for (bool rotate : {false,true}) for (bool reverse : {false,true}) {
        const auto point=[&](double a,double b) {return rotate ? PhysicalPosition(b,a,1.25) : PhysicalPosition(a,b,1.25);};
        const std::array<PhysicalPosition,4> corners{point(0,0),point(2,0),point(2,.875),point(0,.875)};
        std::vector<MaterialRecord> rows;
        for (size_t i=0;i<4;++i) {const size_t a=reverse ? (4-i)%4 : i,b=reverse ? (3-i)%4 : (i+1)%4;
            rows.push_back(bead(i+1,i,corners[a],corners[b],.9,.25,.25));}
        rows.push_back({{5,4,0,rows.back().motion.end,point(reverse ? 2 : 0,.4375),Speed(10),Acceleration(100),Travel{}},{}});
        rows.push_back(bead(6,5,rows.back().motion.end,point(reverse ? 0 : 2,.4375),.9,.25,.25));
        const auto ledger=captured(rows);const auto present=material_at(ledger,rows.size(),0);
        MaterialUnionLimits limits;limits.max_cells=511;limits.maximum_interval_width=Volume(.0001);const SceneBox box{{-1,-1,.7},{3,3,1.6}};
        const auto result=integrate_material_union(present.nominal,box,limits);INFO(result.reason);REQUIRE(result.snapshot);
        const Amount primary=std::get<Deposition>(rows[0].motion.payload).volume.value(),transverse=std::get<Deposition>(rows[1].motion.payload).volume.value();
        const Amount vp=reverse ? transverse : primary,vt=reverse ? primary : transverse,h=.25,a=2,b=.875,pi=acos(Amount(-1));
        const Amount core=vp/a/h+(1-pi/4)*h-h;REQUIRE(core<b);REQUIRE(core>=b/2);
        const Amount sum=3*vp+2*vt,united=vp+vt+a*b*h;
        const auto contains=[&](ScalarBounds bounds,Amount value) {REQUIRE(bounds.lower<=value);REQUIRE(bounds.upper>=value);};
        contains(result.snapshot->individual_volume_mm3,sum);contains(result.snapshot->union_volume_mm3,united);contains(result.snapshot->repeated_volume_mm3,sum-united);
        limits.max_cells=1;REQUIRE_FALSE(integrate_material_union(present.nominal,box,limits).snapshot);
        limits.max_cells=4;limits.maximum_interval_width=Volume(1e-10);
        REQUIRE_FALSE(integrate_material_union(material_at(ledger,5,.5).nominal,box,limits).snapshot);
        auto unequal=rows;unequal.back()=bead(6,5,rows.back().motion.start,rows.back().motion.end,.91,.25,.25);
        REQUIRE_FALSE(integrate_material_union(material_at(captured(unequal),6,0).nominal,box,limits).snapshot);
        // The outer pair alone has a real vertical void; a misplaced interior
        // cannot turn it into a merged slab even though all three rows exist.
        auto unmerged=rows;unmerged[4].motion.end=point(reverse ? 2 : 0,.02);
        unmerged[5]=bead(6,5,unmerged[4].motion.end,point(reverse ? 0 : 2,.02),.9,.25,.25);
        REQUIRE_FALSE(integrate_material_union(material_at(captured(unmerged),6,0).nominal,box,limits).snapshot);
    }
}


TEST_CASE("B07 joining witness is a continuous common lower-material volume at finite corners", "[Nonplanar][B07][MaterialJoin]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<MaterialJoinSnapshot>::value);
    for (auto kind : {BeadSectionKind::Rectangle,BeadSectionKind::RoundedRectangle}) for (bool slope : {false,true}) for (bool rotated : {false,true}) {
        const double c=rotated ? std::sqrt(.5) : 1,s=rotated ? c : 0;
        const PhysicalPosition corner{2*c,2*s,1};
        const auto sequence=captured({bead(1,0,{0,0,slope ? .96 : 1},corner,.8,.3,.3,kind),
            bead(2,1,corner,{2*c-2*s,2*s+2*c,slope ? 1.04 : 1},.8,.3,.3,kind)});
        const auto prefix=material_at(sequence,2,0);
        const SceneBox region{{corner.x()-.5,corner.y()-.5,.6},{corner.x()+.5,corner.y()+.5,1.1}};
        const auto result=find_material_join(prefix.lower,0,1,region);INFO(result.reason);REQUIRE(result.snapshot);
        REQUIRE(result.snapshot->source==prefix.lower.snapshot);REQUIRE(result.snapshot->first_record==0);REQUIRE(result.snapshot->second_record==1);
        test::independent_join_box(*result.snapshot);
        const auto swapped=find_material_join(prefix.lower,1,0,region);INFO(swapped.reason);REQUIRE(swapped.snapshot);test::independent_join_box(*swapped.snapshot);
        const auto half=material_at(sequence,1,.5);const auto partial=find_material_join(half.lower,0,1,region);INFO(partial.reason);REQUIRE(partial.snapshot);
        REQUIRE(partial.snapshot->source==half.lower.snapshot);test::independent_join_box(*partial.snapshot);
        REQUIRE_FALSE(find_material_join(material_at(sequence,1,0).lower,0,1,region).snapshot);
        REQUIRE_FALSE(find_material_join(material_at(sequence,1,.005).lower,0,1,region).snapshot);
    }
}

TEST_CASE("B07 joining refuses surface-only contact future material and incomplete searches", "[Nonplanar][B07][MaterialJoin]")
{
    const auto straight=captured({bead(1,0,{0,0,1},{2,0,1},.8,.3,.3),bead(2,1,{2,0,1},{4,0,1},.8,.3,.3)},model(0,0));
    REQUIRE_FALSE(find_material_join(material_at(straight,2,0).lower,0,1,{{1.8,-.2,.7},{2.2,.2,1}}).snapshot);
    auto rows=std::vector<MaterialRecord>{bead(1,0,{0,0,1},{2,0,1},.8,.3,.3),bead(2,1,{2,0,1},{2,2,1},.8,.3,.3)};
    const auto eroded=captured(rows,model(.01,.2));
    REQUIRE_FALSE(find_material_join(material_at(eroded,2,0).lower,0,1,{{1.6,-.1,.6},{2.1,.4,1.1}}).snapshot);
    const auto sequence=captured({bead(1,0,{0,0,1},{2,0,1},.8,.3,.3),bead(2,1,{2,0,1},{2,2,1},.8,.3,.3)});
    auto view=material_at(sequence,2,0).lower;const auto source=view.snapshot;SceneBox box{{1.6,-.1,.6},{2.1,.4,1.1}};MaterialJoinLimits limits;
    REQUIRE_FALSE(find_material_join({},0,1,box).snapshot);REQUIRE_FALSE(find_material_join(view,0,0,box).snapshot);
    REQUIRE_FALSE(find_material_join(view,0,2,box).snapshot);REQUIRE_FALSE(find_material_join(view,0,1,{{1.6,0,.8},{1.6,.2,.9}}).snapshot);
    limits.max_evaluations=1;REQUIRE_FALSE(find_material_join(view,0,1,box,limits).snapshot);
    limits={};limits.max_cells=1;REQUIRE_FALSE(find_material_join(view,0,1,box,limits).snapshot);
    limits={};limits.max_depth=1;REQUIRE_FALSE(find_material_join(view,0,1,{{1.6,-.1,.6},{2.1,.4,.76}},limits).snapshot);
    limits={};limits.minimum_box_volume=Volume(1);REQUIRE_FALSE(find_material_join(view,0,1,box,limits).snapshot);
    limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(find_material_join(view,0,1,box,limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(find_material_join(view,0,1,box,limits).snapshot);
    limits={};limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(2));return false;};
    REQUIRE_FALSE(find_material_join(view,0,1,box,limits).snapshot);
    limits={};limits.cancelled=[&] {view.snapshot.reset();box={{0,0,0},{1,1,1}};limits.minimum_box_volume=Volume(1);return false;};
    const auto owned=find_material_join(view,0,1,box,limits);INFO(owned.reason);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->source==source);test::independent_join_box(*owned.snapshot);
    size_t polls=0;limits={};limits.cancelled=[&] {++polls;return false;};
    const SceneBox original{{1.6,-.1,.6},{2.1,.4,1.1}};
    REQUIRE(find_material_join({source},0,1,original,limits).snapshot);const size_t publication_poll=polls;polls=0;
    limits.cancelled=[&] {return ++polls>=publication_poll;};REQUIRE_FALSE(find_material_join({source},0,1,original,limits).snapshot);
    limits={};limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
    const auto rounded=find_material_join({source},0,1,{{1.6,-.1,.6},{2.1,.4,1.1}},limits);
    std::fesetround(FE_TONEAREST);REQUIRE_FALSE(rounded.snapshot);REQUIRE(rounded.reason.find("unsupported interval rounding")!=std::string::npos);
}

TEST_CASE("B07 first-cap joins retain every corner and both interior ends with common lower volumes", "[Nonplanar][B07][FirstCapJoin]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<FirstCapJoinsSnapshot>::value);
    for (auto direction : {HatchDirection::AlongX,HatchDirection::AlongY}) for (bool sloped : {false,true}) {
        const auto candidate=plan_first_cap(first_cap_fixture(direction,sloped),{WidthXY(.45),1,true,Volume(.001)},{{1,-.8,.7},{3,.8,1.84}});
        INFO(candidate.reason);REQUIRE(candidate.snapshot);
        INFO("direction=" << int(direction) << " slope=" << sloped << " numeric=" << candidate.snapshot->numerical_error_upper_mm);
        const auto result=assess_first_cap_joins(candidate);INFO(result.reason);REQUIRE(result.snapshot);const auto &joins=*result.snapshot;
        REQUIRE(joins.source==candidate.snapshot);REQUIRE(joins.joins.size()==4+2*(candidate.snapshot->paths.size()-4));
        for (size_t n=0;n<joins.joins.size();++n) {
            const auto &join=joins.joins[n];REQUIRE(join.material->source==candidate.snapshot->fill->occupied->source);test::independent_join_box(*join.material);
            if (n<4) {REQUIRE(join.first_path==n);REQUIRE(join.second_path==(n+1)%4);}
            else {REQUIRE(join.first_path==4+(n-4)/2);REQUIRE(join.second_path<4);}
        }
        FirstCapJoinLimits limits;limits.max_joins=joins.joins.size()-1;REQUIRE_FALSE(assess_first_cap_joins(candidate,limits).snapshot);
        limits={};limits.max_cells=1;REQUIRE_FALSE(assess_first_cap_joins(candidate,limits).snapshot);
        limits={};limits.max_evaluations=joins.evaluations-1;REQUIRE_FALSE(assess_first_cap_joins(candidate,limits).snapshot);
        limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(assess_first_cap_joins(candidate,limits).snapshot);
        limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(assess_first_cap_joins(candidate,limits).snapshot);
        limits={};limits.max_records=1;REQUIRE_FALSE(assess_first_cap_joins(candidate,limits).snapshot);
        limits={};limits.minimum_box_volume=Volume(1);REQUIRE_FALSE(assess_first_cap_joins(candidate,limits).snapshot);
        auto wrapper=candidate;limits={};limits.cancelled=[&] {wrapper.snapshot.reset();limits.max_joins=1;return false;};
        const auto owned=assess_first_cap_joins(wrapper,limits);INFO(owned.reason);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->source==candidate.snapshot);
        size_t polls=0;limits={};limits.cancelled=[&] {++polls;return false;};
        REQUIRE(assess_first_cap_joins(candidate,limits).snapshot);const size_t publication_poll=polls;polls=0;
        limits.cancelled=[&] {return ++polls>=publication_poll;};REQUIRE_FALSE(assess_first_cap_joins(candidate,limits).snapshot);
    }
    REQUIRE_FALSE(assess_first_cap_joins({}).snapshot);
    const auto lost=plan_first_cap(first_cap_fixture(HatchDirection::AlongX,false,.08),{WidthXY(.45),1,true,Volume(.001)},{{1,-.8,.7},{3,.8,1.84}});
    INFO(lost.reason);REQUIRE(lost.snapshot);REQUIRE(lost.snapshot->fill->covered_target_mm3.lower>0);
    const auto no_join=assess_first_cap_joins(lost);INFO(no_join.reason);REQUIRE_FALSE(no_join.snapshot);
}

TEST_CASE("B07 continuous lower runs retain actual short packets and erode only the owned real ends", "[Nonplanar][B07][MaterialRun]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<MaterialRunSnapshot>::value);
    STATIC_REQUIRE_FALSE(std::is_aggregate<MaterialRunCoverSnapshot>::value);
    for (bool x : {false,true}) for (bool reverse : {false,true}) for (auto kind : {BeadSectionKind::Rectangle,BeadSectionKind::RoundedRectangle}) {
        const auto point=[&](double s,double z) {return PhysicalPosition{x ? s : 0,x ? 0 : s,z};};
        std::vector<MaterialRecord> rows;
        for (size_t i=0;i<10;++i) {
            const double a=(reverse ? 10-i : i)*.02,b=(reverse ? 9-i : i+1)*.02;
            rows.push_back(bead(i+1,i,point(a,1+a*.1),point(b,1+b*.1),i%2 ? .48 : .5,.2+a*.1,.2+b*.1,kind));
        }
        const auto sequence=captured(rows,model(.02,.02));const auto prefix=material_at(sequence,10,0);
        REQUIRE(classify_material(prefix.lower,point(.1,.91)).membership==MaterialMembership::Outside);
        const auto run=reconstruct_material_run(prefix.nominal,0,9);INFO(run.reason);REQUIRE(run.snapshot);
        REQUIRE(run.snapshot->source==prefix.nominal.snapshot);REQUIRE(run.snapshot->first_record==0);REQUIRE(run.snapshot->last_record==9);
        const SceneBox volume=x ? SceneBox{{.06,-.015,.87},{.14,.015,.93}} : SceneBox{{-.015,.06,.87},{.015,.14,.93}};
        const auto covered=cover_material_run_lower(run,volume);INFO(covered.reason);REQUIRE(covered.snapshot);
        REQUIRE(covered.snapshot->source==run.snapshot);test::independent_run_box(*run.snapshot,volume);
        const SceneBox end=x ? SceneBox{{0,-.01,.87},{.03,.01,.93}} : SceneBox{{-.01,0,.87},{.01,.03,.93}};
        REQUIRE_FALSE(cover_material_run_lower(run,end).snapshot);
        const auto partial=material_at(sequence,5,.5);const auto active=reconstruct_material_run(partial.nominal,0,5);INFO(active.reason);REQUIRE(active.snapshot);
        const double lo=reverse ? .12 : .05,hi=reverse ? .15 : .08;
        const SceneBox early=x ? SceneBox{{lo,-.01,.88},{hi,.01,.92}} : SceneBox{{-.01,lo,.88},{.01,hi,.92}};
        const auto now=cover_material_run_lower(active,early);INFO(now.reason);REQUIRE(now.snapshot);test::independent_run_box(*active.snapshot,early);
        const SceneBox future=x ? SceneBox{{.09,-.01,.88},{.11,.01,.92}} : SceneBox{{-.01,.09,.88},{.01,.11,.92}};
        REQUIRE_FALSE(cover_material_run_lower(active,future).snapshot);
        REQUIRE_FALSE(reconstruct_material_run(partial.nominal,0,6).snapshot);
        REQUIRE_FALSE(reconstruct_material_run(material_at(sequence,5,0).nominal,0,5).snapshot);
    }
}

TEST_CASE("B07 run reconstruction refuses interrupted reversed foreign or unsupported packets and inflated-box gaps", "[Nonplanar][B07][MaterialRun]")
{
    auto a=bead(1,0,{0,0,1},{.03,0,1},.5,.2,.2),b=bead(2,1,{.03,0,1},{.06,0,1},.5,.2,.2);
    const auto source=captured({a,b});auto view=material_at(source,2,0).nominal;MaterialLimits capture;
    const auto run=reconstruct_material_run(view,0,1);INFO(run.reason);REQUIRE(run.snapshot);
    REQUIRE_FALSE(reconstruct_material_run({},0,1).snapshot);REQUIRE_FALSE(reconstruct_material_run(view,1,0).snapshot);
    capture.max_records=1;REQUIRE_FALSE(reconstruct_material_run(view,0,1,capture).snapshot);
    capture={};capture.cancelled=[] {return true;};REQUIRE_FALSE(reconstruct_material_run(view,0,1,capture).snapshot);
    capture={};capture.is_current=[](uint64_t){return false;};REQUIRE_FALSE(reconstruct_material_run(view,0,1,capture).snapshot);
    auto reversed=b;reversed=bead(2,1,{.03,0,1},{0,0,1},.5,.2,.2);
    REQUIRE_FALSE(reconstruct_material_run(material_at(captured({a,reversed}),2,0).nominal,0,1).snapshot);
    auto kink=bead(2,1,{.03,0,1},{.03,.03,1},.5,.2,.2);
    REQUIRE_FALSE(reconstruct_material_run(material_at(captured({a,kink}),2,0).nominal,0,1).snapshot);
    auto foreign=b;foreign.motion.source_patch_id=99;
    REQUIRE_FALSE(reconstruct_material_run(material_at(captured({a,foreign}),2,0).nominal,0,1).snapshot);
    MaterialRecord travel{{2,1,0,a.motion.end,b.motion.start,Speed(20),Acceleration(100),Travel{}},{}};
    b.motion.event_id=3;b.motion.sequence_index=2;
    REQUIRE_FALSE(reconstruct_material_run(material_at(captured({a,travel,b}),3,0).nominal,0,2).snapshot);
    const SceneBox box{{.02,-.01,.88},{.04,.01,.92}};MaterialCoverageLimits limits;
    const SceneBox plane{{.02,-.01,.9},{.04,.01,.9}};
    REQUIRE(cover_material_run_lower(run,plane).snapshot);test::independent_run_box(*run.snapshot,plane);
    auto numeric=model();numeric.numerical_coordinate_error=Length(.002);
    const auto charged=reconstruct_material_run(material_at(captured(source->records,numeric),2,0).nominal,0,1);
    const SceneBox skin{{.0105,-.01,.88},{.04,.01,.92}};
    REQUIRE(cover_material_run_lower(run,skin).snapshot);REQUIRE_FALSE(cover_material_run_lower(charged,skin).snapshot);
    limits.max_evaluations=1;REQUIRE_FALSE(cover_material_run_lower(run,box,limits).snapshot);
    limits={};limits.max_cells=1;REQUIRE_FALSE(cover_material_run_lower(run,box,limits).snapshot);
    limits={};limits.max_depth=0;REQUIRE_FALSE(cover_material_run_lower(run,box,limits).snapshot);
    limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(cover_material_run_lower(run,box,limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(cover_material_run_lower(run,box,limits).snapshot);
    auto wrapper=run;limits={};limits.cancelled=[&] {wrapper.snapshot.reset();limits.max_cells=0;return false;};
    const auto owned=cover_material_run_lower(wrapper,box,limits);INFO(owned.reason);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->source==run.snapshot);
    size_t calls=0;capture={};capture.cancelled=[&] {++calls;return false;};REQUIRE(reconstruct_material_run(view,0,1,capture).snapshot);
    size_t late=0;capture.cancelled=[&] {return ++late==calls;};REQUIRE_FALSE(reconstruct_material_run(view,0,1,capture).snapshot);
    calls=0;limits={};limits.cancelled=[&] {++calls;return false;};REQUIRE(cover_material_run_lower(run,box,limits).snapshot);
    late=0;limits.cancelled=[&] {return ++late==calls;};REQUIRE_FALSE(cover_material_run_lower(run,box,limits).snapshot);
    limits={};limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(2));return false;};
    REQUIRE_FALSE(cover_material_run_lower(run,box,limits).snapshot);
    REQUIRE_FALSE(cover_material_run_lower(run,{{.04,-.01,.88},{.02,.01,.92}}).snapshot);
    const auto narrow=captured({bead(1,0,{0,0,1},{.03,0,1},.5,.2,.2),bead(2,1,{.03,0,1},{.06,0,1},.25,.2,.2)});
    const auto thin=reconstruct_material_run(material_at(narrow,2,0).nominal,0,1);REQUIRE(thin.snapshot);
    REQUIRE_FALSE(cover_material_run_lower(thin,{{.022,.11,.88},{.028,.12,.92}}).snapshot);
    const auto floor=captured({bead(1,0,{0,0,1},{.03,0,1},.5,.3,.3),bead(2,1,{.03,0,1},{.06,0,1},.5,.1,.1)});
    const auto stepped=reconstruct_material_run(material_at(floor,2,0).nominal,0,1);REQUIRE(stepped.snapshot);
    REQUIRE_FALSE(cover_material_run_lower(stepped,{{.022,-.01,.8},{.028,.01,.82}}).snapshot);
}

TEST_CASE("B07 run joining retains whole-prefix losses across packet cuts and shared cap proofs", "[Nonplanar][B07][MaterialRunJoin]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<MaterialRunJoinSnapshot>::value);
    STATIC_REQUIRE_FALSE(std::is_aggregate<FirstCapRunJoinsSnapshot>::value);
    for (auto direction : {HatchDirection::AlongX,HatchDirection::AlongY}) for (bool sloped : {false,true}) {
        const auto cap=plan_first_cap(first_cap_fixture(direction,sloped),{WidthXY(.45),1,true,Volume(.001)},{{1,-.8,.7},{3,.8,1.84}});REQUIRE(cap.snapshot);
        const auto joined=assess_first_cap_run_joins(cap);INFO(joined.reason);REQUIRE(joined.snapshot);
        REQUIRE(joined.snapshot->source==cap.snapshot);REQUIRE(joined.snapshot->joins.size()==4+2*(cap.snapshot->paths.size()-4));
        for (const auto &j : joined.snapshot->joins) {
            REQUIRE(j.material->box_volume_mm3.lower>=1e-6);
            test::independent_run_box(*j.material->first_run,j.material->witness);test::independent_run_box(*j.material->second_run,j.material->witness);
        }
        const auto &proof=*joined.snapshot->joins.front().material;
        MaterialRunResult first{"",proof.first_run},second{"",proof.second_run};
        const auto local=find_material_run_join(first,second,proof.domain);INFO(local.reason);REQUIRE(local.snapshot);
        test::independent_run_box(*first.snapshot,local.snapshot->witness);test::independent_run_box(*second.snapshot,local.snapshot->witness);
        REQUIRE_FALSE(find_material_run_join(first,first,proof.domain).snapshot);REQUIRE_FALSE(find_material_run_join({},second,proof.domain).snapshot);
        REQUIRE_FALSE(find_material_run_join(first,second,{{1,0,1},{1,1,2}}).snapshot);
        MaterialJoinLimits generic;generic.minimum_box_volume=Volume(100);REQUIRE_FALSE(find_material_run_join(first,second,proof.domain,generic).snapshot);
        generic={};generic.max_evaluations=1;REQUIRE_FALSE(find_material_run_join(first,second,proof.domain,generic).snapshot);
        generic={};generic.max_cells=1;REQUIRE_FALSE(find_material_run_join(first,second,proof.domain,generic).snapshot);
        generic={};generic.is_current=[](uint64_t){return false;};REQUIRE_FALSE(find_material_run_join(first,second,proof.domain,generic).snapshot);
        generic={};generic.cancelled=[&] {first.snapshot.reset();second.snapshot.reset();generic.max_cells=0;return false;};
        REQUIRE(find_material_run_join(first,second,proof.domain,generic).snapshot);
        FirstCapJoinLimits limits;limits.max_evaluations=joined.evaluations-1;REQUIRE_FALSE(assess_first_cap_run_joins(cap,limits).snapshot);
        limits={};limits.max_cells=1;REQUIRE_FALSE(assess_first_cap_run_joins(cap,limits).snapshot);
        limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(assess_first_cap_run_joins(cap,limits).snapshot);
        limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(assess_first_cap_run_joins(cap,limits).snapshot);
        size_t calls=0;limits={};limits.cancelled=[&] {++calls;return false;};REQUIRE(assess_first_cap_run_joins(cap,limits).snapshot);
        size_t late=0;limits.cancelled=[&] {return ++late==calls;};REQUIRE_FALSE(assess_first_cap_run_joins(cap,limits).snapshot);
    }
    REQUIRE_FALSE(assess_first_cap_run_joins({}).snapshot);
    const auto sequence=captured({bead(1,0,{0,0,1},{1,0,1},.5,.2,.2),bead(2,1,{1,0,1},{1,1,1},.5,.2,.2)});
    const SceneBox domain{{.7,-.2,.75},{1.2,.3,1}};
    const auto partial=material_at(sequence,1,.5);
    const auto first=reconstruct_material_run(partial.nominal,0,0),second=reconstruct_material_run(partial.nominal,1,1);
    const auto joined=find_material_run_join(first,second,domain);INFO(joined.reason);REQUIRE(joined.snapshot);
    test::independent_run_box(*first.snapshot,joined.snapshot->witness);test::independent_run_box(*second.snapshot,joined.snapshot->witness);
    const auto tiny=material_at(sequence,1,.001);
    REQUIRE_FALSE(find_material_run_join(reconstruct_material_run(tiny.nominal,0,0),reconstruct_material_run(tiny.nominal,1,1),domain).snapshot);
    REQUIRE_FALSE(find_material_run_join(first,reconstruct_material_run(material_at(sequence,1,.5).nominal,1,1),domain).snapshot);
    size_t calls=0;MaterialJoinLimits limits;limits.cancelled=[&] {++calls;return false;};REQUIRE(find_material_run_join(first,second,domain,limits).snapshot);
    size_t late=0;limits.cancelled=[&] {return ++late==calls;};REQUIRE_FALSE(find_material_run_join(first,second,domain,limits).snapshot);
    const int rounding=std::fegetround();REQUIRE(std::fesetround(FE_UPWARD)==0);
    const auto invalid_capture=reconstruct_material_run(partial.nominal,0,0);
    const auto invalid_cover=cover_material_run_lower(first,domain);
    const auto invalid_pair=find_material_run_join(first,second,domain);REQUIRE(std::fesetround(rounding)==0);
    REQUIRE_FALSE(invalid_capture.snapshot);REQUIRE_FALSE(invalid_cover.snapshot);REQUIRE_FALSE(invalid_pair.snapshot);
}

TEST_CASE("B06 first-cap flat floors retain volumetric body anchors and continuous nominal contact bounds", "[Nonplanar][B06][FirstCapInterface]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<FirstCapInterfaceSnapshot>::value);
    const FirstCapInterfacePolicy policy{Length(.02),Length(.125),Length(.003),Length(.003),Length(.05)};
    for (auto direction : {HatchDirection::AlongX,HatchDirection::AlongY}) for (bool sloped : {false,true}) {
        FirstHatchLayerLimits construction;construction.beads.packets.maximum_width_error=Length(.002);
        const auto cap=plan_first_cap(first_cap_fixture(direction,sloped),{WidthXY(.45),1,true,Volume(.001)},{{1,-.8,.7},{3,.8,1.84}},construction);REQUIRE(cap.snapshot);
        INFO("direction=" << int(direction) << " sloped=" << sloped << " numeric=" << cap.snapshot->numerical_error_upper_mm);
        const auto result=assess_first_cap_interface(cap,policy);INFO(result.reason);REQUIRE(result.snapshot);
        const auto &proof=*result.snapshot;REQUIRE(proof.source==cap.snapshot);REQUIRE(proof.body==cap.snapshot->source->source->source);
        REQUIRE(proof.anchor_volume_mm3.lower>0);REQUIRE(proof.packets.size()==cap.snapshot->fill->occupied->source->completed_records-(cap.snapshot->paths.size()-4));
        test::independent_lower_box(*proof.body,0,proof.anchor);
        using Q=boost::multiprecision::cpp_bin_float_quad;
        const Q volume=(Q(proof.anchor.max.x())-proof.anchor.min.x())*(Q(proof.anchor.max.y())-proof.anchor.min.y())*(Q(proof.anchor.max.z())-proof.anchor.min.z());
        REQUIRE(proof.anchor_volume_mm3.lower<=volume);REQUIRE(proof.anchor_volume_mm3.upper>=volume);
        for (const auto &p : proof.packets) {
            const auto &piece=cap.snapshot->paths[p.path]->pieces[p.packet];
            const Q loss=Q(proof.body->sequence->model.numerical_coordinate_error.value())+cap.snapshot->numerical_error_upper_mm;
            for (auto endpoint : {std::pair<PhysicalPosition,double>{piece.start,piece.section.gap_begin_mm},{piece.end,piece.section.gap_end_mm}}) {
                const Q floor=Q(endpoint.first.z())-endpoint.second;
                REQUIRE(p.nominal_floor_mm.lower<=floor-loss);REQUIRE(p.nominal_floor_mm.upper>=floor+loss);
                REQUIRE(p.nominal_separation_mm.lower<=floor-1-loss);REQUIRE(p.nominal_separation_mm.upper>=floor-1+loss);
            }
            REQUIRE(p.support_distance_mm.upper<=policy.maximum_support_separation.value());
            REQUIRE(p.nominal_separation_mm.upper<=policy.maximum_nominal_gap.value());REQUIRE(p.nominal_separation_mm.lower>=-policy.maximum_nominal_overlap.value());
            const bool x=piece.start.y()==piece.end.y();
            const Q length=abs(Q(x ? piece.end.x() : piece.end.y())-Q(x ? piece.start.x() : piece.start.y()));
            const Q area=Q(piece.volume.value())/length,h=std::max(Q(piece.section.gap_begin_mm),Q(piece.section.gap_end_mm));
            const Q half=(area/h-acos(Q(-1))*h/4)/2,hmin=std::min(Q(piece.section.gap_begin_mm),Q(piece.section.gap_end_mm));
            const Q maximum=(area/hmin-acos(Q(-1))*hmin/4)/2,centre=x ? piece.start.y() : piece.start.x();
            REQUIRE(p.flat_floor_width_mm.lower<=2*half);REQUIRE(p.flat_floor_width_mm.upper>=2*maximum);
            REQUIRE(p.flat_floor_width_mm.lower>=policy.minimum_flat_floor_width.value());
            REQUIRE(Q(x ? p.floor.min_y : p.floor.min_x)<=centre-maximum);REQUIRE(Q(x ? p.floor.max_y : p.floor.max_x)>=centre+maximum);
        }
    }
}

TEST_CASE("B06 first-cap interface refuses missing volume floating floors and exhausted publication budgets", "[Nonplanar][B06][FirstCapInterface]")
{
    const auto cap=plan_first_cap(first_cap_fixture(HatchDirection::AlongX),{WidthXY(.45),1,true,Volume(.001)},{{1,-.8,.7},{3,.8,1.84}});REQUIRE(cap.snapshot);
    const FirstCapInterfacePolicy policy{Length(.02),Length(.125),Length(.003),Length(.003),Length(.05)};
    const auto result=assess_first_cap_interface(cap,policy);REQUIRE(result.snapshot);
    auto bad=policy;bad.support_depth=Length(.5);REQUIRE_FALSE(assess_first_cap_interface(cap,bad).snapshot);
    bad=policy;bad.maximum_support_separation=Length(.05);REQUIRE_FALSE(assess_first_cap_interface(cap,bad).snapshot);
    const auto coarse=plan_first_cap(first_cap_fixture(HatchDirection::AlongX,true),{WidthXY(.45),1,true,Volume(.001)},{{1,-.8,.7},{3,.8,1.84}});REQUIRE(coarse.snapshot);
    REQUIRE(coarse.snapshot->numerical_error_upper_mm>policy.maximum_nominal_gap.value());REQUIRE_FALSE(assess_first_cap_interface(coarse,policy).snapshot);
    bad=policy;bad.minimum_flat_floor_width=Length(.5);REQUIRE_FALSE(assess_first_cap_interface(cap,bad).snapshot);
    bad=policy;bad.support_depth=Length(0);REQUIRE_FALSE(assess_first_cap_interface(cap,bad).snapshot);
    REQUIRE_FALSE(assess_first_cap_interface({},policy).snapshot);
    FirstCapInterfaceLimits limits;limits.max_evaluations=result.evaluations-1;REQUIRE_FALSE(assess_first_cap_interface(cap,policy,limits).snapshot);
    limits={};limits.max_cells=1;REQUIRE_FALSE(assess_first_cap_interface(cap,policy,limits).snapshot);
    limits={};limits.max_patches=1;REQUIRE_FALSE(assess_first_cap_interface(cap,policy,limits).snapshot);
    limits={};limits.max_records=2;REQUIRE_FALSE(assess_first_cap_interface(cap,policy,limits).snapshot);
    limits={};limits.max_depth=0;REQUIRE_FALSE(assess_first_cap_interface(cap,policy,limits).snapshot);
    limits={};limits.cancelled=[] {return true;};REQUIRE_FALSE(assess_first_cap_interface(cap,policy,limits).snapshot);
    limits={};limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(assess_first_cap_interface(cap,policy,limits).snapshot);
    auto wrapper=cap;auto supplied=policy;limits={};limits.cancelled=[&] {wrapper.snapshot.reset();supplied.support_depth=Length(0);limits.max_cells=0;return false;};
    const auto owned=assess_first_cap_interface(wrapper,supplied,limits);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->source==cap.snapshot);
    size_t calls=0;limits={};limits.cancelled=[&] {++calls;return false;};REQUIRE(assess_first_cap_interface(cap,policy,limits).snapshot);
    size_t late=0;limits.cancelled=[&] {return ++late==calls;};REQUIRE_FALSE(assess_first_cap_interface(cap,policy,limits).snapshot);
}

TEST_CASE("B06 cap interface uses the actual body prefix and keeps future high roofs absent", "[Nonplanar][B06][FirstCapInterface]")
{
    auto first=bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle);
    MaterialRecord lift{{2,1,0,first.motion.end,{10,0,1.01},Speed(20),Acceleration(100),Travel{}},{}};
    auto later=bead(3,2,{10,0,1.01},{0,0,1.01},2,.4,.4,BeadSectionKind::Rectangle);
    const auto body=captured({first,lift,later});
    const AffinePassPolicy pass{4,{VerticalGap(.1),VerticalGap(.4),Length(0)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.001)};
    const FirstCapInterfacePolicy policy{Length(.02),Length(.125),Length(.003),Length(.003),Length(.05)};
    for (auto state : {std::pair<size_t,double>{1,0},{2,.5},{3,0}}) {
        const auto present=material_at(body,state.first,state.second);
        const auto stack=plan_affine_pass_stack(present.lower,{{1,-.8,3,.8},1.8,1.8,1.8},.9,pass);REQUIRE(stack.snapshot);
        const auto hatches=plan_affine_hatches(stack,{WidthXY(.45),Length(.2),Length(.05),HatchDirection::AlongX});REQUIRE(hatches.snapshot);
        const auto cap=plan_first_cap(hatches,{WidthXY(.45),1,true,Volume(.001)},{{1,-.8,.7},{3,.8,1.84}});REQUIRE(cap.snapshot);
        const auto interface=assess_first_cap_interface(cap,policy);INFO(interface.reason);REQUIRE(interface.snapshot);
        REQUIRE(interface.snapshot->body==present.lower.snapshot);test::independent_lower_box(*interface.snapshot->body,0,interface.snapshot->anchor);
        const double roof=state.first==3 ? 1.01 : 1;
        for (const auto &p : interface.snapshot->packets) {REQUIRE(p.nominal_floor_mm.lower<=roof);REQUIRE(p.nominal_floor_mm.upper>=roof);}
        FirstCapInterfaceLimits limits;limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(2));return false;};
        REQUIRE_FALSE(assess_first_cap_interface(cap,policy,limits).snapshot);
        const int previous=std::fegetround();REQUIRE(std::fesetround(FE_UPWARD)==0);
        const auto invalid=assess_first_cap_interface(cap,policy);REQUIRE(std::fesetround(previous)==0);REQUIRE_FALSE(invalid.snapshot);
    }
}

TEST_CASE("B07 vertical shadow separates missing material below and above laid prisms", "[Nonplanar][B07][MaterialVoid]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<MaterialVoidSnapshot>::value);
    STATIC_REQUIRE(material_void_contract_version==1);
    const auto target=flat_fill_target();
    for (double top : {1.1,1.2,1.3,1.5}) for (double fraction : {0.,.3,.5,1.}) {
        const auto material=flat_fill_union(top,fraction);
        const auto fill=reconcile_material_fill(target,material);INFO(fill.reason);REQUIRE(fill.snapshot);
        const auto result=classify_material_voids(fill);INFO(result.reason);REQUIRE(result.snapshot);
        using Q=boost::multiprecision::cpp_bin_float_quad;
        const Q roof=1,ceiling=Q(1.2),height=Q(.2),area=Q(2)*Q(.8),t=top,f=fraction;
        const Q shadow=area*f*std::max(Q(0),std::min(ceiling,t)-roof);
        const Q covered=area*f*std::max(Q(0),std::min(ceiling,t)-std::max(roof,Q(t-height)));
        const auto contains=[](ScalarBounds b,const Q &v) {REQUIRE(Q(b.lower)<=v);REQUIRE(Q(b.upper)>=v);};
        const auto &v=*result.snapshot;REQUIRE(v.source==fill.snapshot);
        contains(v.shadow_target_mm3,shadow);
        contains(v.under_material_missing_mm3,shadow-covered);
        contains(v.vertical_clear_missing_mm3,area*(ceiling-roof)-shadow);
        REQUIRE(v.under_material_missing_mm3.lower>=0);REQUIRE(v.vertical_clear_missing_mm3.lower>=0);
        REQUIRE(v.under_material_missing_mm3.lower+v.vertical_clear_missing_mm3.lower<=fill.snapshot->missing_target_mm3.upper);
        REQUIRE(v.under_material_missing_mm3.upper+v.vertical_clear_missing_mm3.upper>=fill.snapshot->missing_target_mm3.lower);
    }
}

TEST_CASE("B07 rounded shoulder voids match independent circular sections in both axes", "[Nonplanar][B07][MaterialVoid]")
{
    using Q=boost::multiprecision::cpp_bin_float_quad;
    for (bool along_x : {true,false}) for (bool reverse : {false,true}) {
        auto body=bead(1,0,PhysicalPosition(0,0,1),
            along_x ? PhysicalPosition(10,0,1) : PhysicalPosition(0,10,1),2,.4,.4,BeadSectionKind::Rectangle);
        const RectangleXY roi=along_x ? RectangleXY{1,-.5,3,.5} : RectangleXY{-.5,1,.5,3};
        const auto target=integrate_material_first_pass(material_at(captured({body}),1,0).lower,{roi,1.2,1.2,1.2},.9,
            {VerticalGap(.1),VerticalGap(.4),Length(0)});REQUIRE(target.proof);
        auto a=along_x ? PhysicalPosition(1,0,1.2) : PhysicalPosition(0,1,1.2);
        auto b=along_x ? PhysicalPosition(3,0,1.2) : PhysicalPosition(0,3,1.2);
        if (reverse) std::swap(a,b);
        auto row=bead(1,0,a,b,.8,.2,.2);
        auto future=bead(2,1,b,a,1,.2,.2,BeadSectionKind::Rectangle);
        const auto ledger=captured({row,future});
        const SceneBox box{{roi.min_x,roi.min_y,.7},{roi.max_x,roi.max_y,1.6}};
        MaterialFillLimits limits;limits.maximum_interval_width=Volume(.001);
        for (double fraction : {.3,1.}) {
            const auto state=material_at(ledger,fraction==1 ? 1 : 0,fraction==1 ? 0 : fraction);
            const auto occupied=integrate_material_union(state.nominal,box,limits);REQUIRE(occupied.snapshot);
            const auto fill=reconcile_material_fill(target,occupied,limits);INFO(fill.reason);REQUIRE(fill.snapshot);
            const auto result=classify_material_voids(fill,limits);INFO(result.reason);REQUIRE(result.snapshot);
            const Q h=.2,r=h/2,length=2,pi=acos(Q(-1)),amount=std::get<Deposition>(row.motion.payload).volume.value();
            // Actual binary amount fixes the flat core. Two half-circle shoulders
            // leave this exact area below their floor; no height-map solid is used.
            const Q under=length*Q(fraction)*r*r*(2-pi/2);
            const Q shadow=Q(fraction)*amount+under;
            const Q total=length*(Q(1.2)-1);
            const auto contains=[](ScalarBounds bounds,const Q &expected) {REQUIRE(Q(bounds.lower)<=expected);REQUIRE(Q(bounds.upper)>=expected);};
            contains(result.snapshot->under_material_missing_mm3,under);
            contains(result.snapshot->shadow_target_mm3,shadow);
            contains(result.snapshot->vertical_clear_missing_mm3,total-shadow);
            REQUIRE(result.snapshot->under_material_missing_mm3.lower>0);
            REQUIRE(result.snapshot->vertical_clear_missing_mm3.lower>0);
        }
    }
}

TEST_CASE("B07 void classification retains protected inputs and refuses exhausted or stale proofs", "[Nonplanar][B07][MaterialVoid]")
{
    auto fill=reconcile_material_fill(flat_fill_target(),flat_fill_union(1.3));REQUIRE(fill.snapshot);
    const auto source=fill.snapshot;MaterialFillLimits limits;
    fill.reason="FORGED";fill.provisional_above_surface_mm3=ScalarBounds{100,100};
    limits.cancelled=[&] {fill.snapshot.reset();limits.max_cells=1;return false;};
    const auto owned=classify_material_voids(fill,limits);INFO(owned.reason);REQUIRE(owned.snapshot);REQUIRE(owned.snapshot->source==source);
    fill.snapshot=source;limits={};REQUIRE_FALSE(classify_material_voids({},limits).snapshot);
    for (int mode=0;mode<8;++mode) {
        limits={};
        if (mode==0) limits.max_cells=0;
        if (mode==1) limits.max_evaluations=1;
        if (mode==2) limits.max_depth=0;
        if (mode==3) limits.maximum_interval_width=Volume(1e-20);
        if (mode==4) limits.cancelled=[] {return true;};
        if (mode==5) limits.is_current=[](uint64_t) {return false;};
        if (mode==6) {limits.timeout=std::chrono::milliseconds(1);limits.cancelled=[] {std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;};}
        if (mode==7) limits.cancelled=[] {std::fesetround(FE_DOWNWARD);return false;};
        const auto result=classify_material_voids(fill,limits);
        if (mode==7) REQUIRE(std::fesetround(FE_TONEAREST)==0);
        INFO(mode << ' ' << result.reason);REQUIRE_FALSE(result.snapshot);
    }
    limits={};size_t callbacks=0;limits.cancelled=[&] {++callbacks;return false;};
    REQUIRE(classify_material_voids(fill,limits).snapshot);REQUIRE(callbacks>=2);
    const auto total=callbacks;callbacks=0;limits.cancelled=[&] {return ++callbacks==total;};
    REQUIRE_FALSE(classify_material_voids(fill,limits).snapshot);REQUIRE(callbacks==total);
}

TEST_CASE("B07 shadow preserves affine target clipping and counts repeated material once", "[Nonplanar][B07][MaterialVoid]")
{
    const auto body=captured({bead(1,0,{0,0,1},{10,0,1},2,.4,.4,BeadSectionKind::Rectangle)});
    const auto target=integrate_material_first_pass(material_at(body,1,0).lower,{{1,-.4,3,.4},1.1,1.3,1.1},.9,
        {VerticalGap(.05),VerticalGap(.5),Length(0)});REQUIRE(target.proof);
    const auto first=bead(1,0,{2,-.4,1.2},{2,.4,1.2},2,.2,.2,BeadSectionKind::Rectangle);
    const auto second=bead(2,1,first.motion.end,first.motion.start,2,.2,.2,BeadSectionKind::Rectangle);
    for (size_t count : {1,2}) {
        const auto state=material_at(captured({first,second}),count,0);
        const auto occupied=integrate_material_union(state.nominal,{{1,-.4,.7},{3,.4,1.6}});REQUIRE(occupied.snapshot);
        const auto fill=reconcile_material_fill(target,occupied);REQUIRE(fill.snapshot);
        const auto result=classify_material_voids(fill);INFO(result.reason);REQUIRE(result.snapshot);
        // Independent prism minus transverse triangle, irrespective of the
        // duplicate returned bead. Its unfilled upper triangle stays clear.
        volume_contains(result.snapshot->shadow_target_mm3,.28L);
        REQUIRE(result.snapshot->under_material_missing_mm3.upper<.001);
        volume_contains(result.snapshot->vertical_clear_missing_mm3,.04L);
    }
}

TEST_CASE("B07 shadow respects original Z clipping and a diagonal finite footprint", "[Nonplanar][B07][MaterialVoid]")
{
    const auto target=flat_fill_target();
    for (double top : {1.25,1.5}) {
        const auto row=bead(1,0,{1,0,top},{3,0,top},.8,.2,.2,BeadSectionKind::Rectangle);
        const auto occupied=integrate_material_union(material_at(captured({row}),1,0).nominal,{{1,-.4,.7},{3,.4,1.21}});REQUIRE(occupied.snapshot);
        const auto fill=reconcile_material_fill(target,occupied);REQUIRE(fill.snapshot);
        const auto result=classify_material_voids(fill);INFO(result.reason);REQUIRE(result.snapshot);
        if (top==1.5) {
            REQUIRE(result.snapshot->shadow_target_mm3.upper==0);
            REQUIRE(result.snapshot->under_material_missing_mm3.lower==0);
            REQUIRE(result.snapshot->under_material_missing_mm3.upper<1e-12);
            volume_contains(result.snapshot->vertical_clear_missing_mm3,.32L);
        } else {
            volume_contains(result.snapshot->shadow_target_mm3,.32L);
            volume_contains(result.snapshot->under_material_missing_mm3,.08L);
            REQUIRE(result.snapshot->vertical_clear_missing_mm3.upper<.001);
        }
    }
    const auto body=captured({bead(1,0,{0,0,1},{4,4,1},2,.4,.4,BeadSectionKind::Rectangle)});
    const auto diagonal_target=integrate_material_first_pass(material_at(body,1,0).lower,{{1.8,1.8,2.2,2.2},1.2,1.2,1.2},.9,
        {VerticalGap(.1),VerticalGap(.4),Length(0)});REQUIRE(diagonal_target.proof);
    const auto row=bead(1,0,{1,1,1.3},{3,3,1.3},2,.2,.2,BeadSectionKind::Rectangle);
    const auto occupied=integrate_material_union(material_at(captured({row}),1,0).nominal,{{1.8,1.8,.7},{2.2,2.2,1.6}});REQUIRE(occupied.snapshot);
    const auto fill=reconcile_material_fill(diagonal_target,occupied);REQUIRE(fill.snapshot);
    const auto result=classify_material_voids(fill);INFO(result.reason);REQUIRE(result.snapshot);
    volume_contains(result.snapshot->shadow_target_mm3,.032L);
    volume_contains(result.snapshot->under_material_missing_mm3,.016L);
    REQUIRE(result.snapshot->vertical_clear_missing_mm3.upper<.001);
}

TEST_CASE("B07 variable-gap shoulder deficit matches an independent cubic integral", "[Nonplanar][B07][MaterialVoid]")
{
    using Q=boost::multiprecision::cpp_bin_float_quad;
    for (bool along_x : {true,false}) for (bool reverse : {false,true}) {
        const auto body=bead(1,0,{0,0,1},along_x ? PhysicalPosition(10,0,1) : PhysicalPosition(0,10,1),2,.4,.4,BeadSectionKind::Rectangle);
        const RectangleXY roi=along_x ? RectangleXY{1,-.7,3,.7} : RectangleXY{-.7,1,.7,3};
        const AffineCapCell surface{roi,1.125,along_x ? 1.25 : 1.125,along_x ? 1.125 : 1.25};
        const auto target=integrate_material_first_pass(material_at(captured({body}),1,0).lower,surface,.9,
            {VerticalGap(.1),VerticalGap(.4),Length(0)});REQUIRE(target.proof);
        auto a=along_x ? PhysicalPosition(1,0,1.125) : PhysicalPosition(0,1,1.125);
        auto b=along_x ? PhysicalPosition(3,0,1.25) : PhysicalPosition(0,3,1.25);
        if (reverse) std::swap(a,b);
        const double h0=reverse ? .25 : .125,h1=reverse ? .125 : .25;
        const auto row=bead(1,0,a,b,.8,h0,h1);
        const auto ledger=captured({row});const SceneBox box{{roi.min_x,roi.min_y,.7},{roi.max_x,roi.max_y,1.6}};
        for (double fraction : {.3,1.}) {
            const auto state=material_at(ledger,fraction==1 ? 1 : 0,fraction==1 ? 0 : fraction);
            const auto occupied=integrate_material_union(state.nominal,box);REQUIRE(occupied.snapshot);
            const auto fill=reconcile_material_fill(target,occupied);INFO(fill.reason);REQUIRE(fill.snapshot);
            MaterialFillLimits detailed;detailed.max_cells=65535;detailed.max_evaluations=2000000;detailed.timeout=std::chrono::seconds(5);
            const auto result=classify_material_voids(fill,detailed);INFO(result.reason);REQUIRE(result.snapshot);
            const Q f=fraction,h=h0,dh=Q(h1)-h,pi=acos(Q(-1)),amount=std::get<Deposition>(row.motion.payload).volume.value();
            // Integrate h(t)^2 over the actual current prefix. Dyadic endpoints
            // keep the complete flat floor exactly at the original body roof.
            const Q under=Q(2)/4*(2-pi/2)*(h*h*f+h*dh*f*f+dh*dh*f*f*f/3);
            const Q shadow=amount*f+under,total=Q(2)*(Q(.7)-Q(-.7))*Q(.1875);
            const auto contains=[](ScalarBounds bounds,const Q &expected) {REQUIRE(Q(bounds.lower)<=expected);REQUIRE(Q(bounds.upper)>=expected);};
            contains(result.snapshot->under_material_missing_mm3,under);
            contains(result.snapshot->shadow_target_mm3,shadow);
            contains(result.snapshot->vertical_clear_missing_mm3,total-shadow);
        }
        const auto state=material_at(ledger,1,0);
        const auto occupied=integrate_material_union(state.nominal,box);REQUIRE(occupied.snapshot);
        const auto fill=reconcile_material_fill(target,occupied);REQUIRE(fill.snapshot);
        MaterialFillLimits limited;limited.max_cells=1;REQUIRE_FALSE(classify_material_voids(fill,limited).snapshot);
        limited={};limited.max_depth=1;REQUIRE_FALSE(classify_material_voids(fill,limited).snapshot);
    }
}
