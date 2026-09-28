// Dedicated negative-test executable. Never packaged as the production worker.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <string>
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
    if (argc!=3 && argc!=4) return 64;
    const auto scenario=[&](const char *name) { return std::strstr(argv[0],name)!=nullptr; };
    auto revision=std::strtoull(argv[2],nullptr,10);
    if (scenario("revision")) ++revision;
    const bool upper=argc==4;
    std::string summary=R"({"upward_faces":2,"selected_faces":2,"patches":1,"holes":0,"creases":0,"affine_patches":1,"slope_limit":0.2,"area_lower_mm2":1,"area_upper_mm2":1,"selected_area_lower_mm2":1,"selected_area_upper_mm2":1,"minimum_z_mm":1,"maximum_z_mm":1,"selected_slope_upper":0})";
    const auto replace=[&](const std::string &from, const std::string &to) {
        summary.replace(summary.find(from),from.size(),to);
    };
    if (scenario("upper-count")) replace("\"selected_faces\":2","\"selected_faces\":3");
    if (scenario("upper-type")) replace("\"upward_faces\":2","\"upward_faces\":2.0");
    if (scenario("upper-area")) replace("\"selected_area_upper_mm2\":1","\"selected_area_upper_mm2\":2");
    if (scenario("upper-range")) replace("\"minimum_z_mm\":1","\"minimum_z_mm\":2");
    if (scenario("upper-affine")) replace("\"creases\":0","\"creases\":1");
    if (scenario("upper-slope")) replace("\"slope_limit\":0.2","\"slope_limit\":0.3");
    if (scenario("upper-field")) replace("\"holes\":0","\"missing_holes\":0");
    std::string analysis="null";
    if (upper) analysis=R"({"status":"NOMINAL_HEIGHTFIELD","reason":"NOMINAL_UPPER_PROJECTION_ONLY","summary":)"+summary+"}";
    if (scenario("upper-null")) analysis="null";
    if (scenario("upper-unknown")) analysis=R"({"status":"UNKNOWN","reason":"OVERLAPPING_UPWARD_PROJECTIONS"})";
    const char *hash=scenario("upper-") ? "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" : "wrong";
    std::printf(R"({"protocol":%u,"revision":%llu,"source_sha256":"%s","analysis":"%s","upper":%s,"status":"VALID_GEOMETRY","reason":"CLOSED_ORIENTED_SINGLE_COMPONENT","faces":12,"volume_lower_mm3":1,"volume_upper_mm3":2,"source_error_upper_mm":0,"peak_rss_bytes":"00000000000001000000"})",
        scenario("upper-old") ? 1u : 2u,revision,hash,upper && !scenario("upper-mode") ? "upper" : "geometry",analysis.c_str());
}
