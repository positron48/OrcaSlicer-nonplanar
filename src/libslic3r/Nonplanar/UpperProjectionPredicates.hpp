#pragma once
#include "../Point.hpp"
#include <array>
namespace Slic3r::nptop::detail {
using ProjectionTriangle = std::array<Vec3f,3>;
// Exact signs for the XY projection of finite nominal mesh coordinates.
int projection_orientation(const ProjectionTriangle &);
// Precondition: both projections are counterclockwise, with positive area.
bool projection_interiors_overlap(const ProjectionTriangle &, const ProjectionTriangle &);
}
