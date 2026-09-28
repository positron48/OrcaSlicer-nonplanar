#include "Policy.hpp"
#include "StlImport.hpp"
#include "../Model.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace Slic3r::nptop {
struct ResolvedConfigSnapshot::Storage {
    explicit Storage(const ConfigBase &source) : values(source)
    {
        for (const auto &key : values.keys()) {
            // Native clone() owns option values but generic enums borrow their
            // dictionaries. Freeze those too before the source can disappear.
            const auto freeze_dictionary = [&](auto *option) {
                if (!option || !option->keys_map) return;
                option->keys_map = &enum_maps.emplace(key, *option->keys_map).first->second;
            };
            auto *option = values.option(key);
            auto *scalar = dynamic_cast<ConfigOptionEnumGeneric *>(option);
            if (scalar && !scalar->keys_map)
                throw ConfigurationError("Nonplanar config snapshot: missing scalar enum dictionary for " + key);
            freeze_dictionary(scalar);
            // Native vector enums permit a null map (e.g. extruder_type in
            // FullPrintConfig); preserve that state and its raw integer values.
            freeze_dictionary(dynamic_cast<ConfigOptionEnumsGeneric *>(option));
            freeze_dictionary(dynamic_cast<ConfigOptionEnumsGenericNullable *>(option));
        }
    }
    // Storage is constructed const and never shared through a mutable alias.
    std::map<std::string, t_config_enum_values> enum_maps;
    DynamicConfig values;
};

ResolvedConfigSnapshot::ResolvedConfigSnapshot(const ConfigBase &source)
    : m_storage(std::make_shared<const Storage>(source)) {}

const ConfigOption *ResolvedConfigSnapshot::optptr(const t_config_option_key &key) const
{
    return m_storage->values.option(key);
}

t_config_option_keys ResolvedConfigSnapshot::keys() const { return m_storage->values.keys(); }

