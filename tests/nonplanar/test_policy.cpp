#include <catch2/catch_test_macros.hpp>
#include "material_join_oracle.hpp"
#include <catch2/matchers/catch_matchers_string.hpp>
#include <libslic3r/Nonplanar/Policy.hpp>
#include <libslic3r/Nonplanar/InputSnapshot.hpp>
#include <libslic3r/Nonplanar/PlanarBody.hpp>
#include <libslic3r/Nonplanar/DepositionModel.hpp>
#include <libslic3r/ClipperUtils.hpp>
#include <libslic3r/Nonplanar/StlFile.hpp>
#include <libslic3r/Format/STL.hpp>
#include <libslic3r/Print.hpp>
#include <libslic3r/GCode.hpp>
#include <libslic3r/Format/bbs_3mf.hpp>
#include <boost/nowide/fstream.hpp>
#include <miniz.h>
#include "../fff_print/test_data.hpp"
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/filesystem.hpp>
#include <cmath>
#include <cfenv>
#include <limits>
#include <cstring>
#include <iomanip>
#include <set>
#include <type_traits>
#include <thread>
#include <nlohmann/json.hpp>
using namespace Slic3r;
using namespace Slic3r::nptop;

TEST_CASE("B01 Print settings identity matches independent canonical and SHA256 vectors", "[Nonplanar][B01][PrintSnapshot]")
{
    const auto path=boost::filesystem::path(__FILE__).parent_path()/"data/print-config-fingerprint-v2.json";
    boost::nowide::ifstream input(path.string());
    REQUIRE(input.good());
    nlohmann::json fixtures; input>>fixtures;
    for (const auto &test : fixtures) {
        DynamicConfig full, empty;
        std::vector<PrintRegionConfigSnapshot> regions;
        std::optional<PolicyConflict> conflict;
        if (test.at("name")=="mixed") {
            full.set_key_value("a",new ConfigOptionFloat(0.1));
            for (int id : {7,9}) {
                ResolvedConfigSnapshot config(id==7 ? empty : full);
                regions.push_back({0,id,config,{Mode::SafeHybrid,{},{},config}});
            }
            conflict=PolicyConflict{"key","value","reason"};
        }
        const PrintConfigSnapshot snapshot{3,Vec3d(0.1,-0.,2.),1,1,ResolvedConfigSnapshot(full),ResolvedConfigSnapshot(full),
            std::move(regions),std::string("blocked\0reason",14),std::move(conflict),42,"source"};
        CHECK(snapshot.canonical_json()==test.at("canonical").get<std::string>());
        CHECK(snapshot.fingerprint()==test.at("sha256").get<std::string>());
    }
}
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
      {"seam_slope_type","none"},{"post_process",""},
      {"single_extruder_multi_material",false},{"manual_filament_change",false},
      {"enable_prime_tower",false},{"enable_filament_dynamic_map",false},
      {"has_filament_switcher",false},{"filament_map_mode","Manual"},
      {"print_sequence","by layer"},{"sparse_infill_pattern","rectilinear"},
      {"internal_solid_infill_pattern","rectilinear"},{"top_surface_pattern","rectilinear"},
      {"bottom_surface_pattern","rectilinear"},{"infill_combination",false},
      {"detect_thin_wall",false},{"gap_fill_target","nowhere"}});
    for (const auto &key : custom_code_hooks) {
        if (c.option(key)->type() == coStrings)
            c.set_key_value(key, new ConfigOptionStrings{});
        else
            c.set_key_value(key, new ConfigOptionString{});
    }
    return c;
}
}
TEST_CASE("B01 config fingerprint matches independent canonical JSON and SHA256 vectors", "[Nonplanar][B01][B01Fingerprint]")
{
    const auto path=boost::filesystem::path(__FILE__).parent_path()/"data/config-fingerprint-v1.json";
    boost::nowide::ifstream input(path.string());
    REQUIRE(input.good());
    nlohmann::json fixtures; input>>fixtures;
    DynamicConfig source;
    t_config_enum_values names{{"a",1},{"b",2}};
    for (const auto &test : fixtures) {
        if (test.at("name")=="mixed") {
            source.set_key_value("bool",new ConfigOptionBool(true));
            source.set_key_value("float",new ConfigOptionFloat(0.1));
            source.set_key_value("float%",new ConfigOptionFloatOrPercent(0.1,true));
            source.set_key_value("ints",new ConfigOptionInts{-1,0,2147483647});
            const uint64_t raw=0x7ff8000000000001ULL; double nil; std::memcpy(&nil,&raw,sizeof nil);
            source.set_key_value("nil",new ConfigOptionFloatsNullable{nil,-0.,std::numeric_limits<double>::denorm_min()});
            source.set_key_value("point",new ConfigOptionPoint3(Vec3d(1,2,3)));
            source.set_key_value("text",new ConfigOptionStrings{"Привет",std::string("line\n\0tail",10)});
            source.set_key_value("enum",new ConfigOptionEnumGeneric(&names,2));
        }
        const ResolvedConfigSnapshot snapshot(source);
        CHECK(snapshot.canonical_json()==test.at("canonical").get<std::string>());
        CHECK(snapshot.fingerprint()==test.at("sha256").get<std::string>());
    }
}
TEST_CASE("B01 config identity includes exact values types percent flags and unknown keys", "[Nonplanar][B01][B01Fingerprint]")
{
    DynamicConfig source; source.set_key_value("future",new ConfigOptionFloat(0.1));
    const auto original=ResolvedConfigSnapshot(source).fingerprint();
    const auto rounded=source.opt_serialize("future");
    source.option<ConfigOptionFloat>("future")->value=std::nextafter(0.1,1.);
    REQUIRE(source.opt_serialize("future")==rounded);
    REQUIRE(ResolvedConfigSnapshot(source).fingerprint()!=original);
    source.set_key_value("future",new ConfigOptionPercent(0.1));
    REQUIRE(ResolvedConfigSnapshot(source).fingerprint()!=original);
    source.set_key_value("future",new ConfigOptionFloatOrPercent(0.1,false));
    const auto absolute=ResolvedConfigSnapshot(source).fingerprint();
    source.option<ConfigOptionFloatOrPercent>("future")->percent=true;
    REQUIRE(ResolvedConfigSnapshot(source).fingerprint()!=absolute);
    source.set_key_value("another",new ConfigOptionString("unknown but present"));
    const ResolvedConfigSnapshot owned(source);
    const auto frozen=owned.fingerprint();
    source.clear();
    REQUIRE(owned.fingerprint()==frozen);
    REQUIRE(ResolvedConfigSnapshot(source).fingerprint()!=frozen);
}
TEST_CASE("B01 canonical config captures all actual native options and nested mutations", "[Nonplanar][B01][B01Fingerprint]")
{
    const auto full=eligible();
    const ResolvedConfigSnapshot snapshot(full);
    REQUIRE(snapshot.fingerprint().size()==64);
    const auto encoded=nlohmann::json::parse(snapshot.canonical_json());
    REQUIRE(encoded.at("options").size()==full.keys().size());
    REQUIRE(encoded.at("schema")==1);
    DynamicConfig nested;
    nested.set_key_value("points",new ConfigOptionPointsGroups{{Vec2d(1,2),Vec2d(3,4)}});
    nested.set_key_value("ints",new ConfigOptionIntsGroups{{1,2},{3}});
    nested.set_key_value("percents",new ConfigOptionFloatsOrPercents{{1,false},{2,true}});
    const auto before=ResolvedConfigSnapshot(nested).fingerprint();
    nested.option<ConfigOptionPointsGroups>("points")->values.front().front().y()=std::nextafter(2.,3.);
    REQUIRE(ResolvedConfigSnapshot(nested).fingerprint()!=before);
}
TEST_CASE("B01 fingerprints preserve enum dictionaries nullable flags and every binary64 bit", "[Nonplanar][B01][B01Fingerprint]")
{
    DynamicConfig source;
    t_config_enum_values names{{"alpha",1},{"beta",2}};
    source.set_key_value("enum",new ConfigOptionEnumsGeneric(&names,2,1));
    const ResolvedConfigSnapshot frozen(source);
    const auto old=frozen.fingerprint();
    names["future"]=3;
    REQUIRE(ResolvedConfigSnapshot(source).fingerprint()!=old);
    names.clear(); source.clear();
    REQUIRE(frozen.fingerprint()==old);
    source.set_key_value("vector",new ConfigOptionEnumsGeneric{1,2});
    const auto required=ResolvedConfigSnapshot(source).fingerprint();
    source.set_key_value("vector",new ConfigOptionEnumsGenericNullable{1,2});
    REQUIRE(ResolvedConfigSnapshot(source).fingerprint()!=required);
    source.clear();
    std::set<std::string> fingerprints;
    for (uint64_t bits : {0ULL,0x8000000000000000ULL,1ULL,0x7ff8000000000001ULL,0x7ff8000000000002ULL,0x7ff0000000000000ULL}) {
        double value; std::memcpy(&value,&bits,sizeof value);
        source.set_key_value("number",new ConfigOptionFloatsNullable{value});
        const ResolvedConfigSnapshot snapshot(source);
        REQUIRE(fingerprints.insert(snapshot.fingerprint()).second);
        // Hex strings represent raw invalid/nil states without JSON NaN/Inf.
        REQUIRE(nlohmann::json::parse(snapshot.canonical_json()).is_object());
    }
    const auto exact=ResolvedConfigSnapshot(source).canonical_json();
    struct RestoreRounding { ~RestoreRounding() { std::fesetround(FE_TONEAREST); } } restore;
    std::fesetround(FE_DOWNWARD);
    REQUIRE(ResolvedConfigSnapshot(source).canonical_json()==exact);
}
TEST_CASE("B01 canonical config rejects unsupported native representations and bounded output overflow", "[Nonplanar][B01][B01Fingerprint]")
{
    struct Unsupported : ConfigOptionString {
        ConfigOptionType type() const override { return coNone; }
        ConfigOption *clone() const override { return new Unsupported(*this); }
    };
    DynamicConfig source; source.set_key_value("unsupported",new Unsupported);
    REQUIRE_THROWS_AS(ResolvedConfigSnapshot(source).canonical_json(),ConfigurationError);
    source.clear(); source.set_key_value("large",new ConfigOptionString(std::string(2*1024*1024,'x')));
    REQUIRE_THROWS_AS(ResolvedConfigSnapshot(source).canonical_json(),std::length_error);
    source.clear();
    for (int i=0; i<4097; ++i) source.set_key_value(std::to_string(i),new ConfigOptionInt(i));
    REQUIRE_THROWS_AS(ResolvedConfigSnapshot(source).fingerprint(),std::length_error);
}
TEST_CASE("B01 wrong custom-code types reject before their native serializers run", "[Nonplanar][B01][B01HookTypes]")
{
    struct UnserializableCode : ConfigOptionInt {
        size_t *attempts;
        explicit UnserializableCode(size_t &count) : ConfigOptionInt(999),attempts(&count) {}
        ConfigOptionType type() const override { return coEnum; }
        ConfigOption *clone() const override { return new UnserializableCode(*this); }
        std::string serialize() const override { ++*attempts; throw std::runtime_error("unchecked native enum serializer"); }
    };
    for (const auto &key : custom_code_hooks) {
        INFO(key);
        size_t attempts=0;
        auto config=eligible(); config.set_key_value(key,new UnserializableCode(attempts));
        Model model;
        CHECK_NOTHROW([&] {
            const auto conflict=input_policy_conflict(model,config);
            REQUIRE(conflict);
            CHECK(conflict->key==key);
            CHECK(conflict->value=="<wrong-native-type:9>");
        }());
        CHECK_NOTHROW([&] {
            const auto resolved=resolve_policy(config,1,1);
            CHECK_FALSE(resolved.passes_config_preflight());
            REQUIRE_FALSE(resolved.conflicts.empty());
            CHECK(resolved.conflicts.front().key==key);
            CHECK(resolved.resolved.at(key)=="<wrong-native-type:9>");
        }());
        CHECK(attempts==0);
        config.set_deserialize_strict("nptop_mode","off");
        CHECK_FALSE(input_policy_conflict(model,config));
        CHECK(resolve_policy(config,1,1).mode==Mode::Off);
    }
}
TEST_CASE("B01 single tool rejects extra diameters maps offsets and malformed native values", "[Nonplanar][B01][B01SingleTool]")
{
    REQUIRE(resolve_policy(eligible(),1,1).passes_config_preflight());
    REQUIRE(single_tool_policy().size() == 14);
    for (const auto &[key, type] : single_tool_policy())
        REQUIRE(print_config_def.get(key)->type == type);
    for (const auto *key : {"nozzle_diameter","filament_diameter"}) {
        for (const auto &values : std::vector<std::vector<double>>{
            {},{0.4,0.4},{0.},{-1.},{std::numeric_limits<double>::quiet_NaN()},
            {std::numeric_limits<double>::infinity()},{std::numeric_limits<double>::denorm_min()}}) {
            INFO(key);
            auto config=eligible(); config.set_key_value(key,new ConfigOptionFloats(values));
            const auto policy=resolve_policy(config,1,1);
            CHECK_FALSE(policy.passes_config_preflight());
            if (!policy.conflicts.empty()) CHECK(policy.conflicts.front().key == key);
        }
    }
    for (const auto *key : {"filament_map","physical_extruder_map"}) {
        const int required=std::string(key) == "filament_map" ? 1 : 0;
        for (const auto &values : std::vector<std::vector<int>>{{},{required,required},{required+1},{-1}}) {
            INFO(key);
            auto config=eligible(); config.set_key_value(key,new ConfigOptionInts(values));
            CHECK_FALSE(resolve_policy(config,1,1).passes_config_preflight());
        }
    }
    for (const auto &values : std::vector<Pointfs>{
        {},{Vec2d::Zero(),Vec2d::Zero()},{Vec2d(0.01,0)},
        {Vec2d(0,std::numeric_limits<double>::denorm_min())},
        {Vec2d(std::numeric_limits<double>::quiet_NaN(),0)}}) {
        auto config=eligible(); config.set_key_value("extruder_offset",new ConfigOptionPoints(values));
        CHECK_FALSE(resolve_policy(config,1,1).passes_config_preflight());
    }
    auto config=eligible(); config.set_key_value("extruder_offset",new ConfigOptionPoints{Vec2d(-0.,0.)});
    CHECK(resolve_policy(config,1,1).passes_config_preflight());
    for (const auto *key : {"nozzle_diameter","filament_diameter","filament_map","physical_extruder_map","extruder_offset"}) {
        config=eligible(); config.set_key_value(key,new ConfigOptionString(config.opt_serialize(key)));
        CHECK_FALSE(resolve_policy(config,1,1).passes_config_preflight());
        config=eligible(); config.erase(key);
        CHECK_FALSE(resolve_policy(config,1,1).passes_config_preflight());
    }
    config=eligible(); config.set_key_value("filament_diameter",new ConfigOptionFloatsNullable{1.75});
    CHECK_FALSE(resolve_policy(config,1,1).passes_config_preflight());
}
TEST_CASE("B01 fixed mapping rejects automatic material switching and tower modes", "[Nonplanar][B01][B01SingleTool]")
{
    for (const auto *key : {"single_extruder_multi_material","manual_filament_change","enable_prime_tower",
                           "enable_filament_dynamic_map","has_filament_switcher"}) {
        INFO(key);
        auto config=eligible(); config.set_key_value(key,new ConfigOptionBool(true));
        const auto policy=resolve_policy(config,1,1);
        CHECK_FALSE(policy.passes_config_preflight());
        if (!policy.conflicts.empty()) CHECK(policy.conflicts.front().key == key);
    }
    // The native enum includes fmmDefault, but its text dictionary does not.
    for (auto mode : {fmmAutoForFlush,fmmAutoForMatch,fmmDefault}) {
        auto config=eligible(); config.set_key_value("filament_map_mode",new ConfigOptionEnum<FilamentMapMode>(mode));
        CHECK_FALSE(resolve_policy(config,1,1).passes_config_preflight());
    }
}
TEST_CASE("B01 single filament source selections allow inheritance but not normalization of other IDs", "[Nonplanar][B01][B01SingleTool]")
{
    for (const auto *key : {"extruder","sparse_infill_filament_id","outer_wall_filament_id","inner_wall_filament_id",
                           "internal_solid_filament_id","top_surface_filament_id","bottom_surface_filament_id",
                           "support_filament","support_interface_filament"}) {
        INFO(key);
        for (int id : {0,1,-1,2,999}) {
            auto config=eligible(); config.set_key_value(key,new ConfigOptionInt(id));
            CHECK(resolve_policy(config,1,1).passes_config_preflight() == (id == 0 || id == 1));
            Model model; auto *object=model.add_object();
            auto *volume=object->add_volume(TriangleMesh(its_make_cube(1,1,1)));
            auto *material=model.add_material("test-material");
            for (auto *source : std::vector<ModelConfig *>{&material->config,&object->config,&volume->config,&object->layer_config_ranges[{0.,1.}]}) {
                source->set_key_value(key,new ConfigOptionInt(id));
                CHECK(model_policy_conflict(model).has_value() == (id != 0 && id != 1));
                source->erase(key);
            }
        }
        auto config=eligible(); config.set_key_value(key,new ConfigOptionString("1"));
        CHECK_FALSE(resolve_policy(config,1,1).passes_config_preflight());
    }
}
TEST_CASE("B01 material painting invalidates guarded cached output and remains intact in OFF", "[Nonplanar][B01][B01SingleTool]")
{
    CachedPrint print; print.is_BBL_printer()=false; Model model;
    auto config=eligible(); Test::init_print({Test::TestMesh::cube_20x20x20},print,model,config);
    auto &painting=model.objects.front()->volumes.front()->mmu_segmentation_facets;
    painting.set_triangle_from_string(0,"4");
    painting.touch();
    REQUIRE(model.is_mm_painted());
    print.set_started(psGCodeExport); print.set_done(psGCodeExport);
    print.apply(model,config);
    CHECK_FALSE(print.is_step_done(psGCodeExport));
    CHECK_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("mmu_segmentation_facets"));
    REQUIRE_THROWS(print.process());
    config.set_deserialize_strict("nptop_mode","off"); print.apply(model,config);
    CHECK(print.nonplanar_block_reason().empty());
    CHECK(model.is_mm_painted());
    painting.reset(); config.set_deserialize_strict("nptop_mode","safe_hybrid"); print.apply(model,config);
    CHECK_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("not implemented"));
}
TEST_CASE("B01 source tool selections reach native Print before fallback can hide them", "[Nonplanar][B01][B01SingleTool]")
{
    unsigned cancellations=0;
    CachedPrint print; print.is_BBL_printer()=false; Model model;
    auto config=eligible(); Test::init_print({Test::TestMesh::cube_20x20x20},print,model,config);
    auto &source=model.objects.front()->config;
    print.set_cancel_callback([&] { ++cancellations; });
    source.set_key_value("extruder",new ConfigOptionInt(2));
    print.set_started(psGCodeExport); print.set_done(psGCodeExport);
    print.apply(model,config);
    REQUIRE_FALSE(print.is_step_done(psGCodeExport));
    REQUIRE(cancellations > 0);
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("extruder = 2"));
    REQUIRE(source.get().option<ConfigOptionInt>("extruder")->value == 2);
    REQUIRE_THROWS(print.process());
    source.erase("extruder"); print.apply(model,config);
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("not implemented"));
    config.set_key_value("enable_prime_tower",new ConfigOptionBool(true));
    print.set_started(psGCodeExport); print.set_done(psGCodeExport);
    print.apply(model,config);
    REQUIRE_FALSE(print.is_step_done(psGCodeExport));
    REQUIRE_FALSE(print.full_print_config().option<ConfigOptionBool>("enable_prime_tower")->value);
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("enable_prime_tower"));
    config.set_key_value("enable_prime_tower",new ConfigOptionBool(false));
    print.set_started(psGCodeExport); print.set_done(psGCodeExport);
    print.apply(model,config);
    REQUIRE_FALSE(print.is_step_done(psGCodeExport));
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("not implemented"));
    config.set_key_value("enable_prime_tower",new ConfigOptionBool(true));
    print.apply(model,config);
    config.set_deserialize_strict("nptop_mode","off"); print.apply(model,config);
    REQUIRE(print.nonplanar_block_reason().empty());
    REQUIRE(config.option<ConfigOptionBool>("enable_prime_tower")->value);
    print.set_cancel_callback([] {});
}
TEST_CASE("B01 resolved snapshot owns every native value without a mutable configuration API", "[Nonplanar][B01][B01Snapshot]")
{
    static_assert(std::is_same_v<decltype(std::declval<const ResolvedConfigSnapshot &>().option("x")), const ConfigOption *>);
    static_assert(!std::is_base_of_v<ConfigBase, ResolvedConfigSnapshot>);
    auto config=eligible();
    const auto snapshot=resolve_policy(config,1,1);
    REQUIRE(snapshot.passes_config_preflight());
    REQUIRE(snapshot.native_config.has_value());
    const auto &copy=*snapshot.native_config;
    REQUIRE(copy.keys() == config.keys());
    for (const auto &key : config.keys()) {
        INFO(key);
        REQUIRE(copy.option(key) != config.option(key));
        REQUIRE(copy.option(key)->type() == config.option(key)->type());
        REQUIRE(copy.option(key)->nullable() == config.option(key)->nullable());
        REQUIRE(*copy.option(key) == *config.option(key));
    }
    config.option<ConfigOptionFloat>("layer_height")->value=0.77;
    REQUIRE(copy.option<ConfigOptionFloat>("layer_height")->value != 0.77);
    config.clear();
    REQUIRE(copy.option<ConfigOptionString>("nptop_mode")->value == "safe_hybrid");
    REQUIRE(snapshot.passes_config_preflight());
    config=eligible(); config.set_deserialize_strict("nptop_mode","off");
    REQUIRE_FALSE(resolve_policy(config,1,1).native_config.has_value());
}
TEST_CASE("B01 discrete constraints reject textual substitutes for native settings", "[Nonplanar][B01][B01Discrete]")
{
    REQUIRE(discrete_policy().size() == 24);
    for (const auto &[key, rule] : discrete_policy()) {
        REQUIRE(print_config_def.get(key)->type == rule.type);
        if (rule.type == coEnum) REQUIRE(print_config_def.get(key)->enum_keys_map->at(rule.label) == rule.required);
    }
    for (const auto &[key, rule] : discrete_policy()) {
        INFO(key);
        auto config=eligible();
        const auto text=config.opt_serialize(key);
        config.set_key_value(key,new ConfigOptionString(text));
        const auto policy=resolve_policy(config,1,1);
        CHECK_FALSE(policy.passes_config_preflight());
        if (!policy.conflicts.empty()) CHECK(policy.conflicts.front().key == key);
        config=eligible(); config.erase(key);
        CHECK_FALSE(resolve_policy(config,1,1).passes_config_preflight());
    }
}
TEST_CASE("B01 discrete source and invalid enum values fail without unsafe serialization", "[Nonplanar][B01][B01Discrete]")
{
    auto config=eligible();
    config.set_key_value("fuzzy_skin",new ConfigOptionEnum<FuzzySkinType>(static_cast<FuzzySkinType>(-1)));
    const auto policy=resolve_policy(config,1,1);
    REQUIRE_FALSE(policy.passes_config_preflight());
    REQUIRE(policy.conflicts.front().key == "fuzzy_skin");
    REQUIRE(policy.conflicts.front().value == "-1");
    Model model; auto *object=model.add_object();
    object->config.set_key_value("raft_layers",new ConfigOptionString("0"));
    const auto conflict=model_policy_conflict(model);
    REQUIRE(conflict.has_value());
    REQUIRE(conflict->key == "raft_layers");
    object->config.set_key_value("raft_layers",new ConfigOptionInt(0));
    REQUIRE_FALSE(model_policy_conflict(model).has_value());
}
TEST_CASE("B01 discrete enum constraints use native values rather than borrowed labels", "[Nonplanar][B01][B01Discrete]")
{
    for (const auto *key : {"gcode_flavor","wall_generator","fuzzy_skin","ironing_type","seam_slope_type","filament_map_mode",
                          "print_sequence","sparse_infill_pattern","internal_solid_infill_pattern",
                          "top_surface_pattern","bottom_surface_pattern","gap_fill_target"}) {
        INFO(key);
        auto config=eligible();
        const auto label=config.opt_serialize(key);
        const int required=config.option(key)->getInt();
        t_config_enum_values misleading{{label,required+1}};
        config.set_key_value(key,new ConfigOptionEnumGeneric(&misleading,required+1));
        const auto policy=resolve_policy(config,1,1);
        CHECK_FALSE(policy.passes_config_preflight());
        if (!policy.conflicts.empty()) CHECK(policy.conflicts.front().key == key);
    }
}
TEST_CASE("B01 basic native paths reject unqualified patterns ordering and variable layers", "[Nonplanar][B01][B01Paths]")
{
    REQUIRE(resolve_policy(eligible(),1,1).passes_config_preflight());
    for (const auto &[key,value] : std::vector<std::pair<std::string,std::string>>{
        {"print_sequence","by object"},{"sparse_infill_pattern","grid"},
        {"internal_solid_infill_pattern","monotonic"},{"top_surface_pattern","concentric"},
        {"bottom_surface_pattern","monotonic"},{"infill_combination","1"},
        {"detect_thin_wall","1"},{"gap_fill_target","everywhere"}}) {
        INFO(key);
        auto config=eligible(); config.set_deserialize_strict(key,value);
        const auto before=ResolvedConfigSnapshot(config).fingerprint();
        const auto policy=resolve_policy(config,1,1);
        CHECK_FALSE(policy.passes_config_preflight());
        if (!policy.conflicts.empty()) CHECK(policy.conflicts.front().key==key);
        const auto conflict=input_policy_conflict(Model{},config);
        CHECK(conflict);
        if (conflict) CHECK(conflict->key==key);
        CHECK(ResolvedConfigSnapshot(config).fingerprint()==before);
        config.set_deserialize_strict("nptop_mode","off");
        CHECK(resolve_policy(config,1,1).conflicts.empty());
        CHECK_FALSE(input_policy_conflict(Model{},config));
    }
}
TEST_CASE("B01 native path overrides invalidate cached exports without replacing user choices", "[Nonplanar][B01][B01Paths]")
{
    CachedPrint print; print.is_BBL_printer()=false; Model model;
    auto config=eligible(); Test::init_print({Test::TestMesh::cube_20x20x20},print,model,config);
    print.apply(model,config);
    print.set_started(psGCodeExport); print.set_done(psGCodeExport);
    auto *object=model.objects.front();
    object->volumes.front()->config.set_key_value("sparse_infill_pattern",new ConfigOptionEnum<InfillPattern>(ipGrid));
    print.apply(model,config);
    CHECK_FALSE(print.is_step_done(psGCodeExport));
    CHECK_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("sparse_infill_pattern"));
    CHECK(object->volumes.front()->config.get().option<ConfigOptionEnum<InfillPattern>>("sparse_infill_pattern")->value==ipGrid);
    object->volumes.front()->config.erase("sparse_infill_pattern");
    object->layer_config_ranges[{0.,1.}].set_key_value("infill_combination",new ConfigOptionBool(true));
    CHECK(model_policy_conflict(model));
    if (const auto conflict=model_policy_conflict(model)) CHECK(conflict->key=="infill_combination");
    config.set_deserialize_strict("nptop_mode","off");
    print.apply(model,config);
    CHECK(print.nonplanar_block_reason().empty());
}
TEST_CASE("B01 mode routing rejects a non-string OFF or guarded-mode lookalike", "[Nonplanar][B01][B01Discrete]")
{
    for (const auto *text : {"off","safe_hybrid","strict_nonplanar"}) {
        auto config=eligible();
        config.set_key_value("nptop_mode",new ConfigOptionStrings{text});
        CHECK(requests_guarded_mode(config));
        const auto policy=resolve_policy(config,1,1);
        CHECK(policy.mode == Mode::Invalid);
        CHECK_FALSE(policy.passes_config_preflight());
    }
}
TEST_CASE("B01 uniform-layer domain rejects owned custom height profiles without rewriting them", "[Nonplanar][B01][B01Layering]")
{
    Model model; auto *object=model.add_object();
    REQUIRE_FALSE(model_policy_conflict(model).has_value());
    // Even a flat custom profile needs its own qualification; this initial
    // domain uses the ordinary native uniform-layer generator only.
    for (const auto &profile : std::vector<std::vector<double>>{
        {0.,0.2,20.,0.3},{0.,0.2,20.,0.2},{0.,-1.}}) {
        object->layer_height_profile.set(profile);
        const auto conflict=model_policy_conflict(model);
        REQUIRE(conflict.has_value());
        REQUIRE_THAT(conflict->key,Catch::Matchers::ContainsSubstring("layer_height_profile"));
        REQUIRE(object->layer_height_profile.get() == profile);
        object->layer_height_profile.clear();
        REQUIRE_FALSE(model_policy_conflict(model).has_value());
    }
}
TEST_CASE("B01 uniform-layer domain rejects layer-height range overrides before normalization", "[Nonplanar][B01][B01Layering]")
{
    Model model; auto *object=model.add_object();
    auto &range=object->layer_config_ranges[{0.,1.}];
    range.set_key_value("wall_loops",new ConfigOptionInt(3));
    REQUIRE_FALSE(model_policy_conflict(model).has_value());
    for (double height : {0.1,0.2,std::numeric_limits<double>::quiet_NaN()}) {
        range.set_key_value("layer_height",new ConfigOptionFloat(height));
        const auto conflict=model_policy_conflict(model);
        REQUIRE(conflict.has_value());
        REQUIRE_THAT(conflict->key,Catch::Matchers::ContainsSubstring("layer_config_ranges"));
        REQUIRE(range.has("layer_height"));
        range.erase("layer_height");
        REQUIRE_FALSE(model_policy_conflict(model).has_value());
    }
}
TEST_CASE("B01 native height edits invalidate cached output and OFF preserves the source profile", "[Nonplanar][B01][B01Layering]")
{
    CachedPrint print; print.is_BBL_printer()=false; Model model;
    auto config=eligible(); Test::init_print({Test::TestMesh::cube_20x20x20},print,model,config);
    auto *object=model.objects.front();
    const std::vector<double> profile{0.,0.2,20.,0.3};
    object->layer_height_profile.set(profile);
    print.set_started(psGCodeExport); print.set_done(psGCodeExport);
    print.apply(model,config);
    REQUIRE_FALSE(print.is_step_done(psGCodeExport));
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("layer_height_profile"));
    REQUIRE_THROWS(print.process());
    config.set_deserialize_strict("nptop_mode","off");
    print.apply(model,config);
    REQUIRE(print.nonplanar_block_reason().empty());
    REQUIRE(object->layer_height_profile.get() == profile);
    object->layer_height_profile.clear();
    object->layer_config_ranges[{0.,1.}].set_key_value("layer_height",new ConfigOptionFloat(0.1));
    config.set_deserialize_strict("nptop_mode","safe_hybrid");
    print.set_started(psGCodeExport); print.set_done(psGCodeExport);
    print.apply(model,config);
    REQUIRE_FALSE(print.is_step_done(psGCodeExport));
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("layer_config_ranges"));
    object->layer_config_ranges.clear(); print.apply(model,config);
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("not implemented"));
}
TEST_CASE("B01 resolved snapshot preserves exact numeric nullable and nested values", "[Nonplanar][B01][B01Snapshot]")
{
    const double precise=std::nextafter(0.2,1.);
    const double nil=ConfigOptionFloatsNullable::nil_value();
    DynamicConfig source;
    source.set_key_value("future_number",new ConfigOptionFloatOrPercent(precise,true));
    source.set_key_value("future_vector",new ConfigOptionFloatsNullable{precise,nil,-0.});
    source.set_key_value("future_text",new ConfigOptionStrings{"alpha","beta"});
    source.set_key_value("future_points",new ConfigOptionPointsGroups{{Vec2d(precise,3),Vec2d(4,5)}});
    const ResolvedConfigSnapshot snapshot(source);
    source.option<ConfigOptionFloatOrPercent>("future_number")->value=1;
    source.option<ConfigOptionFloatsNullable>("future_vector")->values.clear();
    source.option<ConfigOptionStrings>("future_text")->values[1]="changed";
    source.option<ConfigOptionPointsGroups>("future_points")->values.front().front().x()=9;
    source.clear();
    const auto *number=snapshot.option<ConfigOptionFloatOrPercent>("future_number");
    REQUIRE(number->value == precise);
    REQUIRE(number->percent);
    const auto *vector=dynamic_cast<const ConfigOptionFloatsNullable *>(snapshot.option("future_vector"));
    REQUIRE(vector != nullptr);
    REQUIRE(vector->values.size() == 3);
    REQUIRE(std::memcmp(&vector->values[0],&precise,sizeof(double)) == 0);
    REQUIRE(std::isnan(vector->values[1]));
    REQUIRE(std::signbit(vector->values[2]));
    REQUIRE(snapshot.option<ConfigOptionStrings>("future_text")->values[1] == "beta");
    REQUIRE(snapshot.option<ConfigOptionPointsGroups>("future_points")->values.front().front().x() == precise);
    REQUIRE(snapshot.option("absent") == nullptr);
}
TEST_CASE("B01 resolved snapshot owns generic enum dictionaries as well as values", "[Nonplanar][B01][B01Snapshot]")
{
    const auto snapshot=[] {
        t_config_enum_values names{{"alpha",0},{"beta",1}};
        DynamicConfig source;
        source.set_key_value("scalar",new ConfigOptionEnumGeneric(&names,1));
        source.set_key_value("vector",new ConfigOptionEnumsGeneric(&names,2,0));
        source.set_key_value("nullable",new ConfigOptionEnumsGenericNullable(&names,2,1));
        const ResolvedConfigSnapshot captured(source);
        names={{"changed",0},{"changed_too",1}};
        CHECK(captured.option("scalar")->serialize() == "beta");
        CHECK(captured.option("vector")->serialize() == "alpha,alpha");
        CHECK(captured.option("nullable")->serialize() == "beta,beta");
        // A copied snapshot must outlive its original handle, source values
        // and enum maps without borrowing any of them.
        return ResolvedConfigSnapshot(captured);
    }();
    CHECK(snapshot.option("scalar")->serialize() == "beta");
    CHECK(snapshot.option("vector")->serialize() == "alpha,alpha");
    CHECK(snapshot.option("nullable")->serialize() == "beta,beta");
    DynamicConfig invalid;
    invalid.set_key_value("enum",new ConfigOptionEnumGeneric{});
    REQUIRE_THROWS_AS(ResolvedConfigSnapshot(invalid),ConfigurationError);
    DynamicConfig unmapped;
    unmapped.set_key_value("vector",new ConfigOptionEnumsGeneric{0,1});
    unmapped.set_key_value("nullable",new ConfigOptionEnumsGenericNullable{1,0});
    const ResolvedConfigSnapshot null_maps(unmapped);
    REQUIRE(null_maps.option<ConfigOptionEnumsGeneric>("vector")->keys_map == nullptr);
    REQUIRE(null_maps.option<ConfigOptionEnumsGeneric>("vector")->values == std::vector<int>{0,1});
    REQUIRE(dynamic_cast<const ConfigOptionEnumsGenericNullable *>(null_maps.option("nullable"))->keys_map == nullptr);
    REQUIRE(null_maps.option<ConfigOptionEnumsGenericNullable>("nullable")->values == std::vector<int>{1,0});
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
    REQUIRE(neutral_transform_policy().size() == 27);
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
TEST_CASE("B01 native extrusion multipliers require exact neutral captured values", "[Nonplanar][B01][B01Flow]")
{
    const std::vector<std::string> keys={"filament_flow_ratio","print_flow_ratio","bridge_flow","internal_bridge_flow",
        "top_solid_infill_flow_ratio","bottom_solid_infill_flow_ratio","first_layer_flow_ratio","outer_wall_flow_ratio",
        "inner_wall_flow_ratio","overhang_flow_ratio","sparse_infill_flow_ratio","internal_solid_infill_flow_ratio",
        "gap_fill_flow_ratio","support_flow_ratio","support_interface_flow_ratio","brim_flow_ratio"};
    REQUIRE(resolve_policy(eligible(),1,1).passes_config_preflight());
    for (const auto &key : keys) {
        INFO(key);
        for (double value : {0.,0.95,1.05,std::nextafter(1.,2.),std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity()}) {
            auto config=eligible();
            if (key=="filament_flow_ratio") config.set_key_value(key,new ConfigOptionFloats{value});
            else config.set_key_value(key,new ConfigOptionFloat(value));
            const auto before=ResolvedConfigSnapshot(config).fingerprint();
            const auto policy=resolve_policy(config,1,1);
            CHECK_FALSE(policy.passes_config_preflight());
            if (!policy.conflicts.empty()) CHECK(policy.conflicts.front().key==key);
            CHECK(ResolvedConfigSnapshot(config).fingerprint()==before);
            const auto conflict=input_policy_conflict(Model{},config);
            CHECK(conflict);
            if (conflict) CHECK(conflict->key==key);
            config.set_deserialize_strict("nptop_mode","off");
            CHECK(resolve_policy(config,1,1).conflicts.empty());
            CHECK_FALSE(input_policy_conflict(Model{},config));
        }
        auto config=eligible(); config.erase(key);
        CHECK_FALSE(resolve_policy(config,1,1).passes_config_preflight());
        config.set_key_value(key,new ConfigOptionString("1"));
        CHECK_FALSE(resolve_policy(config,1,1).passes_config_preflight());
    }
}
TEST_CASE("B01 every filament flow entry and resolved role override remains unqualified", "[Nonplanar][B01][B01Flow]")
{
    for (const auto &values : std::vector<std::vector<double>>{{},{1.,0.95},{1.,std::nextafter(1.,2.)}}) {
        auto config=eligible(); config.set_key_value("filament_flow_ratio",new ConfigOptionFloats(values));
        CHECK_FALSE(resolve_policy(config,1,1).passes_config_preflight());
        CHECK(input_policy_conflict(Model{},config));
    }
    Print print; print.is_BBL_printer()=false; Model model;
    auto config=eligible(); Test::init_print({Test::TestMesh::cube_20x20x20},print,model,config);
    model.objects.front()->config.set_key_value("top_solid_infill_flow_ratio",new ConfigOptionFloat(1.05));
    const auto value=model.objects.front()->config.opt_float("top_solid_infill_flow_ratio");
    print.apply(model,config);
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("top_solid_infill_flow_ratio"));
    REQUIRE_THROWS(print.process());
    REQUIRE(model.objects.front()->config.opt_float("top_solid_infill_flow_ratio")==value);
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

TEST_CASE("B01 Print captures full object region and native plate settings as one owned snapshot", "[Nonplanar][B01][PrintSnapshot]")
{
    Model model;
    auto *object=model.add_object();
    object->add_volume(TriangleMesh(Slic3r::its_make_cube(10,10,10)));
    auto second=TriangleMesh(Slic3r::its_make_cube(10,10,10)); second.translate(20,0,0);
    object->add_volume(std::move(second));
    object->add_instance();
    object->config.set_key_value("wall_loops",new ConfigOptionInt(3));
    object->volumes.back()->config.set_key_value("wall_loops",new ConfigOptionInt(5));
    object->config.set_key_value("nptop_mode",new ConfigOptionString("off"));
    auto config=eligible();
    Print print; print.apply(model,config);
    print.set_plate_index(3); print.set_plate_origin(Vec3d(0.1,-0.,2.));
    const auto snapshot=capture_print_config(print);
    REQUIRE(snapshot);
    REQUIRE(snapshot->object_count==1);
    REQUIRE(snapshot->instance_count==1);
    REQUIRE(snapshot->plate_index==3);
    REQUIRE(snapshot->plate_origin_mm==Vec3d(0.1,-0.,2.));
    REQUIRE(snapshot->full_config.option<ConfigOptionString>("nptop_mode")->value=="safe_hybrid");
    REQUIRE(snapshot->regions.size()==2);
    std::set<int> walls, ids;
    for (const auto &region : snapshot->regions) {
        REQUIRE(region.object_index==0);
        REQUIRE(region.policy.passes_config_preflight());
        REQUIRE(region.config.option<ConfigOptionString>("nptop_mode")->value=="safe_hybrid");
        REQUIRE(region.policy.native_config->optptr("wall_loops")==region.config.optptr("wall_loops"));
        walls.insert(region.config.option<ConfigOptionInt>("wall_loops")->value);
        ids.insert(region.region_id);
    }
    REQUIRE(walls==std::set<int>{3,5});
    REQUIRE(ids.size()==2);
    REQUIRE_THAT(snapshot->block_reason(),Catch::Matchers::ContainsSubstring("not implemented"));
    REQUIRE(print.nonplanar_block_reason()==snapshot->block_reason());
    const auto identity=snapshot->fingerprint();
    config.set_deserialize_strict("travel_speed","100");
    object->config.set_key_value("wall_loops",new ConfigOptionInt(7));
    print.apply(model,config); print.set_plate_index(4); print.set_plate_origin(Vec3d(1,2,3));
    REQUIRE(capture_print_config(print)->fingerprint()!=identity);
    print.clear(); model.clear_objects(); config.clear();
    REQUIRE(snapshot->fingerprint()==identity);
    REQUIRE(snapshot->regions.front().config.option<ConfigOptionInt>("wall_loops")->value!=7);
    REQUIRE(snapshot->plate_index==3);
    REQUIRE(snapshot->plate_origin_mm.x()==0.1);
}
TEST_CASE("B01 Print snapshot identity distinguishes plate counts and exact full or resolved settings", "[Nonplanar][B01][PrintSnapshot]")
{
    Model model; auto *object=model.add_object();
    object->add_volume(TriangleMesh(Slic3r::its_make_cube(10,10,10))); object->add_instance();
    auto config=eligible(); Print print; print.apply(model,config);
    const auto original=capture_print_config(print);
    REQUIRE(original);
    const auto identity=original->fingerprint();
    print.set_plate_index(1); REQUIRE(capture_print_config(print)->fingerprint()!=identity);
    print.set_plate_index(0); print.set_plate_origin(Vec3d(std::numeric_limits<double>::denorm_min(),0,0));
    REQUIRE(capture_print_config(print)->fingerprint()!=identity);
    print.set_plate_origin(Vec3d(-0.,0,0)); REQUIRE(capture_print_config(print)->fingerprint()!=identity);
    print.set_plate_origin(Vec3d::Zero()); REQUIRE(capture_print_config(print)->fingerprint()==identity);
    const auto speed=config.option<ConfigOptionFloat>("travel_speed")->value;
    const auto adjacent=std::nextafter(speed,speed+1);
    config.set_key_value("travel_speed",new ConfigOptionFloat(adjacent));
    print.apply(model,config);
    // Native apply retains the cached value, but the raw-input identity must
    // still invalidate dependent guarded results.
    REQUIRE(print.full_print_config().option<ConfigOptionFloat>("travel_speed")->value==speed);
    REQUIRE(capture_print_config(print)->fingerprint()!=identity);
    Print fresh; fresh.apply(model,config);
    REQUIRE(fresh.full_print_config().option<ConfigOptionFloat>("travel_speed")->value==adjacent);
    REQUIRE(capture_print_config(fresh)->fingerprint()!=identity);
    config=eligible(); object->config.set_key_value("wall_loops",new ConfigOptionInt(5));
    print.apply(model,config); REQUIRE(capture_print_config(print)->fingerprint()!=identity);
    object->config.erase("wall_loops"); object->add_instance()->set_offset(Vec3d(30,0,0));
    print.apply(model,config);
    const auto copies=capture_print_config(print);
    REQUIRE(copies->instance_count==2);
    REQUIRE(copies->fingerprint()!=identity);
    REQUIRE_THAT(copies->block_reason(),Catch::Matchers::ContainsSubstring("instance_count"));
}
TEST_CASE("B01 Print snapshots retain pre-normalization and source conflicts without mutable aliases", "[Nonplanar][B01][PrintSnapshot]")
{
    Model model; auto *object=model.add_object();
    object->add_volume(TriangleMesh(Slic3r::its_make_cube(10,10,10))); object->add_instance();
    auto config=eligible(); config.set_key_value("extruder",new ConfigOptionInt(2));
    Print print; print.apply(model,config);
    const auto raw=capture_print_config(print);
    REQUIRE(raw);
    REQUIRE_THAT(raw->input_conflict,Catch::Matchers::ContainsSubstring("extruder = 2"));
    REQUIRE(raw->block_reason()==print.nonplanar_block_reason());
    const auto raw_identity=raw->fingerprint();
    config=eligible(); print.apply(model,config);
    REQUIRE(capture_print_config(print)->fingerprint()!=raw_identity);
    REQUIRE_THAT(raw->block_reason(),Catch::Matchers::ContainsSubstring("extruder = 2"));
    object->layer_height_profile.set({0.,0.2,10.,0.1}); print.apply(model,config);
    const auto custom=capture_print_config(print);
    REQUIRE(custom->model_conflict);
    REQUIRE_THAT(custom->block_reason(),Catch::Matchers::ContainsSubstring("layer_height_profile"));
    const auto custom_identity=custom->fingerprint();
    print.clear(); model.clear_objects(); config.clear();
    REQUIRE(raw->fingerprint()==raw_identity);
    REQUIRE(custom->fingerprint()==custom_identity);
}
TEST_CASE("B01 Print snapshot rejects invalid plate state and preserves the OFF capture bypass", "[Nonplanar][B01][PrintSnapshot]")
{
    Model model; auto *object=model.add_object();
    object->add_volume(TriangleMesh(Slic3r::its_make_cube(10,10,10))); object->add_instance();
    auto config=eligible(); Print print; print.apply(model,config);
    for (const Vec3d origin : {Vec3d(std::numeric_limits<double>::quiet_NaN(),0,0),
                              Vec3d(0,std::numeric_limits<double>::infinity(),0),Vec3d(0,0,10001)}) {
        print.set_plate_origin(origin);
        const auto snapshot=capture_print_config(print);
        REQUIRE(snapshot);
        REQUIRE_THAT(snapshot->block_reason(),Catch::Matchers::ContainsSubstring("plate_origin"));
        REQUIRE(print.nonplanar_block_reason()==snapshot->block_reason());
    }
    print.set_plate_origin(Vec3d::Zero()); print.set_plate_index(-1);
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("plate_index"));
    config.set_deserialize_strict("nptop_mode","off"); print.apply(model,config);
    REQUIRE_FALSE(capture_print_config(print));
    REQUIRE(print.nonplanar_block_reason().empty());
    // A region-only request must still capture a full OFF job, and an empty
    // guarded Print must retain the missing-object preflight rejection.
    print.set_plate_index(0);
    object->config.set_key_value("nptop_mode",new ConfigOptionString("safe_hybrid"));
    print.apply(model,config);
    const auto overridden=capture_print_config(print);
    REQUIRE(overridden);
    REQUIRE(overridden->full_config.option<ConfigOptionString>("nptop_mode")->value=="off");
    REQUIRE(overridden->regions.front().policy.mode==Mode::SafeHybrid);
    Model empty; config=eligible(); print.apply(empty,config);
    REQUIRE_THAT(capture_print_config(print)->block_reason(),Catch::Matchers::ContainsSubstring("object_count"));
}

