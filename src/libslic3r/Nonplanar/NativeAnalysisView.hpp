#pragma once
#include "NativeAnalysisWorker.hpp"
#include "../PrintConfig.hpp"

namespace Slic3r::nptop {
struct NativeAnalysisViewInput {
    const std::shared_ptr<const NativeAnalysisRequestSnapshot> request;
    const std::string mode, editing_bytes, source_sha256, identity_sha256;
private:
    NativeAnalysisViewInput(std::shared_ptr<const NativeAnalysisRequestSnapshot> r, std::string m,
        std::string bytes, std::string source, std::string identity)
        : request(std::move(r)), mode(std::move(m)), editing_bytes(std::move(bytes)),
          source_sha256(std::move(source)), identity_sha256(std::move(identity)) {}
    friend std::shared_ptr<const NativeAnalysisViewInput> capture_native_analysis_view_input(
        const Model &, const DynamicPrintConfig &, int, const Vec3d &, const std::string &, const std::string &);
};
// Serialized GUI capture. The explicit analysis mode is applied to a copy;
// the original mode, all original settings, placement and exact editing bytes
// remain identity inputs. This is a display session, not a saved project format.
std::shared_ptr<const NativeAnalysisViewInput> capture_native_analysis_view_input(
    const Model &, const DynamicPrintConfig &, int plate, const Vec3d &origin,
    const std::string &mode, const std::string &editing_bytes);
// Main-thread completion only. Both the recaptured live GUI identity and the
// private native owner must still match. Stops the host attempt as Unknown or
// Cancelled; a diagnostic can never advance it or authorize export.
NativeAnalysisWorkerResult finish_native_analysis_view(Print &, const GuardedJobTask &,
    const NativeAnalysisViewInput &, const NativeAnalysisViewInput *current,
    uint64_t revision, uint64_t current_revision, bool cancelled, NativeAnalysisWorkerResult);
}
