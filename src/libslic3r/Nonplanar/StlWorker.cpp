#include "StlWorker.hpp"
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/process.hpp>
#include <nlohmann/json.hpp>
#include <charconv>
#include <set>
#include <thread>

namespace Slic3r::nptop {
namespace {
namespace fs=boost::filesystem;
namespace process=boost::process;
struct Workspace {
    fs::path path;
    Workspace() : path(fs::temp_directory_path()/fs::unique_path("orca-nptop-worker-%%%%-%%%%-%%%%-%%%%")) {
        if (!fs::create_directory(path)) throw std::runtime_error("worker workspace unavailable");
        try { fs::permissions(path,fs::owner_all); }
        catch (...) { fs::remove(path); throw; }
    }
    ~Workspace() { boost::system::error_code ec; fs::remove_all(path,ec); }
};
struct Child {
    process::child value;
    ~Child() {
        std::error_code ec;
        if (value.running(ec)) value.terminate(ec);
        value.wait(ec);
    }
};
std::string bounded_report(const fs::path &path)
{
    boost::nowide::ifstream input(path.string(),std::ios::binary);
    if (!input) throw std::runtime_error("missing worker report");
    std::string bytes;
    char block[1024];
    while (input.read(block,sizeof(block)) || input.gcount()) {
        if (bytes.size()+size_t(input.gcount())>4096) throw std::runtime_error("worker report limit");
        bytes.append(block,size_t(input.gcount()));
    }
    if (!input.eof()) throw std::runtime_error("worker report read failure");
    return bytes;
}
}
StlWorkerResult run_stl_worker(const std::string &executable, std::string_view bytes,
                               bool millimeters_declared, uint64_t revision, const StlWorkerOptions &requested_options)
{
    const StlWorkerOptions options=requested_options;
    const auto started=std::chrono::steady_clock::now();
    StlWorkerResult result;
    result.revision=revision;
    auto stop=[&] {
        if (options.cancelled && options.cancelled()) { result.reason="CANCELLED"; return true; }
        if (options.is_current && !options.is_current(revision)) { result.reason="STALE_REVISION"; return true; }
        if (std::chrono::steady_clock::now()-started>=options.timeout) { result.reason="WORKER_DEADLINE"; return true; }
        return false;
    };
    if (bytes.size()>2*1024*1024) { result.reason="SOURCE_BYTE_LIMIT"; return result; }
    if (revision==0 || options.timeout.count()<=0 || options.timeout>std::chrono::seconds(30) ||
        options.max_peak_rss_bytes==0 || options.max_peak_rss_bytes>1024ULL*1024*1024) {
        result.reason="INVALID_WORKER_LIMITS"; return result;
    }
    try {
        result.source=capture_stl_snapshot(bytes,millimeters_declared);
        if (!millimeters_declared) { result.reason="UNCONFIRMED_UNITS"; return result; }
        if (stop()) return result;
        Workspace workspace;
        const auto input=workspace.path/"input.stl", output=workspace.path/"report.json";
        {
            boost::nowide::ofstream file(input.string(),std::ios::binary);
            file.write(result.source->bytes.data(),std::streamsize(result.source->bytes.size()));
            file.close();
            if (!file) throw std::runtime_error("worker input write failure");
        }
        if (stop()) return result;
        Child child{process::child(executable,std::vector<std::string>{"--millimeters",std::to_string(revision)},
                                  process::std_in<input.string(),process::std_out>output.string(),process::std_err>process::null)};
        std::error_code ec;
        // Boost's deprecated wait_for changes SIGCHLD state and may fork a timer
        // process on macOS. Poll this one child without changing global handlers.
        while (child.value.running(ec)) {
            if (stop()) return result;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (ec || child.value.exit_code()!=0) { result.reason="WORKER_FAILED"; return result; }
        if (stop()) return result;
        const auto report=nlohmann::json::parse(bounded_report(output));
        const std::set<std::string> keys{"protocol","revision","source_sha256","status","reason","faces",
            "volume_lower_mm3","volume_upper_mm3","source_error_upper_mm","peak_rss_bytes"};
        if (!report.is_object() || report.size()!=keys.size()) throw std::runtime_error("worker report schema");
        for (const auto &item : report.items())
            if (!keys.count(item.key())) throw std::runtime_error("worker report schema");
        if (!report.at("protocol").is_number_unsigned() || report.at("protocol")!=stl_worker_protocol ||
            !report.at("revision").is_number_unsigned() || report.at("revision").get<uint64_t>()!=revision) {
            result.reason="WORKER_IDENTITY_MISMATCH"; return result;
        }
        if (report.at("source_sha256")!=result.source->sha256) { result.reason="WORKER_SOURCE_MISMATCH"; return result; }
        const auto rss=report.at("peak_rss_bytes").get<std::string>();
        uint64_t peak=0;
        const auto parsed=std::from_chars(rss.data(),rss.data()+rss.size(),peak);
        if (rss.size()!=20 || parsed.ec!=std::errc{} || parsed.ptr!=rss.data()+rss.size() || peak==0 ||
            peak>std::numeric_limits<uint64_t>::max()-65536) throw std::runtime_error("worker RSS measurement");
        result.peak_rss_bytes=peak+65536; // Conservative allowance for final fixed-size reporting.
        if (result.peak_rss_bytes>options.max_peak_rss_bytes) { result.reason="WORKER_MEMORY_BUDGET"; return result; }
        const auto status=report.at("status").get<std::string>();
        const auto reason=report.at("reason").get<std::string>();
        if (reason.empty() || reason.size()>80 || reason.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789")!=std::string::npos)
            throw std::runtime_error("worker reason");
        if (status=="UNKNOWN" || status=="INVALID") {
            result.status=status=="INVALID" ? MeshAuditStatus::Invalid : MeshAuditStatus::Unknown;
            result.reason=reason; return result;
        }
        if (status!="VALID_GEOMETRY" || !report.at("faces").is_number_unsigned()) throw std::runtime_error("worker verdict");
        const auto faces=report.at("faces").get<uint64_t>();
        const auto lower=report.at("volume_lower_mm3").get<double>(), upper=report.at("volume_upper_mm3").get<double>();
        const auto error=report.at("source_error_upper_mm").get<double>();
        if (faces==0 || faces>5000 || !std::isfinite(lower) || !std::isfinite(upper) || lower<=0 || lower>upper ||
            !std::isfinite(error) || error<0) throw std::runtime_error("worker geometry bounds");
        if (stop()) return result;
        result.faces=size_t(faces); result.volume_lower_mm3=lower; result.volume_upper_mm3=upper;
        result.source_error_upper_mm=error; result.reason=reason; result.status=MeshAuditStatus::ValidGeometry;
    } catch (const std::exception &) {
        result.status=MeshAuditStatus::Unknown; result.faces=0; result.source_error_upper_mm.reset();
        result.reason="WORKER_PROTOCOL_OR_LAUNCH_FAILURE";
    }
    return result;
}
}