TEST_CASE("B01 Print settings capture and combined canonical output enforce aggregate limits", "[Nonplanar][B01][PrintSnapshot]")
{
    Model model; auto *object=model.add_object(); object->add_instance();
    for (int i=1; i<=257; ++i) {
        auto *volume=object->add_volume(TriangleMesh(Slic3r::its_make_cube(1,1,1)));
        volume->config.set_key_value("wall_loops",new ConfigOptionInt(i));
    }
    auto config=eligible(); Print print; print.apply(model,config);
    REQUIRE(print.objects().front()->all_regions().size()==257);
    REQUIRE_THROWS_AS(capture_print_config(print),std::length_error);
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("capture failed"));
    DynamicConfig large; large.set_key_value("x",new ConfigOptionString(std::string(700000,'x')));
    const ResolvedConfigSnapshot owned(large);
    REQUIRE(owned.canonical_json().size()<4*1024*1024);
    std::vector<PrintRegionConfigSnapshot> regions;
    for (int i=0; i<3; ++i) regions.push_back({0,i,owned,{Mode::SafeHybrid,{},{},owned}});
    const PrintConfigSnapshot oversized{0,Vec3d::Zero(),1,1,owned,owned,std::move(regions),{}, {}};
    REQUIRE_THROWS_AS(oversized.canonical_json(),std::length_error);
    REQUIRE_THROWS_AS(oversized.fingerprint(),std::length_error);
}

