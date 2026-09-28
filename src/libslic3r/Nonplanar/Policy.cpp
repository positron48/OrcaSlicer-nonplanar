#include "Policy.hpp"
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
        {"seam_slope_type", {coEnum, int(SeamScarfType::None), "none"}}
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
        {"elefant_foot_compensation", {coFloat, 0}}
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
    bool empty = false;
    if (option && !option->is_nil() && option->type() == rule->second.type) {
        if (const auto *text = dynamic_cast<const ConfigOptionString *>(option))
            empty = text->value.empty();
        else if (const auto *list = dynamic_cast<const ConfigOptionStrings *>(option))
            empty = rule->second.empty_list ? list->values.empty() :
                std::all_of(list->values.begin(), list->values.end(), [](const std::string &s) { return s.empty(); });
    }
    if (!empty)
        return PolicyConflict{key, option && !option->is_nil() ? option->serialize() : "<missing>",
                              "Custom code is unqualified; requires an empty native field"};
    return {};
}
}

std::optional<PolicyConflict> model_policy_conflict(const Model &model)
{
    for (const auto &[plate, custom] : model.plates_custom_gcodes)
        if (!custom.gcodes.empty())
            return PolicyConflict{"plates_custom_gcodes[" + std::to_string(plate) + "]",
                                  std::to_string(custom.gcodes.size()), "Custom plate actions are unqualified"};
    const auto inspect = [](const ConfigBase &source) -> std::optional<PolicyConflict> {
        for (const auto &key : source.keys()) {
            if (auto conflict = discrete_conflict(key, source.option(key))) return conflict;
            if (auto conflict = custom_code_conflict(key, source.option(key))) return conflict;
            if (auto conflict = neutral_transform_conflict(key, source.option(key))) return conflict;
        }
        return {};
    };
    for (const auto &material : model.materials)
        if (auto conflict = inspect(material.second->config.get())) return conflict;
    for (size_t index = 0; index < model.objects.size(); ++index) {
        const auto *object = model.objects[index];
        if (auto conflict = inspect(object->config.get())) return conflict;
        for (const auto *volume : object->volumes)
            if (auto conflict = inspect(volume->config.get())) return conflict;
        for (const auto &range : object->layer_config_ranges)
            if (auto conflict = inspect(range.second.get())) return conflict;
        // These source controls live outside the resolved PrintRegionConfig.
        // Do not let native profile fallback/normalization qualify an input
        // whose custom layer generation has not been audited for this mode.
        const auto prefix = "objects[" + std::to_string(index) + "].";
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
    for (const auto &entry : custom_code_policy()) {
        read(entry.first);
        if (auto conflict = custom_code_conflict(entry.first, config.option(entry.first)))
            conflicts.push_back(std::move(*conflict));
    }
    // This detects new textual code hooks in a resolved config. It is not a
    // complete audit of arbitrary new geometry-affecting settings, nor of keys
    // discarded earlier by an importer's forward-compatibility substitution.
    for (const auto &key : native_config->keys()) {
        if (custom_code_policy().count(key) == 0)
            if (auto conflict = custom_code_conflict(key, config.option(key))) {
                read(key);
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
