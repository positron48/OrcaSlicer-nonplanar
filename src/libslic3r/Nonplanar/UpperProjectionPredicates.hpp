#pragma once
#include "../Point.hpp"
#include <array>
#include <vector>
namespace Slic3r::nptop::detail {
using ProjectionTriangle = std::array<Vec3f,3>;
// Exact signs for the XY projection of finite nominal mesh coordinates.
int projection_orientation(const ProjectionTriangle &);
bool coplanar_faces(const ProjectionTriangle &, const ProjectionTriangle &);
// Precondition: both projections are counterclockwise, with positive area.
bool projection_interiors_overlap(const ProjectionTriangle &, const ProjectionTriangle &);
// Adjacent boundary edges may meet only at their common endpoint.
bool projected_boundary_conflict(const Vec3f &, const Vec3f &, const Vec3f &, const Vec3f &, bool adjacent);
// -1 outside, 0 on boundary, +1 inside a simple loop, independent of winding.
int projected_bounded_side(const std::vector<Vec3f> &, const Vec2d &);
// Exact rational squared-distance comparison for the entire XY segment.
bool projected_clearance_exceeds(const Vec2d &, const Vec2d &, const Vec3f &, const Vec3f &, double radius);
}