TEST_CASE("B01 Print snapshot binds effective native filament overrides alongside full settings", "[Nonplanar][B01][PrintSnapshot]")
{
    Model model; auto *object=model.add_object();
    object->add_volume(TriangleMesh(Slic3r::its_make_cube(10,10,10))); object->add_instance();
    auto config=eligible();
    config.set_key_value("retraction_length",new ConfigOptionFloats{0.8});
    config.set_key_value("filament_retraction_length",new ConfigOptionFloatsNullable{1.25});
    Print print; print.apply(model,config);
    REQUIRE(print.full_print_config().option<ConfigOptionFloats>("retraction_length")->values==std::vector<double>{0.8});
    REQUIRE(print.config().retraction_length.values==std::vector<double>{1.25});
    const auto snapshot=capture_print_config(print);
    REQUIRE(snapshot);
    REQUIRE(snapshot->full_config.option<ConfigOptionFloats>("retraction_length")->values==std::vector<double>{0.8});
    REQUIRE(snapshot->resolved_print_config.option<ConfigOptionFloats>("retraction_length")->values==std::vector<double>{1.25});
    REQUIRE(snapshot->regions.front().config.option<ConfigOptionFloats>("retraction_length")->values==std::vector<double>{1.25});
    const auto original=snapshot->fingerprint();
    config.set_key_value("filament_retraction_length",new ConfigOptionFloatsNullable{1.5}); print.apply(model,config);
    REQUIRE(capture_print_config(print)->fingerprint()!=original);
    print.clear(); model.clear_objects(); config.clear();
    REQUIRE(snapshot->fingerprint()==original);
    REQUIRE(snapshot->resolved_print_config.option<ConfigOptionFloats>("retraction_length")->values==std::vector<double>{1.25});
}

TEST_CASE("B01 native input identity matches independent byte vectors", "[Nonplanar][B01][InputSnapshot]")
{
    const auto path=boost::filesystem::path(__FILE__).parent_path()/"data/input-fingerprint-v1.json";
    boost::nowide::ifstream input(path.string()); REQUIRE(input.good());
    nlohmann::json fixture; input>>fixture;
    indexed_triangle_set mesh;
    REQUIRE(native_mesh_fingerprint(mesh)==fixture.at("meshes").at(0).at("sha256").get<std::string>());
    mesh.vertices={Vec3f(0.1f,-0.f,1.f),Vec3f(2.f,0.f,1.f),Vec3f(0.f,2.f,1.f)};
    mesh.indices={stl_triangle_vertex_indices(0,1,2)}; mesh.properties={{eNormal,0.125}};
    REQUIRE(native_mesh_fingerprint(mesh)==fixture.at("meshes").at(1).at("sha256").get<std::string>());
    mesh.vertices.front().x()=std::nextafter(0.1f,1.f);
    REQUIRE(native_mesh_fingerprint(mesh)!=fixture.at("meshes").at(1).at("sha256").get<std::string>());
    Model model; DynamicConfig config; config.set_key_value("nptop_mode",new ConfigOptionString("safe_hybrid"));
    const auto snapshot=capture_native_input(model,config); REQUIRE(snapshot);
    REQUIRE(snapshot->canonical_json==fixture.at("input").at("canonical").get<std::string>());
    REQUIRE(snapshot->fingerprint==fixture.at("input").at("sha256").get<std::string>());
}
TEST_CASE("B01 native input owns meshes transforms raw overrides and annotations", "[Nonplanar][B01][InputSnapshot]")
{
    Model model; auto *object=model.add_object();
    auto *volume=object->add_volume(TriangleMesh(its_make_cube(10,10,10))); auto *instance=object->add_instance();
    instance->set_offset(Vec3d(0.1,0,0)); volume->source.mesh_offset=Vec3d(2,3,4);
    object->config.set_key_value("wall_loops",new ConfigOptionInt(4));
    volume->config.set_key_value("wall_loops",new ConfigOptionInt(5));
    auto config=eligible(); const auto snapshot=capture_native_input(model,config); REQUIRE(snapshot);
    const auto identity=snapshot->fingerprint;
    REQUIRE(snapshot->objects.front().volumes.front().mesh.vertices==volume->mesh().its.vertices);
    REQUIRE(snapshot->objects.front().instances.front().transform.matrix()==instance->get_matrix().matrix());
    REQUIRE(snapshot->objects.front().volumes.front().source_offset==Vec3d(2,3,4));
    instance->set_offset(Vec3d(0.2,0,0));
    REQUIRE(capture_native_input(model,config)->fingerprint!=identity);
    instance->set_offset(Vec3d(0.1,0,0)); REQUIRE(capture_native_input(model,config)->fingerprint==identity);
    volume->source.is_converted_from_inches=true; REQUIRE(capture_native_input(model,config)->fingerprint!=identity);
    volume->source.is_converted_from_inches=false;
    volume->config.set_key_value("wall_loops",new ConfigOptionInt(6));
    REQUIRE(capture_native_input(model,config)->fingerprint!=identity);
    volume->config.set_key_value("wall_loops",new ConfigOptionInt(5));
    volume->seam_facets.set_triangle_from_string(0,"1");
    const auto painted=capture_native_input(model,config); REQUIRE(painted->fingerprint!=identity);
    REQUIRE_FALSE(painted->objects.front().volumes.front().annotations[1].triangles_to_split.empty());
    volume->reset_mesh(); model.clear_objects(); config.clear();
    REQUIRE(snapshot->fingerprint==identity);
    REQUIRE(snapshot->objects.front().volumes.front().mesh.indices.size()==12);
    REQUIRE(snapshot->objects.front().volumes.front().config.option<ConfigOptionInt>("wall_loops")->value==5);
    REQUIRE_FALSE(painted->objects.front().volumes.front().annotations[1].triangles_to_split.empty());
}
TEST_CASE("B01 pre-apply exact source changes invalidate native export even when Orca retains cached settings", "[Nonplanar][B01][InputSnapshot]")
{
    Model model; auto *object=model.add_object(); object->add_volume(TriangleMesh(its_make_cube(10,10,10))); object->add_instance();
    auto config=eligible(); CachedPrint print; print.apply(model,config);
    const auto initial=print.nonplanar_input(); REQUIRE(initial.snapshot); REQUIRE(initial.revision>0);
    const auto identity=capture_print_config(print)->fingerprint();
    const auto speed=config.option<ConfigOptionFloat>("travel_speed")->value;
    print.set_started(psGCodeExport); print.set_done(psGCodeExport);
    unsigned cancellations=0; print.set_cancel_callback([&] { ++cancellations; });
    config.set_key_value("travel_speed",new ConfigOptionFloat(std::nextafter(speed,speed+1)));
    print.apply(model,config); const auto changed=print.nonplanar_input(); REQUIRE(changed.snapshot);
    REQUIRE(changed.revision>initial.revision); REQUIRE(changed.snapshot->fingerprint!=initial.snapshot->fingerprint);
    REQUIRE_FALSE(print.is_step_done(psGCodeExport)); REQUIRE(cancellations>0);
    REQUIRE(print.full_print_config().option<ConfigOptionFloat>("travel_speed")->value==speed);
    REQUIRE(capture_print_config(print)->fingerprint()!=identity);
    const auto adjacent=changed.snapshot->config.option<ConfigOptionFloat>("travel_speed")->value;
    REQUIRE(adjacent!=speed);
    print.apply(model,config); REQUIRE(print.nonplanar_input().revision==changed.revision);
    config.set_deserialize_strict("nptop_mode","off"); print.apply(model,config);
    const auto off=print.nonplanar_input(); REQUIRE_FALSE(off.snapshot); REQUIRE(off.revision>changed.revision);
    REQUIRE(print.nonplanar_block_reason().empty());
    print.apply(model,config); REQUIRE(print.nonplanar_input().revision==off.revision);
    config=eligible(); print.apply(model,config); const auto restored=print.nonplanar_input();
    REQUIRE(restored.snapshot); REQUIRE(restored.revision>off.revision);
    print.clear(); REQUIRE_FALSE(print.nonplanar_input().snapshot); REQUIRE(print.nonplanar_input().revision>restored.revision);
    REQUIRE(initial.snapshot->config.option<ConfigOptionFloat>("travel_speed")->value==speed);
    print.set_cancel_callback([] {});
}
TEST_CASE("B01 source capture failure invalidates prior inputs and OFF bypasses input limits", "[Nonplanar][B01][InputSnapshot]")
{
    Model model; auto *object=model.add_object(); object->add_instance(); object->add_volume(TriangleMesh(its_make_cube(1,1,1)));
    auto config=eligible(); Print print; print.apply(model,config); const auto previous=print.nonplanar_input(); REQUIRE(previous.snapshot);
    for (int i=0; i<256; ++i) object->add_volume(TriangleMesh(its_make_cube(1,1,1)));
    REQUIRE_THROWS_AS(capture_native_input(model,config),std::length_error);
    print.apply(model,config); const auto failure=print.nonplanar_input(); REQUIRE_FALSE(failure.snapshot);
    REQUIRE(failure.revision>previous.revision);
    REQUIRE_THAT(print.nonplanar_block_reason(),Catch::Matchers::ContainsSubstring("input capture failed"));
    print.apply(model,config); REQUIRE(print.nonplanar_input().revision>failure.revision);
    config.set_deserialize_strict("nptop_mode","off"); REQUIRE_FALSE(capture_native_input(model,config));
    print.apply(model,config); REQUIRE(print.nonplanar_block_reason().empty());
    Model unsupported; auto *bad=unsupported.add_object(); bad->brim_points.emplace_back();
    config=eligible(); REQUIRE_THROWS_AS(capture_native_input(unsupported,config),ConfigurationError);
}

TEST_CASE("B01 native input binds material range and plate action source controls", "[Nonplanar][B01][InputSnapshot]")
{
    Model model; auto *object=model.add_object();
    auto *volume=object->add_volume(TriangleMesh(its_make_cube(10,10,10))); object->add_instance();
    auto *material=model.add_material("one"); volume->set_material_id("one");
    auto config=eligible(); const auto initial=capture_native_input(model,config); REQUIRE(initial);
    const auto identity=initial->fingerprint;
    material->attributes["origin"]="first";
    auto snapshot=capture_native_input(model,config); REQUIRE(snapshot->fingerprint!=identity);
    const auto attributes_identity=snapshot->fingerprint;
    material->config.set_key_value("wall_loops",new ConfigOptionInt(4));
    snapshot=capture_native_input(model,config); REQUIRE(snapshot->fingerprint!=attributes_identity);
    const auto material_identity=snapshot->fingerprint;
    object->layer_config_ranges[{0.,2.}].set_key_value("wall_loops",new ConfigOptionInt(5));
    snapshot=capture_native_input(model,config); REQUIRE(snapshot->fingerprint!=material_identity);
    const auto range_identity=snapshot->fingerprint;
    object->layer_height_profile.set({0.,0.2,10.,0.1});
    snapshot=capture_native_input(model,config); REQUIRE(snapshot->fingerprint!=range_identity);
    const auto profile_identity=snapshot->fingerprint;
    model.plates_custom_gcodes[0].gcodes.push_back({0.2,CustomGCode::Custom,1,"","unexecuted source bytes"});
    snapshot=capture_native_input(model,config); REQUIRE(snapshot->fingerprint!=profile_identity);
    const auto owned=snapshot;
    model.plates_custom_gcodes[0].gcodes.front().extra="different bytes";
    REQUIRE(capture_native_input(model,config)->fingerprint!=owned->fingerprint);
    model.curr_plate_index=2; REQUIRE(capture_native_input(model,config)->model_plate_index==2);
    material->attributes.clear(); object->layer_config_ranges.clear(); object->layer_height_profile.clear();
    model.clear_objects();
    REQUIRE(owned->materials.front().attributes.at("origin")=="first");
    REQUIRE(owned->materials.front().config.option<ConfigOptionInt>("wall_loops")->value==4);
    REQUIRE(owned->objects.front().layer_ranges.front().config.option<ConfigOptionInt>("wall_loops")->value==5);
    REQUIRE(owned->objects.front().layer_height_profile.size()==4);
}
TEST_CASE("B01 source mesh and raw override changes invalidate Print and aggregate metadata limits fail", "[Nonplanar][B01][InputSnapshot]")
{
    Model model; auto *object=model.add_object();
    auto *volume=object->add_volume(TriangleMesh(its_make_cube(10,10,10))); auto *instance=object->add_instance();
    auto config=eligible(); CachedPrint print; print.apply(model,config);
    auto previous=print.nonplanar_input(); REQUIRE(previous.snapshot);
    const auto changed=[&] {
        print.set_started(psGCodeExport); print.set_done(psGCodeExport); print.apply(model,config);
        const auto current=print.nonplanar_input(); REQUIRE(current.snapshot);
        REQUIRE(current.revision>previous.revision);
        REQUIRE(current.snapshot->fingerprint!=previous.snapshot->fingerprint);
        REQUIRE_FALSE(print.is_step_done(psGCodeExport)); previous=current;
    };
    auto mesh=volume->mesh().its; mesh.vertices.front().x()=std::nextafter(mesh.vertices.front().x(),100.f);
    volume->set_mesh(std::move(mesh)); changed();
    object->config.set_key_value("future_geometry_setting",new ConfigOptionFloat(0.1)); changed();
    volume->source.mesh_offset.x()=0.1; changed();
    instance->set_offset(Vec3d(0.1,0,0)); changed();
    instance->set_offset(Vec3d(std::nextafter(0.1,1.),0,0)); changed();
    instance->arrange_order=2; changed();
    DynamicConfig large; large.set_key_value("nptop_mode",new ConfigOptionString("safe_hybrid"));
    for (const auto *v : object->volumes) REQUIRE_FALSE(v->mesh().its.indices.empty());
    for (int i=0; i<4; ++i) {
        auto *v=object->add_volume(TriangleMesh(its_make_cube(1,1,1)));
        v->config.set_key_value("private_source_data",new ConfigOptionString(std::string(700000,'x')));
    }
    REQUIRE_THROWS_AS(capture_native_input(model,large),std::length_error);
    for (auto *v : object->volumes) v->config.erase("private_source_data");
    TriangleSelector::TriangleSplittingData annotation;
    annotation.triangles_to_split.resize(100001);
    volume->supported_facets.set_data(TriangleSelector::TriangleSplittingData(annotation));
    volume->seam_facets.set_data(std::move(annotation));
    REQUIRE_THROWS_AS(capture_native_input(model,config),std::length_error);
}

