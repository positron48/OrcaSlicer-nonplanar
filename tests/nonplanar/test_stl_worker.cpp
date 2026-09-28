#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/StlWorker.hpp>
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <cfenv>

using namespace Slic3r::nptop;
namespace {
namespace fs=boost::filesystem;
std::string fixture()
{
    boost::nowide::ifstream input(NPTOP_STL_FIXTURE_PATH,std::ios::binary);
    REQUIRE(input);
    return std::string(std::istreambuf_iterator<char>(input),{});
}
struct Probes {
    fs::path root=fs::temp_directory_path()/fs::unique_path("nptop-probes-%%%%-%%%%-%%%%");
    Probes() { fs::create_directory(root); fs::permissions(root,fs::owner_all); }
    ~Probes() { boost::system::error_code ec; fs::remove_all(root,ec); }
    std::string program(const std::string &name) {
        const auto path=root/(name+fs::path(NPTOP_PROBE_PATH).extension().string());
        fs::copy_file(NPTOP_PROBE_PATH,path); fs::permissions(path,fs::owner_all);
        return path.string();
    }
};
}
TEST_CASE("B02 native worker binds geometry to the immutable source and revision", "[Nonplanar][B02][Worker]")
{
    auto bytes=fixture();
    const auto original=bytes;
    StlWorkerOptions options;
    options.cancelled=[&] { bytes="changed after capture"; return false; };
    options.is_current=[](uint64_t revision) { return revision==47; };
    const auto result=run_stl_worker(NPTOP_WORKER_PATH,bytes,true,47,options);
    INFO(result.reason);
    REQUIRE(result.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(result.source->bytes==original);
    REQUIRE(result.revision==47);
    REQUIRE(result.faces>0);
    REQUIRE(result.volume_lower_mm3>0);
    REQUIRE(result.volume_lower_mm3<=result.volume_upper_mm3);
    REQUIRE(result.source_error_upper_mm);
    REQUIRE(result.peak_rss_bytes>0);
    REQUIRE(result.peak_rss_bytes<=options.max_peak_rss_bytes);
}
TEST_CASE("B02 worker rejects malformed source units and exceeded observed memory budget", "[Nonplanar][B02][Worker]")
{
    const auto bytes=fixture();
    const auto malformed=run_stl_worker(NPTOP_WORKER_PATH,"not an STL",true,1);
    REQUIRE(malformed.status==MeshAuditStatus::Unknown);
    REQUIRE(malformed.reason=="STL_PARSE_OR_RESOURCE_REJECTION");
    REQUIRE(run_stl_worker(NPTOP_WORKER_PATH,bytes,false,1).reason=="UNCONFIRMED_UNITS");
    StlWorkerOptions options; options.max_peak_rss_bytes=1;
    const auto memory=run_stl_worker(NPTOP_WORKER_PATH,bytes,true,1,options);
    REQUIRE(memory.reason=="WORKER_MEMORY_BUDGET");
    REQUIRE(memory.status==MeshAuditStatus::Unknown);
    REQUIRE_FALSE(memory.source_error_upper_mm);
    REQUIRE(memory.faces==0);
}
TEST_CASE("B02 parent terminates an unresponsive worker on deadline cancellation and stale revision", "[Nonplanar][B02][Worker]")
{
    Probes probes;
    const auto executable=probes.program("hang");
    StlWorkerOptions options; options.timeout=std::chrono::milliseconds(40);
    const auto started=std::chrono::steady_clock::now();
    const auto deadline=run_stl_worker(executable,"abc",true,11,options);
    REQUIRE(deadline.reason=="WORKER_DEADLINE");
    REQUIRE(deadline.status==MeshAuditStatus::Unknown);
    REQUIRE(std::chrono::steady_clock::now()-started<std::chrono::seconds(2));
    int polls=0;
    options={}; options.cancelled=[&] { return ++polls>=3; };
    REQUIRE(run_stl_worker(executable,"abc",true,11,options).reason=="CANCELLED");
    polls=0;
    options={}; options.is_current=[&](uint64_t) { return ++polls<3; };
    REQUIRE(run_stl_worker(executable,"abc",true,11,options).reason=="STALE_REVISION");
    polls=0;
    options={}; options.cancelled=[&] { if (++polls>=3) throw std::runtime_error("callback failed"); return false; };
    REQUIRE(run_stl_worker(executable,"abc",true,11,options).status==MeshAuditStatus::Unknown);
}
TEST_CASE("B02 worker failure invalid protocol wrong identity and oversized reports fail closed", "[Nonplanar][B02][Worker]")
{
    Probes probes;
    for (const auto &scenario : {"failure","terminated","malformed","revision","hash","oversize"}) {
        const auto result=run_stl_worker(probes.program(scenario),"abc",true,11);
        INFO(scenario); INFO(result.reason);
        REQUIRE(result.status==MeshAuditStatus::Unknown);
        REQUIRE(result.faces==0);
        REQUIRE_FALSE(result.source_error_upper_mm);
        if (std::string(scenario)=="revision") REQUIRE(result.reason=="WORKER_IDENTITY_MISMATCH");
        if (std::string(scenario)=="hash") REQUIRE(result.reason=="WORKER_SOURCE_MISMATCH");
    }
    REQUIRE(run_stl_worker((probes.root/"missing-worker").string(),"abc",true,11).status==MeshAuditStatus::Unknown);
}
TEST_CASE("B03 isolated upper analysis binds captured bytes options and nominal bounds", "[Nonplanar][B03][UpperWorker]")
{
    auto bytes=fixture(); const auto original=bytes;
    StlWorkerOptions options; options.analyze_upper=true;
    options.cancelled=[&] { bytes="changed"; options.analyze_upper=false; return false; };
    const auto result=run_stl_worker(NPTOP_WORKER_PATH,bytes,true,53,options);
    INFO(result.reason); INFO(result.upper_reason);
    REQUIRE(result.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(result.source->bytes==original);
    REQUIRE(result.upper);
    REQUIRE(result.upper_reason=="NOMINAL_UPPER_PROJECTION_ONLY");
    REQUIRE(result.upper->upward_faces==32);
    REQUIRE(result.upper->selected_faces==32);
    REQUIRE(result.upper->patches==1);
    REQUIRE(result.upper->affine_patches==1);
    REQUIRE(result.upper->holes==0);
    REQUIRE(result.upper->creases==0);
    REQUIRE(result.upper->area_lower_mm2<=600);
    REQUIRE(result.upper->area_upper_mm2>=600);
    REQUIRE(result.upper->area_upper_mm2-result.upper->area_lower_mm2<1e-8);
    REQUIRE(result.upper->minimum_z_mm==4);
    REQUIRE(result.upper->maximum_z_mm==4);
    REQUIRE(result.upper->selected_slope_upper==0);
    const auto geometry_only=run_stl_worker(NPTOP_WORKER_PATH,original,true,53);
    REQUIRE(geometry_only.status==MeshAuditStatus::ValidGeometry);
    REQUIRE_FALSE(geometry_only.upper);
}
TEST_CASE("B03 upper worker validates the exact summary schema and request kind", "[Nonplanar][B03][UpperWorker]")
{
    Probes probes;
    StlWorkerOptions options; options.analyze_upper=true;
    // The deliberately synthetic protocol control proves the negative probes
    // reach summary validation, rather than failing unrelated source identity.
    const auto control=run_stl_worker(probes.program("upper-control"),"abc",true,11,options);
    REQUIRE(control.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(control.upper);
    REQUIRE(control.upper->upward_faces==2);
    for (const auto *scenario : {"upper-count","upper-type","upper-area","upper-range","upper-affine",
                                "upper-slope","upper-field","upper-null","upper-mode","upper-old"}) {
        const auto result=run_stl_worker(probes.program(scenario),"abc",true,11,options);
        INFO(scenario); INFO(result.reason);
        REQUIRE(result.status==MeshAuditStatus::Unknown);
        REQUIRE_FALSE(result.upper);
        REQUIRE(result.faces==0);
        REQUIRE_FALSE(result.source_error_upper_mm);
        if (std::string(scenario)=="upper-mode") REQUIRE(result.reason=="WORKER_ANALYSIS_MISMATCH");
        else if (std::string(scenario)=="upper-old") REQUIRE(result.reason=="WORKER_IDENTITY_MISMATCH");
        else REQUIRE(result.reason=="WORKER_PROTOCOL_OR_LAUNCH_FAILURE");
    }
    const auto unknown=run_stl_worker(probes.program("upper-unknown"),"abc",true,11,options);
    REQUIRE(unknown.status==MeshAuditStatus::ValidGeometry);
    REQUIRE_FALSE(unknown.upper);
    REQUIRE(unknown.upper_reason=="OVERLAPPING_UPWARD_PROJECTIONS");
}
TEST_CASE("B03 upper worker publishes no partial result after cancellation staleness or rounding changes", "[Nonplanar][B03][UpperWorker]")
{
    const auto bytes=fixture();
    StlWorkerOptions options; options.analyze_upper=true;
    size_t polls=0;
    options.is_current=[&](uint64_t) { ++polls; return true; };
    REQUIRE(run_stl_worker(NPTOP_WORKER_PATH,bytes,true,11,options).upper);
    REQUIRE(polls>=4);
    // Child timing is variable, so count the deterministic callbacks by using
    // an immediately cancelled/stale request as well as a callback mutation.
    for (const bool stale : {false,true}) {
        options.cancelled=[=] { return !stale; };
        options.is_current=[=](uint64_t) { return !stale; };
        const auto result=run_stl_worker(NPTOP_WORKER_PATH,bytes,true,11,options);
        REQUIRE(result.status==MeshAuditStatus::Unknown);
        REQUIRE_FALSE(result.upper);
        REQUIRE(result.reason==(stale ? "STALE_REVISION" : "CANCELLED"));
    }
    struct Rounding { int mode=std::fegetround(); ~Rounding() { std::fesetround(mode); } } restore;
    options.is_current={};
    options.cancelled=[] { std::fesetround(FE_UPWARD); return false; };
    const auto rounding=run_stl_worker(NPTOP_WORKER_PATH,bytes,true,11,options);
    std::fesetround(restore.mode);
    REQUIRE(rounding.status==MeshAuditStatus::Unknown);
    REQUIRE_FALSE(rounding.upper);
    REQUIRE(rounding.reason=="WORKER_PROTOCOL_OR_LAUNCH_FAILURE");
    options.cancelled={}; options.max_peak_rss_bytes=1;
    const auto memory=run_stl_worker(NPTOP_WORKER_PATH,bytes,true,11,options);
    REQUIRE(memory.reason=="WORKER_MEMORY_BUDGET");
    REQUIRE_FALSE(memory.upper);
}
