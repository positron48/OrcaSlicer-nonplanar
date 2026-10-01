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
