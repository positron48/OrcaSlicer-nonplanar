#include "MeshPlacement.hpp"
#include "Interval.hpp"

namespace Slic3r::nptop {
namespace {
using detail::Interval;
double matrix_norm_upper(const Transform3d &matrix)
{
    double norm_one=0, norm_infinity=0;
    for (int i=0; i<3; ++i) {
        Interval column(0), row(0);
        for (int j=0; j<3; ++j) {
            column=column+Interval(std::abs(matrix(j,i)));
            row=row+Interval(std::abs(matrix(i,j)));
        }
        norm_one=std::max(norm_one,column.hi); norm_infinity=std::max(norm_infinity,row.hi);
    }
    return detail::root(Interval(norm_one)*Interval(norm_infinity)).hi;
}
Interval determinant(const Transform3d &matrix)
{
    auto m=[&](int i,int j) { return Interval(matrix(i,j)); };
    return m(0,0)*(m(1,1)*m(2,2)-m(1,2)*m(2,1))-
           m(0,1)*(m(1,0)*m(2,2)-m(1,2)*m(2,0))+
           m(0,2)*(m(1,0)*m(2,1)-m(1,1)*m(2,0));
}
}
MeshPlacementResult place_imported_mesh(const StlImportResult &requested_source, const Transform3d &requested_matrix,
                                        uint64_t revision, const MeshPlacementLimits &requested_limits)
{
    const auto source=requested_source;
    const auto matrix=requested_matrix;
    const auto limits=requested_limits;
    MeshPlacementResult result;
    const auto started=std::chrono::steady_clock::now();
    auto stop=[&] {
        if (limits.geometry.cancelled && limits.geometry.cancelled()) { result.geometry.reason="CANCELLED"; return true; }
        if (limits.is_current && !limits.is_current(revision)) { result.geometry.reason="STALE_REVISION"; return true; }
        detail::require_interval_environment();
        if (std::chrono::steady_clock::now()-started>=limits.geometry.timeout) { result.geometry.reason="DEADLINE"; return true; }
        return false;
    };
    try {
        detail::require_interval_environment();
        if (revision==0 || !limits.geometry.valid() || !std::isfinite(limits.max_error_upper_mm) ||
            limits.max_error_upper_mm<0 || limits.max_error_upper_mm>0.05) {
            result.geometry.reason="INVALID_PLACEMENT_LIMITS"; return result;
        }
        if (!source.source || !source.source->millimeters_declared || !source.native_geometry_unchanged ||
            source.geometry.status!=MeshAuditStatus::ValidGeometry || !source.geometry.normalized ||
            !source.source_error_upper_mm || !std::isfinite(*source.source_error_upper_mm) || *source.source_error_upper_mm<0) {
            result.geometry.reason="MISSING_IMPORT_PROVENANCE"; return result;
        }
        const auto &input=*source.geometry.normalized;
        if (input.its.vertices.size()>limits.geometry.max_vertices || input.its.indices.size()>limits.geometry.max_faces) {
            result.geometry.reason="RESOURCE_LIMIT"; return result;
        }
        for (int i=0; i<4; ++i)
            for (int j=0; j<4; ++j)
                if (!std::isfinite(matrix(i,j)) || std::abs(matrix(i,j))>(j==3 ? 10000 : 100)) {
                    result.geometry.reason="PLACEMENT_MATRIX_DOMAIN"; return result;
                }
        if (matrix(3,0)!=0 || matrix(3,1)!=0 || matrix(3,2)!=0 || matrix(3,3)!=1 || determinant(matrix).lo<=0) {
            result.geometry.reason="UNSUPPORTED_OR_UNRESOLVED_AFFINE_MAP"; return result;
        }
        result.snapshot=std::make_shared<const MeshPlacementSnapshot>(MeshPlacementSnapshot{
            source.source,source.geometry.normalized,matrix,revision});
        if (stop()) return result;
        const double imported=*source.source_error_upper_mm==0 ? 0 :
            (Interval(*source.source_error_upper_mm)*Interval(matrix_norm_upper(matrix))).hi;
        if (imported>limits.max_error_upper_mm) { result.geometry.reason="PLACEMENT_ERROR_BUDGET"; return result; }
        TriangleMesh placed(input);
        placed.transform(matrix,false);
        double rounding=0;
        for (size_t vertex=0; vertex<input.its.vertices.size(); ++vertex) {
            if (stop()) return result;
            Interval distance(0);
            for (int axis=0; axis<3; ++axis) {
                Interval exact(matrix(axis,3));
                for (int j=0; j<3; ++j)
                    exact=exact+Interval(matrix(axis,j))*Interval(double(input.its.vertices[vertex](j)));
                const auto delta=exact-Interval(double(placed.its.vertices[vertex](axis)));
                distance=distance+Interval(std::max(std::abs(delta.lo),std::abs(delta.hi)));
            }
            rounding=std::max(rounding,distance.hi);
        }
        const double total=(Interval(imported)+Interval(rounding)).hi;
        if (total>limits.max_error_upper_mm) { result.geometry.reason="PLACEMENT_ERROR_BUDGET"; return result; }
        if (stop()) return result;
        auto remaining=limits.geometry;
        remaining.timeout-=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        if (remaining.timeout.count()<=0) { result.geometry.reason="DEADLINE"; return result; }
        auto geometry=audit_mesh(placed,true,remaining);
        if (stop()) return result;
        if (geometry.status!=MeshAuditStatus::ValidGeometry) { result.geometry=std::move(geometry); return result; }
        result.geometry=std::move(geometry);
        result.propagated_source_error_upper_mm=imported;
        result.native_transform_error_upper_mm=rounding;
        result.total_error_upper_mm=total;
    } catch (const std::exception &) {
        result.geometry={}; result.geometry.reason="PLACEMENT_EXCEPTION";
    }
    return result;
}
}
