#include <libslic3r/Nonplanar/StlFile.hpp>
#include <libslic3r/Nonplanar/StlWorker.hpp>
#include "UpperSummary.hpp"
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
    const bool upper=argc==4 && std::string(argv[2])=="--upper";
    if ((argc!=3 && !upper) || std::string(argv[1])!="--millimeters" ||
        (argc==3 && std::string(argv[2])=="--upper")) {
        std::cerr << "Usage: nonplanar_stl_audit --millimeters [--upper] INPUT.stl\n"
                     "Checks bounded nominal STL geometry; never authorizes export or printing.\n";
        return 64;
    }
    constexpr uint64_t revision=1; // One immutable transaction per CLI process.
    StlWorkerResult result;
    result.revision=revision;
    try {
        const auto capture=capture_stl_file(argv[upper ? 3 : 2],true,revision);
        result.reason=capture.reason;
        result.source=capture.source;
        if (capture.source) {
            // Resolve our actual executable, as Orca does; never search PATH or
            // accept a worker command from the source/model/profile/CLI options.
            const auto worker=boost::dll::program_location().parent_path()/NPTOP_WORKER_NAME;
            StlWorkerOptions options; options.analyze_upper=upper;
            result=run_stl_worker(worker.string(),capture.source->bytes,true,revision,options);
        }
    } catch (const std::exception &) { result.reason="AUDIT_LAUNCH_FAILURE"; }
    const auto status=result.status;
    const bool completed=status==MeshAuditStatus::ValidGeometry && (!upper || result.upper);
    nlohmann::json report{
        {"schema_version",1},{"kind",upper ? "stl_upper_audit" : "stl_geometry_audit"},{"revision",revision},{"units","mm"},
        {"status",completed ? (upper ? "NOMINAL_HEIGHTFIELD" : "VALID_GEOMETRY") : status==MeshAuditStatus::Invalid ? "INVALID" : "UNKNOWN"},
        {"reason",upper && status==MeshAuditStatus::ValidGeometry ? result.upper_reason : result.reason},{"export_allowed",false},
        {"source_sha256",result.source ? nlohmann::json(result.source->sha256) : nlohmann::json(nullptr)},
        {"faces",result.faces},{"volume_lower_mm3",result.volume_lower_mm3},{"volume_upper_mm3",result.volume_upper_mm3},
        {"source_error_upper_mm",result.source_error_upper_mm ? nlohmann::json(*result.source_error_upper_mm) : nlohmann::json(nullptr)},
        {"peak_rss_bytes",result.peak_rss_bytes}
    };
    if (upper) {
        report["frame"]="source_model";
        report["scope"]="nominal_geometry_only";
        report["upper"]=result.upper ? upper_summary_json(*result.upper) : nlohmann::json(nullptr);
    }
    std::cout << report.dump(2) << '\n';
    if (!std::cout) return 74;
    return completed ? 0 : 2;
}
