#include "UpperProjectionPredicates.hpp"
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Intersections_2/Segment_2_Segment_2.h>
#include <CGAL/Distance_2/Segment_2_Segment_2.h>
#include <CGAL/Polygon_2_algorithms.h>

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
bool coplanar_faces(const ProjectionTriangle &a, const ProjectionTriangle &b)
{
    const auto point=[](const Vec3f &v) { return Kernel::Point_3(v.x(),v.y(),v.z()); };
    const auto p=point(a[0]), q=point(a[1]), r=point(a[2]);
    for (const auto &v : b) if (!CGAL::coplanar(p,q,r,point(v))) return false;
    return true;
}
bool projection_interiors_overlap(const ProjectionTriangle &a, const ProjectionTriangle &b)
{
    const auto pa=project(a), pb=project(b);
    return !has_separating_edge(pa,pb) && !has_separating_edge(pb,pa);
}
bool projected_boundary_conflict(const Vec3f &a, const Vec3f &b, const Vec3f &c, const Vec3f &d, bool adjacent)
{
    const Kernel::Point_2 pa(a.x(),a.y()), pb(b.x(),b.y()), pc(c.x(),c.y()), pd(d.x(),d.y());
    const Kernel::Segment_2 first(pa,pb), second(pc,pd);
    if (!CGAL::do_intersect(first,second)) return false;
    if (!adjacent) return true;
    // No intersection construction or rounded intersection point is needed.
    // For adjacent segments the only permitted intersection is their endpoint;
    // any further endpoint-on-segment contact implies a collinear overlap.
    if (pa==pc) return first.has_on(pd) || second.has_on(pb);
    if (pa==pd) return first.has_on(pc) || second.has_on(pb);
    if (pb==pc) return first.has_on(pd) || second.has_on(pa);
    if (pb==pd) return first.has_on(pc) || second.has_on(pa);
    return true;
}
int projected_bounded_side(const std::vector<Vec3f> &vertices, const Vec2d &point)
{
    std::vector<Kernel::Point_2> polygon;
    polygon.reserve(vertices.size());
    for (const auto &v : vertices) polygon.emplace_back(v.x(),v.y());
    return int(CGAL::bounded_side_2(polygon.begin(),polygon.end(),Kernel::Point_2(point.x(),point.y()),Kernel{}));
}
bool projected_clearance_exceeds(const Vec2d &a, const Vec2d &b, const Vec3f &c, const Vec3f &d, double radius)
{
    // EPICK's inexact squared-distance construction is insufficient at the
    // inset boundary. EPECK retains the exact supplied binary coordinates and
    // radius for both squaring and the complete segment-distance comparison.
    using Exact = CGAL::Exact_predicates_exact_constructions_kernel;
    const Exact::Segment_2 motion({a.x(),a.y()},{b.x(),b.y()});
    const Exact::Segment_2 edge({c.x(),c.y()},{d.x(),d.y()});
    const Exact::FT r(radius);
    return CGAL::squared_distance(motion,edge)>r*r;
}
std::optional<AffinePlaneBounds> projected_affine_bounds(const ProjectionTriangle &t, const Vec2d &start, const Vec2d &end)
{
    using Exact = CGAL::Exact_predicates_exact_constructions_kernel;
    const Exact::Triangle_2 xy({t[0].x(),t[0].y()},{t[1].x(),t[1].y()},{t[2].x(),t[2].y()});
    if (xy.bounded_side({start.x(),start.y()})==CGAL::ON_UNBOUNDED_SIDE) return {};
    const auto point=[](const Vec3f &v) { return Exact::Point_3(v.x(),v.y(),v.z()); };
    const Exact::Plane_3 plane(point(t[0]),point(t[1]),point(t[2]));
    if (plane.c()==0) throw std::runtime_error("vertical affine plane");
    const auto bound=[](const Exact::FT &value) {
        const auto range=CGAL::to_interval(value);
        if (!std::isfinite(range.first) || !std::isfinite(range.second) || range.first>range.second)
            throw std::runtime_error("unbounded affine plane");
        return std::array<double,2>{range.first,range.second};
    };
    const auto height=[&](const Vec2d &p) {
        return bound(-(plane.a()*Exact::FT(p.x())+plane.b()*Exact::FT(p.y())+plane.d())/plane.c());
    };
    return AffinePlaneBounds{height(start),height(end),bound(-plane.a()/plane.c()),bound(-plane.b()/plane.c())};
}
}
