enable_testing()
include_directories(${PROJECT_BINARY_DIR}/test) # private headers (test)
add_custom_target(multi-memnode-test ${CMAKE_CTEST_COMMAND} --force-new-ctest-process)
function (add_fam_test file_name)
    add_executable(${file_name} ${file_name}.cpp)
    target_link_libraries(${file_name} openfam gtest)
    add_test(NAME ${file_name} COMMAND ${TEST_RUNTIME_BIN} ${TEST_RUNTIME_OPTS} ${CMAKE_CURRENT_BINARY_DIR}/${file_name})
endfunction()

#add tests
add_fam_test(fam_multi_memnode_tests)
add_fam_test(fam_datapath_for_random_regions)
add_fam_test(fam_microbenchmark_datapath_multiple_memnodes)
add_fam_test(fam_datapath_for_memnode_regions)
add_fam_test(fam_microbenchmark_atomic_multiple_memnodes)