namespace {
class CanonicalConfigWriter {
public:
    void append(std::string_view value) {
        if (value.size()>4*1024*1024-bytes.size()) throw std::length_error("Canonical config byte limit");
        bytes.append(value);
    }
    void value(bool v) { append(v ? "true" : "false"); }
    void value(int v) { append(std::to_string(v)); }
    void value(unsigned char v) { value(int(v)); }
    void value(double v) {
        static_assert(sizeof(double)==sizeof(uint64_t) && std::numeric_limits<double>::is_iec559);
        uint64_t bits; std::memcpy(&bits,&v,sizeof bits);
        char encoded[16];
        for (int i=0; i<16; ++i) encoded[i]=hex[(bits>>(60-4*i))&15];
        append("\""); append(std::string_view(encoded,sizeof encoded)); append("\"");
    }
    void value(const std::string &v) {
        append("\"");
        for (unsigned char c : v) { const char pair[]{hex[c>>4],hex[c&15]}; append(std::string_view(pair,2)); }
        append("\"");
    }
    void value(const Vec2d &v) { append("["); value(v.x()); append(","); value(v.y()); append("]"); }
    void value(const Vec3d &v) { append("["); value(v.x()); append(","); value(v.y()); append(","); value(v.z()); append("]"); }
    void value(const FloatOrPercent &v) { append("["); value(v.value); append(","); value(v.percent); append("]"); }
    template<class T, class Allocator> void value(const std::vector<T,Allocator> &values) {
        append("["); bool first=true;
        for (const auto &v : values) { if (!first) append(","); first=false; value(v); }
        append("]");
    }
    void dictionary(const t_config_enum_values *names) {
        if (!names) { append("null"); return; }
        append("["); bool first=true;
        for (const auto &[name,number] : *names) {
            if (!first) append(","); first=false;
            append("["); value(name); append(","); value(number); append("]");
        }
        append("]");
    }
    template<class T> const T &native(const ConfigOption &option) {
        const auto *typed=dynamic_cast<const T *>(&option);
        if (!typed) throw ConfigurationError("Unsupported native canonical config representation");
        return *typed;
    }
    void option(const ConfigOption &option) {
        switch (option.type()) {
        case coFloat: case coPercent: value(native<ConfigOptionSingle<double>>(option).value); break;
        case coFloats: case coPercents: value(native<ConfigOptionVector<double>>(option).values); break;
        case coInt: value(native<ConfigOptionInt>(option).value); break;
        case coInts: value(native<ConfigOptionVector<int>>(option).values); break;
        case coString: value(native<ConfigOptionString>(option).value); break;
        case coStrings: value(native<ConfigOptionStrings>(option).values); break;
        case coBool: value(native<ConfigOptionBool>(option).value); break;
        case coBools: value(native<ConfigOptionVector<unsigned char>>(option).values); break;
        case coFloatOrPercent: {
            const auto &v=native<ConfigOptionFloatOrPercent>(option);
            value(FloatOrPercent{v.value,v.percent}); break;
        }
        case coFloatsOrPercents: value(native<ConfigOptionVector<FloatOrPercent>>(option).values); break;
        case coPoint: value(native<ConfigOptionPoint>(option).value); break;
        case coPoints: value(native<ConfigOptionPoints>(option).values); break;
        case coPoint3: value(native<ConfigOptionPoint3>(option).value); break;
        case coPointsGroups: value(native<ConfigOptionPointsGroups>(option).values); break;
        case coIntsGroups: value(native<ConfigOptionIntsGroups>(option).values); break;
        case coEnum:
            if (const auto *v=dynamic_cast<const ConfigOptionEnumGeneric *>(&option)) {
                append("[\"generic\","); dictionary(v->keys_map); append(","); value(v->value); append("]");
            } else { append("[\"native\","); value(option.getInt()); append("]"); }
            break;
        case coEnums: {
            const auto *plain=dynamic_cast<const ConfigOptionEnumsGeneric *>(&option);
            const auto *nullable=dynamic_cast<const ConfigOptionEnumsGenericNullable *>(&option);
            if (!plain && !nullable) throw ConfigurationError("Unsupported native canonical enum vector");
            append("[\"generic\","); dictionary(plain ? plain->keys_map : nullable->keys_map); append(",");
            value(native<ConfigOptionVector<int>>(option).values); append("]"); break;
        }
        default: throw ConfigurationError("Unsupported native canonical config type");
        }
    }
    std::string take() { return std::move(bytes); }
private:
    static constexpr char hex[]="0123456789abcdef";
    std::string bytes;
};
}

std::string ResolvedConfigSnapshot::canonical_json() const
{
    auto ordered=keys();
    if (ordered.size()>4096) throw std::length_error("Canonical config option limit");
    std::sort(ordered.begin(),ordered.end());
    CanonicalConfigWriter writer;
    writer.append("{\"options\":["); bool first=true;
    for (const auto &key : ordered) {
        const auto *option=optptr(key);
        if (!option) throw ConfigurationError("Missing captured config option");
        if (!first) writer.append(","); first=false;
        writer.append("["); writer.value(key); writer.append(","); writer.value(int(option->type()));
        writer.append(","); writer.value(option->nullable()); writer.append(","); writer.option(*option); writer.append("]");
    }
    writer.append("],\"schema\":1}");
    return writer.take();
}
std::string ResolvedConfigSnapshot::fingerprint() const { return sha256_bytes(canonical_json()); }

