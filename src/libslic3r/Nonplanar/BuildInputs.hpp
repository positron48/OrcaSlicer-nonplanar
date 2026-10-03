#pragma once
#include <memory>
#include <string>
#include <utility>

namespace Slic3r::nptop {
// The inventory compiled into this library. It binds observed source/resource
// bytes and declared build context, not every object, dependency or loaded plugin.
// Only the local factory may create it; imported resources cannot certify it.
struct CompiledBuildInputs {
    const std::string canonical_json,sha256;
    const size_t file_count;
private:
    CompiledBuildInputs(std::string json,std::string hash,size_t count)
        :canonical_json(std::move(json)),sha256(std::move(hash)),file_count(count){}
    friend std::shared_ptr<const CompiledBuildInputs> compiled_build_inputs();
};
std::shared_ptr<const CompiledBuildInputs> compiled_build_inputs();
}
