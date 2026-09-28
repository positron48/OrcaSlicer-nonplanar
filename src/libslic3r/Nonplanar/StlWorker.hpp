#pragma once
#include "StlImport.hpp"

namespace Slic3r::nptop {
inline constexpr unsigned stl_worker_protocol=2;
inline constexpr double stl_worker_upper_slope_limit=0.2;
struct StlWorkerOptions {
    std::chrono::milliseconds timeout{3000};
    uint64_t max_peak_rss_bytes=256*1024*1024;
    std::function<bool()> cancelled;
    std::function<bool(uint64_t)> is_current;
    bool analyze_upper=false;
};
struct StlUpperSummary {
    size_t upward_faces=0, selected_faces=0, patches=0, holes=0, creases=0, affine_patches=0;
    double area_lower_mm2=0, area_upper_mm2=0, selected_area_lower_mm2=0, selected_area_upper_mm2=0;
    double minimum_z_mm=0, maximum_z_mm=0, selected_slope_upper=0;
};
struct StlWorkerResult {
    std::shared_ptr<const StlSourceSnapshot> source;
    uint64_t revision=0, peak_rss_bytes=0;
    MeshAuditStatus status=MeshAuditStatus::Unknown;
    std::string reason;
    size_t faces=0;
    double volume_lower_mm3=0, volume_upper_mm3=0;
    std::optional<double> source_error_upper_mm;
    std::string upper_reason;
    std::optional<StlUpperSummary> upper;
};
// Blocking background-job API; not a GUI-thread callback or export approval.
// executable must be the trusted bundled native worker, never a profile/model
// field or a shell command. Its geometry domain is fixed by protocol version 2.
// RSS is an observed peak acceptance limit, not instantaneous OS containment.
StlWorkerResult run_stl_worker(const std::string &executable, std::string_view bytes,
                               bool millimeters_declared, uint64_t revision,
                               const StlWorkerOptions &options = {});
}
