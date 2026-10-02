#pragma once
#include "Job.hpp"
#include "PlanarBody.hpp"
#include "GCodeAdapter.hpp"

namespace Slic3r::nptop {
struct GuardedNativeLimits {
    std::chrono::milliseconds timeout{30000};
    std::function<bool()> cancelled;
};
struct GuardedNativeBodyLimits : GuardedNativeLimits {
    MeshAuditLimits import;
    MeshPlacementLimits placement;
    VolumePartitionLimits partition;
    PlanarBodyLimits body;
    MaterialLimits material;
};
struct GuardedNativeBodyRequest {
    bool millimeters_declared;
    TriangleMesh reservation; // Plate coordinates, copied before callbacks.
    BodyMaterialParameters material;
};
struct GuardedNativeBodyResult;
struct GuardedNativeBodySnapshot {
    const std::shared_ptr<const GuardedJobSnapshot> job;
    const uint64_t attempt;
    const std::shared_ptr<const NativeInputSnapshot> slicing_input;
    const std::shared_ptr<const BodyMaterialSnapshot> body;
    const std::string request_json,canonical_json,sha256;
private:
    GuardedNativeBodySnapshot(std::shared_ptr<const GuardedJobSnapshot> owner,uint64_t generation,
        std::shared_ptr<const NativeInputSnapshot> input,std::shared_ptr<const BodyMaterialSnapshot> material,
        std::string request,std::string json,std::string hash)
        :job(std::move(owner)),attempt(generation),slicing_input(std::move(input)),body(std::move(material)),
        request_json(std::move(request)),canonical_json(std::move(json)),sha256(std::move(hash)){}
    friend GuardedNativeBodyResult analyze_guarded_native_body(const GuardedJobTask &,const GuardedNativeBodyRequest &,const GuardedNativeBodyLimits &);
};
struct GuardedNativeBodyResult {std::string reason;std::shared_ptr<const GuardedNativeBodySnapshot> snapshot;};
// Isolated worker (or serialized native test): executes actual import,
// pre-apply placement, partition, native body and material reconstruction from
// this Analyzing job. No live Print/Model/file access, GUI membership proof,
// measured resources, motion order or export qualification. Cooperative limits
// do not replace process isolation for untrusted interactive native/CGAL work.
GuardedNativeBodyResult analyze_guarded_native_body(const GuardedJobTask &,const GuardedNativeBodyRequest &,
    const GuardedNativeBodyLimits &limits={});

struct GuardedNativeHatchLimits : GuardedNativeLimits {NativeAffinePassLimits passes;AffineHatchLimits hatches;};
struct GuardedNativeHatchResult;
struct GuardedNativeHatchSnapshot {
    const std::shared_ptr<const GuardedNativeBodySnapshot> body;
    const std::shared_ptr<const NativeAffineHatchSnapshot> native;
    const std::string request_json,canonical_json,sha256;
private:
    GuardedNativeHatchSnapshot(std::shared_ptr<const GuardedNativeBodySnapshot> parent,std::shared_ptr<const NativeAffineHatchSnapshot> paths,
        std::string request,std::string json,std::string hash)
        :body(std::move(parent)),native(std::move(paths)),request_json(std::move(request)),canonical_json(std::move(json)),sha256(std::move(hash)){}
    friend GuardedNativeHatchResult plan_guarded_native_hatches(const GuardedJobTask &,std::shared_ptr<const GuardedNativeBodySnapshot>,
        const NativeAffinePassRequest &,const AffineHatchPolicy &,const GuardedNativeHatchLimits &);
};
struct GuardedNativeHatchResult {std::string reason;std::shared_ptr<const GuardedNativeHatchSnapshot> snapshot;};
// Planning token must be the exact current job/attempt that produced the body.
// Prospective affine candidates retain all existing geometry/volume checks.
GuardedNativeHatchResult plan_guarded_native_hatches(const GuardedJobTask &,std::shared_ptr<const GuardedNativeBodySnapshot>,
    const NativeAffinePassRequest &,const AffineHatchPolicy &,const GuardedNativeHatchLimits &limits={});

struct GuardedNativePlanResult;
struct GuardedNativePlanSnapshot {
    const std::shared_ptr<const GuardedNativeHatchSnapshot> hatches;
    const std::shared_ptr<const FirstCapMaterialSnapshot> assembly;
    const std::shared_ptr<const LinearCandidateSnapshot> candidate;
    const std::shared_ptr<const SimulationCapDepartureSnapshot> departure;
    const std::string departure_json,departure_sha256;
    const std::string canonical_json,sha256;
private:
    GuardedNativePlanSnapshot(std::shared_ptr<const GuardedNativeHatchSnapshot> paths,std::shared_ptr<const FirstCapMaterialSnapshot> material,
        std::shared_ptr<const LinearCandidateSnapshot> bytes,std::string json,std::string hash,
        std::shared_ptr<const SimulationCapDepartureSnapshot> exit={},std::string exit_json={},std::string exit_hash={})
        :hatches(std::move(paths)),assembly(std::move(material)),candidate(std::move(bytes)),departure(std::move(exit)),
          departure_json(std::move(exit_json)),departure_sha256(std::move(exit_hash)),canonical_json(std::move(json)),sha256(std::move(hash)){}
    friend GuardedNativePlanResult capture_guarded_native_plan(const GuardedJobTask &,std::shared_ptr<const GuardedNativeHatchSnapshot>,
        const FirstCapMaterialResult &,const LinearCandidateResult &,const GuardedJobLimits &,std::shared_ptr<const SimulationCapDepartureSnapshot>);
};
struct GuardedNativePlanResult {std::string reason;std::shared_ptr<const GuardedNativePlanSnapshot> snapshot;};
// Serializing worker checks the actual protected cap parent, complete body and
// selected cap journal, and the motion planner's exact input/output. Only speed
// and acceleration reductions may differ. An optional protected departure must
// own this exact laid assembly and the motion planner's exact routed source.
// Unbound route edits still refuse. This is bounded dependency lineage, not full
// source/target/support/contact/whole-route or final-byte geometry qualification.
GuardedNativePlanResult capture_guarded_native_plan(const GuardedJobTask &,std::shared_ptr<const GuardedNativeHatchSnapshot>,
    const FirstCapMaterialResult &,const LinearCandidateResult &,const GuardedJobLimits &limits={},
    std::shared_ptr<const SimulationCapDepartureSnapshot> departure={});
}
