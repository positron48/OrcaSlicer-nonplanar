#include "UpperProjectionPredicates.hpp"
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>

namespace Slic3r::nptop::detail {
namespace {
using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using Triangle = std::array<Kernel::Point_2,3>;
Triangle project(const ProjectionTriangle &t)
{
    return {Kernel::Point_2(t[0].x(),t[0].y()), Kernel::Point_2(t[1].x(),t[1].y()), Kernel::Point_2(t[2].x(),t[2].y())};
}
bool has_separating_edge(const Triangle &a, const Triangle &b)
{
    for (size_t i=0; i<3; ++i) {
        bool separated=true;
        for (const auto &point : b)
            if (CGAL::orientation(a[i],a[(i+1)%3],point)==CGAL::LEFT_TURN) { separated=false; break; }
        if (separated) return true;
    }
    return false;
}
}
int projection_orientation(const ProjectionTriangle &t)
{
    const auto p=project(t);
    return int(CGAL::orientation(p[0],p[1],p[2]));
}
bool projection_interiors_overlap(const ProjectionTriangle &a, const ProjectionTriangle &b)
{
    const auto pa=project(a), pb=project(b);
    return !has_separating_edge(pa,pb) && !has_separating_edge(pb,pa);
}
}
