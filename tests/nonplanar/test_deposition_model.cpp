#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/DepositionModel.hpp>
#include <libslic3r/Nonplanar/Collision.hpp>
#include <cmath>
#include <cfenv>
#include <thread>
#include <set>
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <nlohmann/json.hpp>

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
