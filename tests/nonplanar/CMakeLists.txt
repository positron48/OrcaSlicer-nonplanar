add_executable(nonplanar_tests test_contracts.cpp test_collision.cpp test_profile_scene.cpp test_stl_metadata.cpp test_mesh_audit.cpp test_stl_import.cpp test_stl_import_error.cpp test_stl_worker.cpp test_stl_file.cpp test_mesh_placement.cpp)
add_executable(nonplanar_worker_probe worker_probe.cpp)
target_link_libraries(nonplanar_worker_probe Threads::Threads)
set_target_properties(nonplanar_worker_probe PROPERTIES MACOSX_BUNDLE FALSE)
add_dependencies(nonplanar_tests nonplanar_stl_audit nonplanar_worker_probe)
target_compile_definitions(nonplanar_tests PRIVATE
    NPTOP_WORKER_PATH="$<TARGET_FILE:nonplanar_import_worker>"
    NPTOP_PROBE_PATH="$<TARGET_FILE:nonplanar_worker_probe>"
    NPTOP_STL_FIXTURE_PATH="${CMAKE_SOURCE_DIR}/docs/nonplanar/fixtures/models/flat_block.stl")
target_link_libraries(nonplanar_tests test_common libslic3r Catch2::Catch2WithMain)
set_property(TARGET nonplanar_tests PROPERTY FOLDER "tests")
orcaslicer_copy_test_dlls()
orcaslicer_discover_tests(nonplanar_tests)
