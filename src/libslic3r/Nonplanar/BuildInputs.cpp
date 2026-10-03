#include "BuildInputs.hpp"
#include "Contracts.hpp"
#include "StlImport.hpp"
#include "NonplanarBuildInputsData.hpp"
#include <nlohmann/json.hpp>

namespace Slic3r::nptop {
std::shared_ptr<const CompiledBuildInputs> compiled_build_inputs()
{
    static const auto snapshot=[] {
        std::string bytes;
        for(const auto *part:detail::build_input_parts)bytes+=part;
        require(bytes.size()<=16*1024*1024,"BUILD_INPUT_BYTE_LIMIT");
        require(sha256_bytes(bytes)==detail::build_inputs_sha256,"BUILD_INPUT_HASH_MISMATCH");
        const auto value=nlohmann::json::parse(bytes);
        require(value.dump()==bytes,"BUILD_INPUT_CANONICAL_JSON");
        require(value.is_object() && value.size()==4 && value.at("schema")==1 &&
            value.at("scope")=="build-input-inventory" && value.at("build").is_object(),"BUILD_INPUT_SCHEMA");
        const auto &files=value.at("files");require(files.is_array() && !files.empty() && files.size()<=100000,"BUILD_INPUT_FILE_LIMIT");
        std::string previous;
        for(const auto &file:files){
            require(file.is_array() && file.size()==2 && file[0].is_string() && file[1].is_string(),"BUILD_INPUT_FILE_SCHEMA");
            const auto path=file[0].get<std::string>(),hash=file[1].get<std::string>();
            require(!path.empty() && path.size()<=4096 && path>previous && path.front()!='/' && path.find('\\')==std::string::npos &&
                path.find("../")==std::string::npos && hash.size()==64 && hash.find_first_not_of("0123456789abcdef")==std::string::npos,"BUILD_INPUT_FILE_IDENTITY");
            previous=path;
        }
        return std::shared_ptr<const CompiledBuildInputs>(new CompiledBuildInputs(std::move(bytes),detail::build_inputs_sha256,files.size()));
    }();
    return snapshot;
}
}