const std::map<std::string, DiscreteRule> &discrete_policy()
{
    static const std::map<std::string, DiscreteRule> rules = {
        {"zaa_enabled", {coBool, 0, "0"}},
        {"gcode_flavor", {coEnum, gcfKlipper, "klipper"}},
        {"spiral_mode", {coBool, 0, "0"}},
        {"enable_arc_fitting", {coBool, 0, "0"}},
        {"enable_support", {coBool, 0, "0"}},
        {"raft_layers", {coInt, 0, "0"}},
        {"wall_generator", {coEnum, int(PerimeterGeneratorType::Classic), "classic"}},
        {"fuzzy_skin", {coEnum, int(FuzzySkinType::Disabled_fuzzy), "disabled_fuzzy"}},
        {"ironing_type", {coEnum, int(IroningType::NoIroning), "no ironing"}},
        {"seam_slope_type", {coEnum, int(SeamScarfType::None), "none"}},
        {"single_extruder_multi_material", {coBool, 0, "0"}},
        {"manual_filament_change", {coBool, 0, "0"}},
        {"enable_prime_tower", {coBool, 0, "0"}},
        {"enable_filament_dynamic_map", {coBool, 0, "0"}},
        {"has_filament_switcher", {coBool, 0, "0"}},
        {"filament_map_mode", {coEnum, int(fmmManual), "Manual"}},
        {"print_sequence", {coEnum, int(PrintSequence::ByLayer), "by layer"}},
        {"sparse_infill_pattern", {coEnum, int(ipRectilinear), "rectilinear"}},
        {"internal_solid_infill_pattern", {coEnum, int(ipRectilinear), "rectilinear"}},
        {"top_surface_pattern", {coEnum, int(ipRectilinear), "rectilinear"}},
        {"bottom_surface_pattern", {coEnum, int(ipRectilinear), "rectilinear"}},
        {"infill_combination", {coBool, 0, "0"}},
        {"detect_thin_wall", {coBool, 0, "0"}},
        {"gap_fill_target", {coEnum, int(gftNowhere), "nowhere"}}
    };
    return rules;
}

const std::map<std::string, CustomCodeRule> &custom_code_policy()
{
    static const std::map<std::string, CustomCodeRule> rules = {
        {"file_start_gcode", {coString}}, {"machine_start_gcode", {coString}},
        {"machine_end_gcode", {coString}}, {"before_layer_change_gcode", {coString}},
        {"layer_change_gcode", {coString}}, {"time_lapse_gcode", {coString}},
        {"wrapping_detection_gcode", {coString}}, {"printing_by_object_gcode", {coString}},
        {"machine_pause_gcode", {coString}}, {"template_custom_gcode", {coString}},
        {"change_filament_gcode", {coString}}, {"change_extrusion_role_gcode", {coString}},
        {"process_change_extrusion_role_gcode", {coString}},
        {"filament_start_gcode", {coStrings}}, {"filament_end_gcode", {coStrings}},
        {"filament_change_extrusion_role_gcode", {coStrings}}, {"post_process", {coStrings, true}}
    };
    return rules;
}

const std::map<std::string, NeutralTransformRule> &neutral_transform_policy()
{
    static const std::map<std::string, NeutralTransformRule> rules = {
        {"adaptive_pressure_advance", {coBools, 0}},
        {"adaptive_pressure_advance_overhangs", {coBools, 0}},
        {"adaptive_pressure_advance_bridges", {coFloats, 0}},
        {"filament_adaptive_volumetric_speed", {coBools, 0}},
        {"small_area_infill_flow_compensation", {coBool, 0}},
        {"max_volumetric_extrusion_rate_slope", {coFloat, 0}},
        {"filament_shrink", {coPercents, 100}},
        {"filament_shrinkage_compensation_z", {coPercents, 100}},
        {"xy_hole_compensation", {coFloat, 0}},
        {"xy_contour_compensation", {coFloat, 0}},
        {"elefant_foot_compensation", {coFloat, 0}},
        {"filament_flow_ratio", {coFloats, 1}},
        {"print_flow_ratio", {coFloat, 1}},
        {"bridge_flow", {coFloat, 1}},
        {"internal_bridge_flow", {coFloat, 1}},
        {"top_solid_infill_flow_ratio", {coFloat, 1}},
        {"bottom_solid_infill_flow_ratio", {coFloat, 1}},
        {"first_layer_flow_ratio", {coFloat, 1}},
        {"outer_wall_flow_ratio", {coFloat, 1}},
        {"inner_wall_flow_ratio", {coFloat, 1}},
        {"overhang_flow_ratio", {coFloat, 1}},
        {"sparse_infill_flow_ratio", {coFloat, 1}},
        {"internal_solid_infill_flow_ratio", {coFloat, 1}},
        {"gap_fill_flow_ratio", {coFloat, 1}},
        {"support_flow_ratio", {coFloat, 1}},
        {"support_interface_flow_ratio", {coFloat, 1}},
        {"brim_flow_ratio", {coFloat, 1}}
    };
    return rules;
}

