#pragma once
#include "../Config.hpp"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace Slic3r { class Model; class Print; }
namespace Slic3r::nptop {
bool requests_guarded_mode(const ConfigBase &);
// Conservatively includes overrides before resolution, for cached publication.
bool requests_guarded_mode(const Model &, const ConfigBase &);
enum class Mode { Off, SafeHybrid, StrictNonplanar, Invalid };
struct DiscreteRule { ConfigOptionType type; int required; const char *label; };
const std::map<std::string, DiscreteRule> &discrete_policy();
// Explicit subset of the compatibility registry: no imported code is an
// approved prologue/epilogue. Post-process lists must contain no entries.
struct CustomCodeRule { ConfigOptionType type; bool empty_list = false; };
const std::map<std::string, CustomCodeRule> &custom_code_policy();
struct NeutralTransformRule { ConfigOptionType type; double neutral; };
const std::map<std::string, NeutralTransformRule> &neutral_transform_policy();
// One physical nozzle and one filament, identity offset and fixed maps.
// Scalar selections may inherit (0) or select the sole filament (1).
const std::map<std::string, ConfigOptionType> &single_tool_policy();
struct PolicyConflict { std::string key, value, reason; };
// Covers sparse source overrides and plate actions that are not necessarily
// present in the resolved native PrintRegionConfig.
std::optional<PolicyConflict> model_policy_conflict(const Model &);
// Capture a guarded input conflict before native normalization/clamping can
// erase it. OFF returns no conflict and does not inspect the policy registries.
std::optional<PolicyConflict> input_policy_conflict(const Model &, const ConfigBase &);
// An owned copy of all present native options, including unknown keys. The
// caller must keep the source stable while capturing it. Its canonical identity
// covers present options, not a whole job. No mutable DynamicConfig escapes.
class ResolvedConfigSnapshot final : public ConfigOptionResolver {
public:
    explicit ResolvedConfigSnapshot(const ConfigBase &);
    const ConfigOption *optptr(const t_config_option_key &) const override;
    t_config_option_keys keys() const;
    // Versioned exact byte identity, bounded to 4 MiB; not compatibility approval.
    std::string canonical_json() const;
    std::string fingerprint() const;
private:
    struct Storage;
    std::shared_ptr<const Storage> m_storage;
};
struct PolicySnapshot {
    const Mode mode;
    const std::map<std::string,std::string> resolved;
    const std::vector<PolicyConflict> conflicts;
    // OFF keeps the upstream path and does not capture settings.
    const std::optional<ResolvedConfigSnapshot> native_config;
    bool passes_config_preflight() const { return mode != Mode::Off && mode != Mode::Invalid && conflicts.empty(); }
};
// Configuration preflight only: no profile qualification or export approval.
PolicySnapshot resolve_policy(const ConfigBase &, size_t objects, size_t instances);

struct PrintRegionConfigSnapshot {
    const size_t object_index;
    const int region_id;
    const ResolvedConfigSnapshot config;
    const PolicySnapshot policy;
};
// Actual Print settings bound to the pre-apply source token when available.
// No tool/scene qualification or export permission is implied. Caller must
// to own/synchronize Print for the entire synchronous operation.
struct PrintConfigSnapshot {
    const int plate_index;
    const Vec3d plate_origin_mm;
    const size_t object_count, instance_count;
    const ResolvedConfigSnapshot full_config;
    const ResolvedConfigSnapshot resolved_print_config;
    const std::vector<PrintRegionConfigSnapshot> regions;
    const std::string input_conflict;
    const std::optional<PolicyConflict> model_conflict;
    const uint64_t input_revision = 0;
    const std::string input_fingerprint;
    std::string block_reason() const;
    std::string canonical_json() const;
    std::string fingerprint() const;
};
// OFF returns null without capturing native options or plate state.
std::shared_ptr<const PrintConfigSnapshot> capture_print_config(const Print &);
}
