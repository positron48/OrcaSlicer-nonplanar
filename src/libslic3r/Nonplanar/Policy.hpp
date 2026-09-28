#pragma once
#include "../Config.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>
namespace Slic3r { class Model; }
namespace Slic3r::nptop {
bool requests_guarded_mode(const ConfigBase &);
// Conservatively includes overrides before resolution, for cached publication.
bool requests_guarded_mode(const Model &, const ConfigBase &);
enum class Mode { Off, SafeHybrid, StrictNonplanar, Invalid };
// Explicit subset of the compatibility registry: no imported code is an
// approved prologue/epilogue. Post-process lists must contain no entries.
struct CustomCodeRule { ConfigOptionType type; bool empty_list = false; };
const std::map<std::string, CustomCodeRule> &custom_code_policy();
struct PolicyConflict { std::string key, value, reason; };
// Covers sparse source overrides and plate actions that are not necessarily
// present in the resolved native PrintRegionConfig.
std::optional<PolicyConflict> model_custom_code_conflict(const Model &);
struct PolicySnapshot {
    const Mode mode;
    const std::map<std::string,std::string> resolved;
    const std::vector<PolicyConflict> conflicts;
    bool passes_config_preflight() const { return mode != Mode::Off && mode != Mode::Invalid && conflicts.empty(); }
};
// Configuration preflight only: no profile qualification or export approval.
PolicySnapshot resolve_policy(const ConfigBase &, size_t objects, size_t instances);
}
