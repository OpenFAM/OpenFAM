 #
 # CMakeLists.txt
 # Copyright (c) 2019 Hewlett Packard Enterprise Development, LP. All rights reserved.
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
    target_link_libraries(${file_name} openfam gtest)
    add_test(NAME ${file_name} COMMAND ${TEST_RUNTIME_BIN} ${TEST_RUNTIME_OPTS} ${CMAKE_CURRENT_BINARY_DIR}/${file_name})
endfunction()

#add tests
add_fam_test(fam_initialize_reg_test)
add_fam_test(fam_options_reg_test)
if (${TEST_RUNTIME} STREQUAL "PMI2")
	add_fam_test(fam_runtime_pmi2_reg_test)
	add_fam_test(fam_runtime_pmi2_abort_reg_test)
elseif(${TEST_RUNTIME} STREQUAL "PMIX") 
	add_fam_test(fam_runtime_pmix_reg_test)
	add_fam_test(fam_runtime_pmix_abort_reg_test)
endif()
add_fam_test(fam_scatter_gather_index_user_desc_reg_test)
add_fam_test(fam_scatter_gather_stride_user_desc_reg_test)
add_fam_test(fam_put_get_user_desc_reg_test)
add_fam_test(fam_scatter_gather_index_blocking_reg_test)
add_fam_test(fam_scatter_gather_stride_blocking_reg_test)
add_fam_test(fam_put_get_reg_test)
add_fam_test(fam_put_get_quiet_nonblock_reg_test)
add_fam_test(fam_scatter_gather_index_nonblocking_reg_test)
add_fam_test(fam_scatter_gather_stride_nonblocking_reg_test)
add_fam_test(fam_noperm_reg_test)
add_fam_test(fam_invalid_offset_reg_test)
add_fam_test(fam_size_reg_test)
add_fam_test(fam_fetch_arithmatic_atomics_reg_test)
add_fam_test(fam_fetch_logical_atomics_reg_test)
add_fam_test(fam_fetch_min_max_atomics_reg_test)
add_fam_test(fam_compare_swap_atomics_reg_test)
add_fam_test(fam_swap_reg_test)
add_fam_test(fam_min_max_atomics_reg_test)
add_fam_test(fam_logical_atomic_reg_test)
add_fam_test(fam_cas_atomics_reg_test)
add_fam_test(fam_swap_atomics_reg_test)
add_fam_test(fam_arithmatic_atomics_reg_test)
add_fam_test(fam_atomics_reg_test)
add_fam_test(fam_nonfetch_arithmatic_atomics_reg_test)
add_fam_test(fam_nonfetch_logical_atomics_reg_test)
add_fam_test(fam_nonfetch_min_max_atomics_reg_test)
add_fam_test(fam_mm_reg_test)
#add_fam_test(fam_shm_tests)
add_fam_test(fam_copy_reg_test)
add_fam_test(fam_put_get_mt_reg_test)
add_fam_test(fam_scatter_gather_blocking_mt_reg_test)
# Following tests have been commented as there are some issues with non-blocking calls. 
# Will be uncommented after the issue has been addressed.
#add_fam_test(fam_arithmetic_atomics_mt_reg_test)
#add_fam_test(fam_cas_atomics_mt_reg_test)
#add_fam_test(fam_compare_swap_atomics_mt_reg_test)
#add_fam_test(fam_fetch_arithmetic_atomics_mt_reg_test)
#add_fam_test(fam_fetch_logical_atomics_mt_reg_test)
#add_fam_test(fam_fetch_min_max_atomics_mt_reg_test)
#add_fam_test(fam_min_max_atomics_mt_reg_test)
#add_fam_test(fam_nonfetch_arithmetic_atomics_mt_reg_test)
#add_fam_test(fam_nonfetch_logical_atomics_mt_reg_test)
#add_fam_test(fam_nonfetch_min_max_atomics_mt_reg_test)
#add_fam_test(fam_put_get_nb_mt_reg_test)
#add_fam_test(fam_scatter_gather_nonblocking_mt_reg_test)
#add_fam_test(fam_swap_atomics_mt_reg_test)


if (${TEST_ALLOCATOR} STREQUAL "grpc")
	add_fam_test(fam_put_get_negative_test)
	add_fam_test(fam_fence_reg_test)
	add_fam_test(fam_invalidkey_reg_test)
	add_fam_test(fam_barrier_reg_test)
endif()
#add_fam_test(fam_put_get)
#add_fam_test(fam_put_get_multiple)
#add_fam_test(fam_put_get_region_ctx)
#add_fam_test(fam_quiet_put_get_nonblocking)
#add_fam_test(fam_scatter_gather_index_nonblocking)
#add_fam_test(fam_scatter_gather_stride_nonblocking)
#add_fam_test(fam_scatter_gather_index_blocking)
#add_fam_test(fam_scatter_gather_stride_blocking)
#add_fam_test(fam_fence_test)
#add_fam_test(fam_allocate_map_nvmm)
#add_fam_test(fam_nonfetch_arithmatic_atomics_test)
#add_fam_test(fam_nonfetch_logical_atomics_test)
#add_fam_test(fam_nonfetch_min_max_atomics_test)
#add_fam_test(fam_swap_test)
#add_fam_test(fam_compare_swap_atomics_test)
#add_fam_test(fam_fetch_arithmatic_atomics_test)
#add_fam_test(fam_fetch_logical_atomics_test)
#add_fam_test(fam_fetch_min_max_atomics_test)
#add_fam_test(fam_invalid_key_test)
#add_fam_test(fam_invalid_offset_test)
#add_fam_test(fam_noperm_test)
