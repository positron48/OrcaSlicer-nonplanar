#include "MeshPlacement.hpp"
#include "Interval.hpp"
#include "../Model.hpp"

namespace Slic3r::nptop {
CenteredVolumeResult capture_centered_volume(const StlImportResult &requested_source, const ModelVolume &volume,
                                             uint64_t revision, const MeshPlacementLimits &requested_limits)
{
    const auto source=requested_source;
    const auto limits=requested_limits;
    CenteredVolumeResult result;
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
            result.geometry.reason="INVALID_CENTERING_LIMITS"; return result;
        }
        if (!source.source || !source.source->millimeters_declared || !source.native_geometry_unchanged ||
            source.geometry.status!=MeshAuditStatus::ValidGeometry || !source.geometry.normalized ||
            !source.source_error_upper_mm || !std::isfinite(*source.source_error_upper_mm) || *source.source_error_upper_mm<0) {
            result.geometry.reason="MISSING_IMPORT_PROVENANCE"; return result;
        }
        if (!volume.is_model_part() || volume.source.is_converted_from_inches || volume.source.is_converted_from_meters ||
            volume.source.is_from_builtin_objects || volume.source.transform.get_matrix().matrix()!=Transform3d::Identity().matrix()) {
            result.geometry.reason="UNQUALIFIED_VOLUME_SOURCE"; return result;
        }
        const auto &input=*source.geometry.normalized;
        if (input.its.vertices.size()>limits.geometry.max_vertices || input.its.indices.size()>limits.geometry.max_faces ||
            volume.mesh().its.vertices.size()>limits.geometry.max_vertices || volume.mesh().its.indices.size()>limits.geometry.max_faces) {
            result.geometry.reason="RESOURCE_LIMIT"; return result;
        }
        const Vec3d offset=volume.source.mesh_offset;
        if (offset!=input.bounding_box().center()) { result.geometry.reason="SOURCE_CENTER_OFFSET_MISMATCH"; return result; }
        // This is the last read from the mutable native model. In particular,
        // mesh_ptr() alone would not protect against center_geometry_after_creation.
        auto owned=std::make_shared<const TriangleMesh>(volume.mesh());
        result.snapshot=std::make_shared<const CenteredVolumeSnapshot>(CenteredVolumeSnapshot{
            source.source,owned,offset,*source.source_error_upper_mm,revision});
        if (stop()) return result;
        auto remaining=limits.geometry;
        remaining.timeout-=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        if (remaining.timeout.count()<=0) { result.geometry.reason="DEADLINE"; return result; }
        auto geometry=audit_mesh(*owned,true,remaining);
        if (stop()) return result;
        if (geometry.status!=MeshAuditStatus::ValidGeometry) { result.geometry=std::move(geometry); return result; }
        TriangleMesh expected(input);
        if (!offset.isApprox(Vec3d::Zero()))
            expected.translate(-float(offset.x()),-float(offset.y()),-float(offset.z()));
        if (!same_oriented_triangles(expected.its,geometry.normalized->its)) {
            result.geometry.status=MeshAuditStatus::Invalid;
            result.geometry.reason="CENTERED_SOURCE_GEOMETRY_MISMATCH"; return result;
        }
        double rounding=0;
        for (size_t vertex=0; vertex<input.its.vertices.size(); ++vertex) {
            if (stop()) return result;
            detail::Interval distance(0);
            for (int axis=0; axis<3; ++axis) {
                const auto delta=detail::Interval(double(input.its.vertices[vertex](axis)))-detail::Interval(offset(axis))-
                                 detail::Interval(double(expected.its.vertices[vertex](axis)));
                distance=distance+detail::Interval(std::max(std::abs(delta.lo),std::abs(delta.hi)));
            }
            rounding=std::max(rounding,distance.hi);
        }
        const double total=(detail::Interval(*source.source_error_upper_mm)+detail::Interval(rounding)).hi;
        if (total>limits.max_error_upper_mm) { result.geometry.reason="CENTERING_ERROR_BUDGET"; return result; }
        if (stop()) return result;
        result.geometry=std::move(geometry);
        result.centering_error_upper_mm=rounding;
        result.total_error_upper_mm=total;
    } catch (const std::exception &) {
        result.geometry={}; result.geometry.reason="CENTERING_EXCEPTION";
    }
    return result;
}
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
MeshPlacementResult place_source_geometry(std::shared_ptr<const StlSourceSnapshot> source, MeshAuditResult input_geometry,
                                          std::optional<double> source_error, const Transform3d &requested_matrix,
                                          uint64_t revision, const MeshPlacementLimits &requested_limits)
{
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
        if (!source || !source->millimeters_declared ||
            input_geometry.status!=MeshAuditStatus::ValidGeometry || !input_geometry.normalized ||
            !source_error || !std::isfinite(*source_error) || *source_error<0) {
            result.geometry.reason="MISSING_IMPORT_PROVENANCE"; return result;
        }
        const auto &input=*input_geometry.normalized;
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
            source,input_geometry.normalized,matrix,revision});
        if (stop()) return result;
        const double imported=*source_error==0 ? 0 :
            (Interval(*source_error)*Interval(matrix_norm_upper(matrix))).hi;
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

