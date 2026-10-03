#include <libslic3r/Nonplanar/NativeAnalysisWorker.hpp>
#include <boost/log/core.hpp>
#define NANOSVG_IMPLEMENTATION
#include "nanosvg/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg/nanosvgrast.h"
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
bool limits()
{
#ifdef _WIN32
    const HANDLE job=CreateJobObjectW(nullptr,nullptr);
    if(!job) return false;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION value{};
    value.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_PROCESS_TIME;
    value.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart=30LL*10000000;
    // Owned until _Exit. Parent supervises working set and output size.
    return SetInformationJobObject(job,JobObjectExtendedLimitInformation,&value,sizeof(value)) &&
        AssignProcessToJobObject(job,GetCurrentProcess());
#else
    const auto limit=[](int kind,rlim_t cap){rlimit value{};if(getrlimit(kind,&value)!=0)return false;
        value.rlim_cur=std::min(value.rlim_cur,cap);value.rlim_max=std::min(value.rlim_max,cap);return setrlimit(kind,&value)==0;};
    return limit(RLIMIT_CORE,0) && limit(RLIMIT_CPU,30) && limit(RLIMIT_FSIZE,native_analysis_worker_byte_limit);
#endif
}
uint64_t peak_rss()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS value{};
    return GetProcessMemoryInfo(GetCurrentProcess(),&value,sizeof(value)) ? value.PeakWorkingSetSize : 0;
#else
    rusage value{};if(getrusage(RUSAGE_SELF,&value)!=0 || value.ru_maxrss<=0)return 0;
    return uint64_t(value.ru_maxrss)
#ifndef __APPLE__
        *1024
#endif
        ;
#endif
}
bool write_all(int fd,const char *bytes,size_t size)
{
    while(size){
#ifdef _WIN32
        const auto n=::_write(fd,bytes,unsigned(size));
#else
        const auto n=::write(fd,bytes,size);
#endif
        if(n<=0)return false;bytes+=n;size-=size_t(n);
    }
    return true;
}
[[noreturn]] void emit(std::string bytes,uint64_t cap)
{
    // Only fixed-buffer formatting, syscalls and _Exit follow peak observation.
    const std::string marker="\"peak_rss_bytes\":\"00000000000000000000\"";
    const auto offset=bytes.find(marker);
    if(bytes.size()>native_analysis_worker_byte_limit || offset==std::string::npos)std::_Exit(70);
    const auto peak=peak_rss();if(!peak || peak>cap)std::_Exit(70);
    char digits[20];const auto encoded=std::to_chars(digits,digits+sizeof(digits),peak);
    if(encoded.ec!=std::errc{})std::_Exit(70);
    const size_t count=size_t(encoded.ptr-digits);
    std::memcpy(bytes.data()+offset+marker.size()-1-count,digits,count);
    std::_Exit(write_all(1,bytes.data(),bytes.size()) ? 0 : 70);
}
}
int main(int argc,char **argv)
{
#ifdef _WIN32
    _setmode(0,_O_BINARY);_setmode(1,_O_BINARY);_setmode(2,_O_BINARY);
#endif
    uint64_t cap=0;
    if(argc!=3 || std::strcmp(argv[1],"--resident-cap")!=0)return 64;
    const auto end=argv[2]+std::strlen(argv[2]);const auto parsed=std::from_chars(argv[2],end,cap);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end || !cap || cap>8ULL*1024*1024*1024)return 64;
    if(!limits())return 70;
    boost::log::core::get()->set_logging_enabled(false);
    try{
        std::string input;char block[8192];
        while(const auto size=std::fread(block,1,sizeof(block),stdin)){
            if(size>native_analysis_worker_byte_limit-input.size())return 65;input.append(block,size);
        }
        if(std::ferror(stdin))return 74;
        emit(execute_native_analysis_worker(input,[](NativeAnalysisStage stage){
            const char record=char('0'+unsigned(stage));if(!write_all(2,&record,1))std::_Exit(70);
        }),cap);
    }catch(...){return 70;}
}
