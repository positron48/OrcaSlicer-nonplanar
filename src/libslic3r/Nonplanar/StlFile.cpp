#include "StlFile.hpp"
#include <boost/nowide/convert.hpp>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Slic3r::nptop {
namespace {
constexpr size_t byte_limit=2*1024*1024;
#ifdef _WIN32
struct File {
    HANDLE handle=INVALID_HANDLE_VALUE;
    explicit File(const std::string &path) {
        handle=CreateFileW(boost::nowide::widen(path).c_str(),GENERIC_READ,FILE_SHARE_READ,
            nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
        if (handle==INVALID_HANDLE_VALUE) throw std::runtime_error("open");
    }
    ~File() { CloseHandle(handle); }
    File(const File &)=delete;
    File &operator=(const File &)=delete;
    BY_HANDLE_FILE_INFORMATION stamp() const {
        BY_HANDLE_FILE_INFORMATION value{};
        if (GetFileType(handle)!=FILE_TYPE_DISK || !GetFileInformationByHandle(handle,&value) ||
            value.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("not a regular file");
        return value;
    }
    size_t read(char *buffer, size_t capacity) const {
        DWORD size=0;
        if (!ReadFile(handle,buffer,DWORD(capacity),&size,nullptr)) throw std::runtime_error("read");
        return size;
    }
};
uint64_t size_of(const BY_HANDLE_FILE_INFORMATION &value)
{ return uint64_t(value.nFileSizeHigh)<<32 | value.nFileSizeLow; }
bool same(const FILETIME &a,const FILETIME &b)
{ return a.dwLowDateTime==b.dwLowDateTime && a.dwHighDateTime==b.dwHighDateTime; }
bool same(const BY_HANDLE_FILE_INFORMATION &a,const BY_HANDLE_FILE_INFORMATION &b)
{
    return a.dwVolumeSerialNumber==b.dwVolumeSerialNumber && a.nFileIndexHigh==b.nFileIndexHigh &&
        a.nFileIndexLow==b.nFileIndexLow && size_of(a)==size_of(b) &&
        same(a.ftLastWriteTime,b.ftLastWriteTime) && same(a.ftCreationTime,b.ftCreationTime);
}
auto path_stamp(const std::string &path) { return File(path).stamp(); }
#else
struct File {
    int descriptor=-1;
    explicit File(const std::string &path) {
        // Nonblocking rejects FIFO/device inputs without waiting for a writer;
        // the regular-file check runs before any read. Do not follow a symlink.
        descriptor=::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NONBLOCK|O_NOFOLLOW);
        if (descriptor<0) throw std::runtime_error("open");
    }
    ~File() { ::close(descriptor); }
    File(const File &)=delete;
    File &operator=(const File &)=delete;
    struct stat stamp() const {
        struct stat value{};
        if (::fstat(descriptor,&value)!=0 || !S_ISREG(value.st_mode) || value.st_size<0)
            throw std::runtime_error("not a regular file");
        return value;
    }
    size_t read(char *buffer, size_t capacity) const {
        const auto size=::read(descriptor,buffer,capacity);
        if (size<0) throw std::runtime_error("read");
        return size_t(size);
    }
};
uint64_t size_of(const struct stat &value) { return uint64_t(value.st_size); }
bool same(const timespec &a,const timespec &b) { return a.tv_sec==b.tv_sec && a.tv_nsec==b.tv_nsec; }
bool same(const struct stat &a,const struct stat &b)
{
    return a.st_dev==b.st_dev && a.st_ino==b.st_ino && a.st_size==b.st_size &&
#ifdef __APPLE__
        same(a.st_mtimespec,b.st_mtimespec) && same(a.st_ctimespec,b.st_ctimespec);
#else
        same(a.st_mtim,b.st_mtim) && same(a.st_ctim,b.st_ctim);
#endif
}
auto path_stamp(const std::string &path)
{
    struct stat value{};
    if (::lstat(path.c_str(),&value)!=0 || !S_ISREG(value.st_mode) || value.st_size<0)
        throw std::runtime_error("path changed");
    return value;
}
#endif
}
StlFileCapture capture_stl_file(const std::string &requested_path, bool millimeters_declared,
                                uint64_t revision, const StlFileOptions &requested_options)
{
    StlFileCapture result;
    result.path=requested_path; result.revision=revision;
    const StlFileOptions options=requested_options;
    const auto started=std::chrono::steady_clock::now();
    auto stop=[&] {
        if (options.cancelled && options.cancelled()) { result.reason="CANCELLED"; return true; }
        if (options.is_current && !options.is_current(revision)) { result.reason="STALE_REVISION"; return true; }
        if (std::chrono::steady_clock::now()-started>=options.timeout) { result.reason="SOURCE_DEADLINE"; return true; }
        return false;
    };
    if (result.path.empty() || result.path.size()>32768 || result.path.find('\0')!=std::string::npos ||
        revision==0 || options.timeout.count()<=0 || options.timeout>std::chrono::seconds(30)) {
        result.reason="INVALID_SOURCE_REQUEST"; return result;
    }
    if (!millimeters_declared) { result.reason="UNCONFIRMED_UNITS"; return result; }
    try {
        if (stop()) return result;
        File file(result.path);
        const auto before=file.stamp();
        if (size_of(before)>byte_limit) { result.reason="SOURCE_BYTE_LIMIT"; return result; }
        std::string bytes;
        bytes.reserve(size_t(size_of(before)));
        char block[8192];
        for (;;) {
            if (stop()) return result;
            const auto count=file.read(block,sizeof(block));
            if (count==0) break;
            if (count>byte_limit-bytes.size()) { result.reason="SOURCE_BYTE_LIMIT"; return result; }
            bytes.append(block,count);
        }
        if (stop()) return result;
        if (bytes.size()!=size_of(before) || !same(before,file.stamp()) || !same(before,path_stamp(result.path))) {
            result.reason="SOURCE_CHANGED_DURING_CAPTURE"; return result;
        }
        auto source=capture_stl_snapshot(bytes,true);
        if (stop()) return result;
        result.source=std::move(source); result.reason="SOURCE_CAPTURED";
    } catch (const std::exception &) {
        result.reason="SOURCE_IO_OR_CALLBACK_FAILURE";
    }
    return result;
}
}
