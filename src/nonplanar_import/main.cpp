#include <libslic3r/Nonplanar/StlWorker.hpp>
#include <libslic3r/Nonplanar/UpperProjection.hpp>
#include "UpperSummary.hpp"
#include <boost/log/core.hpp>
#include <nlohmann/json.hpp>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

using namespace Slic3r::nptop;
namespace {
bool cpu_and_file_limits()
{
#ifdef _WIN32
    const HANDLE job=CreateJobObjectW(nullptr,nullptr);
    if (!job) return false;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_PROCESS_TIME;
    limits.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart=5LL*10000000;
    // Keep this handle until _Exit. Closing it is handled by process teardown.
    return SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)) &&
        AssignProcessToJobObject(job,GetCurrentProcess());
#else
    const auto limit=[](int kind,rlim_t cap) {
        rlimit value{};
        if (getrlimit(kind,&value)!=0) return false;
        value.rlim_cur=std::min(value.rlim_cur,cap); value.rlim_max=std::min(value.rlim_max,cap);
        return setrlimit(kind,&value)==0;
    };
    return limit(RLIMIT_CORE,0) && limit(RLIMIT_CPU,5) && limit(RLIMIT_FSIZE,4*1024*1024);
#endif
}
uint64_t peak_rss()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    return GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters)) ? counters.PeakWorkingSetSize : 0;
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF,&usage)!=0 || usage.ru_maxrss<=0) return 0;
    return uint64_t(usage.ru_maxrss)
#ifndef __APPLE__
        *1024
#endif
        ;
#endif
}
[[noreturn]] void emit(nlohmann::json report)
{
    // Serialize before observing peak memory. Only fixed-buffer integer encoding,
    // write syscalls and _Exit follow; library cleanup cannot change the report.
    report["peak_rss_bytes"]="00000000000000000000";
    auto bytes=report.dump();
    if (bytes.size()>4096) std::_Exit(70);
    const std::string marker="\"peak_rss_bytes\":\"";
    const auto offset=bytes.find(marker);
    if (offset==std::string::npos) std::_Exit(70);
    char digits[20];
    const auto peak=peak_rss();
    const auto encoded=std::to_chars(digits,digits+sizeof(digits),peak);
    if (encoded.ec!=std::errc{}) std::_Exit(70);
    const size_t size=size_t(encoded.ptr-digits);
    std::memcpy(bytes.data()+offset+marker.size()+20-size,digits,size);
    size_t written=0;
    while (written<bytes.size()) {
#ifdef _WIN32
        const auto count=::_write(1,bytes.data()+written,unsigned(bytes.size()-written));
#else
        const auto count=::write(1,bytes.data()+written,bytes.size()-written);
#endif
        if (count<=0) std::_Exit(70);
        written+=size_t(count);
    }
    std::_Exit(0);
}
}
int main(int argc,char **argv)
{
#ifdef _WIN32
    _setmode(0,_O_BINARY); _setmode(1,_O_BINARY);
#endif
    boost::log::core::get()->set_logging_enabled(false);
    uint64_t revision=0;
    const bool upper=argc==4 && std::strcmp(argv[3],"--upper")==0;
    if ((argc!=3 && !upper) || std::strcmp(argv[1],"--millimeters")!=0) return 64;
    const auto end=argv[2]+std::strlen(argv[2]);
    const auto parsed=std::from_chars(argv[2],end,revision);
    if (parsed.ec!=std::errc{} || parsed.ptr!=end || revision==0) return 64;
    if (!cpu_and_file_limits()) return 70;
    try {
        std::string input;
        char block[8192];
        while (const auto size=std::fread(block,1,sizeof(block),stdin)) {
            if (input.size()+size>2*1024*1024) return 65;
            input.append(block,size);
        }
        if (std::ferror(stdin)) return 74;
        const auto imported=import_stl_snapshot(input,true);
        if (!imported.source) return 70;
        const auto status=imported.geometry.status;
        nlohmann::json report{
            {"protocol",stl_worker_protocol},{"revision",revision},{"source_sha256",imported.source->sha256},
            {"analysis",upper ? "upper" : "geometry"},{"upper",nullptr},
            {"status",status==MeshAuditStatus::ValidGeometry ? "VALID_GEOMETRY" : status==MeshAuditStatus::Invalid ? "INVALID" : "UNKNOWN"},
            {"reason",imported.geometry.reason},{"faces",imported.geometry.normalized ? imported.geometry.normalized->facets_count() : size_t(0)},
            {"volume_lower_mm3",imported.geometry.volume_lower_mm3},{"volume_upper_mm3",imported.geometry.volume_upper_mm3},
            {"source_error_upper_mm",imported.source_error_upper_mm ? nlohmann::json(*imported.source_error_upper_mm) : nlohmann::json(nullptr)}
        };
        if (upper && status==MeshAuditStatus::ValidGeometry) {
            UpperProjectionLimits limits;
            limits.max_slope=stl_worker_upper_slope_limit;
            const auto analyzed=analyze_upper_projection(*imported.geometry.normalized,true,revision,limits);
            report["upper"]={{"status","UNKNOWN"},{"reason",analyzed.reason}};
            if (analyzed.status==UpperProjectionStatus::NominalHeightfield && analyzed.snapshot) {
                const auto &snapshot=*analyzed.snapshot;
                StlUpperSummary summary;
                summary.upward_faces=snapshot.upward_facets.size();
                for (const auto &facet : snapshot.upward_facets) if (facet.within_slope_limit) {
                    ++summary.selected_faces;
                    summary.selected_slope_upper=std::max(summary.selected_slope_upper,facet.slope_upper);
                }
                summary.patches=snapshot.slope_patches.size();
                for (const auto &patch : snapshot.slope_patches) {
                    for (const auto &boundary : patch.boundaries) if (boundary.hole) ++summary.holes;
                    summary.creases+=patch.creases.size();
                    if (patch.nominal_curvature_upper_mm_inv==0) ++summary.affine_patches;
                }
                summary.area_lower_mm2=snapshot.xy_area_lower_mm2;
                summary.area_upper_mm2=snapshot.xy_area_upper_mm2;
                summary.selected_area_lower_mm2=snapshot.filtered_area_lower_mm2;
                summary.selected_area_upper_mm2=snapshot.filtered_area_upper_mm2;
                summary.minimum_z_mm=snapshot.minimum_z_mm; summary.maximum_z_mm=snapshot.maximum_z_mm;
                report["upper"]["status"]="NOMINAL_HEIGHTFIELD";
                report["upper"]["summary"]=upper_summary_json(summary);
            }
        }
        emit(std::move(report));
    } catch (const std::exception &) { return 70; }
}
