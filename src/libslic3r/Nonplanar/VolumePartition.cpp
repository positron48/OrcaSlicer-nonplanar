#include "VolumePartition.hpp"
#include "VolumePartitionExact.hpp"
#include "InputSnapshot.hpp"
#include "Canonical.hpp"
#include "Interval.hpp"
#include "StlImport.hpp"

namespace Slic3r::nptop {
std::string VolumePartitionSnapshot::canonical_json() const
{
    detail::CanonicalConfigWriter w;
    w.append("{\"body\":"); w.value(native_mesh_fingerprint(body->its));
    w.append(",\"cap\":"); w.value(native_mesh_fingerprint(cap->its));
    w.append(",\"input\":"); w.value(placement->native_input->fingerprint);
    w.append(",\"original\":"); w.value(native_mesh_fingerprint(original->its));
    w.append(",\"partition_error\":"); w.value(partition_error_upper_mm);
    w.append(",\"plate_index\":"); w.append(std::to_string(placement->plate.index));
    w.append(",\"plate_origin\":"); w.value(placement->plate.world_origin_mm);
    w.append(",\"reservation\":"); w.value(native_mesh_fingerprint(reservation->its));
    w.append(",\"revision\":"); w.append(std::to_string(revision));
    w.append(",\"schema\":1,\"source\":"); w.value(placement->centered->source->sha256);
    w.append(",\"total_error\":"); w.value(total_error_upper_mm); w.append("}");
    return w.take();
}

std::string VolumePartitionSnapshot::fingerprint() const { return sha256_bytes(canonical_json()); }

VolumePartitionResult partition_cap(const ModelPlacementResult &requested_source, const TriangleMesh &requested_reservation,
                                    const VolumePartitionLimits &requested_limits)
{
    const auto source=requested_source;
    const auto limits=requested_limits;
    VolumePartitionResult result;
    const auto started=std::chrono::steady_clock::now();
    struct Rejection { const char *reason; };
    const auto stop=[&] {
        if (limits.geometry.cancelled && limits.geometry.cancelled()) throw Rejection{"CANCELLED"};
        if (limits.is_current && !limits.is_current(source.snapshot->centered->revision)) throw Rejection{"STALE_REVISION"};
        if (std::chrono::steady_clock::now()-started>=limits.geometry.timeout) throw Rejection{"DEADLINE"};
        detail::require_interval_environment();
    };
    const auto audit=[&](const TriangleMesh &mesh) {
        auto remaining=limits.geometry;
        remaining.timeout-=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        remaining.cancelled=[&] { stop(); return false; };
        return audit_mesh(mesh,true,remaining);
    };
    try {
        detail::require_interval_environment();
        if (!limits.geometry.valid() || limits.geometry.timeout>std::chrono::seconds(30) ||
            !std::isfinite(limits.max_total_error_mm) || limits.max_total_error_mm<=0 || limits.max_total_error_mm>0.05) {
            result.reason="INVALID_PARTITION_LIMITS"; return result;
        }
        if (source.geometry.status!=MeshAuditStatus::ValidGeometry || !source.geometry.normalized ||
            !source.snapshot || !source.snapshot->centered || !source.snapshot->native_input ||
            !source.snapshot->centered->source || !source.snapshot->centered->source->millimeters_declared ||
            source.snapshot->centered->revision==0 ||
            !source.total_error_upper_mm || !std::isfinite(*source.total_error_upper_mm) || *source.total_error_upper_mm<0) {
            result.reason="MISSING_OWNED_PLACEMENT_PROVENANCE"; return result;
        }
        if (requested_reservation.its.indices.size()>limits.geometry.max_faces ||
            requested_reservation.its.vertices.size()>limits.geometry.max_vertices ||
            source.geometry.normalized->its.indices.size()>limits.geometry.max_faces ||
            source.geometry.normalized->its.vertices.size()>limits.geometry.max_vertices) {
            result.reason="RESOURCE_LIMIT"; return result;
        }
        const auto reservation=std::make_shared<const TriangleMesh>(requested_reservation);
        stop();
        const auto selection=audit(*reservation); stop();
        if (selection.status!=MeshAuditStatus::ValidGeometry) {
            result.status=selection.status==MeshAuditStatus::Invalid ? VolumePartitionStatus::Invalid : VolumePartitionStatus::Unknown;
            result.reason="RESERVATION_"+selection.reason; return result;
        }
        auto exact=detail::exact_partition(*source.geometry.normalized,*selection.normalized,limits.geometry,stop);
        stop();
        const auto body=audit(exact.body); stop();
        const auto cap=audit(exact.cap); stop();
        if (body.status!=MeshAuditStatus::ValidGeometry || cap.status!=MeshAuditStatus::ValidGeometry) {
            result.reason="NATIVE_PARTITION_TOPOLOGY_REJECTED"; return result;
        }
        const double total=(detail::Interval(*source.total_error_upper_mm)+detail::Interval(exact.coordinate_error_upper_mm)).hi;
        if (total>limits.max_total_error_mm) { result.reason="PARTITION_ERROR_BUDGET"; return result; }
        const auto volume_error=[](const MeshAuditResult &native, ScalarBounds exact) {
            const auto delta=detail::Interval(native.volume_lower_mm3,native.volume_upper_mm3)-detail::Interval(exact.lower,exact.upper);
            return std::max(std::abs(delta.lo),std::abs(delta.hi));
        };
        auto snapshot=std::make_shared<const VolumePartitionSnapshot>(VolumePartitionSnapshot{
            source.snapshot->centered->revision,source.snapshot,source.geometry.normalized,reservation,body.normalized,cap.normalized,
            exact.original_volume,exact.body_volume,exact.cap_volume,
            {body.volume_lower_mm3,body.volume_upper_mm3},{cap.volume_lower_mm3,cap.volume_upper_mm3},
            exact.coordinate_error_upper_mm,total,volume_error(body,exact.body_volume),volume_error(cap,exact.cap_volume),
            exact.shared_interface_triangles});
        stop();
        return {VolumePartitionStatus::Partitioned,"NOMINAL_BODY_CAP_PARTITION_ONLY",std::move(snapshot)};
    } catch (const Rejection &rejected) { result.reason=rejected.reason; }
    catch (const std::exception &error) { result.reason="PARTITION_EXCEPTION: "+std::string(error.what()); }
    return result;
}
}