namespace {
VolumePartitionResult native_body_partition(DynamicPrintConfig config,
    const DynamicPrintConfig &object_config={}, const DynamicPrintConfig &volume_config={}, double reserve_bottom=2,
    const char *fixture="docs/nonplanar/fixtures/models/flat_block.stl")
{
    const auto path=boost::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/fixture;
    const auto file=capture_stl_file(path.string(),true,71); REQUIRE(file.source);
    const auto imported=import_stl_snapshot(file.source->bytes,true); REQUIRE(imported.geometry.status==MeshAuditStatus::ValidGeometry);
    Model model; REQUIRE(load_stl(path.string().c_str(),&model));
    model.objects.front()->config.apply(object_config);
    model.objects.front()->volumes.front()->config.apply(volume_config);
    model.objects.front()->add_instance()->set_offset(Vec3d(20,20,0));
    const NativeInputBinding input{71,capture_native_input(model,config)};
    const auto placed=capture_input_placement(imported,input,{}); INFO(placed.geometry.reason);
    REQUIRE(placed.geometry.status==MeshAuditStatus::ValidGeometry);
    auto reservation=make_cube(8,8,4); reservation.translate(16,16,reserve_bottom);
    auto partition=partition_cap(placed,reservation); INFO(partition.reason);
    REQUIRE(partition.status==VolumePartitionStatus::Partitioned); return partition;
}
DynamicPrintConfig planar_body_config(double density=100)
{
    auto config=eligible();
    config.set_deserialize_strict({{"layer_height",.2},{"initial_layer_print_height",.2},
        {"wall_loops",2},{"sparse_infill_density",density},{"top_shell_layers",3},{"bottom_shell_layers",3},
        {"brim_type","no_brim"},{"skirt_loops",0}});
    return config;
}
bool segment_inside_reservation(const Position<Frame::BuildPlate> &a, const Position<Frame::BuildPlate> &b)
{
    // Independent strict-interior slab clipping: endpoints outside the reserved
    // rectangle do not excuse a long segment crossing its interior.
    long double lower=0, upper=1;
    for (const auto axis : {std::pair<double,double>{a.x(),b.x()},{a.y(),b.y()}}) {
        const long double delta=static_cast<long double>(axis.second)-axis.first;
        if (delta==0) { if (axis.first<=17 || axis.first>=23) return false; continue; }
        long double t0=(17.L-axis.first)/delta, t1=(23.L-axis.first)/delta;
        if (t0>t1) std::swap(t0,t1);
        lower=std::max(lower,t0); upper=std::min(upper,t1);
    }
    return lower<upper;
}
}
TEST_CASE("B04 whole native body adapter slices reserved mesh and retains all semantic layers", "[Nonplanar][B04][PlanarBody]")
{
    DynamicPrintConfig object_config,volume_config;
    object_config.set_key_value("wall_loops",new ConfigOptionInt(3));
    volume_config.set_key_value("outer_wall_line_width",new ConfigOptionFloatOrPercent(.55,false));
    auto input=native_body_partition(planar_body_config(),object_config,volume_config); const auto owned=input.snapshot;
    const auto original_id=owned->placement->native_input->fingerprint;
    PlanarBodyLimits limits; limits.paths.cancelled=[&] { input={}; return false; };
    const auto result=generate_planar_body(input,limits); INFO(result.reason); REQUIRE(result.snapshot);
    const auto &body=*result.snapshot; REQUIRE(body.partition==owned); REQUIRE(body.revision==71);
    REQUIRE_FALSE(input.snapshot); REQUIRE(body.fingerprint().size()==64);
    REQUIRE(body.guarded_settings->regions.front().policy.passes_config_preflight());
    REQUIRE(body.executed_full_config.option<ConfigOptionString>("nptop_mode")->value=="off");
    REQUIRE(owned->placement->native_input->config.option<ConfigOptionString>("nptop_mode")->value=="safe_hybrid");
    REQUIRE(owned->placement->native_input->fingerprint==original_id);
    REQUIRE(owned->placement->native_input->config.option<ConfigOptionInt>("wall_loops")->value==2);
    REQUIRE(body.regions.size()==20); REQUIRE(body.native_volume_mm3.lower>0);
    bool floor=false, surrounding_roof=false, walls=false;
    for (const auto &region : body.regions) {
        REQUIRE(region.geometry->revision==71);
        REQUIRE(region.config.option<ConfigOptionPercent>("sparse_infill_density")->value==100);
        REQUIRE(region.config.option<ConfigOptionInt>("wall_loops")->value==3);
        REQUIRE(region.config.option<ConfigOptionFloatOrPercent>("outer_wall_line_width")->value==.55);
        for (const auto &path : region.geometry->paths) {
            REQUIRE(path.width_mm>path.height_mm); REQUIRE(path.native_volume_mm3.lower>0);
            walls=walls || path.role==erExternalPerimeter;
            if (path.role==erExternalPerimeter) REQUIRE(std::abs(path.width_mm-.55)<1e-6);
            for (size_t i=1; i<path.points.size(); ++i) {
                const bool enters=segment_inside_reservation(path.points[i-1],path.points[i]);
                if (path.points[i].z()>2.000001) REQUIRE_FALSE(enters);
                floor=floor || (enters && std::abs(path.points[i].z()-2)<1e-6 && path.role==erTopSolidInfill);
            }
            for (const auto &point : path.points) {
                const bool interior=point.x()>17 && point.x()<23 && point.y()>17 && point.y()<23;
                if (interior) REQUIRE(point.z()<=2.000001);
                floor=floor || (interior && std::abs(point.z()-2)<1e-6 && path.role==erTopSolidInfill);
                surrounding_roof=surrounding_roof || (!interior && std::abs(point.z()-4)<1e-6);
            }
        }
    }
    REQUIRE(floor); REQUIRE(surrounding_roof); REQUIRE(walls);
}
TEST_CASE("B04 native body preserves sparse settings and refuses incompatible or stale processing", "[Nonplanar][B04][PlanarBody]")
{
    const auto dense=generate_planar_body(native_body_partition(planar_body_config())); REQUIRE(dense.snapshot);
    const auto sparse_input=native_body_partition(planar_body_config(10));
    const auto sparse=generate_planar_body(sparse_input); INFO(sparse.reason);
    // Actual native sparse paths include internal bridges outside the retained
    // ordinary planar role contract. Preserve the input and refuse the snapshot.
    REQUIRE_FALSE(sparse.snapshot); REQUIRE(sparse.reason=="UNSUPPORTED_PLANAR_ROLE");
    REQUIRE(sparse_input.snapshot->placement->native_input->config.option<ConfigOptionPercent>("sparse_infill_density")->value==10);
    REQUIRE(sparse_input.snapshot->fingerprint()!=dense.snapshot->partition->fingerprint());
    const auto rejected=[](const PlanarBodyResult &result) { INFO(result.reason); REQUIRE_FALSE(result.snapshot); };
    rejected(generate_planar_body({}));
    auto incompatible=planar_body_config(); incompatible.set_key_value("machine_start_gcode",new ConfigOptionString("G28"));
    const auto invalid=generate_planar_body(native_body_partition(incompatible));
    REQUIRE(invalid.reason.find("machine_start_gcode")!=std::string::npos); rejected(invalid);
    const auto input=native_body_partition(planar_body_config());
    PlanarBodyLimits limits; limits.paths.cancelled=[] { return true; }; rejected(generate_planar_body(input,limits));
    limits={}; unsigned polls=0; limits.paths.is_current=[&](uint64_t) { return ++polls<5; };
    auto stale=generate_planar_body(input,limits); REQUIRE(stale.reason=="STALE_REVISION"); rejected(stale);
    REQUIRE(polls>=5);
    limits={}; limits.max_layers=1; auto bounded=generate_planar_body(input,limits);
    REQUIRE(bounded.reason=="BODY_LAYER_LIMIT"); rejected(bounded);
    limits={}; limits.max_regions=1; rejected(generate_planar_body(input,limits));
    auto helpers=planar_body_config(); helpers.set_key_value("skirt_loops",new ConfigOptionInt(1));
    const auto helper_result=generate_planar_body(native_body_partition(helpers));
    REQUIRE(helper_result.reason=="UNSUPPORTED_BODY_HELPER_EXTRUSIONS"); rejected(helper_result);
    limits={}; limits.paths.timeout=std::chrono::milliseconds(1);
    limits.paths.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(5)); return false; };
    auto late=generate_planar_body(input,limits); REQUIRE(late.reason=="BODY_DEADLINE"); rejected(late);
}
TEST_CASE("B04 native body identity matches independent complete semantic encoding", "[Nonplanar][B04][PlanarBody]")
{
    const auto path=boost::filesystem::path(__FILE__).parent_path()/"data/planar-body-fingerprint-v1.json";
    boost::nowide::ifstream input(path.string()); REQUIRE(input.good()); nlohmann::json oracle; input>>oracle;
    DynamicConfig empty; const ResolvedConfigSnapshot config(empty);
    const auto mesh=std::make_shared<const TriangleMesh>();
    const auto native=std::make_shared<const NativeInputSnapshot>(NativeInputSnapshot{config,{},{},0,"","input"});
    const auto bytes=std::make_shared<const StlSourceSnapshot>(StlSourceSnapshot{"","source",true});
    const auto centered=std::make_shared<const CenteredVolumeSnapshot>(CenteredVolumeSnapshot{bytes,mesh,Vec3d::Zero(),0,19});
    const auto placement=std::make_shared<const ModelPlacementSnapshot>(ModelPlacementSnapshot{
        centered,Transform3d::Identity(),Transform3d::Identity(),{2,Vec3d(.1,-0.,2)},native});
    const auto partition=std::make_shared<const VolumePartitionSnapshot>(VolumePartitionSnapshot{
        19,placement,mesh,mesh,mesh,mesh,{0,0},{0,0},{0,0},{0,0},{0,0},.1,.2,0,0,0});
    const auto guarded=std::make_shared<const PrintConfigSnapshot>(PrintConfigSnapshot{
        3,Vec3d(.1,-0.,2),1,1,config,config,{},std::string("blocked\0reason",14),{},42,"source"});
    const auto scale=NativeScale::capture();
    const PlanarPathRecord record{3,erSolidInfill,.4,.2,.08,{Point3(int64_t(0),int64_t(0),int64_t(0)),
        Point3(int64_t(1000000),int64_t(0),int64_t(0))},
        {Position<Frame::BuildPlate>(20,-0.,3.6),Position<Frame::BuildPlate>(21,-0.,3.6)},.01,{1,1},{.08,.08}};
    const auto geometry=std::make_shared<const PlanarRegionSnapshot>(PlanarRegionSnapshot{
        71,2,.2,.6,Position<Frame::BuildPlate>(20,-0.,3),scale,
        {{0,PlanarEntityKind::Collection,true,false,-1,{}},{0,PlanarEntityKind::Collection,false,true,0,{}},
         {2,PlanarEntityKind::Path,false,false,0,{}}},{record},{.08,.08}});
    // Placeholder provenance and dimensions are serialization data only; this
    // vector never enters the body factory or constitutes a material proof.
    const PlanarBodySnapshot body{71,partition,guarded,config,config,config,scale,.1,{{7,config,geometry}},{.08,.08}};
    REQUIRE(body.canonical_json()==oracle.at("canonical").get<std::string>());
    REQUIRE(body.fingerprint()==oracle.at("sha256").get<std::string>());
}

TEST_CASE("B05 native body material retains every source segment and conserves native flow", "[Nonplanar][B05][BodyMaterial]")
{
    auto body=generate_planar_body(native_body_partition(planar_body_config())); REQUIRE(body.snapshot);
    const auto source=body.snapshot;
    const BodyMaterialParameters parameters{{.1,-.2,1},{17,Length(.01),Length(.01),Length(.01),Length(.01),Length(0)},
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},Speed(20),Speed(30),Acceleration(100),7,8};
    MaterialLimits limits; limits.timeout=std::chrono::seconds(5);
    limits.cancelled=[&] { body={}; return false; };
    const auto result=reconstruct_planar_body_material(body,parameters,limits); INFO(result.reason); REQUIRE(result.snapshot);
    const auto &snapshot=*result.snapshot; REQUIRE_FALSE(body.snapshot); REQUIRE(snapshot.body==source);
    REQUIRE(snapshot.material->source_fingerprint==source->fingerprint()); REQUIRE(snapshot.material->revision==71);
    REQUIRE(snapshot.material->model.numerical_coordinate_error.value()>source->partition->total_error_upper_mm);
    REQUIRE(snapshot.material->model.numerical_coordinate_error.value()<.05);
    size_t expected=0; for (const auto &region : source->regions) for (const auto &path : region.geometry->paths) expected+=path.points.size()-1;
    REQUIRE(snapshot.references.size()==expected);
    long double amount=0; std::set<size_t> referenced;
    for (const auto &ref : snapshot.references) {
        REQUIRE(referenced.insert(ref.record_index).second);
        const auto &path=source->regions.at(ref.region_index).geometry->paths.at(ref.path_index);
        const auto &motion=snapshot.material->records.at(ref.record_index).motion;
        const auto &deposit=std::get<Deposition>(motion.payload);
        const auto &a=path.points.at(ref.segment_index), &b=path.points.at(ref.segment_index+1);
        REQUIRE(motion.start.x()==a.x()+.1); REQUIRE(motion.start.y()==a.y()-.2); REQUIRE(motion.start.z()==a.z()+1);
        REQUIRE(motion.end.x()==b.x()+.1); REQUIRE(motion.end.y()==b.y()-.2); REQUIRE(motion.end.z()==b.z()+1);
        const long double dx=static_cast<long double>(motion.end.x())-motion.start.x(), dy=static_cast<long double>(motion.end.y())-motion.start.y();
        const long double native=std::sqrt(dx*dx+dy*dy)*path.mm3_per_mm;
        REQUIRE(std::abs(static_cast<long double>(deposit.volume.value())-native)<1e-12L);
        const long double h=path.height_mm, k=1-std::acos(-1.L)/4;
        const long double width=path.mm3_per_mm/h+k*h;
        REQUIRE(std::abs(deposit.width.value()-width)<1e-12L);
        REQUIRE(ref.width_reconciliation_upper_mm<1e-6); REQUIRE(ref.volume_reconciliation_upper_mm3<1e-8);
        amount+=deposit.volume.value();
        if (referenced.size()==1) {
            const auto state=material_at(snapshot.material,ref.record_index,.5); REQUIRE(state.lower.snapshot);
            const PhysicalPosition middle((motion.start.x()+motion.end.x())/2,(motion.start.y()+motion.end.y())/2,motion.start.z()-h/2);
            // Behind the advancing prefix end, rather than exactly on its cap.
            const PhysicalPosition deposited(motion.start.x()*.75+motion.end.x()*.25,motion.start.y()*.75+motion.end.y()*.25,middle.z());
            REQUIRE(classify_material(state.lower,deposited).membership==MaterialMembership::Inside);
        }
    }
    const auto complete=material_at(snapshot.material,snapshot.material->records.size(),0,limits); REQUIRE(complete.nominal.snapshot);
    REQUIRE(complete.nominal.snapshot->nominal_deposited_volume_mm3.lower<=amount);
    REQUIRE(complete.nominal.snapshot->nominal_deposited_volume_mm3.upper>=amount);
    REQUIRE(std::abs(amount-(source->native_volume_mm3.lower+source->native_volume_mm3.upper)/2)<1e-6L);
    REQUIRE(snapshot.fingerprint().size()==64);
    // No body material is silently manufactured in the reserved cap interior.
    REQUIRE(classify_material(complete.upper,{20.1,19.8,4}).membership==MaterialMembership::Outside);
}
TEST_CASE("B05 native body material refuses unsupported flow stale budgets and malformed context", "[Nonplanar][B05][BodyMaterial]")
{
    const auto body=generate_planar_body(native_body_partition(planar_body_config())); REQUIRE(body.snapshot);
    const BodyMaterialParameters params{{0,0,0},{17,Length(.01),Length(.01),Length(.01),Length(.01),Length(0)},
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},Speed(20),Speed(30),Acceleration(100),7,8};
    const auto rejected=[](const BodyMaterialResult &r) { INFO(r.reason); REQUIRE_FALSE(r.snapshot); };
    rejected(reconstruct_planar_body_material({},params));
    auto invalid=params; invalid.support_reference_id=0; rejected(reconstruct_planar_body_material(body,invalid));
    invalid=params; invalid.model.numerical_coordinate_error=Length(.05); rejected(reconstruct_planar_body_material(body,invalid));
    invalid=params; invalid.plate_origin={10001,0,0}; rejected(reconstruct_planar_body_material(body,invalid));
    MaterialLimits limits; limits.cancelled=[] { return true; }; rejected(reconstruct_planar_body_material(body,params,limits));
    limits={}; limits.is_current=[](uint64_t) { return false; }; rejected(reconstruct_planar_body_material(body,params,limits));
    limits={}; limits.max_records=1; rejected(reconstruct_planar_body_material(body,params,limits));
    limits={}; limits.timeout=std::chrono::milliseconds(1);
    limits.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(5)); return false; };
    rejected(reconstruct_planar_body_material(body,params,limits));
    limits={}; limits.cancelled=[] { std::fesetround(FE_DOWNWARD); return false; };
    const auto rounding=reconstruct_planar_body_material(body,params,limits);
    REQUIRE(std::fesetround(FE_TONEAREST)==0); rejected(rounding);
    const auto mutate=[&](const std::function<void(PlanarPathRecord &)> &change) {
        const auto &original=body.snapshot->regions; auto paths=original.front().geometry->paths; change(paths.front());
        const auto &g=*original.front().geometry;
        auto altered=std::make_shared<const PlanarRegionSnapshot>(PlanarRegionSnapshot{g.revision,g.native_layer_id,g.native_layer_height_mm,
            g.native_print_z_mm,g.object_origin,g.native_scale,g.entities,std::move(paths),g.native_volume_mm3});
        std::vector<PlanarBodyRegion> regions;
        for (size_t i=0; i<original.size(); ++i) regions.push_back({original[i].native_region_id,original[i].config,i ? original[i].geometry : altered});
        const auto &b=*body.snapshot;
        return PlanarBodyResult{"mutation",std::make_shared<const PlanarBodySnapshot>(PlanarBodySnapshot{b.revision,b.partition,b.guarded_settings,
            b.executed_full_config,b.executed_print_config,b.executed_object_config,b.native_scale,b.origin_error_upper_mm,std::move(regions),b.native_volume_mm3})};
    };
    const auto flow=reconstruct_planar_body_material(mutate([](auto &p){ p.mm3_per_mm*=1.01; }),params);
    REQUIRE(flow.reason=="UNSUPPORTED_NATIVE_MATERIAL_FLOW"); rejected(flow);
    rejected(reconstruct_planar_body_material(mutate([](auto &p){ p.native_points.front().x()=std::numeric_limits<coord_t>::min(); }),params));
    const auto mismatch=reconstruct_planar_body_material(mutate([](auto &p){ p.native_points.front().x()+=100; }),params);
    REQUIRE(mismatch.reason=="BODY_MATERIAL_NATIVE_POINT_BINDING"); rejected(mismatch);
    const auto bridge=reconstruct_planar_body_material(mutate([](auto &p){ p.role=erBridgeInfill; }),params);
    REQUIRE(bridge.reason=="UNSUPPORTED_BODY_MATERIAL_ROLE"); rejected(bridge);

}
TEST_CASE("B05 native material source references match independent chained encoding", "[Nonplanar][B05][BodyMaterial]")
{
    const auto directory=boost::filesystem::path(__FILE__).parent_path()/"data";
    boost::nowide::ifstream input((directory/"body-material-fingerprint-v1.json").string());
    REQUIRE(input.good()); nlohmann::json oracle; input>>oracle;
    boost::nowide::ifstream body_input((directory/"planar-body-fingerprint-v1.json").string());
    boost::nowide::ifstream material_input((directory/"material-fingerprint-v1.json").string());
    nlohmann::json body,material; body_input>>body; material_input>>material;
    // Stored parent IDs and references are encoding data only. Null parents
    // never enter reconstruction or become an accepted material result.
    const BodyMaterialSnapshot snapshot{{},{},body.at("sha256").get<std::string>(),material.at("sha256").get<std::string>(),
        {.1,-.2,1},{{0,0,0,0,.01,.02},{3,2,3,4,1e-6,-0.}}};
    REQUIRE(snapshot.canonical_context()==oracle.at("context").get<std::string>());
    for (size_t i=0; i<snapshot.references.size(); ++i)
        REQUIRE(snapshot.canonical_reference(i)==oracle.at("references").at(i).get<std::string>());
    REQUIRE(snapshot.fingerprint()==oracle.at("sha256").get<std::string>());
}

