 #
 # CMakeLists.txt
 # Copyright (c) 2020-2022 Hewlett Packard Enterprise Development, LP. All rights reserved.
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

enable_testing()
include_directories(${PROJECT_BINARY_DIR}/test) # private headers (test)
add_custom_target(examples-api ${CMAKE_CTEST_COMMAND} --force-new-ctest-process)

function (add_fam_test file_name)
    add_executable(${file_name} ${file_name}.cpp)
    target_link_libraries(${file_name} openfam pmix pmi2)
    add_test(NAME ${file_name} COMMAND  ${PROJECT_SOURCE_DIR}/third-party/build/bin/mpirun --allow-run-as-root -np 1 ${CMAKE_CURRENT_BINARY_DIR}/${file_name})
endfunction()

#add tests
add_fam_test(api_fam_create_destroy_region)
add_fam_test(api_fam_allocate_deallocate)
add_fam_test(api_fam_get_option)
add_fam_test(api_fam_initialze)
add_fam_test(api_fam_list_options)
add_fam_test(api_fam_lookup)

add_fam_test(api_fam_add)
add_fam_test(api_fam_and)
add_fam_test(api_fam_change_permission)
add_fam_test(api_fam_compare_swap)
add_fam_test(api_fam_copy)
add_fam_test(api_fam_fence)
add_fam_test(api_fam_fetch_add)
add_fam_test(api_fam_fetch_and)
add_fam_test(api_fam_fetch)
add_fam_test(api_fam_fetch_max)
add_fam_test(api_fam_fetch_min)
add_fam_test(api_fam_fetch_or)
add_fam_test(api_fam_fetch_subtract)
add_fam_test(api_fam_fetch_xor)
add_fam_test(api_fam_gather)
add_fam_test(api_fam_get_blocking)
add_fam_test(api_fam_get_nonblocking)
add_fam_test(api_fam_map)
add_fam_test(api_fam_max)
add_fam_test(api_fam_min)
add_fam_test(api_fam_or)
add_fam_test(api_fam_put_blocking)
add_fam_test(api_fam_quiet)
add_fam_test(api_fam_progress)
add_fam_test(api_fam_scatter)
add_fam_test(api_fam_set)
add_fam_test(api_fam_subtract)
add_fam_test(api_fam_swap)
add_fam_test(api_fam_xor)
add_fam_test(api_fam_resize_region)
add_fam_test(api_fam_context)
add_fam_test(api_fam_backup)
add_fam_test(api_fam_restore_backup)
add_fam_test(api_fam_delete_backup)
