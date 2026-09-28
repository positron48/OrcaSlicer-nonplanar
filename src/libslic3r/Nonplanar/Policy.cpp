#include "Policy.hpp"
#include "../Model.hpp"

namespace Slic3r::nptop {
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
        {"ironing_type", "no ironing"}, {"seam_slope_type", "none"},
        {"post_process", ""}
    };
    for (const auto &requirement : requirements) {
        const auto actual = read(requirement.first);
        if (actual != requirement.second)
            conflicts.push_back({requirement.first, actual, std::string("Requires ") + requirement.second});
    }
    resolved.emplace("object_count", std::to_string(objects));
    resolved.emplace("instance_count", std::to_string(instances));
    if (objects != 1) conflicts.push_back({"object_count", std::to_string(objects), "Requires one object"});
    if (instances != 1) conflicts.push_back({"instance_count", std::to_string(instances), "Requires one instance"});
    return {mode, std::move(resolved), std::move(conflicts)};
}
}