TEST_CASE("B04 actual dense native floor has continuous inner coverage under the cap", "[Nonplanar][B04][B05][NativeCoverage]")
{
    const auto body=generate_planar_body(native_body_partition(planar_body_config())); REQUIRE(body.snapshot);
    const BodyMaterialParameters params{{0,0,0},{17,Length(.01),Length(.01),Length(.01),Length(.01),Length(0)},
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},Speed(20),Speed(30),Acceleration(100),7,8};
    MaterialLimits material_limits; material_limits.timeout=std::chrono::seconds(5);
    const auto reconstructed=reconstruct_planar_body_material(body,params,material_limits); REQUIRE(reconstructed.snapshot);
    const auto ledger=reconstructed.snapshot->material;
    const auto complete=material_at(ledger,ledger->records.size(),0,material_limits); REQUIRE(complete.lower.snapshot);
    MaterialCoverageLimits limits; limits.timeout=std::chrono::seconds(5);
    const auto coverage=cover_material(complete.lower,{{17,17,1.9},{23,23,1.9}},limits);
    INFO(coverage.reason); INFO(coverage.cells); INFO(coverage.evaluations);
    REQUIRE(coverage.status==MaterialCoverageStatus::Covered);
    REQUIRE(coverage.cells>1); REQUIRE(coverage.evaluations<=limits.max_evaluations);
    // Independent continuous oracle: inscribed bead rectangles at this plane,
    // long-double section math and native integer polygon subtraction. Shrink
    // by 64 microns before quantizing to 1 micron, so rounding cannot grow them.
    constexpr long double scale=1000000.L, inset=.000064L, plane=1.9L;
    Polygons guaranteed;
    const auto &model=ledger->model;
    for (const auto &row : ledger->records) if (row.bead) {
        const auto &m=row.motion; const auto &deposit=std::get<Deposition>(m.payload);
        const long double dx=static_cast<long double>(m.end.x())-m.start.x(),dy=static_cast<long double>(m.end.y())-m.start.y();
        const long double length=std::sqrt(dx*dx+dy*dy),height=row.bead->gap_begin_mm;
        const long double area=deposit.volume.value()/length, k=1-std::acos(-1.L)/4;
        const long double width=area/height+k*height, center=m.start.z()-height/2;
        const long double radius=height/2-model.inner_xy_loss.value()-model.inner_z_loss.value()-2*model.numerical_coordinate_error.value();
        const long double vertical=plane-center;
        if (radius<=0 || std::abs(vertical)>=radius) continue;
        const long double half=(width-height)/2+std::sqrt(radius*radius-vertical*vertical)-inset;
        const long double end_loss=model.inner_xy_loss.value()+model.numerical_coordinate_error.value()+inset;
        if (half<=0 || length<=2*end_loss) continue;
        const long double ux=dx/length,uy=dy/length,nx=-uy,ny=ux;
        const long double ax=m.start.x()+ux*end_loss,ay=m.start.y()+uy*end_loss;
        const long double bx=m.end.x()-ux*end_loss,by=m.end.y()-uy*end_loss;
        const auto point=[&](long double x,long double y){return Point(coord_t(std::llround(x*scale)),coord_t(std::llround(y*scale)));};
        guaranteed.emplace_back(Points{point(ax-nx*half,ay-ny*half),point(bx-nx*half,by-ny*half),
            point(bx+nx*half,by+ny*half),point(ax+nx*half,ay+ny*half)});
    }
    const Polygons target{Polygon(Points{Point(17000000,17000000),Point(23000000,17000000),Point(23000000,23000000),Point(17000000,23000000)})};
    REQUIRE_FALSE(guaranteed.empty()); REQUIRE(diff(target,guaranteed).empty());
    const auto future=cover_material(complete.lower,{{17,17,2.5},{23,23,2.5}},limits);
    REQUIRE(future.status==MaterialCoverageStatus::Uncovered); REQUIRE(future.witness);
    REQUIRE(classify_material(complete.lower,*future.witness).membership==MaterialMembership::Outside);
}

TEST_CASE("B06 actual native reserved body bounds the first affine pass and missing cap volume", "[Nonplanar][B06][NativeTransition]")
{
    const auto body=generate_planar_body(native_body_partition(planar_body_config())); REQUIRE(body.snapshot);
    const BodyMaterialParameters params{{0,0,0},{17,Length(.01),Length(.01),Length(.01),Length(.01),Length(0)},
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},Speed(20),Speed(30),Acceleration(100),7,8};
    MaterialLimits material_limits; material_limits.timeout=std::chrono::seconds(5);
    const auto material=reconstruct_planar_body_material(body,params,material_limits); REQUIRE(material.snapshot);
    const auto ledger=material.snapshot->material;
    const auto all=material_at(ledger,ledger->records.size(),0,material_limits); REQUIRE(all.lower.snapshot);
    MaterialCoverageLimits limits; limits.timeout=std::chrono::seconds(5);
    const TransitionPolicy policy{VerticalGap(.1),VerticalGap(.32),Length(.00001)};
    const AffineCapCell cell{{17,17,23,23},2.2,2.2,2.2};
    const auto first=assess_material_first_pass(all.lower,cell,1.9,policy,limits); INFO(first.reason);
    REQUIRE(first.status==TransitionStatus::Compatible);
    REQUIRE(first.source==all.lower.snapshot); REQUIRE(first.support.status==MaterialCoverageStatus::Covered);
    REQUIRE(first.gap_mm); REQUIRE(first.gap_mm->lower>.1); REQUIRE(first.gap_mm->upper<.32);
    // Body walls reach Z=4 elsewhere. They cannot become a fictitious roof
    // inside the reserved cap interior. Real native paths, not CAD, are queried.
    REQUIRE(first.nominal_roof_ceiling_mm); REQUIRE(*first.nominal_roof_ceiling_mm>=2);
    REQUIRE(*first.nominal_roof_ceiling_mm<2.00000001);
    REQUIRE(first.upper_roof_ceiling_mm); REQUIRE(*first.upper_roof_ceiling_mm<2.03);
    REQUIRE(first.nominal_volume_mm3);
    // Every nominal bead roof is at/below 2 and the whole plane at 1.9 lies
    // inside nominal material. Thus the true missing vertical cell volume is
    // within [36*(2.2-2),36*(2.2-1.9)], including corrugated shoulders/overlaps.
    REQUIRE(first.nominal_volume_mm3->lower<=7.2); REQUIRE(first.nominal_volume_mm3->lower>7.19999);
    REQUIRE(first.nominal_volume_mm3->upper>=10.8); REQUIRE(first.nominal_volume_mm3->upper<10.80001);
    REQUIRE(first.roof_evaluations+first.support.evaluations<=limits.max_evaluations);
    const auto unsupported=assess_material_first_pass(all.lower,cell,2.5,policy,limits);
    REQUIRE(unsupported.status==TransitionStatus::Rejected); REQUIRE(unsupported.support.witness);
    const auto too_high=assess_material_first_pass(all.lower,{{17,17,23,23},2.6,2.6,2.6},1.9,policy,limits);
    REQUIRE(too_high.status==TransitionStatus::Rejected);
    const auto edge=assess_material_first_pass(all.lower,{{15,17,23,23},2.2,2.2,2.2},1.9,policy,limits);
    REQUIRE(edge.status!=TransitionStatus::Compatible);
}

TEST_CASE("B06 actual native bead union refines the corrugated first-pass volume", "[Nonplanar][B06][NativeIntegral]")
{
    const auto body=generate_planar_body(native_body_partition(planar_body_config())); REQUIRE(body.snapshot);
    const BodyMaterialParameters params{{0,0,0},{17,Length(.01),Length(.01),Length(.01),Length(.01),Length(0)},
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},Speed(20),Speed(30),Acceleration(100),7,8};
    MaterialLimits material_limits; material_limits.timeout=std::chrono::seconds(5);
    const auto material=reconstruct_planar_body_material(body,params,material_limits); REQUIRE(material.snapshot);
    const auto ledger=material.snapshot->material;
    const auto all=material_at(ledger,ledger->records.size(),0,material_limits); REQUIRE(all.lower.snapshot);
    MaterialIntegralLimits limits; limits.timeout=std::chrono::seconds(5); limits.maximum_interval_width=Volume(.01);
    limits.max_cells=8191; // Explicit work budget for this 36 mm2 / 0.01 mm3 query.
    const TransitionPolicy policy{VerticalGap(.1),VerticalGap(.32),Length(.00001)};
    const auto result=integrate_material_first_pass(all.lower,{{17,17,23,23},2.2,2.2,2.2},1.9,policy,limits);
    INFO(result.reason); INFO(result.cells); INFO(result.evaluations);
    REQUIRE(result.status==MaterialIntegralStatus::Bounded); REQUIRE(result.nominal_volume_mm3);
    REQUIRE(result.first_pass.source==all.lower.snapshot);
    const auto amount=*result.nominal_volume_mm3;
    INFO(amount.lower); INFO(amount.upper);
    REQUIRE(amount.upper-amount.lower<=limits.maximum_interval_width.value());
    REQUIRE(amount.lower>7.2); REQUIRE(amount.upper<10.8);
    REQUIRE(amount.upper-amount.lower<.01*(result.first_pass.nominal_volume_mm3->upper-result.first_pass.nominal_volume_mm3->lower));
    REQUIRE(result.evaluations+result.first_pass.roof_evaluations+result.first_pass.support.evaluations<=limits.max_evaluations);
    REQUIRE(result.cells+result.first_pass.support.cells<=limits.max_cells);
    // Independent layer-cake oracle: upper-half sections near native Z=2
    // beads shrink monotonically from the supported plane. Quantization-safe
    // inner/outer rectangles bound their continuous XY union at each Z, then
    // monotone Riemann bounds enclose the whole integral between those planes.
    constexpr long double scale=1000000.L,inset=.000064L;
    const long double plane=double(1.9); long double roof=plane;
    const Polygons target{Polygon(Points{Point(17000000,17000000),Point(23000000,17000000),Point(23000000,23000000),Point(17000000,23000000)})};
    struct Section { long double ax,ay,bx,by,ux,uy,length,core,height,top; };
    std::vector<Section> sections;
    const auto rectangle=[&](const Section &s,long double half,long double extend) {
        const auto point=[&](long double x,long double y) { return Point(coord_t(std::llround(x*scale)),coord_t(std::llround(y*scale))); };
        const auto ax=s.ax-s.ux*extend,ay=s.ay-s.uy*extend,bx=s.bx+s.ux*extend,by=s.by+s.uy*extend;
        return Polygon(Points{point(ax+s.uy*half,ay-s.ux*half),point(bx+s.uy*half,by-s.ux*half),
            point(bx-s.uy*half,by+s.ux*half),point(ax-s.uy*half,ay+s.ux*half)});
    };
    for (const auto &row : ledger->records) if (row.bead && row.motion.start.z()>plane) {
        const auto &m=row.motion; REQUIRE(m.start.z()==m.end.z()); REQUIRE(row.bead->gap_begin_mm==row.bead->gap_end_mm);
        REQUIRE(row.bead->kind==BeadSectionKind::RoundedRectangle);
        const long double dx=static_cast<long double>(m.end.x())-m.start.x(),dy=static_cast<long double>(m.end.y())-m.start.y();
        const long double length=std::sqrt(dx*dx+dy*dy),height=row.bead->gap_begin_mm;
        const long double width=std::get<Deposition>(m.payload).volume.value()/length/height+(1-std::acos(-1.L)/4)*height;
        const Section s{m.start.x(),m.start.y(),m.end.x(),m.end.y(),dx/length,dy/length,length,(width-height)/2,height,m.start.z()};
        if (intersection(Polygons{rectangle(s,width/2+inset,inset)},target).empty()) continue;
        REQUIRE(std::abs(s.top-2)<1e-12); REQUIRE(s.top-height/2<=plane); REQUIRE(length>2*inset);
        roof=std::max(roof,s.top); // Keep every actual native Z, including its ULPs.
        sections.push_back(s);
    }
    REQUIRE_FALSE(sections.empty());
    const auto cross_section=[&](long double z,bool outer) {
        Polygons paths;
        for (const auto &s : sections) {
            if (z>s.top) continue;
            const long double down=s.top-z;
            const long double half=s.core+std::sqrt(std::max(0.L,down*(s.height-down)))+(outer ? inset : -inset);
            if (half>0) paths.push_back(rectangle(s,half,outer ? inset : -inset));
        }
        return intersection(paths,target);
    };
    REQUIRE(diff(target,cross_section(plane,false)).empty());
    const auto cross_area=[&](long double z,bool outer) {
        long double area=0; for (const auto &p : cross_section(z,outer)) area+=p.area(); return area/(scale*scale);
    };
    long double lower=0,upper=0,previous=plane;
    constexpr size_t steps=2000;
    for (size_t i=1; i<=steps; ++i) {
        const long double z=plane+(roof-plane)*i/steps;
        lower+=(z-previous)*cross_area(z,false);upper+=(z-previous)*cross_area(previous,true);previous=z;
    }
    // The fixed 64 micron inset dominates coordinate/section arithmetic and
    // 1 micron quantization in this small fixture. Keep a separate 1e-7 mm3
    // allowance on these bounded floating sums (Apple long double is binary64).
    const long double cell_volume=36*(static_cast<long double>(double(2.2))-plane);
    const ScalarBounds oracle{double(cell_volume-upper)-1e-7,double(cell_volume-lower)+1e-7};
    INFO(oracle.lower); INFO(oracle.upper);
    REQUIRE(oracle.upper-oracle.lower<limits.maximum_interval_width.value());
    REQUIRE(amount.lower<=oracle.lower); REQUIRE(amount.upper>=oracle.upper);
    const auto impossible=integrate_material_first_pass(all.lower,{{17,17,23,23},2.2,2.2,2.2},2.5,policy,limits);
    REQUIRE(impossible.status==MaterialIntegralStatus::Rejected); REQUIRE(impossible.first_pass.support.witness);
}

TEST_CASE("B06 native pass stack derives the target from the owned source and stays in the reserved cap", "[Nonplanar][B06][NativePassStack]")
{
    const auto body=generate_planar_body(native_body_partition(planar_body_config(),{},{},3.2)); REQUIRE(body.snapshot);
    const BodyMaterialParameters material_params{{0,0,0},{17,Length(.01),Length(.01),Length(.01),Length(.01),Length(0)},
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},Speed(20),Speed(30),Acceleration(100),7,8};
    MaterialLimits material_limits; material_limits.timeout=std::chrono::seconds(5);
    auto material=reconstruct_planar_body_material(body,material_params,material_limits); REQUIRE(material.snapshot);
    const NativeAffinePassRequest request{{17,17,23,23},0,3.1,
        {4,{VerticalGap(.1),VerticalGap(.32),Length(.00001)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.01)}};
    NativeAffinePassLimits limits; limits.material.timeout=std::chrono::seconds(5); limits.material.max_cells=8191;
    limits.material.maximum_interval_width=Volume(.01);
    const auto result=plan_native_affine_pass_stack(material,request,limits); INFO(result.reason); REQUIRE(result.snapshot);
    const auto &owned=*result.snapshot; REQUIRE(owned.body_material==material.snapshot); REQUIRE(owned.source_projection);
    const auto &stack=*owned.stack; REQUIRE(stack.surfaces.size()==4);
    INFO(stack.first_offset_mm); INFO(stack.total_volume_mm3.lower); INFO(stack.total_volume_mm3.upper);
    INFO(stack.total_allocated_volume.value()); INFO(stack.total_allocation_error_mm3); INFO(owned.reservation_tests);
    REQUIRE(stack.final_surface.z00==4); REQUIRE(stack.final_surface.z10==4); REQUIRE(stack.final_surface.z01==4);
    REQUIRE(stack.surfaces.front().cell.z00>3.35); REQUIRE(stack.surfaces.front().cell.z00<3.4);
    REQUIRE(stack.first_pass.first_pass.gap_mm); REQUIRE(stack.first_pass.first_pass.gap_mm->lower>.1);
    REQUIRE(stack.first_pass.first_pass.gap_mm->upper<.32);
    for (const auto &surface : stack.surfaces) for (double z : {surface.cell.z00,surface.cell.z10,surface.cell.z01}) {
        REQUIRE(z>3.2); REQUIRE(z<=4);
    }
    REQUIRE(stack.total_volume_mm3.lower>28.8); REQUIRE(stack.total_volume_mm3.upper<29.5);
    REQUIRE(stack.total_allocation_error_mm3<=request.policy.total_volume_error.value());
    REQUIRE(owned.source_projection->revision==body.snapshot->revision);
    REQUIRE(owned.reservation_tests>0);
    auto strip_limits=limits.material; strip_limits.max_cells=32768;
    const auto first_strips=split_material_integral(stack.first_pass,IntegralSplitAxis::X,
        {17,17.5,18,18.5,19,19.5,20,20.5,21,21.5,22,22.5,23},strip_limits);
    INFO(first_strips.reason); REQUIRE(first_strips.snapshot); REQUIRE(first_strips.snapshot->strips.size()==12);
    REQUIRE(first_strips.snapshot->source==stack.first_pass.proof);
    REQUIRE(first_strips.snapshot->total_volume_mm3.lower<=stack.first_pass.nominal_volume_mm3->upper);
    REQUIRE(first_strips.snapshot->total_volume_mm3.upper>=stack.first_pass.nominal_volume_mm3->lower);
    REQUIRE(first_strips.snapshot->total_volume_mm3.upper-first_strips.snapshot->total_volume_mm3.lower<=.01);
    auto invalid=request; invalid.patch=100; REQUIRE_FALSE(plan_native_affine_pass_stack(material,invalid,limits).snapshot);
    invalid=request; invalid.policy.passes=2; REQUIRE_FALSE(plan_native_affine_pass_stack(material,invalid,limits).snapshot);
    invalid=request; invalid.footprint={15,17,23,23}; REQUIRE_FALSE(plan_native_affine_pass_stack(material,invalid,limits).snapshot);
    limits.material.is_current=[](uint64_t){return false;}; REQUIRE_FALSE(plan_native_affine_pass_stack(material,request,limits).snapshot);
    limits.material.is_current={}; limits.max_reservation_tests=1; REQUIRE_FALSE(plan_native_affine_pass_stack(material,request,limits).snapshot);
    limits.max_reservation_tests=50000;
    auto unreserved=generate_planar_body(native_body_partition(planar_body_config())); REQUIRE(unreserved.snapshot);
    const auto deep=reconstruct_planar_body_material(unreserved,material_params,material_limits); REQUIRE(deep.snapshot);
    invalid=request; invalid.support_plane_z_mm=1.9;
    REQUIRE_FALSE(plan_native_affine_pass_stack(deep,invalid,limits).snapshot);
    auto moved_params=material_params; moved_params.plate_origin=PhysicalPosition(100,200,10);
    const auto moved=reconstruct_planar_body_material(body,moved_params,material_limits); REQUIRE(moved.snapshot);
    auto moved_request=request; moved_request.footprint={117,217,123,223}; moved_request.support_plane_z_mm=13.1;
    const auto translated=plan_native_affine_pass_stack(moved,moved_request,limits); INFO(translated.reason); REQUIRE(translated.snapshot);
    REQUIRE(translated.snapshot->stack->final_surface.z00==14);
    REQUIRE(translated.snapshot->stack->final_surface.footprint.min_x==117);
    REQUIRE(translated.snapshot->stack->total_volume_mm3.lower<stack.total_volume_mm3.upper);
    REQUIRE(translated.snapshot->stack->total_volume_mm3.upper>stack.total_volume_mm3.lower);
    REQUIRE_FALSE(plan_native_affine_pass_stack(moved,request,limits).snapshot);
    const auto &parent=*material.snapshot;
    const BodyMaterialResult mismatched{"",std::make_shared<const BodyMaterialSnapshot>(BodyMaterialSnapshot{
        parent.body,parent.material,"other body",parent.material_fingerprint,parent.plate_origin,parent.references})};
    REQUIRE(plan_native_affine_pass_stack(mismatched,request,limits).reason=="NATIVE_PASS_PARENT_BINDING");
    limits.material.cancelled=[] { return true; };
    REQUIRE(plan_native_affine_pass_stack(material,request,limits).reason=="CANCELLED");
    limits.material.timeout=std::chrono::milliseconds(1);
    limits.material.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(3));return false; };
    REQUIRE(plan_native_affine_pass_stack(material,request,limits).reason=="NATIVE_PASS_DEADLINE");
    limits.material.timeout=std::chrono::seconds(5);
    limits.material.cancelled=[&] { material.snapshot.reset();return false; };
    const auto retained=plan_native_affine_pass_stack(material,request,limits); INFO(retained.reason); REQUIRE(retained.snapshot);
    REQUIRE(retained.snapshot->body_material); REQUIRE(retained.snapshot->stack->final_surface.z00==4);
}

TEST_CASE("B06 native refined integral allocates complete X and Y strips from the same owned proof", "[Nonplanar][B06][NativeIntegralStrips]")
{
    const auto body=generate_planar_body(native_body_partition(planar_body_config())); REQUIRE(body.snapshot);
    const BodyMaterialParameters parameters{{0,0,0},{17,Length(.01),Length(.01),Length(.01),Length(.01),Length(0)},
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},Speed(20),Speed(30),Acceleration(100),7,8};
    MaterialLimits capture; capture.timeout=std::chrono::seconds(5);
    const auto material=reconstruct_planar_body_material(body,parameters,capture); REQUIRE(material.snapshot);
    const auto present=material_at(material.snapshot->material,material.snapshot->material->records.size(),0,capture);
    REQUIRE(present.lower.snapshot);
    MaterialIntegralLimits limits; limits.timeout=std::chrono::seconds(5); limits.max_cells=8191;
    limits.maximum_interval_width=Volume(.01);
    auto integral=integrate_material_first_pass(present.lower,{{17,17,23,23},2.2,2.2,2.2},1.9,
        {VerticalGap(.1),VerticalGap(.4),Length(.00001)},limits);
    INFO(integral.reason); REQUIRE(integral.status==MaterialIntegralStatus::Bounded); REQUIRE(integral.proof);
    std::vector<double> cuts; for (size_t i=0; i<=12; ++i) cuts.push_back(17+double(i)/2);
    REQUIRE(split_material_integral(integral,IntegralSplitAxis::X,cuts,limits).reason=="INTEGRAL_STRIP_CELL_LIMIT");
    limits.max_cells=32768; // Orthogonal strips split each long proof leaf.
    for (auto axis : {IntegralSplitAxis::X,IntegralSplitAxis::Y}) {
        const auto split=split_material_integral(integral,axis,cuts,limits); INFO(split.reason); REQUIRE(split.snapshot);
        const auto &s=*split.snapshot; REQUIRE(s.strips.size()==12); REQUIRE(s.source==integral.proof);
        INFO(s.proof_cells); INFO(s.evaluations); INFO(s.total_volume_mm3.lower); INFO(s.total_volume_mm3.upper);
        REQUIRE(s.total_volume_mm3.lower<=7.37753727144254245);
        REQUIRE(s.total_volume_mm3.upper>=7.37927018186442929);
        REQUIRE(s.total_volume_mm3.upper-s.total_volume_mm3.lower<=.01);
        REQUIRE(s.evaluations<=limits.max_evaluations); REQUIRE(s.proof_cells<=limits.max_cells);
        for (const auto &strip : s.strips) {
            REQUIRE(strip.volume_mm3.lower>.6); REQUIRE(strip.volume_mm3.upper<.7);
        }
    }
    integral.nominal_volume_mm3=ScalarBounds{100,100}; integral.first_pass.source.reset();
    const auto retained=split_material_integral(integral,IntegralSplitAxis::X,cuts,limits); REQUIRE(retained.snapshot);
    REQUIRE(retained.snapshot->total_volume_mm3.upper<8);
}

