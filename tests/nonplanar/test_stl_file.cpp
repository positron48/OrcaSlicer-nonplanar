#include <catch2/catch_test_macros.hpp>
#include <libslic3r/Nonplanar/StlFile.hpp>
#include <libslic3r/Nonplanar/StlWorker.hpp>
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <thread>
#ifndef _WIN32
#include <sys/stat.h>
#endif

using namespace Slic3r::nptop;
namespace {
namespace fs=boost::filesystem;
struct Workspace {
    fs::path path=fs::temp_directory_path()/fs::unique_path("nptop-file-%%%%-%%%%-%%%%");
    Workspace() { REQUIRE(fs::create_directory(path)); fs::permissions(path,fs::owner_all); }
    ~Workspace() { boost::system::error_code ec; fs::remove_all(path,ec); }
    std::string file(const std::string &name) const { return (path/name).string(); }
};
void write_file(const std::string &path, const std::string &bytes)
{
    boost::nowide::ofstream output(path,std::ios::binary|std::ios::trunc);
    output.write(bytes.data(),std::streamsize(bytes.size())); output.close();
    REQUIRE(output);
}
std::string read_file(const std::string &path)
{
    boost::nowide::ifstream input(path,std::ios::binary);
    REQUIRE(input);
    return std::string(std::istreambuf_iterator<char>(input),{});
}
}
TEST_CASE("B02 source file capture preserves bytes and analysis does not reopen a replaced pathname", "[Nonplanar][B02][SourceFile]")
{
    Workspace workspace;
    const auto path=workspace.file("исходник.stl");
    const auto bytes=read_file(NPTOP_STL_FIXTURE_PATH);
    write_file(path,bytes);
    auto caller_path=path;
    StlFileOptions options;
    options.cancelled=[&] { caller_path=workspace.file("missing.stl"); return false; };
    options.is_current=[](uint64_t revision) { return revision==67; };
    const auto capture=capture_stl_file(caller_path,true,67,options);
    INFO(capture.reason);
    REQUIRE(capture.source);
    REQUIRE(capture.path==path);
    REQUIRE(capture.revision==67);
    REQUIRE(capture.source->bytes==bytes);
    REQUIRE(capture.source->sha256=="2006064a6ca57fe1da6c1f6a9ae6a0c8abbc4d543de38379f073c47890730b46");
    REQUIRE(read_file(path)==bytes);
    write_file(path,"replaced after capture");
    const auto result=run_stl_worker(NPTOP_WORKER_PATH,capture.source->bytes,true,capture.revision);
    REQUIRE(result.status==MeshAuditStatus::ValidGeometry);
    REQUIRE(result.source->sha256==capture.source->sha256);
    REQUIRE(read_file(path)=="replaced after capture");
}
TEST_CASE("B02 file capture refuses special paths missing units and oversized input", "[Nonplanar][B02][SourceFile]")
{
    Workspace workspace;
    const auto path=workspace.file("source.stl");
    write_file(path,"abc");
    REQUIRE(capture_stl_file(path,false,1).reason=="UNCONFIRMED_UNITS");
    REQUIRE(capture_stl_file(path,true,0).reason=="INVALID_SOURCE_REQUEST");
    REQUIRE(capture_stl_file(path+std::string("\0suffix",7),true,1).reason=="INVALID_SOURCE_REQUEST");
    REQUIRE_FALSE(capture_stl_file(workspace.file("missing"),true,1).source);
    REQUIRE_FALSE(capture_stl_file(workspace.path.string(),true,1).source);
#ifndef _WIN32
    const auto link=workspace.file("symlink");
    fs::create_symlink(path,link);
    REQUIRE_FALSE(capture_stl_file(link,true,1).source);
    const auto fifo=workspace.file("fifo");
    REQUIRE(::mkfifo(fifo.c_str(),0600)==0);
    const auto started=std::chrono::steady_clock::now();
    REQUIRE_FALSE(capture_stl_file(fifo,true,1).source);
    REQUIRE(std::chrono::steady_clock::now()-started<std::chrono::seconds(1));
#endif
    write_file(path,std::string(2*1024*1024+1,'x'));
    const auto result=capture_stl_file(path,true,1);
    REQUIRE(result.reason=="SOURCE_BYTE_LIMIT");
    REQUIRE_FALSE(result.source);
}
TEST_CASE("B02 source capture cancellation staleness deadline and callback failure discard partial bytes", "[Nonplanar][B02][SourceFile]")
{
    Workspace workspace;
    const auto path=workspace.file("source.stl");
    write_file(path,std::string(20000,'x'));
    StlFileOptions options;
    int polls=0;
    options.cancelled=[&] { return ++polls>=3; };
    const auto cancelled=capture_stl_file(path,true,1,options);
    REQUIRE(cancelled.reason=="CANCELLED"); REQUIRE_FALSE(cancelled.source);
    options={}; polls=0;
    options.is_current=[&](uint64_t) { return ++polls<3; };
    const auto stale=capture_stl_file(path,true,1,options);
    REQUIRE(stale.reason=="STALE_REVISION"); REQUIRE_FALSE(stale.source);
    options={}; options.timeout=std::chrono::milliseconds(1);
    options.cancelled=[] { std::this_thread::sleep_for(std::chrono::milliseconds(5)); return false; };
    const auto late=capture_stl_file(path,true,1,options);
    REQUIRE(late.reason=="SOURCE_DEADLINE"); REQUIRE_FALSE(late.source);
    options={}; polls=0;
    options.cancelled=[&]() -> bool { if (++polls>=3) throw std::runtime_error("cancel callback"); return false; };
    REQUIRE_FALSE(capture_stl_file(path,true,1,options).source);
}
TEST_CASE("B02 file capture rejects observed mutation and pathname replacement during reading", "[Nonplanar][B02][SourceFile]")
{
    Workspace workspace;
    const auto path=workspace.file("source.stl");
    const std::string original(20000,'x');
    write_file(path,original);
    StlFileOptions options;
    int polls=0;
#ifdef _WIN32
    bool write_blocked=false;
    options.cancelled=[&] {
        if (++polls==3) {
            boost::nowide::ofstream output(path,std::ios::binary|std::ios::trunc);
            write_blocked=!output;
        }
        return false;
    };
    const auto result=capture_stl_file(path,true,1,options);
    REQUIRE(write_blocked);
    REQUIRE(result.source);
    REQUIRE(result.source->bytes==original);
#else
    options.cancelled=[&] { if (++polls==3) write_file(path,std::string(20000,'y')); return false; };
    const auto changed=capture_stl_file(path,true,1,options);
    REQUIRE(changed.reason=="SOURCE_CHANGED_DURING_CAPTURE"); REQUIRE_FALSE(changed.source);
    write_file(path,original); polls=0;
    const auto replacement=workspace.file("replacement.stl");
    write_file(replacement,original);
    options.cancelled=[&] { if (++polls==3) fs::rename(replacement,path); return false; };
    const auto replaced=capture_stl_file(path,true,1,options);
    REQUIRE(replaced.reason=="SOURCE_CHANGED_DURING_CAPTURE"); REQUIRE_FALSE(replaced.source);
#endif
}
TEST_CASE("B02 file capture freezes timeout options and rechecks revision after hashing", "[Nonplanar][B02][SourceFile]")
{
    Workspace workspace;
    const auto path=workspace.file("source.stl");
    write_file(path,"abc");
    StlFileOptions options;
    options.timeout=std::chrono::milliseconds(1);
    options.cancelled=[&] {
        options.timeout=std::chrono::seconds(30);
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); return false;
    };
    REQUIRE(capture_stl_file(path,true,1,options).reason=="SOURCE_DEADLINE");
    options={}; int polls=0;
    // start, before read, before EOF, after read, after hash
    options.is_current=[&](uint64_t) { return ++polls<5; };
    const auto late=capture_stl_file(path,true,1,options);
    REQUIRE(polls==5);
    REQUIRE(late.reason=="STALE_REVISION"); REQUIRE_FALSE(late.source);
}