MeshPlacementResult place_imported_mesh(const StlImportResult &source, const Transform3d &matrix,
                                        uint64_t revision, const MeshPlacementLimits &limits)
{
    if (!source.native_geometry_unchanged) {
        MeshPlacementResult result; result.geometry.reason="MISSING_IMPORT_PROVENANCE"; return result;
    }
    return place_source_geometry(source.source,source.geometry,source.source_error_upper_mm,matrix,revision,limits);
}

ModelPlacementResult capture_model_placement(const StlImportResult &requested_source, const Model &model,
                                             const PlateFrame &requested_plate, uint64_t revision,
                                             const MeshPlacementLimits &requested_limits)
{
    const auto source=requested_source;
    const auto plate=requested_plate;
    const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    ModelPlacementResult result;
    auto stop=[&] {
        if (limits.geometry.cancelled && limits.geometry.cancelled()) { result.geometry.reason="CANCELLED"; return true; }
        if (limits.is_current && !limits.is_current(revision)) { result.geometry.reason="STALE_REVISION"; return true; }
        detail::require_interval_environment();
        if (std::chrono::steady_clock::now()-started>=limits.geometry.timeout) { result.geometry.reason="DEADLINE"; return true; }
        return false;
    };
    const auto remaining_limits=[&] {
        auto remaining=limits;
        remaining.geometry.timeout-=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        return remaining;
    };
    try {
        detail::require_interval_environment();
        if (model.objects.size()!=1 || !model.objects.front() || !model.objects.front()->printable ||
            model.objects.front()->volumes.size()!=1 || !model.objects.front()->volumes.front() ||
            model.objects.front()->instances.size()!=1 || !model.objects.front()->instances.front() ||
            !model.objects.front()->instances.front()->is_printable()) {
            result.geometry.reason="REQUIRES_ONE_PRINTABLE_OBJECT_VOLUME_INSTANCE"; return result;
        }
        if (!plate.world_origin_mm.allFinite() || plate.world_origin_mm.cwiseAbs().maxCoeff()>10000) {
            result.geometry.reason="PLATE_FRAME_DOMAIN"; return result;
        }
        const auto *object=model.objects.front();
        const auto *volume=object->volumes.front();
        const Transform3d volume_matrix=volume->get_matrix(), instance_matrix=object->instances.front()->get_matrix();
        // Capture matrices before the centering helper's first callback. That
        // helper owns the mesh before any callback may mutate/delete the Model.
        const auto centered=capture_centered_volume(source,*volume,revision,remaining_limits());
        if (centered.geometry.status!=MeshAuditStatus::ValidGeometry) { result.geometry=centered.geometry; return result; }
        result.snapshot=std::make_shared<const ModelPlacementSnapshot>(ModelPlacementSnapshot{
            centered.snapshot,volume_matrix,instance_matrix,plate});
        Transform3d world_to_plate=Transform3d::Identity(); world_to_plate.translation()=-plate.world_origin_mm;
        const std::array<Transform3d,3> matrices{volume_matrix,instance_matrix,world_to_plate};
        auto geometry=centered.geometry;
        auto error=centered.total_error_upper_mm;
        std::array<double,3> roundings{};
        for (size_t stage=0; stage<matrices.size(); ++stage) {
            if (stop()) return result;
            // Match ModelObject::raw_mesh then ModelInstance::transform_mesh:
            // separate native binary32 writes, not a fused double matrix.
            auto placed=place_source_geometry(source.source,geometry,error,matrices[stage],revision,remaining_limits());
            if (placed.geometry.status!=MeshAuditStatus::ValidGeometry) { result.geometry=std::move(placed.geometry); return result; }
            geometry=std::move(placed.geometry);
            error=placed.total_error_upper_mm;
            roundings[stage]=*placed.native_transform_error_upper_mm;
        }
        if (stop()) return result;
        result.geometry=std::move(geometry);
        result.centering_error_upper_mm=centered.centering_error_upper_mm;
        result.native_transform_errors_upper_mm=roundings;
        result.total_error_upper_mm=error;
    } catch (const std::exception &) {
        result.geometry={}; result.geometry.reason="MODEL_PLACEMENT_EXCEPTION";
    }
    return result;
}
}