TEST_CASE("B07 native affine wedge creates finite nonplanar hatch candidates from the owned source", "[Nonplanar][B07][NativeAffineHatches]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<AffinePassStackSnapshot>::value);
    const auto body=generate_planar_body(native_body_partition(planar_body_config(),{},{},4.2,
        "tests/nonplanar/data/affine-wedge-1-in-16.stl")); REQUIRE(body.snapshot);
    REQUIRE(body.snapshot->partition->original_exact_volume_mm3.lower<=3200);
    REQUIRE(body.snapshot->partition->original_exact_volume_mm3.upper>=3200);
    const BodyMaterialParameters parameters{{0,0,0},{17,Length(.01),Length(.01),Length(.01),Length(.01),Length(0)},
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},Speed(20),Speed(30),Acceleration(100),7,8};
    MaterialLimits capture; capture.timeout=std::chrono::seconds(5);
    auto material=reconstruct_planar_body_material(body,parameters,capture); REQUIRE(material.snapshot);
    const NativeAffinePassRequest request{{19,17,21,23},0,4.1,
        {4,{VerticalGap(.1),VerticalGap(.4),Length(.00001)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.01)}};
    const AffineHatchPolicy policy{WidthXY(.45),Length(.4),Length(.2),HatchDirection::AlongX};
    NativeAffinePassLimits pass_limits;pass_limits.material.max_cells=8191;pass_limits.material.timeout=std::chrono::seconds(5);
    pass_limits.material.maximum_interval_width=Volume(.01);
    AffineHatchLimits hatch_limits;hatch_limits.timeout=std::chrono::seconds(10);hatch_limits.volumes.max_cells=32768;
    hatch_limits.volumes.timeout=std::chrono::seconds(5);hatch_limits.volumes.maximum_interval_width=Volume(.01);
    const auto result=plan_native_affine_hatches(material,request,policy,pass_limits,hatch_limits); INFO(result.reason); REQUIRE(result.snapshot);
    const auto &native=*result.snapshot; const auto &layout=*native.hatches; REQUIRE(layout.source==native.passes->stack);
    INFO("hatch lines=" << layout.line_count << " total=[" << std::setprecision(18) <<
        layout.total_prospective_volume_mm3.lower << ',' << layout.total_prospective_volume_mm3.upper << ']');
    REQUIRE(layout.passes.size()==4); REQUIRE(layout.line_count>0);
    REQUIRE(layout.passes.front().lines.front().start.z()!=layout.passes.front().lines.front().end.z());
    for (const auto &line : layout.passes.back().lines) for (const auto &point : {line.start,line.end}) {
        const long double expected=5+(static_cast<long double>(point.x())-20)/16;
        REQUIRE(std::abs(point.z()-expected)<=line.coordinate_error_upper_mm);
        REQUIRE(line.width.value()==.45);
    }
    REQUIRE(layout.total_prospective_volume_mm3.lower>9.6); REQUIRE(layout.total_prospective_volume_mm3.upper<10);
    REQUIRE(layout.total_prospective_volume_mm3.upper-layout.total_prospective_volume_mm3.lower<=.01);
    REQUIRE(native.passes->body_material==material.snapshot);
    hatch_limits.is_current=[](uint64_t){return false;};
    REQUIRE_FALSE(plan_native_affine_hatches(material,request,policy,pass_limits,hatch_limits).snapshot);
    hatch_limits.is_current={};hatch_limits.max_lines=1;
    REQUIRE_FALSE(plan_native_affine_hatches(material,request,policy,pass_limits,hatch_limits).snapshot);
    hatch_limits.max_lines=20000;hatch_limits.cancelled=[] { return true; };
    REQUIRE(plan_native_affine_hatches(material,request,policy,pass_limits,hatch_limits).reason=="CANCELLED");
    hatch_limits.timeout=std::chrono::milliseconds(1);
    hatch_limits.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(3));return false; };
    REQUIRE(plan_native_affine_hatches(material,request,policy,pass_limits,hatch_limits).reason=="NATIVE_HATCH_DEADLINE");
    hatch_limits.timeout=std::chrono::seconds(10);hatch_limits.cancelled=[&] { material.snapshot.reset();return false; };
    const auto retained=plan_native_affine_hatches(material,request,policy,pass_limits,hatch_limits);INFO(retained.reason);REQUIRE(retained.snapshot);
    REQUIRE(retained.snapshot->passes->body_material);
}

TEST_CASE("B07 native finite hatch cells keep end and boundary volume in an explicit remainder", "[Nonplanar][B07][NativeHatchCells]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<AffineHatchSnapshot>::value);
    STATIC_REQUIRE_FALSE(std::is_aggregate<AffineHatchCellsSnapshot>::value);
    const auto body=generate_planar_body(native_body_partition(planar_body_config(),{},{},4.2,
        "tests/nonplanar/data/affine-wedge-1-in-16.stl")); REQUIRE(body.snapshot);
    const BodyMaterialParameters parameters{{0,0,0},{17,Length(.01),Length(.01),Length(.01),Length(.01),Length(0)},
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},Speed(20),Speed(30),Acceleration(100),7,8};
    MaterialLimits capture; capture.timeout=std::chrono::seconds(5);
    const auto material=reconstruct_planar_body_material(body,parameters,capture); REQUIRE(material.snapshot);
    const NativeAffinePassRequest request{{19,17,21,23},0,4.1,
        {4,{VerticalGap(.1),VerticalGap(.4),Length(.00001)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.01)}};
    NativeAffinePassLimits pass_limits;pass_limits.material.max_cells=8191;pass_limits.material.timeout=std::chrono::seconds(5);
    pass_limits.material.maximum_interval_width=Volume(.01);
    AffineHatchLimits hatch_limits;hatch_limits.timeout=std::chrono::seconds(10);hatch_limits.volumes.max_cells=32768;
    hatch_limits.volumes.timeout=std::chrono::seconds(5);hatch_limits.volumes.maximum_interval_width=Volume(.01);
    const auto native=plan_native_affine_hatches(material,request,{WidthXY(.45),Length(.4),Length(.2),HatchDirection::AlongX},
        pass_limits,hatch_limits); INFO(native.reason); REQUIRE(native.snapshot);
    const auto cells=allocate_affine_hatch_cells({"",native.snapshot->hatches},hatch_limits.volumes); INFO(cells.reason); REQUIRE(cells.snapshot);
    const auto &allocation=*cells.snapshot; REQUIRE(allocation.source==native.snapshot->hatches); REQUIRE(allocation.passes.size()==4);
    INFO("finite=[" << std::setprecision(18) << allocation.finite_volume_mm3.lower << ',' << allocation.finite_volume_mm3.upper <<
        "] remainder=[" << allocation.remainder_volume_mm3.lower << ',' << allocation.remainder_volume_mm3.upper <<
        "] total=[" << allocation.total_volume_mm3.lower << ',' << allocation.total_volume_mm3.upper <<
        "] cells=" << allocation.proof_cells << " work=" << allocation.evaluations);
    REQUIRE(allocation.finite_volume_mm3.lower>0); REQUIRE(allocation.remainder_volume_mm3.lower>0);
    REQUIRE(allocation.finite_volume_mm3.upper<allocation.total_volume_mm3.lower);
    REQUIRE(allocation.total_volume_mm3.upper-allocation.total_volume_mm3.lower<=.01);
    const auto &parent=native.snapshot->hatches->total_prospective_volume_mm3;
    REQUIRE(allocation.total_volume_mm3.lower<=parent.upper); REQUIRE(allocation.total_volume_mm3.upper>=parent.lower);
    for (size_t p=0; p<4; ++p) {
        const auto &pass=allocation.passes[p]; REQUIRE(pass.finite_cells.size()==native.snapshot->hatches->passes[p].lines.size());
        REQUIRE(pass.remainder_cells.size()==4);
        for (const auto &cell : pass.finite_cells) REQUIRE(cell.volume_mm3.lower>0);
        for (const auto &cell : pass.remainder_cells) REQUIRE(cell.volume_mm3.lower>0);
        if (p) {
            const auto &above=native.snapshot->passes->stack->surfaces[p].cell;
            const auto &below=native.snapshot->passes->stack->surfaces[p-1].cell;
            const auto &r=pass.finite_footprint;
            const long double x=(static_cast<long double>(r.min_x)+r.max_x)/2, y=(static_cast<long double>(r.min_y)+r.max_y)/2;
            const auto z=[&](const AffineCapCell &c) { return static_cast<long double>(c.z00)+
                (static_cast<long double>(c.z10)-c.z00)*(x-19)/2+(static_cast<long double>(c.z01)-c.z00)*(y-17)/6; };
            const long double expected=(static_cast<long double>(r.max_x)-r.min_x)*(static_cast<long double>(r.max_y)-r.min_y)*(z(above)-z(below));
            REQUIRE(pass.finite_volume_mm3.lower<=expected); REQUIRE(pass.finite_volume_mm3.upper>=expected);
        }
    }
    auto limits=hatch_limits.volumes; limits.is_current=[](uint64_t){return false;};
    REQUIRE_FALSE(allocate_affine_hatch_cells({"",native.snapshot->hatches},limits).snapshot);
}

TEST_CASE("B07 native prospective later affine hatches select consistent rounded bead amounts", "[Nonplanar][B07][NativeFixedWidthBeads]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<FixedWidthBeadSnapshot>::value);
    const auto body=generate_planar_body(native_body_partition(planar_body_config(),{},{},4.2,
        "tests/nonplanar/data/affine-wedge-1-in-16.stl")); REQUIRE(body.snapshot);
    const BodyMaterialParameters parameters{{0,0,0},{17,Length(.01),Length(.01),Length(.01),Length(.01),Length(0)},
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},Speed(20),Speed(30),Acceleration(100),7,8};
    MaterialLimits capture; capture.timeout=std::chrono::seconds(5);
    const auto material=reconstruct_planar_body_material(body,parameters,capture); REQUIRE(material.snapshot);
    const NativeAffinePassRequest request{{19,17,21,23},0,4.1,
        {4,{VerticalGap(.1),VerticalGap(.4),Length(.00001)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.01)}};
    NativeAffinePassLimits pass_limits;pass_limits.material.max_cells=8191;pass_limits.material.timeout=std::chrono::seconds(5);
    pass_limits.material.maximum_interval_width=Volume(.01);
    AffineHatchLimits hatch_limits;hatch_limits.timeout=std::chrono::seconds(10);hatch_limits.volumes.max_cells=32768;
    hatch_limits.volumes.timeout=std::chrono::seconds(5);hatch_limits.volumes.maximum_interval_width=Volume(.01);
    const auto native=plan_native_affine_hatches(material,request,{WidthXY(.45),Length(.4),Length(.2),HatchDirection::AlongX},
        pass_limits,hatch_limits); INFO(native.reason); REQUIRE(native.snapshot);
    size_t lines=0,packets=0;long double amount=0;
    // Only the prospective affine later interfaces have declared affine gaps.
    // Actual first-roof gaps and later deposited support are separate proofs.
    for (size_t p=1; p<native.snapshot->hatches->passes.size(); ++p) {
        const auto &lower=native.snapshot->passes->stack->surfaces[p-1].cell;
        const auto gap=[&](PhysicalPosition point) {
            const long double z=static_cast<long double>(lower.z00)+(static_cast<long double>(lower.z10)-lower.z00)*(point.x()-19)/2+
                (static_cast<long double>(lower.z01)-lower.z00)*(point.y()-17)/6;
            return double(point.z()-z);
        };
        for (const auto &line : native.snapshot->hatches->passes[p].lines) {
            ++lines; const FixedWidthBeadRequest input{line.start,line.end,line.width,VerticalGap(gap(line.start)),VerticalGap(gap(line.end)),
                BeadSectionKind::RoundedRectangle,body.snapshot->revision,material.snapshot->material_fingerprint};
            FixedWidthBeadLimits limits;limits.maximum_width_error=Length(.002);limits.maximum_volume_error=Volume(.00001);
            const auto planned=plan_fixed_width_bead(input,limits);INFO(planned.reason);REQUIRE(planned.snapshot);
            REQUIRE(planned.snapshot->request.source_fingerprint==material.snapshot->material_fingerprint);
            std::vector<MaterialRecord> rows;
            for (const auto &piece : planned.snapshot->pieces) {
                const auto index=rows.size();++packets;amount+=piece.volume.value();
                rows.push_back({{index+1,index,33,piece.start,piece.end,Speed(20),Acceleration(100),Deposition{piece.volume,piece.nominal_width,
                    VerticalGap(std::min(piece.section.gap_begin_mm,piece.section.gap_end_mm)),VerticalGap(std::max(piece.section.gap_begin_mm,piece.section.gap_end_mm)),
                    parameters.material,7,8}},piece.section});
            }
            const auto accepted=capture_material_sequence(rows,material.snapshot->material->model,body.snapshot->revision,material.snapshot->material_fingerprint);
            INFO(accepted.reason);REQUIRE(accepted.snapshot);
            const long double dx=static_cast<long double>(line.end.x())-line.start.x(),dy=static_cast<long double>(line.end.y())-line.start.y();
            const long double h0=input.gap_begin.value(),h1=input.gap_end.value(),k=1-std::acos(-1.L)/4;
            const long double expected=std::sqrt(dx*dx+dy*dy)*(.45L*(h0+h1)/2-k*(h0*h0+h0*h1+h1*h1)/3);
            REQUIRE(planned.snapshot->target_volume_mm3.lower<=expected);REQUIRE(planned.snapshot->target_volume_mm3.upper>=expected);
        }
    }
    INFO("later lines=" << lines << " packets=" << packets << " rounded amount=" << std::setprecision(18) << amount);
    REQUIRE(lines>0);REQUIRE(packets>=lines);REQUIRE(amount>0);
}

TEST_CASE("B07 native first hatches reconstruct actual laid roof gaps and consume bounded amounts", "[Nonplanar][B07][NativeFirstHatchBeads]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<FirstHatchBeadSnapshot>::value);
    const auto body=generate_planar_body(native_body_partition(planar_body_config(),{},{},4.2,
        "tests/nonplanar/data/affine-wedge-1-in-16.stl")); REQUIRE(body.snapshot);
    const BodyMaterialParameters parameters{{0,0,0},{17,Length(.01),Length(.01),Length(.01),Length(.01),Length(0)},
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},Speed(20),Speed(30),Acceleration(100),7,8};
    MaterialLimits capture;capture.timeout=std::chrono::seconds(5);
    const auto material=reconstruct_planar_body_material(body,parameters,capture);REQUIRE(material.snapshot);
    const NativeAffinePassRequest request{{19,17,21,23},0,4.1,
        {4,{VerticalGap(.1),VerticalGap(.4),Length(.00001)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.01)}};
    NativeAffinePassLimits pass_limits;pass_limits.material.max_cells=8191;pass_limits.material.timeout=std::chrono::seconds(5);
    pass_limits.material.maximum_interval_width=Volume(.01);
    AffineHatchLimits hatch_limits;hatch_limits.timeout=std::chrono::seconds(10);hatch_limits.volumes.max_cells=32768;
    hatch_limits.volumes.timeout=std::chrono::seconds(5);hatch_limits.volumes.maximum_interval_width=Volume(.01);
    const auto native=plan_native_affine_hatches(material,request,{WidthXY(.45),Length(.4),Length(.2),HatchDirection::AlongX},
        pass_limits,hatch_limits);INFO(native.reason);REQUIRE(native.snapshot);
    FirstHatchBeadLimits limits;limits.timeout=std::chrono::seconds(5);limits.packets.timeout=std::chrono::seconds(5);
    limits.maximum_gap_error=Length(.0001);limits.packets.maximum_width_error=Length(.002);limits.packets.maximum_volume_error=Volume(.0001);
    size_t packets=0,roof_segments=0,work=0;long double amount=0,plane_amount=0;double gap_error=0,width_error=0,volume_error=0;
    const long double k=1-std::acos(-1.L)/4;
    // An independent point section reconstruction checks generated packet
    // interior gaps/widths. Continuous acceptance comes from the production
    // interval proof plus the independent analytic geometric cases, not samples.
    const auto roof_at=[&](PhysicalPosition p) {
        long double roof=-10000;
        for (const auto &row : material.snapshot->material->records) {
            if (!row.bead || std::max(row.motion.start.z(),row.motion.end.z())<4.1) continue;
            const long double dx=static_cast<long double>(row.motion.end.x())-row.motion.start.x(),dy=static_cast<long double>(row.motion.end.y())-row.motion.start.y();
            const long double length=std::sqrt(dx*dx+dy*dy),x=static_cast<long double>(p.x())-row.motion.start.x(),y=static_cast<long double>(p.y())-row.motion.start.y();
            const long double t=(x*dx+y*dy)/(length*length);if (t<0 || t>1) continue;
            const long double h=row.bead->gap_begin_mm+t*(static_cast<long double>(row.bead->gap_end_mm)-row.bead->gap_begin_mm);
            const long double top=row.motion.start.z()+t*(static_cast<long double>(row.motion.end.z())-row.motion.start.z());
            const long double area=std::get<Deposition>(row.motion.payload).volume.value()/length;
            const long double width=area/h+(row.bead->kind==BeadSectionKind::RoundedRectangle ? k*h : 0),normal=std::abs((-dy*x+dx*y)/length);
            if (normal>width/2) continue;
            if (row.bead->kind==BeadSectionKind::Rectangle) roof=std::max(roof,top);
            else {const long double transverse=std::max(normal-(width-h)/2,0.L);
                roof=std::max(roof,top-h/2+std::sqrt(std::max(h*h/4-transverse*transverse,0.L)));}
        }
        return roof;
    };
    const auto &lines=native.snapshot->hatches->passes.front().lines;
    for (size_t i=0;i<lines.size();++i) {
        const auto planned=plan_first_hatch_bead({"",native.snapshot->hatches},i,limits);INFO("line=" << i << " " << planned.reason);REQUIRE(planned.snapshot);
        const auto &plan=*planned.snapshot;REQUIRE(plan.source==native.snapshot->hatches);REQUIRE(plan.line_index==i);
        REQUIRE(plan.total_volume_error_mm3<=limits.packets.maximum_volume_error.value());
        REQUIRE(plan.maximum_gap_error_mm<=limits.maximum_gap_error.value());REQUIRE(plan.maximum_width_error_mm<=limits.packets.maximum_width_error.value());
        REQUIRE(plan.numerical_error_upper_mm<=.05);
        roof_segments+=plan.roof_segments;work+=plan.evaluations;gap_error=std::max(gap_error,plan.maximum_gap_error_mm);
        width_error=std::max(width_error,plan.maximum_width_error_mm);volume_error=std::max(volume_error,plan.total_volume_error_mm3);
        std::vector<MaterialRecord> rows;
        for (const auto &piece : plan.pieces) {
            const auto index=rows.size();++packets;amount+=piece.volume.value();
            const PhysicalPosition middle{(piece.start.x()+piece.end.x())/2,(piece.start.y()+piece.end.y())/2,(piece.start.z()+piece.end.z())/2};
            const long double roof=roof_at(middle),gap=middle.z()-roof,model_gap=(static_cast<long double>(piece.section.gap_begin_mm)+piece.section.gap_end_mm)/2;
            REQUIRE(roof>=4.1);REQUIRE(std::abs(gap-model_gap)<=plan.maximum_gap_error_mm);
            const long double dx=static_cast<long double>(piece.end.x())-piece.start.x(),dy=static_cast<long double>(piece.end.y())-piece.start.y();
            const long double width=piece.volume.value()/std::sqrt(dx*dx+dy*dy)/gap+k*gap;
            REQUIRE(std::abs(width-.45L)<=plan.maximum_width_error_mm);
            rows.push_back({{index+1,index,33,piece.start,piece.end,Speed(20),Acceleration(100),Deposition{piece.volume,piece.nominal_width,
                VerticalGap(std::min(piece.section.gap_begin_mm,piece.section.gap_end_mm)),VerticalGap(std::max(piece.section.gap_begin_mm,piece.section.gap_end_mm)),
                parameters.material,7,8}},piece.section});
        }
        auto charged=material.snapshot->material->model;charged.numerical_coordinate_error=Length(plan.numerical_error_upper_mm);
        const auto accepted=capture_material_sequence(rows,charged,body.snapshot->revision,material.snapshot->material_fingerprint,capture);
        INFO(accepted.reason);REQUIRE(accepted.snapshot);
        const auto &line=lines[i];const long double h0=static_cast<long double>(line.start.z())-4.1,h1=static_cast<long double>(line.end.z())-4.1;
        const long double length=static_cast<long double>(line.end.x())-line.start.x();
        plane_amount+=length*(.45L*(h0+h1)/2-k*(h0*h0+h0*h1+h1*h1)/3);
    }
    INFO("first lines=" << lines.size() << " packets=" << packets << " roof_segments=" << roof_segments << " roof_work=" << work <<
        " rounded amount=" << std::setprecision(18) << amount << " support-plane amount=" << plane_amount <<
        " max_gap_error=" << gap_error << " max_width_error=" << width_error << " max_line_volume_error=" << volume_error);
    REQUIRE(lines.size()==14);REQUIRE(packets>lines.size());REQUIRE(roof_segments>lines.size());
    REQUIRE(amount>0);REQUIRE(amount<plane_amount-.1L);
    limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(plan_first_hatch_bead({"",native.snapshot->hatches},0,limits).snapshot);
}