const std::map<std::string, ConfigOptionType> &single_tool_policy()
{
    static const std::map<std::string, ConfigOptionType> rules = {
        {"nozzle_diameter", coFloats}, {"filament_diameter", coFloats},
        {"filament_map", coInts}, {"physical_extruder_map", coInts}, {"extruder_offset", coPoints},
        {"extruder", coInt}, {"sparse_infill_filament_id", coInt}, {"outer_wall_filament_id", coInt},
        {"inner_wall_filament_id", coInt}, {"internal_solid_filament_id", coInt},
        {"top_surface_filament_id", coInt}, {"bottom_surface_filament_id", coInt},
        {"support_filament", coInt}, {"support_interface_filament", coInt}
    };
    return rules;
}

namespace {
std::optional<PolicyConflict> discrete_conflict(const std::string &key, const ConfigOption *option)
{
    const auto rule = discrete_policy().find(key);
    if (rule == discrete_policy().end()) return {};
    std::optional<int> value;
    if (option && option->type() == rule->second.type) {
        if (const auto *flag = dynamic_cast<const ConfigOptionBool *>(option)) value = flag->value ? 1 : 0;
        else if (option->type() == coInt || option->type() == coEnum) value = option->getInt();
    }
    if (value && *value == rule->second.required) return {};
    // Do not serialize an unchecked enum: typed native serializers index their
    // name tables, and generic dictionaries may not match the numeric meaning.
    const auto actual = value ? std::to_string(*value) : option ?
        "<wrong-native-type:" + std::to_string(int(option->type())) + ">" : "<missing>";
    return PolicyConflict{key, actual, std::string("Requires native type and value ") + rule->second.label};
}

bool is_neutral(double value, double expected)
{
    static_assert(sizeof(double) == sizeof(uint64_t) && std::numeric_limits<double>::is_iec559);
    uint64_t value_bits, expected_bits;
    std::memcpy(&value_bits, &value, sizeof value_bits);
    std::memcpy(&expected_bits, &expected, sizeof expected_bits);
    constexpr uint64_t magnitude = 0x7fffffffffffffffULL;
    // Both signs of zero are neutral. Bit comparison also rejects subnormals
    // under a caller's flush-to-zero mode without doing floating arithmetic.
    return value_bits == expected_bits || ((value_bits & magnitude) == 0 && (expected_bits & magnitude) == 0);
}

std::optional<PolicyConflict> single_tool_conflict(const std::string &key, const ConfigOption *option)
{
    const auto rule = single_tool_policy().find(key);
    if (rule == single_tool_policy().end()) return {};
    std::ostringstream actual;
    actual.imbue(std::locale::classic());
    actual << std::setprecision(std::numeric_limits<double>::max_digits10);
    bool accepted = false;
    const char *requirement = "Requires the native single-tool setting";
    if (option && option->type() == rule->second && !option->nullable()) {
        if (const auto *selection = dynamic_cast<const ConfigOptionInt *>(option)) {
            actual << selection->value;
            accepted = selection->value == 0 || selection->value == 1;
            requirement = "Requires filament 1 or inherited selection 0";
        } else if (const auto *diameters = dynamic_cast<const ConfigOptionFloats *>(option)) {
            // Inspect IEEE bits so NaN and subnormal inputs cannot become
            // accepted through fast-math assumptions or caller FTZ modes.
            accepted = diameters->values.size() == 1;
            actual << '[';
            for (size_t i = 0; i < diameters->values.size(); ++i) {
                const double value = diameters->values[i];
                if (i) actual << ',';
                actual << value;
                uint64_t bits;
                std::memcpy(&bits, &value, sizeof bits);
                const auto exponent = (bits >> 52) & 0x7ff;
                accepted = accepted && (bits >> 63) == 0 && exponent > 0 && exponent < 0x7ff;
            }
            actual << ']';
            requirement = "Requires exactly one finite positive normal diameter; physical qualification is separate";
        } else if (const auto *mapping = dynamic_cast<const ConfigOptionInts *>(option)) {
            // Filament -> logical extruder is one-based; logical -> physical
            // is zero-based in native Orca. Neither map may be auto-rewritten.
            const int required = key == "filament_map" ? 1 : 0;
            accepted = mapping->values.size() == 1 && mapping->values.front() == required;
            actual << '[';
            for (size_t i = 0; i < mapping->values.size(); ++i) {
                if (i) actual << ',';
                actual << mapping->values[i];
            }
            actual << ']';
            requirement = key == "filament_map" ? "Requires exactly [1]" : "Requires exactly [0]";
        } else if (const auto *offsets = dynamic_cast<const ConfigOptionPoints *>(option)) {
            accepted = offsets->values.size() == 1;
            actual << '[';
            for (size_t i = 0; i < offsets->values.size(); ++i) {
                const auto &point = offsets->values[i];
                if (i) actual << ',';
                actual << '(' << point.x() << ',' << point.y() << ')';
                accepted = accepted && is_neutral(point.x(), 0.) && is_neutral(point.y(), 0.);
            }
            actual << ']';
            requirement = "Requires exactly one identity extruder offset (0,0)";
        }
    }
    if (accepted) return {};
    return PolicyConflict{key, actual.str().empty() ? "<missing-or-wrong-native-type>" : actual.str(), requirement};
}

std::optional<PolicyConflict> neutral_transform_conflict(const std::string &key, const ConfigOption *option)
{
    const auto rule = neutral_transform_policy().find(key);
    if (rule == neutral_transform_policy().end()) return {};
    bool neutral = false;
    std::ostringstream actual;
    actual.imbue(std::locale::classic());
    actual << std::setprecision(std::numeric_limits<double>::max_digits10);
    const auto inspect = [&](const auto &values) {
        neutral = !values.empty();
        actual << '[';
        bool first = true;
        for (const auto value : values) {
            if (!first) actual << ',';
            first = false;
            actual << static_cast<double>(value);
            // Raw native values, not rounded serialize() text. NaN, infinity
            // and nullable sentinels cannot equal these finite neutral values.
            neutral = neutral && is_neutral(static_cast<double>(value), rule->second.neutral);
        }
        actual << ']';
    };
    if (option && option->type() == rule->second.type) {
        if (const auto *scalar = dynamic_cast<const ConfigOptionFloat *>(option))
            inspect(std::array<double, 1>{scalar->value});
        else if (const auto *flag = dynamic_cast<const ConfigOptionBool *>(option))
            inspect(std::array<double, 1>{flag->value ? 1. : 0.});
        else if (const auto *values = dynamic_cast<const ConfigOptionVector<double> *>(option))
            inspect(values->values);
        else if (const auto *flags = dynamic_cast<const ConfigOptionVector<unsigned char> *>(option))
            inspect(flags->values);
    }
    if (!neutral)
        return PolicyConflict{key, actual.str().empty() ? "<missing-or-wrong-type>" : actual.str(),
                              "Unqualified transform requires native type and exact neutral value " +
                                  std::to_string(rule->second.neutral) + " in every entry"};
    return {};
}

std::optional<PolicyConflict> custom_code_conflict(const std::string &key, const ConfigOption *option)
{
    const auto rule = custom_code_policy().find(key);
    if (rule == custom_code_policy().end()) {
        if (key.find("gcode") != std::string::npos && option &&
            (option->type() == coString || option->type() == coStrings))
            return PolicyConflict{key, option->serialize(), "Unknown custom code field requires compatibility audit"};
        return {};
    }
    const auto reject=[&](std::string value) {
        return PolicyConflict{key,std::move(value),"Custom code is unqualified; requires an empty native field"};
    };
    if (!option) return reject("<missing>");
    // An unchecked enum serializer may index outside its name table. Validate
    // both native tag and representation before diagnostic formatting.
    if (option->type()!=rule->second.type)
        return reject("<wrong-native-type:"+std::to_string(int(option->type()))+">");
    const auto *text=dynamic_cast<const ConfigOptionString *>(option);
    const auto *list=dynamic_cast<const ConfigOptionStrings *>(option);
    if (option->nullable() || (rule->second.type==coString ? !text : !list))
        return reject("<unsupported-native-representation>");
    const bool empty=text ? text->value.empty() : rule->second.empty_list ? list->values.empty() :
        std::all_of(list->values.begin(),list->values.end(),[](const std::string &s) { return s.empty(); });
    if (!empty) return reject(option->serialize());
    return {};
}

std::optional<PolicyConflict> source_policy_conflict(const ConfigBase &source)
{
    for (const auto &key : source.keys()) {
        if (auto conflict = discrete_conflict(key, source.option(key))) return conflict;
        if (auto conflict = custom_code_conflict(key, source.option(key))) return conflict;
        if (auto conflict = neutral_transform_conflict(key, source.option(key))) return conflict;
        if (auto conflict = single_tool_conflict(key, source.option(key))) return conflict;
    }
    return {};
}
}

