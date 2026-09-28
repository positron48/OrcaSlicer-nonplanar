#pragma once
#include "MeshAudit.hpp"
#include <optional>
#include <string_view>

namespace Slic3r::nptop {
struct StlSourceSnapshot {
    const std::string bytes;
    const std::string sha256;
    const bool millimeters_declared;
};
struct StlImportResult {
    std::shared_ptr<const StlSourceSnapshot> source;
    std::shared_ptr<const TriangleMesh> parsed;
    MeshAuditResult geometry;
    stl_stats native_repair;
    bool native_geometry_unchanged = false;
    // Conversion from exact source decimals / binary32 to parsed model-local mm.
    // Empty means unknown; zero is reserved for an established exact conversion.
    std::optional<double> source_error_upper_mm;
};
// Owns a bounded copy before hashing, callbacks or parsing. Uses the native STL
// reader and repair path; reports a source-coordinate conversion bound. Source
// file capture, transforms, hard worker limits and full job/import qualification
// are separate contracts, not established here.
StlImportResult import_stl_snapshot(std::string_view bytes, bool millimeters_declared,
                                    const MeshAuditLimits &limits = {});
}
