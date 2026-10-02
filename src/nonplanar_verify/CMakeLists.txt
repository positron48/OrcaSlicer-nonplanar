add_library(nonplanar_final_rates STATIC LinearRates.cpp)
target_link_libraries(nonplanar_final_rates PUBLIC boost_headeronly)
target_include_directories(nonplanar_final_rates PUBLIC ${CMAKE_SOURCE_DIR}/src)
set_target_properties(nonplanar_final_rates PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_source_files_properties(LinearRates.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
if(MSVC)
    target_compile_options(nonplanar_final_rates PRIVATE /fp:strict)
else()
    target_compile_options(nonplanar_final_rates PRIVATE -fno-fast-math -ffp-contract=off)
endif()

# The adapter maps an owned source; the verifier library has no planner dependency.
target_link_libraries(libslic3r PRIVATE nonplanar_final_rates)

add_executable(nonplanar_rate_audit audit.cpp)
target_link_libraries(nonplanar_rate_audit PRIVATE nonplanar_final_rates nlohmann_json)
target_include_directories(nonplanar_rate_audit SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/deps_src)
set_target_properties(nonplanar_rate_audit PROPERTIES MACOSX_BUNDLE FALSE)