std::optional<PolicyConflict> model_policy_conflict(const Model &model)
{
    for (const auto &[plate, custom] : model.plates_custom_gcodes)
        if (!custom.gcodes.empty())
            return PolicyConflict{"plates_custom_gcodes[" + std::to_string(plate) + "]",
                                  std::to_string(custom.gcodes.size()), "Custom plate actions are unqualified"};
    for (const auto &material : model.materials)
        if (auto conflict = source_policy_conflict(material.second->config.get())) return conflict;
    for (size_t index = 0; index < model.objects.size(); ++index) {
        const auto *object = model.objects[index];
        if (auto conflict = source_policy_conflict(object->config.get())) return conflict;
        for (const auto *volume : object->volumes)
            if (auto conflict = source_policy_conflict(volume->config.get())) return conflict;
        for (const auto &range : object->layer_config_ranges)
            if (auto conflict = source_policy_conflict(range.second.get())) return conflict;
        // These source controls live outside the resolved PrintRegionConfig.
        // Do not let native profile fallback/normalization qualify an input
        // whose custom layer generation has not been audited for this mode.
        const auto prefix = "objects[" + std::to_string(index) + "].";
        if (object->is_mm_painted())
            return PolicyConflict{prefix + "mmu_segmentation_facets", "present",
                                  "Material painting is outside the single-material domain"};
        if (!object->layer_height_profile.empty())
            return PolicyConflict{prefix + "layer_height_profile", "present",
                                  "Custom layer-height profiles are outside the uniform-layer domain"};
        for (const auto &range : object->layer_config_ranges)
            if (range.second.has("layer_height"))
                return PolicyConflict{prefix + "layer_config_ranges", std::to_string(object->layer_config_ranges.size()),
                                      "Layer-height range overrides are outside the uniform-layer domain"};
    }
    return {};
}

