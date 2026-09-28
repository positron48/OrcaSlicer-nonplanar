#include <libslic3r/Nonplanar/StlFile.hpp>
#include <libslic3r/Nonplanar/StlWorker.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/log/core.hpp>
#include <boost/nowide/args.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

using namespace Slic3r::nptop;
int main(int argc,char **argv)
{
    boost::nowide::args utf8(argc,argv);
    boost::log::core::get()->set_logging_enabled(false);
    if (argc!=3 || std::string(argv[1])!="--millimeters") {
        std::cerr << "Usage: nonplanar_stl_audit --millimeters INPUT.stl\n"
                     "Checks bounded STL geometry only; never authorizes export or printing.\n";
        return 64;
    }
    constexpr uint64_t revision=1; // One immutable transaction per CLI process.
    StlWorkerResult result;
    result.revision=revision;
    try {
        const auto capture=capture_stl_file(argv[2],true,revision);
        result.reason=capture.reason;
        result.source=capture.source;
        if (capture.source) {
            // Resolve our actual executable, as Orca does; never search PATH or
            // accept a worker command from the source/model/profile/CLI options.
            const auto worker=boost::dll::program_location().parent_path()/NPTOP_WORKER_NAME;
            result=run_stl_worker(worker.string(),capture.source->bytes,true,revision);
        }
    } catch (const std::exception &) { result.reason="AUDIT_LAUNCH_FAILURE"; }
    const auto status=result.status;
    const nlohmann::json report{
        {"schema_version",1},{"kind","stl_geometry_audit"},{"revision",revision},{"units","mm"},
        {"status",status==MeshAuditStatus::ValidGeometry ? "VALID_GEOMETRY" : status==MeshAuditStatus::Invalid ? "INVALID" : "UNKNOWN"},
        {"reason",result.reason},{"export_allowed",false},
        {"source_sha256",result.source ? nlohmann::json(result.source->sha256) : nlohmann::json(nullptr)},
        {"faces",result.faces},{"volume_lower_mm3",result.volume_lower_mm3},{"volume_upper_mm3",result.volume_upper_mm3},
        {"source_error_upper_mm",result.source_error_upper_mm ? nlohmann::json(*result.source_error_upper_mm) : nlohmann::json(nullptr)},
        {"peak_rss_bytes",result.peak_rss_bytes}
    };
    std::cout << report.dump(2) << '\n';
    if (!std::cout) return 74;
    return status==MeshAuditStatus::ValidGeometry ? 0 : 2;
}
