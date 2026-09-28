#include "StlImport.hpp"
#include "StlImportError.hpp"
#include <openssl/evp.h>

namespace Slic3r::nptop {
namespace {
std::string sha256(const std::string &bytes)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int size = 0;
    if (EVP_Digest(bytes.data(), bytes.size(), digest, &size, EVP_sha256(), nullptr) != 1 || size != 32)
        throw std::runtime_error("SHA-256 unavailable");
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (unsigned int i = 0; i < size; ++i) {
        result += hex[digest[i] >> 4];
        result += hex[digest[i] & 15];
    }
    return result;
}
}
std::shared_ptr<const StlSourceSnapshot> capture_stl_snapshot(std::string_view bytes, bool millimeters_declared)
{
    if (bytes.size()>2*1024*1024) throw std::length_error("STL source byte limit");
    std::string owned(bytes);
    auto hash=sha256(owned);
    return std::make_shared<const StlSourceSnapshot>(StlSourceSnapshot{std::move(owned),std::move(hash),millimeters_declared});
}
StlImportResult import_stl_snapshot(std::string_view bytes, bool millimeters_declared, const MeshAuditLimits &requested_limits)
{
    const MeshAuditLimits limits = requested_limits;
    StlImportResult result;
    const auto started = std::chrono::steady_clock::now();
    auto stop = [&] {
        if (limits.cancelled && limits.cancelled()) { result.geometry.reason = "CANCELLED"; return true; }
        if (std::chrono::steady_clock::now()-started >= limits.timeout) { result.geometry.reason = "DEADLINE"; return true; }
        return false;
    };
    if (bytes.size() > 2 * 1024 * 1024) { result.geometry.reason = "SOURCE_BYTE_LIMIT"; return result; }
    try {
        result.source = capture_stl_snapshot(bytes,millimeters_declared);
        if (!millimeters_declared) { result.geometry.reason = "UNCONFIRMED_UNITS"; return result; }
        if (!limits.valid()) {
            result.geometry.reason = "INVALID_LIMITS"; return result;
        }
        if (stop()) return result;
        stl_file original;
        if (!stl_open_from_memory(&original, result.source->bytes, limits.max_faces,
                [&](int, int, bool &cancel, std::string &, std::string &) { cancel = stop(); })) {
            if (result.geometry.reason.empty()) result.geometry.reason = "STL_PARSE_OR_RESOURCE_REJECTION";
            return result;
        }
        if (stop()) return result;
        for (const auto &facet : original.facet_start)
            for (const auto &vertex : facet.vertex)
                for (int axis = 0; axis < 3; ++axis)
                    if (std::abs(double(vertex(axis))) > limits.max_coordinate_mm) {
                        result.geometry.reason = "COORDINATE_DOMAIN"; return result;
                    }
        TriangleMesh parsed;
        if (!parsed.from_stl(original, false)) { result.geometry.reason = "NATIVE_IMPORT_FAILURE"; return result; }
        result.parsed = std::make_shared<const TriangleMesh>(std::move(parsed));
        try {
            result.source_error_upper_mm=detail::stl_source_error_upper(result.source->bytes,original,stop);
        } catch (const std::exception &) {
            if (result.geometry.reason.empty()) result.geometry.reason="SOURCE_ERROR_UNKNOWN";
            return result;
        }
        auto remaining = limits;
        remaining.timeout -= std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        if (remaining.timeout.count() <= 0) { result.geometry.reason = "DEADLINE"; return result; }
        auto geometry = audit_mesh(*result.parsed, true, remaining);
        if (geometry.status != MeshAuditStatus::ValidGeometry) { result.geometry = std::move(geometry); return result; }
        if (stop()) return result;

        // Do not infer repair provenance from TriangleMesh::stats: the pinned
        // native importer discards original repair counters. Keep the stl copy.
        stl_file repaired = original;
        TriangleMesh native;
        if (!native.from_stl(repaired, true)) { result.geometry.reason = "NATIVE_REPAIR_FAILURE"; return result; }
        result.native_repair = repaired.stats;
        if (stop()) return result;
        if (!same_oriented_triangles(geometry.normalized->its, native.its)) {
            result.geometry.status = MeshAuditStatus::Invalid;
            result.geometry.reason = "NATIVE_REPAIR_CHANGED_GEOMETRY";
            return result;
        }
        if (stop()) return result;
        result.native_geometry_unchanged = true;
        result.geometry = std::move(geometry);
    } catch (const std::exception &) {
        result.geometry = {};
        result.geometry.reason = "IMPORT_EXCEPTION";
        result.native_geometry_unchanged = false;
    }
    return result;
}
}