TEST_CASE("B07 native first footprint derives bounded amounts inside an actual flat bead core", "[Nonplanar][B07][NativeFirstHatchFootprint][NativeRemainderHatch][NativeFirstHatchLayer][NativeFirstHatchEndReplan][NativeFirstHatchWidthReplan][NativeFirstContour][NativeFirstCap][NativeFirstCapJoin][NativeMaterialRun][NativeCapInterface]")
{
    auto config=planar_body_config();
    config.set_deserialize_strict({{"infill_direction",0},{"solid_infill_direction",0},
        {"internal_solid_infill_line_width",1.0},{"top_surface_line_width",1.0}});
    const auto body=generate_planar_body(native_body_partition(config,{},{},4.2,
        "tests/nonplanar/data/affine-wedge-1-in-16.stl"));REQUIRE(body.snapshot);
    const BodyMaterialParameters parameters{{0,0,0},{17,Length(.01),Length(.01),Length(.01),Length(.01),Length(0)},
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},Speed(20),Speed(30),Acceleration(100),7,8};
    MaterialLimits capture;capture.timeout=std::chrono::seconds(5);
    const auto material=reconstruct_planar_body_material(body,parameters,capture);INFO(material.reason);REQUIRE(material.snapshot);
    std::optional<RectangleXY> roi;size_t support_row=0;HatchDirection direction=HatchDirection::AlongX;long double flat_top=0;
    // Independent source-section calculation chooses a real native flat core.
    // No model or golden fixture is replaced with an idealized solid slab.
    for (const auto &row : material.snapshot->material->records) {
        if (!row.bead || std::abs(row.motion.start.z()-4.2)>1e-8 || row.motion.end.z()!=row.motion.start.z()) continue;
        const bool x=row.motion.start.y()==row.motion.end.y(),y=row.motion.start.x()==row.motion.end.x();if (x==y) continue;
        const long double dx=static_cast<long double>(row.motion.end.x())-row.motion.start.x(),dy=static_cast<long double>(row.motion.end.y())-row.motion.start.y();
        const long double h=row.bead->gap_begin_mm;if (row.bead->gap_end_mm!=h) continue;
        const long double width=std::get<Deposition>(row.motion.payload).volume.value()/std::sqrt(dx*dx+dy*dy)/h+(1-std::acos(-1.L)/4)*h;
        if (width-h<=.7) continue;
        if (x && std::min(row.motion.start.x(),row.motion.end.x())<18.8 && std::max(row.motion.start.x(),row.motion.end.x())>21.2 &&
            row.motion.start.y()>19 && row.motion.start.y()<21) {
            roi=RectangleXY{19,row.motion.start.y()-.3,21,row.motion.start.y()+.3};direction=HatchDirection::AlongX;
        } else if (y && std::min(row.motion.start.y(),row.motion.end.y())<18.8 && std::max(row.motion.start.y(),row.motion.end.y())>21.2 &&
            row.motion.start.x()>19 && row.motion.start.x()<21) {
            roi=RectangleXY{row.motion.start.x()-.3,19,row.motion.start.x()+.3,21};direction=HatchDirection::AlongY;
        }
        if (roi) {flat_top=row.motion.start.z();support_row=row.motion.sequence_index;break;}
    }
    REQUIRE(roi);
    const NativeAffinePassRequest request{*roi,0,4.1,
        {4,{VerticalGap(.1),VerticalGap(.4),Length(.00001)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.01)}};
    NativeAffinePassLimits pass_limits;pass_limits.material.max_cells=8191;pass_limits.material.timeout=std::chrono::seconds(5);
    pass_limits.material.maximum_interval_width=Volume(.01);
    AffineHatchLimits hatch_limits;hatch_limits.timeout=std::chrono::seconds(5);hatch_limits.volumes.timeout=std::chrono::seconds(5);
    const auto native=plan_native_affine_hatches(material,request,{WidthXY(.45),Length(.4),Length(.05),direction},pass_limits,hatch_limits);
    INFO(native.reason);REQUIRE(native.snapshot);
    FirstHatchBeadLimits limits;limits.timeout=std::chrono::seconds(5);limits.packets.timeout=std::chrono::seconds(5);
    limits.maximum_gap_error=Length(.0001);limits.packets.maximum_width_error=Length(.002);limits.packets.maximum_volume_error=Volume(.0001);
    const auto plan=plan_first_hatch_footprint_bead({"",native.snapshot->hatches},0,limits);INFO(plan.reason);REQUIRE(plan.snapshot);
    REQUIRE(plan.snapshot->roof_domain==FirstHatchRoofDomain::FiniteWidth);REQUIRE(plan.snapshot->source==native.snapshot->hatches);
    const auto &line=native.snapshot->hatches->passes.front().lines.front();
    const long double h0=static_cast<long double>(line.start.z())-flat_top,h1=static_cast<long double>(line.end.z())-flat_top;
    const long double dx=static_cast<long double>(line.end.x())-line.start.x(),dy=static_cast<long double>(line.end.y())-line.start.y();
    const long double k=1-std::acos(-1.L)/4,expected=std::sqrt(dx*dx+dy*dy)*(.45L*(h0+h1)/2-k*(h0*h0+h0*h1+h1*h1)/3);
    INFO("native footprint packets=" << plan.snapshot->pieces.size() << " roof leaves=" << plan.snapshot->roof_segments <<
        " work=" << plan.snapshot->evaluations << " expected amount=" << std::setprecision(18) << expected);
    REQUIRE(plan.snapshot->actual_target_volume_mm3.lower<=expected);REQUIRE(plan.snapshot->actual_target_volume_mm3.upper>=expected);
    REQUIRE(plan.snapshot->maximum_gap_error_mm<=limits.maximum_gap_error.value());
    REQUIRE(plan.snapshot->maximum_width_error_mm<=limits.packets.maximum_width_error.value());
    REQUIRE(plan.snapshot->total_volume_error_mm3<=limits.packets.maximum_volume_error.value());
    std::vector<MaterialRecord> rows;
    for (const auto &piece : plan.snapshot->pieces) {
        const size_t i=rows.size();rows.push_back({{i+1,i,33,piece.start,piece.end,Speed(20),Acceleration(100),Deposition{piece.volume,piece.nominal_width,
            VerticalGap(std::min(piece.section.gap_begin_mm,piece.section.gap_end_mm)),VerticalGap(std::max(piece.section.gap_begin_mm,piece.section.gap_end_mm)),
            parameters.material,7,8}},piece.section});
    }
    auto charged=material.snapshot->material->model;charged.numerical_coordinate_error=Length(plan.snapshot->numerical_error_upper_mm);
    const auto ledger=capture_material_sequence(rows,charged,body.snapshot->revision,material.snapshot->material_fingerprint,capture);REQUIRE(ledger.snapshot);
    const auto prefix=material_at(ledger.snapshot,rows.size()/2,.3,capture);REQUIRE(prefix.nominal.snapshot);
    MaterialUnionLimits volume;volume.maximum_interval_width=Volume(.00025);volume.max_cells=65535;volume.timeout=std::chrono::seconds(5);
    const SceneBox box{{roi->min_x,roi->min_y,4.0},{roi->max_x,roi->max_y,4.7}};
    const auto occupied=integrate_material_union(prefix.nominal,box,volume);INFO(occupied.reason);REQUIRE(occupied.snapshot);
    MaterialFillLimits fill_limits;fill_limits.maximum_interval_width=Volume(.001);fill_limits.max_cells=65535;fill_limits.timeout=std::chrono::seconds(5);
    const auto fill=reconcile_material_fill(native.snapshot->hatches->source->first_pass,occupied,fill_limits);INFO(fill.reason);REQUIRE(fill.snapshot);
    RemainingHatchLimits remainder;remainder.timeout=std::chrono::seconds(5);remainder.beads=limits;
    remainder.volumes.max_cells=65535;remainder.volumes.timeout=std::chrono::seconds(5);
    const auto repair=plan_remaining_first_hatch({"",native.snapshot->hatches},0,fill,{},remainder);INFO(repair.reason);REQUIRE(repair.snapshot);
    REQUIRE(repair.snapshot->paths.size()==1);REQUIRE(repair.snapshot->before->occupied->source==prefix.nominal.snapshot);
    REQUIRE(repair.snapshot->covered_target_mm3.lower>fill.snapshot->covered_target_mm3.upper);
    REQUIRE(repair.snapshot->missing_target_mm3.upper<fill.snapshot->missing_target_mm3.lower);
    REQUIRE(repair.snapshot->covered_gain_lower_mm3>.03);REQUIRE(repair.snapshot->outside_target_mm3.upper<=.001);
    INFO("native remaining gain >= " << std::setprecision(18) << repair.snapshot->covered_gain_lower_mm3 <<
        " covered=[" << repair.snapshot->covered_target_mm3.lower << ',' << repair.snapshot->covered_target_mm3.upper <<
        "] missing=[" << repair.snapshot->missing_target_mm3.lower << ',' << repair.snapshot->missing_target_mm3.upper <<
        "] cells=" << repair.snapshot->cells << " work=" << repair.snapshot->evaluations);
    FirstHatchLayerLimits layer_limits;layer_limits.beads=limits;
    layer_limits.volumes.max_cells=65535;layer_limits.volumes.max_evaluations=2000000;layer_limits.volumes.timeout=std::chrono::seconds(5);
    const auto complete=plan_first_hatch_layer({"",native.snapshot->hatches},box,layer_limits);INFO(complete.reason);REQUIRE(complete.snapshot);
    const auto &layer=*complete.snapshot;const auto &layer_fill=*layer.fill;
    REQUIRE(layer.paths.size()==native.snapshot->hatches->passes.front().lines.size());REQUIRE(layer.paths.size()==2);
    REQUIRE(layer.section_target_volume_mm3.lower<=2*expected);REQUIRE(layer.section_target_volume_mm3.upper>=2*expected);
    using LayerAmount=boost::multiprecision::cpp_bin_float_quad;LayerAmount commanded=0,one=0,extra=0;
    const bool along_x=direction==HatchDirection::AlongX;
    const auto coordinate=[&](PhysicalPosition p) {return along_x ? p.x() : p.y();};
    const auto center=[&](PhysicalPosition p) {return along_x ? p.y() : p.x();};
    const LayerAmount separation=LayerAmount(center(layer.paths[1]->path_start))-center(layer.paths[0]->path_start);
    for (size_t i=0;i<layer.paths.size();++i) for (const auto &piece : layer.paths[i]->pieces) {
        commanded+=piece.volume.value();if (i==0) {
            one+=piece.volume.value();extra+=separation*abs(LayerAmount(coordinate(piece.end))-coordinate(piece.start))*
                (LayerAmount(piece.section.gap_begin_mm)+piece.section.gap_end_mm)/2;
        }
    }
    const auto contains=[&](ScalarBounds measured,LayerAmount amount) {REQUIRE(measured.lower<=amount);REQUIRE(measured.upper>=amount);};
    contains(layer_fill.occupied->individual_volume_mm3,commanded);contains(layer_fill.occupied->union_volume_mm3,one+extra);
    contains(layer_fill.occupied->repeated_volume_mm3,one-extra);
    REQUIRE(layer.global_volume_error_mm3<=layer_limits.beads.packets.maximum_volume_error.value());
    REQUIRE(layer_fill.occupied->repeated_volume_mm3.lower>0);
    REQUIRE(layer_fill.occupied->union_volume_mm3.upper<layer_fill.occupied->individual_volume_mm3.lower);
    REQUIRE(layer_fill.covered_target_mm3.lower>fill.snapshot->covered_target_mm3.upper);
    REQUIRE(layer_fill.missing_target_mm3.lower>0);REQUIRE(layer_fill.outside_target_mm3.upper<=.001);
    INFO("native complete first layer paths=" << layer.paths.size() << " amount=[" << layer.deposited_volume_mm3.lower << ',' <<
        layer.deposited_volume_mm3.upper << "] covered=[" << layer_fill.covered_target_mm3.lower << ',' << layer_fill.covered_target_mm3.upper <<
        "] missing=[" << layer_fill.missing_target_mm3.lower << ',' << layer_fill.missing_target_mm3.upper << "] repeated=[" <<
        layer_fill.occupied->repeated_volume_mm3.lower << ',' << layer_fill.occupied->repeated_volume_mm3.upper <<
        "] cells=" << layer.cells << " work=" << layer.evaluations);
    const auto replanned=replan_first_hatch_ends(complete,{},layer_limits);INFO(replanned.reason);REQUIRE(replanned.snapshot);
    const auto &end_plan=*replanned.snapshot;const auto &end_fill=*end_plan.after->fill;
    REQUIRE(end_plan.before==complete.snapshot);REQUIRE(end_plan.after->source->source==layer.source->source);
    REQUIRE(end_fill.target==layer_fill.target);REQUIRE(end_plan.after->source->first_pass_extent==FirstHatchExtent::FiniteButtInset);
    REQUIRE(end_plan.after->source->policy.boundary_band.value()==layer.source->policy.boundary_band.value());
    REQUIRE(end_plan.after->paths.size()==layer.paths.size());REQUIRE(end_plan.covered_gain_mm3.lower>.04);
    REQUIRE(end_plan.missing_reduction_mm3.lower>.04);REQUIRE(end_fill.missing_target_mm3.lower>0);REQUIRE(end_fill.outside_target_mm3.upper<=.001);
    LayerAmount end_one=0,end_extra=0,end_sum=0;
    for (size_t i=0;i<end_plan.after->paths.size();++i) for (const auto &piece : end_plan.after->paths[i]->pieces) {
        end_sum+=piece.volume.value();if (i==0) {end_one+=piece.volume.value();end_extra+=separation*abs(LayerAmount(coordinate(piece.end))-coordinate(piece.start))*
            (LayerAmount(piece.section.gap_begin_mm)+piece.section.gap_end_mm)/2;}
    }
    contains(end_fill.occupied->individual_volume_mm3,end_sum);contains(end_fill.occupied->union_volume_mm3,end_one+end_extra);
    contains(end_fill.occupied->repeated_volume_mm3,end_one-end_extra);
    REQUIRE(end_plan.after->global_volume_error_mm3<=layer_limits.beads.packets.maximum_volume_error.value());
    INFO("native whole end replan covered=[" << end_fill.covered_target_mm3.lower << ',' << end_fill.covered_target_mm3.upper <<
        "] missing=[" << end_fill.missing_target_mm3.lower << ',' << end_fill.missing_target_mm3.upper << "] gain=[" <<
        end_plan.covered_gain_mm3.lower << ',' << end_plan.covered_gain_mm3.upper << "] cells=" << end_plan.cells << " work=" << end_plan.evaluations);
    const auto narrower=replan_first_hatch_width({"",end_plan.after},WidthXY(.4),{},layer_limits);INFO(narrower.reason);REQUIRE(narrower.snapshot);
    const auto &width_plan=*narrower.snapshot;const auto &width_fill=*width_plan.after->fill;
    REQUIRE(width_plan.before==end_plan.after);REQUIRE(width_plan.after->source->source==layer.source->source);
    REQUIRE(width_fill.target==layer_fill.target);REQUIRE(width_plan.after->source->policy.width.value()==.45);
    REQUIRE(width_plan.repeated_reduction_mm3.lower>.03);REQUIRE(width_plan.commanded_reduction_mm3.lower>.03);
    REQUIRE(width_plan.covered_change_mm3.lower>=-.001);REQUIRE(width_fill.missing_target_mm3.lower>0);REQUIRE(width_fill.outside_target_mm3.upper<=.001);
    LayerAmount width_sum=0,width_one=0,width_extra=0;
    const LayerAmount width_separation=LayerAmount(center(width_plan.after->paths[1]->path_start))-center(width_plan.after->paths[0]->path_start);
    for (size_t i=0;i<width_plan.after->paths.size();++i) for (const auto &piece : width_plan.after->paths[i]->pieces) {
        REQUIRE(piece.nominal_width.value()==.4);width_sum+=piece.volume.value();
        if (!i) {width_one+=piece.volume.value();width_extra+=width_separation*abs(LayerAmount(coordinate(piece.end))-coordinate(piece.start))*
            (LayerAmount(piece.section.gap_begin_mm)+piece.section.gap_end_mm)/2;}
    }
    contains(width_fill.occupied->individual_volume_mm3,width_sum);contains(width_fill.occupied->union_volume_mm3,width_one+width_extra);
    contains(width_fill.occupied->repeated_volume_mm3,width_one-width_extra);
    REQUIRE(width_plan.after->global_volume_error_mm3<=layer_limits.beads.packets.maximum_volume_error.value());
    INFO("native whole width replan covered=[" << width_fill.covered_target_mm3.lower << ',' << width_fill.covered_target_mm3.upper <<
        "] missing=[" << width_fill.missing_target_mm3.lower << ',' << width_fill.missing_target_mm3.upper << "] repeated reduction=[" <<
        width_plan.repeated_reduction_mm3.lower << ',' << width_plan.repeated_reduction_mm3.upper << "] covered change=[" <<
        width_plan.covered_change_mm3.lower << ',' << width_plan.covered_change_mm3.upper << "] cells=" << width_plan.cells << " work=" << width_plan.evaluations);
    const auto contour=plan_first_contour({"",native.snapshot->hatches},{WidthXY(.4),0,false,Volume(.001)},box,layer_limits);INFO(contour.reason);REQUIRE(contour.snapshot);
    const auto &loop=*contour.snapshot;REQUIRE(loop.edges.size()==4);REQUIRE(loop.source==native.snapshot->hatches);REQUIRE(loop.fill->target==layer_fill.target);
    LayerAmount contour_sum=0;
    for (size_t i=0;i<loop.edges.size();++i) {
        const auto &edge=*loop.edges[i],&next=*loop.edges[(i+1)%loop.edges.size()];
        REQUIRE_FALSE(edge.line_index.has_value());REQUIRE(edge.roof_domain==FirstHatchRoofDomain::FiniteWidth);
        REQUIRE(edge.path_end.x()==next.path_start.x());REQUIRE(edge.path_end.y()==next.path_start.y());REQUIRE(edge.path_end.z()==next.path_start.z());
        const LayerAmount h0=LayerAmount(edge.path_start.z())-LayerAmount(flat_top),h1=LayerAmount(edge.path_end.z())-LayerAmount(flat_top);
        const LayerAmount l=sqrt(pow(LayerAmount(edge.path_end.x())-edge.path_start.x(),2)+pow(LayerAmount(edge.path_end.y())-edge.path_start.y(),2));
        const LayerAmount k=1-acos(LayerAmount(-1))/4,ideal=l*(LayerAmount(.4)*(h0+h1)/2-k*(h0*h0+h0*h1+h1*h1)/3);
        contains(edge.actual_target_volume_mm3,ideal);
        for (const auto &piece : edge.pieces) {REQUIRE(piece.nominal_width.value()==.4);contour_sum+=piece.volume.value();}
    }
    contains(loop.fill->occupied->individual_volume_mm3,contour_sum);
    REQUIRE(loop.fill->occupied->repeated_volume_mm3.lower>0);REQUIRE(loop.fill->covered_target_mm3.lower>0);
    REQUIRE(loop.fill->missing_target_mm3.lower>0);REQUIRE(loop.fill->outside_target_mm3.upper<=.001);
    REQUIRE(loop.global_volume_error_mm3<=layer_limits.beads.packets.maximum_volume_error.value());
    INFO("native closed first contour amount=[" << loop.deposited_volume_mm3.lower << ',' << loop.deposited_volume_mm3.upper <<
        "] covered=[" << loop.fill->covered_target_mm3.lower << ',' << loop.fill->covered_target_mm3.upper <<
        "] missing=[" << loop.fill->missing_target_mm3.lower << ',' << loop.fill->missing_target_mm3.upper <<
        "] outside=[" << loop.fill->outside_target_mm3.lower << ',' << loop.fill->outside_target_mm3.upper <<
        "] cells=" << loop.cells << " work=" << loop.evaluations);
    // A wider ROI on the same actual sliced bead admits an interior owner.
    // The original narrow ROI and all its earlier obligations remain tested.
    auto wide=*roi;
    const double centre=along_x ? (wide.min_y+wide.max_y)/2 : (wide.min_x+wide.max_x)/2;
    if (along_x) {wide.min_y=centre-.375;wide.max_y=centre+.375;}
    else {wide.min_x=centre-.375;wide.max_x=centre+.375;}
    bool wide_actual_core=false;
    for (const auto &row : material.snapshot->material->records) {
        if (!row.bead || row.motion.start.z()!=flat_top || row.motion.end.z()!=flat_top || row.bead->gap_begin_mm!=row.bead->gap_end_mm) continue;
        const bool x=row.motion.start.y()==row.motion.end.y(),y=row.motion.start.x()==row.motion.end.x();if (x==y || x!=along_x) continue;
        const LayerAmount l=sqrt(pow(LayerAmount(row.motion.end.x())-row.motion.start.x(),2)+pow(LayerAmount(row.motion.end.y())-row.motion.start.y(),2));
        const LayerAmount h=row.bead->gap_begin_mm,k=1-acos(LayerAmount(-1))/4;
        const LayerAmount core=(LayerAmount(std::get<Deposition>(row.motion.payload).volume.value())/l/h+k*h-h)/2;
        const LayerAmount normal=x ? row.motion.start.y() : row.motion.start.x();
        wide_actual_core=abs(LayerAmount(x ? wide.min_y : wide.min_x)-normal)<core && abs(LayerAmount(x ? wide.max_y : wide.max_x)-normal)<core &&
            (x ? wide.min_x : wide.min_y)>std::min(x ? row.motion.start.x() : row.motion.start.y(),x ? row.motion.end.x() : row.motion.end.y()) &&
            (x ? wide.max_x : wide.max_y)<std::max(x ? row.motion.start.x() : row.motion.start.y(),x ? row.motion.end.x() : row.motion.end.y());
        if (wide_actual_core) break;
    }
    REQUIRE(wide_actual_core);
    auto wide_request=request;wide_request.footprint=wide;
    const auto wide_hatches=plan_native_affine_hatches(material,wide_request,{WidthXY(.4),Length(.15),Length(.05),direction},pass_limits,hatch_limits);
    INFO(wide_hatches.reason);REQUIRE(wide_hatches.snapshot);REQUIRE(wide_hatches.snapshot->passes->body_material==native.snapshot->passes->body_material);
    const SceneBox wide_box{{wide.min_x,wide.min_y,4.0},{wide.max_x,wide.max_y,4.7}};
    const auto first_cap=plan_first_cap({"",wide_hatches.snapshot->hatches},{WidthXY(.4),0,false,Volume(.001)},wide_box,layer_limits);
    INFO(first_cap.reason);REQUIRE(first_cap.snapshot);const auto &cap=*first_cap.snapshot;
    REQUIRE(cap.paths.size()==5);REQUIRE(cap.paths.back()->line_index==1);REQUIRE((cap.replaced_boundary_lines==std::vector<size_t>{0,2}));
    REQUIRE(cap.source==wide_hatches.snapshot->hatches);REQUIRE(cap.fill->target==cap.source->source->first_pass.proof);
    LayerAmount cap_sum=0;size_t cap_packets=0;
    for (size_t i=0;i<cap.paths.size();++i) {
        const auto &path=*cap.paths[i];REQUIRE(path.line_index.has_value()==(i>=4));REQUIRE(path.roof_domain==FirstHatchRoofDomain::FiniteWidth);
        const LayerAmount h0=LayerAmount(path.path_start.z())-flat_top,h1=LayerAmount(path.path_end.z())-flat_top;
        const LayerAmount l=sqrt(pow(LayerAmount(path.path_end.x())-path.path_start.x(),2)+pow(LayerAmount(path.path_end.y())-path.path_start.y(),2));
        const LayerAmount k=1-acos(LayerAmount(-1))/4,ideal=l*(LayerAmount(.4)*(h0+h1)/2-k*(h0*h0+h0*h1+h1*h1)/3);
        contains(path.actual_target_volume_mm3,ideal);
        for (const auto &p : path.pieces) {REQUIRE(p.nominal_width.value()==.4);cap_sum+=p.volume.value();++cap_packets;}
    }
    contains(cap.deposited_volume_mm3,cap_sum);contains(cap.fill->occupied->individual_volume_mm3,cap_sum);
    REQUIRE(cap.fill->occupied->repeated_volume_mm3.lower>0);REQUIRE(cap.fill->covered_target_mm3.lower>0);REQUIRE(cap.fill->missing_target_mm3.lower>0);
    REQUIRE(cap.fill->outside_target_mm3.upper<=.001);REQUIRE(cap.global_volume_error_mm3<=layer_limits.beads.packets.maximum_volume_error.value());
    INFO("native combined first cap paths=" << cap.paths.size() << " packets=" << cap_packets << " amount=[" << cap.deposited_volume_mm3.lower << ',' << cap.deposited_volume_mm3.upper <<
        "] covered=[" << cap.fill->covered_target_mm3.lower << ',' << cap.fill->covered_target_mm3.upper << "] missing=[" << cap.fill->missing_target_mm3.lower << ',' << cap.fill->missing_target_mm3.upper <<
        "] outside=[" << cap.fill->outside_target_mm3.lower << ',' << cap.fill->outside_target_mm3.upper << "] cells=" << cap.cells << " work=" << cap.evaluations);
    const auto joined=assess_first_cap_joins(first_cap);INFO(joined.reason);REQUIRE_FALSE(joined.snapshot);
    REQUIRE(joined.reason=="FIRST_CAP_JOIN_NOT_CERTIFIED paths=2,3");
    // A real high corner has a common lower volume. At the low corner all
    // overlapping long-side packets are shorter than their two eroded butts;
    // do not turn nominal overlap into a fabricated full lower-material join.
    const auto cap_prefix=cap.fill->occupied->source;const auto &sequence=*cap_prefix->sequence;
    const size_t high_end=cap.paths[0]->pieces.size()-1;
    const auto high_join=find_material_join({cap_prefix},high_end,high_end+1,wide_box);INFO(high_join.reason);REQUIRE(high_join.snapshot);
    REQUIRE(high_join.snapshot->source==cap_prefix);test::independent_join_box(*high_join.snapshot);
    const auto &end=cap.paths[3]->pieces.front();
    const LayerAmount half=LayerAmount(end.section.width_mm.upper)/2;
    const LayerAmount loss=LayerAmount(sequence.model.inner_xy_loss.value())+sequence.model.numerical_coordinate_error.value();
    size_t empty_packets=0;
    for (const auto &p : cap.paths[2]->pieces) {
        const LayerAmount xmin=std::min(p.start.x(),p.end.x()),xmax=std::max(p.start.x(),p.end.x());
        if (xmin>LayerAmount(end.start.x())+half || xmax<LayerAmount(end.start.x())-half) continue;
        REQUIRE(xmax-xmin<=2*loss);++empty_packets;
    }
    REQUIRE(empty_packets>0);
    INFO("native high-corner common lower box=[" << high_join.snapshot->box_volume_mm3.lower << ',' << high_join.snapshot->box_volume_mm3.upper <<
        "] cells=" << high_join.cells << " work=" << high_join.evaluations << " low-corner empty packets=" << empty_packets <<
        " whole candidate join refusal cells=" << joined.cells << " work=" << joined.evaluations);
    const auto run_joins=assess_first_cap_run_joins(first_cap);INFO(run_joins.reason);REQUIRE(run_joins.snapshot);
    REQUIRE(run_joins.snapshot->source==first_cap.snapshot);REQUIRE(run_joins.snapshot->joins.size()==6);
    double smallest=1;
    for (const auto &join : run_joins.snapshot->joins) {
        REQUIRE(join.material->first_run->source==cap_prefix);REQUIRE(join.material->second_run->source==cap_prefix);
        REQUIRE(join.material->box_volume_mm3.lower>=1e-6);smallest=std::min(smallest,join.material->box_volume_mm3.lower);
        test::independent_run_box(*join.material->first_run,join.material->witness);
        test::independent_run_box(*join.material->second_run,join.material->witness);
    }
    const auto run=reconstruct_material_run({cap_prefix},0,high_end);REQUIRE(run.snapshot);
    const auto a=cap.paths[0]->path_start,b=cap.paths[0]->path_end;const bool x=run.snapshot->axis==MaterialRunAxis::X;
    const SceneBox whole=x ? SceneBox{{std::min(a.x(),b.x())+.03,a.y()-.025,double(flat_top)+.025},
                                     {std::max(a.x(),b.x())-.03,a.y()+.025,double(flat_top)+.035}} :
                            SceneBox{{a.x()-.025,std::min(a.y(),b.y())+.03,double(flat_top)+.025},
                                     {a.x()+.025,std::max(a.y(),b.y())-.03,double(flat_top)+.035}};
    const auto continuous=cover_material_run_lower(run,whole);INFO(continuous.reason);REQUIRE(continuous.snapshot);
    test::independent_run_box(*run.snapshot,whole);
    INFO("native continuous-run joins=" << run_joins.snapshot->joins.size() << " smallest box=" << smallest <<
        " cells=" << run_joins.cells << " work=" << run_joins.evaluations << " whole long run cells=" << continuous.cells << " work=" << continuous.evaluations);
    const FirstCapInterfacePolicy interface_policy{Length(.02),Length(.125),Length(.003),Length(.003),Length(.05)};
    const auto interface=assess_first_cap_interface(first_cap,interface_policy);INFO(interface.reason);REQUIRE(interface.snapshot);
    REQUIRE(interface.snapshot->source==first_cap.snapshot);REQUIRE(interface.snapshot->packets.size()==cap_packets);
    test::independent_lower_box(*interface.snapshot->body,support_row,interface.snapshot->anchor);
    for (const auto &p : interface.snapshot->packets) {
        const auto &piece=cap.paths[p.path]->pieces[p.packet];
        const LayerAmount error=LayerAmount(interface.snapshot->body->sequence->model.numerical_coordinate_error.value())+cap.numerical_error_upper_mm;
        for (auto endpoint : {std::pair<PhysicalPosition,double>{piece.start,piece.section.gap_begin_mm},{piece.end,piece.section.gap_end_mm}}) {
            const LayerAmount floor=LayerAmount(endpoint.first.z())-endpoint.second;
            REQUIRE(p.nominal_floor_mm.lower<=floor-error);REQUIRE(p.nominal_floor_mm.upper>=floor+error);
            REQUIRE(p.nominal_separation_mm.lower<=floor-LayerAmount(flat_top)-error);
            REQUIRE(p.nominal_separation_mm.upper>=floor-LayerAmount(flat_top)+error);
        }
        REQUIRE(p.support_distance_mm.upper<=interface_policy.maximum_support_separation.value());
    }
    INFO("native cap/body flat-floor packets=" << interface.snapshot->packets.size() << " anchor volume=[" << interface.snapshot->anchor_volume_mm3.lower << ',' <<
        interface.snapshot->anchor_volume_mm3.upper << "] cells=" << interface.cells << " work=" << interface.evaluations);
    limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(plan_first_hatch_footprint_bead({"",native.snapshot->hatches},0,limits).snapshot);
}

