// Dedicated negative-test executable. Never packaged as the production worker.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#endif
int main(int argc,char **argv)
{
    if (std::strstr(argv[0],"hang")) std::this_thread::sleep_for(std::chrono::seconds(60));
    if (std::strstr(argv[0],"failure")) return 23;
    if (std::strstr(argv[0],"terminated")) {
#ifdef _WIN32
        TerminateProcess(GetCurrentProcess(),99);
#else
        std::raise(SIGKILL);
#endif
        return 99;
    }
    if (std::strstr(argv[0],"oversize")) {
        for (int i=0; i<8192; ++i) std::putchar('x');
        return 0;
    }
    if (std::strstr(argv[0],"malformed")) { std::puts("{}"); return 0; }
    if (argc!=3) return 64;
    auto revision=std::strtoull(argv[2],nullptr,10);
    if (std::strstr(argv[0],"revision")) ++revision;
    std::printf("{\"protocol\":1,\"revision\":%llu,\"source_sha256\":\"wrong\","
        "\"status\":\"VALID_GEOMETRY\",\"reason\":\"CLOSED_ORIENTED_SINGLE_COMPONENT\","
        "\"faces\":12,\"volume_lower_mm3\":1,\"volume_upper_mm3\":2,"
        "\"source_error_upper_mm\":0,\"peak_rss_bytes\":\"00000000000001000000\"}",revision);
}
