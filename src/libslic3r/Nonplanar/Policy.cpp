#include "Policy.hpp"
#include "../Model.hpp"
#include <algorithm>

namespace Slic3r::nptop {
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

namespace {
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

std::optional<PolicyConflict> model_custom_code_conflict(const Model &model)
{
    for (const auto &[plate, custom] : model.plates_custom_gcodes)
        if (!custom.gcodes.empty())
            return PolicyConflict{"plates_custom_gcodes[" + std::to_string(plate) + "]",
                                  std::to_string(custom.gcodes.size()), "Custom plate actions are unqualified"};
    const auto inspect = [](const ConfigBase &source) -> std::optional<PolicyConflict> {
        for (const auto &key : source.keys())
            if (auto conflict = custom_code_conflict(key, source.option(key))) return conflict;
        return {};
    };
    for (const auto &material : model.materials)
        if (auto conflict = inspect(material.second->config.get())) return conflict;
    for (const auto *object : model.objects) {
        if (auto conflict = inspect(object->config.get())) return conflict;
        for (const auto *volume : object->volumes)
            if (auto conflict = inspect(volume->config.get())) return conflict;
        for (const auto &range : object->layer_config_ranges)
            if (auto conflict = inspect(range.second.get())) return conflict;
    }
    return {};
}

bool requests_guarded_mode(const ConfigBase &config)
{
    const auto *option = config.option("nptop_mode");
    return option && (option->is_nil() || option->serialize() != "off");
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

PolicySnapshot resolve_policy(const ConfigBase &config, size_t objects, size_t instances)
{
    std::map<std::string, std::string> resolved;
    std::vector<PolicyConflict> conflicts;
    auto read = [&](const std::string &key) {
        const auto *option = config.option(key);
        const std::string value = option && !option->is_nil() ? option->serialize() : "<missing>";
        resolved.emplace(key, value);
        return value;
    };
    const auto value = read("nptop_mode");
    // Absence is the legacy OFF default. Unknown explicit values fail closed.
    const Mode mode = value == "off" || config.option("nptop_mode") == nullptr ? Mode::Off :
        value == "safe_hybrid" ? Mode::SafeHybrid :
        value == "strict_nonplanar" ? Mode::StrictNonplanar : Mode::Invalid;
    if (mode == Mode::Off) return {mode, std::move(resolved), {}};
    if (mode == Mode::Invalid)
        conflicts.push_back({"nptop_mode", value, "Unknown nonplanar mode"});

    // Bounded configuration preflight. Scene/material/firmware qualification
    // and the remaining trajectory transforms must precede any export gate.
    const std::pair<const char *, const char *> requirements[] = {
        {"zaa_enabled", "0"}, {"gcode_flavor", "klipper"},
        {"spiral_mode", "0"}, {"enable_arc_fitting", "0"},
        {"enable_support", "0"}, {"raft_layers", "0"},
        {"wall_generator", "classic"}, {"fuzzy_skin", "disabled_fuzzy"},
        {"ironing_type", "no ironing"}, {"seam_slope_type", "none"}
    };
    for (const auto &requirement : requirements) {
        const auto actual = read(requirement.first);
        if (actual != requirement.second)
            conflicts.push_back({requirement.first, actual, std::string("Requires ") + requirement.second});
    }
    for (const auto &entry : custom_code_policy()) {
        read(entry.first);
        if (auto conflict = custom_code_conflict(entry.first, config.option(entry.first)))
            conflicts.push_back(std::move(*conflict));
    }
    // This detects new textual code hooks in a resolved config. It is not a
    // complete audit of arbitrary new geometry-affecting settings, nor of keys
    // discarded earlier by an importer's forward-compatibility substitution.
    for (const auto &key : config.keys()) {
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
    return {mode, std::move(resolved), std::move(conflicts)};
}
}
