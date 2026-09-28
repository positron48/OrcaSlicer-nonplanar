#pragma once
#include "StlImport.hpp"

namespace Slic3r::nptop {
inline constexpr unsigned stl_worker_protocol=1;
struct StlWorkerOptions {
    std::chrono::milliseconds timeout{3000};
    uint64_t max_peak_rss_bytes=256*1024*1024;
    std::function<bool()> cancelled;
    std::function<bool(uint64_t)> is_current;
};
struct StlWorkerResult {
    std::shared_ptr<const StlSourceSnapshot> source;
    uint64_t revision=0, peak_rss_bytes=0;
    MeshAuditStatus status=MeshAuditStatus::Unknown;
    std::string reason;
    size_t faces=0;
    double volume_lower_mm3=0, volume_upper_mm3=0;
    std::optional<double> source_error_upper_mm;
};
// Blocking background-job API; not a GUI-thread callback or export approval.
// executable must be the trusted bundled native worker, never a profile/model
// field or a shell command. Its geometry domain is fixed by protocol version 1.
// RSS is an observed peak acceptance limit, not instantaneous OS containment.
StlWorkerResult run_stl_worker(const std::string &executable, std::string_view bytes,
                               bool millimeters_declared, uint64_t revision,
                               const StlWorkerOptions &options = {});
}
