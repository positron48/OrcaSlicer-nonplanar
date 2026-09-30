#pragma once
#include "VolumePartition.hpp"

namespace Slic3r::nptop::detail {
struct ExactPartition {
    TriangleMesh body, cap;
    ScalarBounds original_volume, body_volume, cap_volume;
    double coordinate_error_upper_mm=0;
    size_t shared_interface_triangles=0;
};
// Internal native CGAL dependency boundary; caller already audited inputs.
ExactPartition exact_partition(const TriangleMesh &, const TriangleMesh &, const MeshAuditLimits &,
                               const std::function<void()> &stop);
}
