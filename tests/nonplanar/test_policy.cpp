#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <libslic3r/Nonplanar/Policy.hpp>
#include <libslic3r/Print.hpp>
#include <libslic3r/GCode.hpp>
#include <libslic3r/Format/bbs_3mf.hpp>
#include <boost/nowide/fstream.hpp>
#include <miniz.h>
#include "../fff_print/test_data.hpp"
#include <boost/filesystem.hpp>
using namespace Slic3r;
using namespace Slic3r::nptop;
namespace {
struct CachedPrint : Print {
    using Print::set_started;
    using Print::set_done;
};
DynamicPrintConfig eligible()
{
    auto c=DynamicPrintConfig::full_print_config();
    c.set_deserialize_strict({{"nptop_mode","safe_hybrid"},{"zaa_enabled",false},{"gcode_flavor","klipper"},
      {"spiral_mode",false},{"enable_arc_fitting",false},{"enable_support",false},{"raft_layers",0},
      {"wall_generator","classic"},{"fuzzy_skin","disabled_fuzzy"},{"ironing_type","no ironing"},
      {"seam_slope_type","none"},{"post_process",""}});
    return c;
}
}
TEST_CASE("B01 policy copies resolved values and accepts only supported planning modes", "[Nonplanar][B01]")
{
    auto c=eligible(); const auto snapshot=resolve_policy(c,1,1);
    REQUIRE(snapshot.passes_config_preflight());
    c.set_deserialize_strict("zaa_enabled","1");
    REQUIRE(snapshot.resolved.at("zaa_enabled") == "0");
    REQUIRE_FALSE(resolve_policy(c,1,1).passes_config_preflight());
    c.set_deserialize_strict("nptop_mode","off");
    REQUIRE(resolve_policy(c,1,1).conflicts.empty());
    for (const auto *value : {"future_mode", "<missing>", "", "OFF"}) {
        c.set_deserialize_strict("nptop_mode",value);
        REQUIRE(resolve_policy(c,1,1).mode == Mode::Invalid);
    }
    c=eligible(); c.set_deserialize_strict("nptop_mode","strict_nonplanar");
    REQUIRE(resolve_policy(c,1,1).passes_config_preflight());
    REQUIRE_FALSE(resolve_policy(c,2,2).passes_config_preflight());
}
TEST_CASE("B01 policy rejects conflicting resolved transforms and missing settings", "[Nonplanar][B01]")
{
    for (const auto &kv : std::vector<std::pair<std::string,std::string>>{
        {"zaa_enabled","1"},{"gcode_flavor","marlin"},{"enable_arc_fitting","1"},
        {"spiral_mode","1"},{"enable_support","1"},{"raft_layers","1"},
        {"wall_generator","arachne"},{"fuzzy_skin","all"},{"ironing_type","top"},
        {"seam_slope_type","all"},{"post_process","/never/execute/this"}}) {
        auto c=eligible(); c.set_deserialize_strict(kv.first,kv.second);
        const auto result=resolve_policy(c,1,1);
        REQUIRE_FALSE(result.passes_config_preflight());
        REQUIRE(result.conflicts.front().key == kv.first);
    }
    DynamicPrintConfig sparse; sparse.set_deserialize_strict("nptop_mode","safe_hybrid");
    REQUIRE_FALSE(resolve_policy(sparse,1,1).passes_config_preflight());
}
TEST_CASE("B01 native paths block guarded mode before any output file or cached result", "[Nonplanar][B01]")
{
    Print print; print.is_BBL_printer()=false; Model model;
    auto config=eligible(); Test::init_print({Test::TestMesh::cube_20x20x20},print,model,config);
    REQUIRE_FALSE(print.nonplanar_block_reason().empty());
    REQUIRE_FALSE(print.validate().string.empty());
    REQUIRE_THROWS(print.process());
    const auto path=boost::filesystem::temp_directory_path()/boost::filesystem::unique_path("nptop-block-%%%%-%%%%.gcode");
    REQUIRE_FALSE(boost::filesystem::exists(path));
    const auto blocked = Catch::Matchers::ContainsSubstring("Nonplanar Top Lab:");
    REQUIRE_THROWS_WITH(print.export_gcode(path.string(),nullptr,nullptr),blocked);
    GCode gcode; REQUIRE_THROWS_WITH(gcode.do_export(&print,path.string().c_str(),nullptr,nullptr),blocked);
    REQUIRE_THROWS_WITH(print.export_gcode_from_previous_file(path.string(),nullptr,nullptr),blocked);
    REQUIRE_FALSE(boost::filesystem::exists(path));
    REQUIRE_FALSE(boost::filesystem::exists(path.string()+".tmp"));
}
TEST_CASE("B01 default OFF remains stock and object region overrides cannot enable export", "[Nonplanar][B01]")
{
    Print print; print.is_BBL_printer()=false; Model model;
    auto c=eligible(); c.set_deserialize_strict("nptop_mode","off");
    Test::init_print({Test::TestMesh::cube_20x20x20},print,model,c);
    REQUIRE(print.nonplanar_block_reason().empty());
    model.objects.front()->config.set_key_value("nptop_mode",new ConfigOptionString("safe_hybrid"));
    print.apply(model,c);
    REQUIRE_FALSE(print.nonplanar_block_reason().empty());
    REQUIRE_THROWS(print.process());
}
TEST_CASE("B01 publication detects unresolved volume and layer overrides", "[Nonplanar][B01]")
{
    for (bool volume : {true,false}) {
        INFO((volume ? "volume" : "layer range"));
        Print print; print.is_BBL_printer()=false; Model model;
        auto c=eligible(); c.set_deserialize_strict("nptop_mode","off");
        Test::init_print({Test::TestMesh::cube_20x20x20},print,model,c);
        REQUIRE_FALSE(requests_guarded_mode(model,c));
        if (volume)
            model.objects.front()->volumes.front()->config.set_key_value("nptop_mode",new ConfigOptionString("strict_nonplanar"));
        else
            model.objects.front()->layer_config_ranges[{0.,1.}].set_key_value("nptop_mode",new ConfigOptionString("future_mode"));
        REQUIRE(requests_guarded_mode(model,c));
        print.apply(model,c);
        REQUIRE_FALSE(print.nonplanar_block_reason().empty());
    }
}
TEST_CASE("ORC-10 policy diagnoses an incompatible resolved region override", "[Nonplanar][B01]")
{
    Print print; print.is_BBL_printer()=false; Model model;
    auto c=eligible(); Test::init_print({Test::TestMesh::cube_20x20x20},print,model,c);
    model.objects.front()->config.set_key_value("zaa_enabled",new ConfigOptionBool(true));
    print.apply(model,c);
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("zaa_enabled = 1"));
    // Turning the region mode off cannot bypass the job's guarded request.
    model.objects.front()->config.set_key_value("nptop_mode",new ConfigOptionString("off"));
    print.apply(model,c);
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("zaa_enabled = 1"));
}
TEST_CASE("B01 mode changes invalidate a cached export and preserve unknown serialized modes", "[Nonplanar][B01]")
{
    CachedPrint print; print.is_BBL_printer()=false; Model model;
    auto c=eligible(); c.set_deserialize_strict("nptop_mode","off");
    Test::init_print({Test::TestMesh::cube_20x20x20},print,model,c);
    print.set_started(psGCodeExport);
    print.set_done(psGCodeExport);
    REQUIRE(print.is_step_done(psGCodeExport));
    c.set_deserialize_strict("nptop_mode","safe_hybrid");
    print.apply(model,c);
    REQUIRE_FALSE(print.is_step_done(psGCodeExport));
    DynamicPrintConfig loaded;
    loaded.load_from_ini_string("nptop_mode = future_mode\n",ForwardCompatibilitySubstitutionRule::Enable);
    REQUIRE(loaded.opt_serialize("nptop_mode") == "future_mode");
    REQUIRE(resolve_policy(loaded,1,1).mode == Mode::Invalid);
    REQUIRE(resolve_policy(DynamicPrintConfig{},1,1).mode == Mode::Off);
}
TEST_CASE("B01 embedded G-code rejects guarded mode without touching existing files", "[Nonplanar][B01]")
{
    CachedPrint print; print.is_BBL_printer()=false; Model model;
    auto c=eligible(); Test::init_print({Test::TestMesh::cube_20x20x20},print,model,c);
    struct TemporaryDirectory {
        boost::filesystem::path path = boost::filesystem::temp_directory_path()/boost::filesystem::unique_path("nptop-archive-%%%%-%%%%");
        TemporaryDirectory() { boost::filesystem::create_directory(path); }
        ~TemporaryDirectory() { boost::system::error_code ec; boost::filesystem::remove_all(path,ec); }
    } directory;
    model.set_backup_path((directory.path/"backup").string());
    const auto archive=(directory.path/"candidate.3mf").string();
    for (const auto &path : {archive,archive+".tmp"}) {
        boost::nowide::ofstream output(path); output << "preserve existing bytes";
    }
    print.set_started(psGCodeExport);
    print.set_done(psGCodeExport);
    GCode gcode;
    REQUIRE_THROWS_WITH(gcode.do_export(&print,archive.c_str(),nullptr,nullptr),
                        Catch::Matchers::ContainsSubstring("Nonplanar Top Lab:"));
    StoreParams params; params.path=archive.c_str(); params.model=&model; params.config=&c;
    params.strategy=SaveStrategy::Silence | SaveStrategy::WithGcode;
    REQUIRE_FALSE(store_bbs_3mf(params));
    for (const auto &path : {archive,archive+".tmp"}) {
        boost::nowide::ifstream input(path);
        const std::string bytes{std::istreambuf_iterator<char>(input),{}};
        REQUIRE(bytes == "preserve existing bytes");
    }
    // A plate-only request also blocks cached archives.
    PlateData plate;
    plate.plate_index=0;
    plate.config.set_deserialize_strict("nptop_mode","strict_nonplanar");
    params.plate_data_list={&plate};
    c.set_deserialize_strict("nptop_mode","off");
    REQUIRE_FALSE(store_bbs_3mf(params));
    c.set_deserialize_strict("nptop_mode","safe_hybrid");
    // Source project persistence is still permitted, without a sliced payload.
    const auto stale_gcode=(directory.path/"cached.gcode").string();
    { boost::nowide::ofstream output(stale_gcode); output << "unchecked cached payload"; }
    plate.gcode_file=stale_gcode;
    plate.is_sliced_valid=true;
    for (auto strategy : {SaveStrategy::Silence, SaveStrategy::Backup}) {
        params.strategy=strategy;
        REQUIRE(store_bbs_3mf(params));
        REQUIRE(boost::filesystem::file_size(archive)>100);
        mz_zip_archive zip{};
        REQUIRE(mz_zip_reader_init_file(&zip,archive.c_str(),0));
        struct CloseZip { mz_zip_archive *zip; ~CloseZip() { mz_zip_reader_end(zip); } } close{&zip};
        for (mz_uint index=0; index<mz_zip_reader_get_num_files(&zip); ++index) {
            mz_zip_archive_file_stat stat{};
            REQUIRE(mz_zip_reader_file_stat(&zip,index,&stat));
            REQUIRE(std::string(stat.m_filename).find(".gcode") == std::string::npos);
        }
        size_t length=0;
        void *data=mz_zip_reader_extract_file_to_heap(&zip,"Metadata/project_settings.config",&length,0);
        REQUIRE(data != nullptr);
        const std::string settings(static_cast<const char *>(data),length);
        mz_free(data);
        REQUIRE_THAT(settings,Catch::Matchers::ContainsSubstring("nptop_mode"));
        REQUIRE_THAT(settings,Catch::Matchers::ContainsSubstring("safe_hybrid"));
    }
}
