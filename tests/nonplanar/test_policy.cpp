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
#include <cmath>
#include <limits>
using namespace Slic3r;
using namespace Slic3r::nptop;
namespace {
const std::vector<std::string> custom_code_hooks = {
    "file_start_gcode", "machine_start_gcode", "machine_end_gcode",
    "before_layer_change_gcode", "layer_change_gcode", "time_lapse_gcode",
    "wrapping_detection_gcode", "printing_by_object_gcode", "machine_pause_gcode",
    "template_custom_gcode", "change_filament_gcode", "change_extrusion_role_gcode",
    "process_change_extrusion_role_gcode", "filament_start_gcode", "filament_end_gcode",
    "filament_change_extrusion_role_gcode", "post_process"
};
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
    for (const auto &key : custom_code_hooks) {
        if (c.option(key)->type() == coStrings)
            c.set_key_value(key, new ConfigOptionStrings{});
        else
            c.set_key_value(key, new ConfigOptionString{});
    }
    return c;
}
}
TEST_CASE("B01 unqualified flow and geometry compensators fail preflight", "[Nonplanar][B01][B01Transforms]")
{
    REQUIRE(resolve_policy(eligible(),1,1).passes_config_preflight());
    for (const auto &[key, value] : std::vector<std::pair<std::string,std::string>>{
        {"adaptive_pressure_advance","1"},{"adaptive_pressure_advance_overhangs","1"},
        {"adaptive_pressure_advance_bridges","0.02"},{"filament_adaptive_volumetric_speed","1"},
        {"small_area_infill_flow_compensation","1"},{"max_volumetric_extrusion_rate_slope","10"},
        {"filament_shrink","99%"},{"filament_shrinkage_compensation_z","99%"},
        {"xy_hole_compensation","0.1"},{"xy_contour_compensation","-0.1"},
        {"elefant_foot_compensation","0.2"}}) {
        INFO(key);
        auto config=eligible(); config.set_deserialize_strict(key,value);
        const auto before=config.opt_serialize(key);
        const auto policy=resolve_policy(config,1,1);
        REQUIRE_FALSE(policy.passes_config_preflight());
        REQUIRE(policy.conflicts.front().key == key);
        REQUIRE(config.opt_serialize(key) == before);
        config.set_deserialize_strict("nptop_mode","off");
        REQUIRE(resolve_policy(config,1,1).conflicts.empty());
    }
}
TEST_CASE("B01 neutral values require native types exact numbers and every vector entry", "[Nonplanar][B01][B01Transforms]")
{
    REQUIRE(neutral_transform_policy().size() == 11);
    for (const auto &[key, rule] : neutral_transform_policy()) {
        REQUIRE(print_config_def.get(key) != nullptr);
        REQUIRE(print_config_def.get(key)->type == rule.type);
    }
    for (int mutation=0; mutation<9; ++mutation) {
        INFO(mutation);
        auto config=eligible();
        std::string key="filament_shrink";
        if (mutation==0) config.set_key_value(key,new ConfigOptionPercents{100,std::nextafter(100.,101.)});
        if (mutation==1) config.set_key_value(key,new ConfigOptionPercents{100,std::numeric_limits<double>::quiet_NaN()});
        if (mutation==2) config.set_key_value(key,new ConfigOptionPercents{});
        if (mutation==3) config.set_key_value(key,new ConfigOptionFloats{100});
        if (mutation==4) config.erase(key);
        if (mutation==5) {
            key="adaptive_pressure_advance";
            config.set_key_value(key,new ConfigOptionBools{false,true});
        }
        if (mutation==6) {
            key="filament_adaptive_volumetric_speed";
            config.set_key_value(key,new ConfigOptionBoolsNullable{static_cast<unsigned char>(0),ConfigOptionBoolsNullable::nil_value()});
        }
        if (mutation==7) {
            key="xy_contour_compensation";
            config.set_key_value(key,new ConfigOptionFloat(std::numeric_limits<double>::infinity()));
        }
        if (mutation==8) {
            key="xy_contour_compensation";
            config.set_key_value(key,new ConfigOptionFloat(std::numeric_limits<double>::denorm_min()));
        }
        const auto policy=resolve_policy(config,1,1);
        REQUIRE_FALSE(policy.passes_config_preflight());
        REQUIRE(policy.conflicts.front().key == key);
        if (mutation==0)
            REQUIRE_THAT(policy.conflicts.front().value,Catch::Matchers::ContainsSubstring("100.00000000000001"));
    }
    auto config=eligible();
    config.set_key_value("xy_contour_compensation",new ConfigOptionFloat(-0.));
    REQUIRE(resolve_policy(config,1,1).passes_config_preflight());
}
TEST_CASE("B01 native region override cannot hide an unqualified compensator", "[Nonplanar][B01][B01Transforms]")
{
    Print print; print.is_BBL_printer()=false; Model model;
    auto config=eligible(); Test::init_print({Test::TestMesh::cube_20x20x20},print,model,config);
    model.objects.front()->config.set_key_value("small_area_infill_flow_compensation",new ConfigOptionBool(true));
    print.apply(model,config);
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("small_area_infill_flow_compensation"));
    REQUIRE_THROWS(print.process());
    config.set_deserialize_strict("nptop_mode","off");
    print.apply(model,config);
    REQUIRE(print.nonplanar_block_reason().empty());
}
TEST_CASE("B01 every native custom code hook is rejected before compatibility acceptance", "[Nonplanar][B01][B01Hooks]")
{
    REQUIRE(resolve_policy(eligible(),1,1).passes_config_preflight());
    for (const auto &key : custom_code_hooks) {
        INFO(key);
        auto config=eligible();
        if (config.option(key)->type() == coStrings)
            config.set_key_value(key,new ConfigOptionStrings{"", "G1 X99 E9"});
        else
            config.set_key_value(key,new ConfigOptionString("G1 X99 E9"));
        const auto before=config.opt_serialize(key);
        const auto result=resolve_policy(config,1,1);
        REQUIRE_FALSE(result.passes_config_preflight());
        REQUIRE(result.conflicts.front().key == key);
        REQUIRE(result.resolved.at(key) == before);
        REQUIRE(config.opt_serialize(key) == before);
        config.set_deserialize_strict("nptop_mode","off");
        REQUIRE(resolve_policy(config,1,1).conflicts.empty());
    }
}
TEST_CASE("B01 custom hook preflight rejects missing wrong typed and future fields", "[Nonplanar][B01][B01Hooks]")
{
    for (int mutation=0; mutation<4; ++mutation) {
        auto config=eligible();
        std::string key="machine_start_gcode";
        if (mutation==0) config.erase(key);
        if (mutation==1) config.set_key_value(key,new ConfigOptionStrings{});
        if (mutation==2) config.set_key_value(key,new ConfigOptionString(" \n; comment-only is still unqualified"));
        if (mutation==3) {
            key="future_motion_gcode";
            config.set_key_value(key,new ConfigOptionString{});
        }
        INFO(key << " mutation " << mutation);
        const auto result=resolve_policy(config,1,1);
        REQUIRE_FALSE(result.passes_config_preflight());
        REQUIRE(result.conflicts.front().key == key);
    }
}
TEST_CASE("B01 code registry covers the native definitions and checks every vector entry", "[Nonplanar][B01][B01Hooks]")
{
    size_t native_hooks=0;
    for (const auto &[key, definition] : print_config_def.options) {
        if ((key.find("gcode") != std::string::npos || key == "post_process") &&
            (definition.type == coString || definition.type == coStrings)) {
            INFO(key);
            REQUIRE(custom_code_policy().count(key) == 1);
            REQUIRE(custom_code_policy().at(key).type == definition.type);
            ++native_hooks;
        }
    }
    REQUIRE(native_hooks == custom_code_policy().size());
    REQUIRE(native_hooks == 17);
    auto config=eligible();
    config.set_key_value("filament_start_gcode",new ConfigOptionStrings{"", ""});
    auto snapshot=resolve_policy(config,1,1);
    REQUIRE(snapshot.passes_config_preflight());
    config.option<ConfigOptionStrings>("filament_start_gcode")->values[1]="G1 E20";
    REQUIRE_FALSE(resolve_policy(config,1,1).passes_config_preflight());
    REQUIRE(snapshot.passes_config_preflight());
    REQUIRE(snapshot.resolved.at("filament_start_gcode") != config.opt_serialize("filament_start_gcode"));
    config=eligible();
    config.set_key_value("post_process",new ConfigOptionStrings{""});
    REQUIRE_FALSE(resolve_policy(config,1,1).passes_config_preflight());
}
TEST_CASE("B01 source code checks retain sparse override provenance", "[Nonplanar][B01][B01Hooks]")
{
    for (int location=0; location<4; ++location) {
        Print print; print.is_BBL_printer()=false; Model model;
        auto config=eligible(); Test::init_print({Test::TestMesh::cube_20x20x20},print,model,config);
        auto *object=model.objects.front();
        ModelConfig *source=nullptr;
        if (location==0) source=&object->config;
        if (location==1) source=&object->volumes.front()->config;
        if (location==2) source=&object->layer_config_ranges[{0.,1.}];
        if (location==3) source=&model.add_material("test")->config;
        REQUIRE(source != nullptr);
        REQUIRE_FALSE(model_policy_conflict(model).has_value());
        source->set_key_value("future_motion_gcode",new ConfigOptionString{});
        auto conflict=model_policy_conflict(model);
        REQUIRE(conflict.has_value());
        REQUIRE(conflict->key == "future_motion_gcode");
        source->erase("future_motion_gcode");
        source->set_key_value("filament_end_gcode",new ConfigOptionStrings{"", "G1 Z99"});
        conflict=model_policy_conflict(model);
        REQUIRE(conflict.has_value());
        REQUIRE(conflict->key == "filament_end_gcode");
    }
}
TEST_CASE("B01 actual resolved hooks and plate actions diagnose guarded publication", "[Nonplanar][B01][B01Hooks]")
{
    CachedPrint print; print.is_BBL_printer()=false; Model model;
    auto config=eligible(); Test::init_print({Test::TestMesh::cube_20x20x20},print,model,config);
    model.objects.front()->config.set_key_value("process_change_extrusion_role_gcode",new ConfigOptionString("G1 Z99"));
    print.apply(model,config);
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("process_change_extrusion_role_gcode"));
    model.objects.front()->config.erase("process_change_extrusion_role_gcode");
    for (auto type : {CustomGCode::ColorChange, CustomGCode::PausePrint, CustomGCode::ToolChange,
                      CustomGCode::Template, CustomGCode::Custom, CustomGCode::Unknown}) {
        INFO(static_cast<int>(type));
        // Inactive plates also block: this initial domain permits one plate.
        model.plates_custom_gcodes[7].gcodes={{0.2,type,1,"","G1 Z99"}};
        print.set_started(psGCodeExport);
        print.set_done(psGCodeExport);
        print.apply(model,config);
        REQUIRE_FALSE(print.is_step_done(psGCodeExport));
        REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("plates_custom_gcodes[7]"));
        REQUIRE_THROWS(print.process());
    }
    model.plates_custom_gcodes.clear();
    print.apply(model,config);
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("not implemented"));
    model.plates_custom_gcodes[7].gcodes={{0.2,CustomGCode::Custom,1,"","G1 Z99"}};
    config.set_deserialize_strict("nptop_mode","off");
    print.apply(model,config);
    REQUIRE(print.nonplanar_block_reason().empty());
    REQUIRE(model.plates_custom_gcodes.at(7).gcodes.front().extra == "G1 Z99");
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
