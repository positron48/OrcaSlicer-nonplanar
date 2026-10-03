#include "NativeAnalysisWorker.hpp"
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/process.hpp>
#include <nlohmann/json.hpp>
#include <charconv>
#include <set>
#include <thread>
#include <condition_variable>
#include <mutex>
#ifdef __APPLE__
#include <libproc.h>
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

namespace Slic3r::nptop {
namespace {
namespace fs=boost::filesystem;
namespace process=boost::process;
using Json=nlohmann::json;
struct Workspace {
    fs::path path=fs::temp_directory_path()/fs::unique_path("orca-nptop-analysis-%%%%-%%%%-%%%%-%%%%");
    Workspace(){require(fs::create_directory(path),"WORKER_WORKSPACE");try{fs::permissions(path,fs::owner_all);}catch(...){fs::remove(path);throw;}}
    ~Workspace(){boost::system::error_code ec;fs::remove_all(path,ec);}
};
uint64_t resident(process::child &child)
{
#ifdef __APPLE__
    proc_taskinfo info{};return proc_pidinfo(child.id(),PROC_PIDTASKINFO,0,&info,sizeof(info))==sizeof(info) ? info.pti_resident_size : 0;
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS info{};return GetProcessMemoryInfo(child.native_handle(),&info,sizeof(info)) ? info.WorkingSetSize : 0;
#else
    boost::nowide::ifstream file("/proc/"+std::to_string(child.id())+"/status");std::string key;
    while(file>>key){if(key=="VmRSS:"){uint64_t kib=0;std::string unit;if(file>>kib>>unit && unit=="kB" && kib<=UINT64_MAX/1024)return kib*1024;return 0;}std::string rest;std::getline(file,rest);}return 0;
#endif
}
// Only this owner touches the process handle, under one mutex. The monitor
// never calls host callbacks: UI progress can block without suspending the
// child's deadline, RSS, output bounds or atomic task invalidation.
class Child {
    process::child value;
    std::mutex mutex;
    std::condition_variable wake;
    std::thread monitor;
    bool stopping=false;
    const char *failure=nullptr;
    uint64_t peak=0;
    void terminate_wait() noexcept
    {std::error_code ec;if(value.running(ec))value.terminate(ec);value.wait(ec);}
public:
    Child(process::child child,std::shared_ptr<const GuardedJobTask> task,
        std::chrono::steady_clock::time_point deadline,uint64_t cap,fs::path output,fs::path progress)
        :value(std::move(child))
    {
        try {monitor=std::thread([this,task=std::move(task),deadline,cap,output=std::move(output),progress=std::move(progress)] {
            try {
                std::unique_lock<std::mutex> lock(mutex);
                std::chrono::steady_clock::time_point missing_rss{};
                while(!stopping){
                    std::error_code ec;
                    if(!value.running(ec)){if(ec)failure="WORKER_PROCESS_FAILED";return;}
                    const auto now=std::chrono::steady_clock::now();
                    if(!task->is_current())failure="WORKER_STALE_HOST_TASK";
                    else if(now>=deadline)failure="WORKER_DEADLINE";
                    else {
                        const auto rss=resident(value);
                        if(rss){missing_rss={};peak=std::max(peak,rss);if(rss>cap)failure="WORKER_MEMORY_BUDGET";}
                        else if(value.running(ec)){
                            // macOS drops task info briefly before waitpid sees
                            // exit; terminal adoption still requires OS peak RSS.
                            if(missing_rss==std::chrono::steady_clock::time_point{})missing_rss=now;
                            if(now-missing_rss>=std::chrono::milliseconds(20))failure="WORKER_MEMORY_OBSERVATION_FAILED";
                        }
                        if(!failure && fs::file_size(output)>native_analysis_worker_byte_limit)failure="WORKER_OUTPUT_BYTE_LIMIT";
                        if(!failure && fs::file_size(progress)>10)failure="WORKER_PROGRESS_BYTE_LIMIT";
                    }
                    if(failure){terminate_wait();return;}
                    wake.wait_for(lock,std::chrono::milliseconds(5),[this]{return stopping;});
                }
            } catch(...) {
                std::lock_guard<std::mutex> lock(mutex);
                failure="WORKER_PROTOCOL_OR_CALLBACK_FAILURE";terminate_wait();
            }
        });}catch(...){terminate_wait();throw;}
    }
    ~Child()
    {
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;}
        wake.notify_one();monitor.join();terminate_wait();
    }
    bool running(std::error_code &ec){std::lock_guard<std::mutex> lock(mutex);return value.running(ec);}
    int exit_code(){std::lock_guard<std::mutex> lock(mutex);return value.exit_code();}
    void check(uint64_t &observed)
    {
        std::lock_guard<std::mutex> lock(mutex);observed=std::max(observed,peak);
        if(failure)require(false,failure);
    }
};
std::string read(const fs::path &path,size_t limit)
{
    boost::nowide::ifstream file(path.string(),std::ios::binary);require(bool(file),"WORKER_OUTPUT_MISSING");std::string bytes;char block[8192];
    while(file.read(block,sizeof(block)) || file.gcount()){require(size_t(file.gcount())<=limit-bytes.size(),"WORKER_OUTPUT_BYTE_LIMIT");bytes.append(block,size_t(file.gcount()));}
    require(file.eof(),"WORKER_OUTPUT_READ");return bytes;
}
void exact_keys(const Json &j,std::initializer_list<const char*> expected)
{require(j.is_object(),"WORKER_REPORT_OBJECT");std::set<std::string> actual;for(auto it=j.begin();it!=j.end();++it)actual.insert(it.key());require(actual==std::set<std::string>(expected.begin(),expected.end()),"WORKER_REPORT_KEYS");}
Json parse(const std::string &bytes)
{
    std::vector<std::set<std::string>> objects;return Json::parse(bytes,[&](int depth,Json::parse_event_t event,Json &v){require(depth<=32,"WORKER_REPORT_DEPTH");if(event==Json::parse_event_t::object_start)objects.emplace_back();if(event==Json::parse_event_t::key)require(objects.back().insert(v.get<std::string>()).second,"WORKER_REPORT_DUPLICATE");if(event==Json::parse_event_t::object_end)objects.pop_back();return true;});
}
void diagnostic(const Json &j,const NativeAnalysisWorkerInput &input)
{
    if(j.at("completed")==true)exact_keys(j,{"schema","stage","reason","completed","export_allowed","job","report","replay","request_sha256","report_sha256","manifest","manifest_sha256","replay_evaluations"});
    else exact_keys(j,{"schema","stage","reason","completed","export_allowed","job","report","replay"});
    require(j.at("schema")==1 && j.at("export_allowed")==false && j.at("completed").is_boolean() && !j.contains("candidate_bytes"),"WORKER_BLOCKED_DIAGNOSTIC");
    require(j.at("reason").is_string() && j.at("reason").get_ref<const std::string&>().size()<=1024 && j.at("stage").is_string(),"WORKER_DIAGNOSTIC_TEXT");
    require(j.at("replay").is_array() && j.at("replay").size()<=200000,"WORKER_REPLAY_COUNT");
    if(!j.at("completed").get<bool>()){require(j.at("report").is_null() && j.at("replay").empty(),"WORKER_PARTIAL_REPORT");return;}
    const auto &report=j.at("report"),&manifest=j.at("manifest");require(sha256_bytes(report.dump())==j.at("report_sha256") && sha256_bytes(manifest.dump())==j.at("manifest_sha256") && report.at("manifest_sha256")==j.at("manifest_sha256"),"WORKER_REPORT_BINDING");
    const auto &job=j.at("job");exact_keys(job,{"id","revision","fingerprint","canonical"});const auto canonical=job.at("canonical").get<std::string>();const auto identity=parse(canonical);
    const auto encoded=[](const std::string &s){std::string out;for(unsigned char c:s){out.push_back("0123456789abcdef"[c>>4]);out.push_back("0123456789abcdef"[c&15]);}return out;};
    require(job.at("id")==input.task->snapshot->job_id && sha256_bytes(canonical)==job.at("fingerprint") && identity.at("native_input")==encoded(input.input_fingerprint) &&
        manifest.at("job_fingerprint")==encoded(job.at("fingerprint").get<std::string>()) && report.at("job_fingerprint")==job.at("fingerprint"),"WORKER_CHILD_JOB_BINDING");
    const auto &validation=report.at("validation");require(validation.at("export_decision")=="BLOCK" && validation.at("checks").size()==17 && report.at("replay").at("records")==j.at("replay").size(),"WORKER_MANDATORY_GATE");
    const auto &registry=guarded_mandatory_checks();require((validation.at("overall_status")=="UNKNOWN" || validation.at("overall_status")=="FAIL") && validation.at("mandatory_check_ids")==registry,"WORKER_CHECK_REGISTRY");
    const std::set<std::string> implemented{"candidate_manifest_identity","final_material","final_rates","independent_replay"};
    for(size_t i=0;i<registry.size();++i){const auto &check=validation.at("checks")[i];exact_keys(check,{"id","mandatory","status","execution","reason"});
        require(check.at("id")==registry[i] && check.at("mandatory")==true && check.at("reason").is_string(),"WORKER_MANDATORY_CHECK");
        if(!implemented.count(registry[i]))require(check.at("status")=="UNKNOWN" && check.at("execution")=="NOT_RUN","WORKER_UNIMPLEMENTED_CHECK");
        else require((check.at("status")=="PASS" || check.at("status")=="FAIL" || check.at("status")=="UNKNOWN") &&
            (check.at("execution")=="RUN" || check.at("execution")=="SKIPPED") && (check.at("status")!="PASS" || check.at("execution")=="RUN"),"WORKER_COMPONENT_CHECK");}
    std::array<double,3> previous{};bool have_previous=false;uint64_t previous_byte=0;
    for(size_t index=0;index<j.at("replay").size();++index){const auto &row=j.at("replay")[index];exact_keys(row,{"index","kind","start_mm","end_mm","e_mm","feed_mm_min","duration_s","nominal_volume_mm3","candidate_byte_range"});
        require(row.at("index").is_number_unsigned() && row.at("index").get<uint64_t>()==index && row.at("kind").is_number_unsigned() && row.at("kind").get<uint64_t>()<=2,"WORKER_MOVEMENT_ID");
        for(const char *key:{"start_mm","end_mm"}){require(row.at(key).is_array() && row.at(key).size()==3,"WORKER_MOVEMENT_AXES");for(const auto &v:row.at(key))require(v.is_number() && std::isfinite(v.get<double>()) && std::abs(v.get<double>())<=10000,"WORKER_MOVEMENT_COORDINATE");}
        const auto start=row.at("start_mm").get<std::array<double,3>>(),end=row.at("end_mm").get<std::array<double,3>>();require(!have_previous || start==previous,"WORKER_MOVEMENT_CONTINUITY");previous=end;have_previous=true;
        for(const char *key:{"e_mm","feed_mm_min"})require(row.at(key).is_number() && std::isfinite(row.at(key).get<double>()),"WORKER_MOVEMENT_NUMBER");
        require(row.at("feed_mm_min").get<double>()>=0,"WORKER_MOVEMENT_FEED");
        for(const char *key:{"duration_s","nominal_volume_mm3"}){const auto &v=row.at(key);require(v.is_array() && v.size()==2 && v[0].is_number() && v[1].is_number() && std::isfinite(v[0].get<double>()) && std::isfinite(v[1].get<double>()) && v[0].get<double>()>=0 && v[0].get<double>()<=v[1].get<double>(),"WORKER_MOVEMENT_BOUNDS");}
        const auto &range=row.at("candidate_byte_range");require(range.is_array() && range.size()==2 && range[0].is_number_unsigned() && range[1].is_number_unsigned() && range[0].get<uint64_t>()<range[1].get<uint64_t>(),"WORKER_MOVEMENT_RANGE");
        require(range[0].get<uint64_t>()>=previous_byte && range[1]<=manifest.at("candidate_size"),"WORKER_MOVEMENT_RANGE_ORDER");previous_byte=range[1].get<uint64_t>();
    }
}
}
NativeAnalysisWorkerResult run_native_analysis_worker(const std::string &executable,std::shared_ptr<const NativeAnalysisWorkerInput> input,
    const NativeAnalysisWorkerOptions &requested)
{
    const auto options=requested;const auto started=std::chrono::steady_clock::now();NativeAnalysisWorkerResult result;
    const auto check=[&]{require(input && input->task->is_current(),"WORKER_STALE_HOST_TASK");bool cancelled=false;
        try{if(options.cancelled)cancelled=options.cancelled();}catch(...){require(false,"WORKER_PROTOCOL_OR_CALLBACK_FAILURE");}
        require(!cancelled,"WORKER_CANCELLED");require(std::chrono::steady_clock::now()-started<options.timeout,"WORKER_DEADLINE");};
    try{
        require(options.timeout.count()>0 && options.timeout<=std::chrono::seconds(30) && options.max_peak_rss_bytes && options.max_peak_rss_bytes<=8ULL*1024*1024*1024,"WORKER_INVALID_LIMITS");check();
        require(fs::path(executable).is_absolute() && fs::is_regular_file(executable),"WORKER_EXECUTABLE");
        Workspace workspace;const auto source=workspace.path/"input.json",output=workspace.path/"report.json",progress=workspace.path/"progress.txt";
        {boost::nowide::ofstream file(source.string(),std::ios::binary);file.write(input->bytes.data(),std::streamsize(input->bytes.size()));file.close();require(bool(file),"WORKER_INPUT_WRITE");}
        check();Child child{process::child(executable,std::vector<std::string>{"--resident-cap",std::to_string(options.max_peak_rss_bytes)},process::std_in<source.string(),process::std_out>output.string(),process::std_err>progress.string(),process::start_dir=workspace.path.string()),input->task,started+options.timeout,options.max_peak_rss_bytes,output,progress};
        const auto supervised_check=[&]{child.check(result.peak_rss_bytes);check();};
        const auto stages=[&]{const auto records=read(progress,10);require(records.size()>=result.progress_stages,"WORKER_PROGRESS_TRUNCATED");
            for(size_t index=result.progress_stages;index<records.size();++index){supervised_check();require(records[index]=='0'+int(index),"WORKER_PROGRESS_SEQUENCE");result.progress_stages=index+1;
                try{if(options.progress)options.progress(NativeAnalysisStage(index));}catch(...){require(false,"WORKER_PROTOCOL_OR_CALLBACK_FAILURE");}supervised_check();}};
        std::error_code ec;
        while(child.running(ec)){supervised_check();stages();std::this_thread::sleep_for(std::chrono::milliseconds(5));}
        supervised_check();require(!ec && child.exit_code()==0,"WORKER_PROCESS_FAILED");stages();
        const auto report=parse(read(output,native_analysis_worker_byte_limit));exact_keys(report,{"schema","host_job","input_payload_sha256","input_sha256","request_sha256","software_sha256","diagnostic","peak_rss_bytes"});
        const auto &job=*input->task->snapshot;require(report.at("schema").is_number_unsigned() && report.at("schema")==native_analysis_worker_protocol && report.at("host_job")==Json::array({job.job_id,job.input_revision,input->task->attempt,job.fingerprint}) &&
            report.at("input_payload_sha256")==input->sha256 && report.at("input_sha256")==input->input_fingerprint && report.at("request_sha256")==input->request_sha256 && report.at("software_sha256")==job.software->sha256,"WORKER_REPORT_IDENTITY");
        const auto rss=report.at("peak_rss_bytes").get<std::string>();uint64_t peak=0;const auto parsed=std::from_chars(rss.data(),rss.data()+rss.size(),peak);
        require(rss.size()==20 && parsed.ec==std::errc{} && parsed.ptr==rss.data()+rss.size() && peak && peak<=UINT64_MAX-65536,"WORKER_PEAK_MEMORY");result.peak_rss_bytes=std::max(result.peak_rss_bytes,peak+65536);require(result.peak_rss_bytes<=options.max_peak_rss_bytes,"WORKER_MEMORY_BUDGET");
        const auto &value=report.at("diagnostic");diagnostic(value,*input);require(!value.at("completed").get<bool>() || (result.progress_stages==10 && value.at("request_sha256")==input->request_sha256),"WORKER_COMPLETED_IDENTITY");supervised_check();
        auto bytes=value.dump();supervised_check();result.reason=value.at("completed").get<bool>() ? "WORKER_BLOCKED_DIAGNOSTIC_COMPLETE" : "WORKER_ANALYSIS_REFUSED";result.diagnostic=std::move(bytes);
    }catch(const std::exception &e){result.diagnostic.clear();const std::string reason=e.what();result.reason=reason.rfind("WORKER_",0)==0 && reason.size()<=80 ? reason : "WORKER_PROTOCOL_OR_CALLBACK_FAILURE";}
    catch(...){result.diagnostic.clear();result.reason="WORKER_PROTOCOL_OR_CALLBACK_FAILURE";}
    return result;
}
}
