#pragma once
#include "../TriangleMesh.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace Slic3r::nptop {
enum class MeshAuditStatus { ValidGeometry, Invalid, Unknown };
struct MeshAuditLimits {
    size_t max_faces = 5000;
    size_t max_vertices = 15000;
    double max_coordinate_mm = 10000;
    std::chrono::milliseconds timeout{1000};
    std::function<bool()> cancelled;
    bool valid() const {
        return max_faces>0 && max_faces<=5000 && max_vertices>0 && max_vertices<=15000 &&
            std::isfinite(max_coordinate_mm) && max_coordinate_mm>0 && max_coordinate_mm<=10000 && timeout.count()>0;
    }
};
struct MeshAuditResult {
    MeshAuditStatus status = MeshAuditStatus::Unknown;
    std::string reason;
    size_t components = 0;
    size_t exact_duplicate_vertices_removed = 0;
    double volume_lower_mm3 = 0, volume_upper_mm3 = 0;
    // Owned derived geometry. No result aliases the caller's mutable mesh.
    std::shared_ptr<const TriangleMesh> normalized;
};
// Geometry-only audit of already parsed coordinates declared in millimeters.
// Does not establish source-byte provenance, import error, placement, support or
// export approval. Cancellation/deadline checks are cooperative; isolate CGAL in
// a worker before exposing untrusted imports through an interactive job pipeline.
MeshAuditResult audit_mesh(const TriangleMesh &source, bool millimeters_declared,
                           const MeshAuditLimits &limits = {});
// Exact comparison for bounded, finite, index-valid meshes already audited by
// the caller. Ignores face order and cyclic vertex rotation, never winding.
bool same_oriented_triangles(const indexed_triangle_set &, const indexed_triangle_set &);
}
