 #
 # CMakeLists.txt
 # Copyright (c) 2019-2021 Hewlett Packard Enterprise Development, LP. All rights reserved.
 # Redistribution and use in source and binary forms, with or without modification, are permitted provided
 # that the following conditions are met:
 # 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 # 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
 #    in the documentation and/or other materials provided with the distribution.
 # 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products
 #    derived from this software without specific prior written permission.
 #
 #    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 #    BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 #    SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 #    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 #    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 #    OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 #
 # See https://spdx.org/licenses/BSD-3-Clause
 #
 #
function (add_fam_test file_name)
    add_executable(${file_name} ${file_name}.cpp)
    target_link_libraries(${file_name} openfam pmix pmi2)
	add_test(NAME ${file_name} COMMAND python3 ${PROJECT_SOURCE_DIR}/run_test.py ${CMAKE_CURRENT_BINARY_DIR}/${file_name})
	IF (${CMAKE_MAJOR_VERSION} GREATER 2)
        set_tests_properties(${file_name} PROPERTIES SKIP_RETURN_CODE ${TEST_SKIP_STATUS})
    ENDIF()

endfunction()

#add tests
add_fam_test(fam_put_get)
add_fam_test(fam_initialize_test)
add_fam_test(fam_initialize_null_test)
add_fam_test(fam_options_test)
add_fam_test(fam_scatter_gather_index_user_desc)
add_fam_test(fam_scatter_gather_stride_user_desc)
add_fam_test(fam_put_get_user_desc)
add_fam_test(fam_scatter_gather_index_blocking)
add_fam_test(fam_scatter_gather_stride_blocking)
add_fam_test(fam_quiet_put_get_nonblocking)
add_fam_test(fam_progress)
add_fam_test(fam_scatter_gather_index_nonblocking)
add_fam_test(fam_scatter_gather_stride_nonblocking)
add_fam_test(fam_scatter_gather_stride_blocking_with_validation)
add_fam_test(fam_scatter_gather_stride_nonblocking_with_validation)
add_fam_test(fam_put_get_multiple)
add_fam_test(fam_invalid_offset_test)
add_fam_test(fam_noperm_test)
add_fam_test(fam_create_destroy_region_test)
add_fam_test(fam_create_destroy_region_test_mt)
if (${TEST_ENABLE_KNOWN_ISSUES} STREQUAL "yes")
    add_fam_test(fam_create_alloc_destroy_mt)
endif()
add_fam_test(fam_nonfetch_arithmatic_atomics_test)
add_fam_test(fam_nonfetch_logical_atomics_test)
add_fam_test(fam_nonfetch_min_max_atomics_test)
add_fam_test(fam_swap_test)
add_fam_test(fam_compare_swap_atomics_test)
add_fam_test(fam_fetch_arithmatic_atomics_test)
add_fam_test(fam_fetch_logical_atomics_test)
add_fam_test(fam_fetch_min_max_atomics_test)
add_fam_test(fam_copy_test)
if (${TEST_ENABLE_KNOWN_ISSUES} STREQUAL "yes")
    add_fam_test(fam_invalid_key_test)
    add_fam_test(fam_fence_test)
endif()
add_fam_test(fam_allocate_map_nvmm)
add_fam_test(fam_compare_swap_atomics_nvmm_test)
add_fam_test(fam_fetch_arithmatic_atomics_nvmm_test)
add_fam_test(fam_context_open_close)
add_fam_test(fam_progress_mt)
