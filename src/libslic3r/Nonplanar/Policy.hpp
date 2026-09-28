#pragma once
#include "../Config.hpp"
#include <map>
#include <string>
#include <vector>
namespace Slic3r { class Model; }
namespace Slic3r::nptop {
bool requests_guarded_mode(const ConfigBase &);
// Conservatively includes overrides before resolution, for cached publication.
bool requests_guarded_mode(const Model &, const ConfigBase &);
enum class Mode { Off, SafeHybrid, StrictNonplanar, Invalid };
struct PolicyConflict { std::string key, value, reason; };
struct PolicySnapshot {
    const Mode mode;
    const std::map<std::string,std::string> resolved;
    const std::vector<PolicyConflict> conflicts;
    bool passes_config_preflight() const { return mode != Mode::Off && mode != Mode::Invalid && conflicts.empty(); }
};
// Configuration preflight only: no profile qualification or export approval.
PolicySnapshot resolve_policy(const ConfigBase &, size_t objects, size_t instances);
}
