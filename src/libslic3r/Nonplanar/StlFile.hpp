#pragma once
#include "StlImport.hpp"

namespace Slic3r::nptop {
struct StlFileOptions {
    std::chrono::milliseconds timeout{1000};
    std::function<bool()> cancelled;
    std::function<bool(uint64_t)> is_current;
};
struct StlFileCapture {
    std::shared_ptr<const StlSourceSnapshot> source;
    std::string path, reason;
    uint64_t revision=0;
};
// Background file I/O, never a GUI-thread operation. Accept only an ordinary
// file with unchanged observed identity/size/timestamps across this bounded read.
// This detects concurrent changes; it is not an atomic filesystem snapshot or
// a hard deadline for a blocked kernel/network filesystem read. The returned
// immutable bytes, not the pathname, are the authority for subsequent analysis.
StlFileCapture capture_stl_file(const std::string &path, bool millimeters_declared,
                                uint64_t revision, const StlFileOptions &options = {});
}