bool requests_guarded_mode(const ConfigBase &config)
{
    const auto *option = config.option("nptop_mode");
    const auto *mode = dynamic_cast<const ConfigOptionString *>(option);
    return option && (!mode || option->type() != coString || mode->value != "off");
}

bool requests_guarded_mode(const Model &model, const ConfigBase &config)
{
    if (requests_guarded_mode(config)) return true;
    for (const auto &material : model.materials)
        if (requests_guarded_mode(material.second->config.get())) return true;
    for (const auto *object : model.objects) {
        if (requests_guarded_mode(object->config.get())) return true;
        for (const auto *volume : object->volumes)
            if (requests_guarded_mode(volume->config.get())) return true;
        for (const auto &range : object->layer_config_ranges)
            if (requests_guarded_mode(range.second.get())) return true;
    }
    return false;
}

std::optional<PolicyConflict> input_policy_conflict(const Model &model, const ConfigBase &config)
{
    if (!requests_guarded_mode(model, config)) return {};
    if (auto conflict = model_policy_conflict(model)) return conflict;
    return source_policy_conflict(config);
}

PolicySnapshot resolve_policy(const ConfigBase &source, size_t objects, size_t instances)
{
    std::optional<ResolvedConfigSnapshot> native_config;
    if (requests_guarded_mode(source)) native_config.emplace(source);
    const ConfigOptionResolver &config = native_config ? static_cast<const ConfigOptionResolver &>(*native_config) : source;
    std::map<std::string, std::string> resolved;
    std::vector<PolicyConflict> conflicts;
    auto read = [&](const std::string &key) {
        const auto *option = config.option(key);
        const std::string value = option && !option->is_nil() ? option->serialize() : "<missing>";
        resolved.emplace(key, value);
        return value;
    };
    const auto *mode_option = config.option("nptop_mode");
    const auto *mode_string = dynamic_cast<const ConfigOptionString *>(mode_option);
    const std::string value = mode_string && mode_option->type() == coString ? mode_string->value :
        mode_option ? "<wrong-native-type>" : "<missing>";
    resolved.emplace("nptop_mode", value);
    // Absence is the legacy OFF default. Unknown explicit values fail closed.
    const Mode mode = value == "off" || mode_option == nullptr ? Mode::Off :
        value == "safe_hybrid" ? Mode::SafeHybrid :
        value == "strict_nonplanar" ? Mode::StrictNonplanar : Mode::Invalid;
    if (mode == Mode::Off) return {mode, std::move(resolved), {}, std::nullopt};
    if (mode == Mode::Invalid)
        conflicts.push_back({"nptop_mode", value, "Unknown nonplanar mode"});

    // Bounded configuration preflight. Scene/material/firmware qualification
    // and the remaining trajectory transforms must precede any export gate.
    for (const auto &[key, rule] : discrete_policy()) {
        if (auto conflict = discrete_conflict(key, config.option(key))) {
            resolved.emplace(key, conflict->value);
            conflicts.push_back(std::move(*conflict));
        } else {
            resolved.emplace(key, rule.label);
        }
    }
    for (const auto &entry : neutral_transform_policy()) {
        if (auto conflict = neutral_transform_conflict(entry.first, config.option(entry.first))) {
            // Invalid NaN/native values may throw in Orca's serialize(). Keep
            // a structured rejection and the precise diagnostic value instead.
            resolved.emplace(entry.first, conflict->value);
            conflicts.push_back(std::move(*conflict));
        } else {
            read(entry.first);
        }
    }
    for (const auto &[key, type] : single_tool_policy()) {
        // Native normalize_fdm removes this optional source-only selector.
        // All resolved region selections and tool vectors remain required.
        if (key == "extruder" && !config.option(key)) continue;
        if (auto conflict = single_tool_conflict(key, config.option(key))) {
            resolved.emplace(key, conflict->value);
            conflicts.push_back(std::move(*conflict));
        } else {
            read(key);
        }
    }
    for (const auto &entry : custom_code_policy()) {
        if (auto conflict = custom_code_conflict(entry.first, config.option(entry.first))) {
            resolved.emplace(entry.first,conflict->value);
            conflicts.push_back(std::move(*conflict));
        } else {
            read(entry.first);
        }
    }
    // This detects new textual code hooks in a resolved config. It is not a
    // complete audit of arbitrary new geometry-affecting settings, nor of keys
    // discarded earlier by an importer's forward-compatibility substitution.
    for (const auto &key : native_config->keys()) {
        if (custom_code_policy().count(key) == 0)
            if (auto conflict = custom_code_conflict(key, config.option(key))) {
                resolved.emplace(key,conflict->value);
                conflicts.push_back(std::move(*conflict));
            }
    }
    resolved.emplace("object_count", std::to_string(objects));
    resolved.emplace("instance_count", std::to_string(instances));
    if (objects != 1) conflicts.push_back({"object_count", std::to_string(objects), "Requires one object"});
    if (instances != 1) conflicts.push_back({"instance_count", std::to_string(instances), "Requires one instance"});
    return {mode, std::move(resolved), std::move(conflicts), std::move(native_config)};
}
}