TEST_CASE("B07 native first-hatch union measures rounded overlap rather than summed extrusion", "[Nonplanar][B07][NativeMaterialUnion][NativeMaterialFill][NativeMaterialDeficit]")
{
    STATIC_REQUIRE_FALSE(std::is_aggregate<MaterialUnionSnapshot>::value);
    STATIC_REQUIRE_FALSE(std::is_aggregate<MaterialFillSnapshot>::value);
    const auto body=generate_planar_body(native_body_partition(planar_body_config(),{},{},4.2,
        "tests/nonplanar/data/affine-wedge-1-in-16.stl"));REQUIRE(body.snapshot);
    const BodyMaterialParameters parameters{{0,0,0},{17,Length(.01),Length(.01),Length(.01),Length(.01),Length(0)},
        {NominalMaterialId(1),UpperMaterialId(2),LowerMaterialId(3)},Speed(20),Speed(30),Acceleration(100),7,8};
    MaterialLimits capture;capture.timeout=std::chrono::seconds(5);
    const auto material=reconstruct_planar_body_material(body,parameters,capture);REQUIRE(material.snapshot);
    const NativeAffinePassRequest request{{19,17,21,23},0,4.1,
        {4,{VerticalGap(.1),VerticalGap(.4),Length(.00001)},VerticalGap(.14),VerticalGap(.24),NormalGap(.14),NormalGap(.24),Volume(.01)}};
    NativeAffinePassLimits pass_limits;pass_limits.material.max_cells=8191;pass_limits.material.timeout=std::chrono::seconds(5);
    pass_limits.material.maximum_interval_width=Volume(.01);
    AffineHatchLimits hatch_limits;hatch_limits.timeout=std::chrono::seconds(10);hatch_limits.volumes.max_cells=32768;
    hatch_limits.volumes.timeout=std::chrono::seconds(5);hatch_limits.volumes.maximum_interval_width=Volume(.01);
    const auto native=plan_native_affine_hatches(material,request,{WidthXY(.45),Length(.4),Length(.2),HatchDirection::AlongX},
        pass_limits,hatch_limits);INFO(native.reason);REQUIRE(native.snapshot);
    FirstHatchBeadLimits bead_limits;bead_limits.timeout=std::chrono::seconds(5);bead_limits.packets.timeout=std::chrono::seconds(5);
    bead_limits.packets.maximum_width_error=Length(.002);
    std::vector<MaterialRecord> rows;double numeric=0;size_t packets=0;
    // Apple ARM64 long double is binary64. Sum the binary amounts in 113 bits.
    using Amount=boost::multiprecision::cpp_bin_float_quad;Amount amount=0;
    const auto &lines=native.snapshot->hatches->passes.front().lines;
    for (size_t i=0;i<lines.size();++i) {
        const auto planned=plan_first_hatch_bead({"",native.snapshot->hatches},i,bead_limits);INFO(planned.reason);REQUIRE(planned.snapshot);
        numeric=std::max(numeric,planned.snapshot->numerical_error_upper_mm);
        // Declared geometry ledger only. These connector travels are not
        // approved motion or an executable order; the public gate remains shut.
        if (!rows.empty()) {const auto index=rows.size();rows.push_back({{index+1,index,0,rows.back().motion.end,
            planned.snapshot->pieces.front().start,Speed(30),Acceleration(100),Travel{}},{}});}
        for (const auto &piece : planned.snapshot->pieces) {
            ++packets;amount+=piece.volume.value();const auto index=rows.size();
            rows.push_back({{index+1,index,33,piece.start,piece.end,Speed(20),Acceleration(100),Deposition{piece.volume,piece.nominal_width,
                VerticalGap(std::min(piece.section.gap_begin_mm,piece.section.gap_end_mm)),VerticalGap(std::max(piece.section.gap_begin_mm,piece.section.gap_end_mm)),
                parameters.material,7,8}},piece.section});
        }
    }
    auto charged=material.snapshot->material->model;charged.numerical_coordinate_error=Length(numeric);
    const auto ledger=capture_material_sequence(rows,charged,body.snapshot->revision,material.snapshot->material_fingerprint,capture);
    INFO(ledger.reason);REQUIRE(ledger.snapshot);const auto prefix=material_at(ledger.snapshot,rows.size(),0,capture);REQUIRE(prefix.nominal.snapshot);
    MaterialUnionLimits limits;limits.max_cells=65535;limits.max_evaluations=2000000;limits.timeout=std::chrono::seconds(10);
    limits.maximum_interval_width=Volume(.01);
    const auto union_started=std::chrono::steady_clock::now();
    const auto result=integrate_material_union(prefix.nominal,{{19,17,4.0},{21,23,4.7}},limits);
    INFO("native union seconds=" << std::chrono::duration<double>(std::chrono::steady_clock::now()-union_started).count());
    const auto occupied=result.provisional_union_mm3.value_or(ScalarBounds{0,0}),excess=result.provisional_excess_mm3.value_or(ScalarBounds{0,0});
    INFO("provisional occupied=[" << std::setprecision(18) << occupied.lower << ',' << occupied.upper << "] excess=[" << excess.lower << ',' << excess.upper << ']');
    INFO(result.reason << " packets=" << packets << " cells=" << result.cells << " work=" << result.evaluations);REQUIRE(result.snapshot);
    const auto &proof=*result.snapshot;
    INFO("first lines=" << lines.size() << " packets=" << packets << " summed amount=" << std::setprecision(18) << amount <<
        " occupied=[" << proof.union_volume_mm3.lower << ',' << proof.union_volume_mm3.upper << "] repeated=[" <<
        proof.repeated_volume_mm3.lower << ',' << proof.repeated_volume_mm3.upper << "] cells=" << proof.cells << " work=" << proof.evaluations);
    REQUIRE(lines.size()==14);REQUIRE(packets>lines.size());REQUIRE(proof.source==prefix.nominal.snapshot);
    REQUIRE(Amount(proof.individual_volume_mm3.lower)<=amount);REQUIRE(Amount(proof.individual_volume_mm3.upper)>=amount);
    REQUIRE(proof.union_volume_mm3.lower>1);REQUIRE(proof.repeated_volume_mm3.lower>0);
    REQUIRE(proof.union_volume_mm3.upper<proof.individual_volume_mm3.lower);
    REQUIRE(proof.union_volume_mm3.upper-proof.union_volume_mm3.lower<=.01);
    REQUIRE(proof.repeated_volume_mm3.upper-proof.repeated_volume_mm3.lower<=.01);
    limits.is_current=[](uint64_t){return false;};REQUIRE_FALSE(integrate_material_union(prefix.nominal,proof.domain,limits).snapshot);
    MaterialFillLimits fill_limits;fill_limits.maximum_interval_width=Volume(.02);fill_limits.max_cells=65535;
    fill_limits.max_evaluations=2000000;fill_limits.timeout=std::chrono::seconds(20);
    const auto &stack=*native.snapshot->hatches->source;
    MaterialIntegralLimits target_limits;target_limits.maximum_interval_width=Volume(.001);target_limits.max_cells=65535;
    target_limits.timeout=std::chrono::seconds(5);
    const auto first=integrate_material_first_pass({stack.source},stack.surfaces.front().cell,stack.support_plane_z_mm,stack.policy.first_gap,target_limits);
    INFO(first.reason);REQUIRE(first.proof);
    INFO("target cells=" << first.cells << " target interval=[" << first.nominal_volume_mm3->lower << ',' << first.nominal_volume_mm3->upper << ']');
    const auto fill_started=std::chrono::steady_clock::now();
    const auto fit=reconcile_material_fill(first,result,fill_limits);
    INFO("native fill seconds=" << std::chrono::duration<double>(std::chrono::steady_clock::now()-fill_started).count());
    const auto below=fit.provisional_below_roof_mm3.value_or(ScalarBounds{0,0}),above=fit.provisional_above_surface_mm3.value_or(ScalarBounds{0,0});
    INFO("provisional below=[" << below.lower << ',' << below.upper << "] above=[" << above.lower << ',' << above.upper << ']');
    INFO(fit.reason << " fill cells=" << fit.cells << " work=" << fit.evaluations);REQUIRE(fit.snapshot);
    const auto &f=*fit.snapshot;
    INFO("target=[" << f.target_volume_mm3.lower << ',' << f.target_volume_mm3.upper << "] covered=[" << f.covered_target_mm3.lower << ',' <<
        f.covered_target_mm3.upper << "] missing=[" << f.missing_target_mm3.lower << ',' << f.missing_target_mm3.upper << "] outside=[" <<
        f.outside_target_mm3.lower << ',' << f.outside_target_mm3.upper << "] below=[" << f.below_roof_mm3.lower << ',' << f.below_roof_mm3.upper <<
        "] above=[" << f.above_surface_mm3.lower << ',' << f.above_surface_mm3.upper << ']');
    REQUIRE(f.target==first.proof);REQUIRE(f.occupied==result.snapshot);
    REQUIRE(f.missing_target_mm3.lower>0);REQUIRE(f.covered_target_mm3.upper<f.target_volume_mm3.lower);
    for (auto volume : {f.covered_target_mm3,f.missing_target_mm3,f.outside_target_mm3,f.below_roof_mm3,f.above_surface_mm3}) {
        REQUIRE(volume.lower>=0);REQUIRE(volume.upper-volume.lower<=fill_limits.maximum_interval_width.value());
    }
    MaterialDeficitLimits locate;locate.maximum_interval_width=Volume(.02);locate.max_cells=65535;
    locate.timeout=std::chrono::seconds(5);
    const auto map=locate_material_deficit(fit,{19,20,21},{17,20,23},locate);
    INFO(map.reason);REQUIRE(map.snapshot);REQUIRE(map.snapshot->cells.size()==4);REQUIRE(map.snapshot->source==fit.snapshot);
    INFO("localized missing >= " << map.snapshot->localized_missing_lower_mm3 << " regions=" << map.snapshot->cells.size() <<
        " fragments=" << map.snapshot->proof_cells << " work=" << map.snapshot->evaluations);
    for (const auto &cell : map.snapshot->cells) {
        INFO("cell " << cell.footprint.min_x << ',' << cell.footprint.min_y << " target=[" << cell.target_volume_mm3.lower << ',' <<
            cell.target_volume_mm3.upper << "] possible amount <= " << cell.candidate_amount_upper_mm3 << " missing >= " << cell.missing_lower_mm3);
        REQUIRE(cell.missing_lower_mm3>0);
    }
    REQUIRE(map.snapshot->localized_missing_lower_mm3>.5);
    REQUIRE(map.snapshot->localized_missing_lower_mm3<=fit.snapshot->missing_target_mm3.upper);
    REQUIRE(map.snapshot->target_volume_mm3.upper-map.snapshot->target_volume_mm3.lower<=.02);

}
