#pragma once
#include "Policy.hpp"
#include "../TriangleMesh.hpp"
#include "../TriangleSelector.hpp"
#include <array>

namespace Slic3r::nptop {
struct InputVolume {
    ResolvedConfigSnapshot config;
    indexed_triangle_set mesh;
    Transform3d transform, source_transform;
    Vec3d source_offset;
    std::string mesh_sha256, source_file, material_id;
    int type, source_object_index, source_volume_index;
    bool from_inches, from_meters, builtin;
    // Support, seam, material and fuzzy painting, in that order. Capturing is
    // identity/provenance only and does not qualify any annotation for planning.
    std::array<TriangleSelector::TriangleSplittingData,4> annotations;
};
struct InputInstance {
    Transform3d transform;
    bool printable, auto_drop;
    int print_volume_state, arrange_order;
};
struct InputLayerRange { double lower_z, upper_z; ResolvedConfigSnapshot config; };
struct InputObject {
    ResolvedConfigSnapshot config;
    std::vector<InputVolume> volumes;
    std::vector<InputInstance> instances;
    std::vector<InputLayerRange> layer_ranges;
    std::vector<double> layer_height_profile;
    Vec3d origin_translation;
    bool printable;
};
struct InputMaterial {
    std::string id;
    ResolvedConfigSnapshot config;
    std::map<std::string,std::string> attributes;
};
// Owned source state before Print::apply normalization and approximate diffing.
// No native Model pointer (including mutable pointers inside const Model) escapes.
// This is not the whole job: file bytes, import error, plate placement, qualified
// tool/scene and verification products must still be bound by the job owner.
struct NativeInputSnapshot {
    const ResolvedConfigSnapshot config;
    const std::vector<InputMaterial> materials;
    const std::vector<InputObject> objects;
    const int model_plate_index;
    const std::string canonical_json, fingerprint;
};
struct NativeInputBinding {
    uint64_t revision;
    std::shared_ptr<const NativeInputSnapshot> snapshot;
};
// Exact binary32 geometry, signed indices and face properties; no mesh audit.
std::string native_mesh_fingerprint(const indexed_triangle_set &);
// OFF bypasses all work. Guarded graph/geometry/metadata limits are fixed;
// unsupported out-of-band object controls fail rather than silently disappear.
// Caller keeps Model and config stable during this synchronous capture.
std::shared_ptr<const NativeInputSnapshot> capture_native_input(const Model &, const ConfigBase &);
}
