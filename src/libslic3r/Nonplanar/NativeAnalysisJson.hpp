#pragma once
#include "NativeAnalysis.hpp"
#include <nlohmann/json_fwd.hpp>

namespace Slic3r::nptop {
// Shared exact bounded mesh transport, preserving all original array order.
nlohmann::json native_mesh_document(const indexed_triangle_set &);
indexed_triangle_set parse_native_mesh_document(const nlohmann::json &, size_t max_faces=5000);
// Versioned editing transport, not a project replacement or proof credential.
// Strict registry/types, duplicate-key/depth/byte limits and exact float32
// reservation coordinates. All numerical checks remain in the native factories.
std::string native_analysis_document(const NativeAnalysisRequestSnapshot &);
std::shared_ptr<const NativeAnalysisRequestSnapshot> parse_native_analysis_document(const std::string &);
const char *native_analysis_stage_name(NativeAnalysisStage);
// Public blocked diagnostic and final-byte movement/rate replay. Never includes
// raw candidate G-code, secrets, or an export credential.
std::string native_analysis_diagnostic(const NativeAnalysisResult &);
}
