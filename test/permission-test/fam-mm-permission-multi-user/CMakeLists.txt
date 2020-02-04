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
function (add_fam_exe file_name)
    add_executable(${file_name} ${file_name}.cpp)
    target_link_libraries(${file_name} openfam pmix pmi2)
endfunction()

function (add_fam_test_script script_file_name)
    add_custom_target( ${script_file_name} ALL
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${script_file_name}.sh ${CMAKE_CURRENT_BINARY_DIR}/
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${script_file_name}.sh)
    add_test(NAME ${script_file_name} COMMAND  ${PROJECT_SOURCE_DIR}/third-party/build/bin/mpirun --allow-run-as-root -np 1 ${CMAKE_CURRENT_BINARY_DIR}/${script_file_name}.sh)
endfunction()

#add executables
add_fam_exe(fam_create_region)
add_fam_exe(fam_destroy_region)
add_fam_exe(fam_allocate_data)
add_fam_exe(fam_deallocate_data)
add_fam_exe(fam_changepermission_region)
add_fam_exe(fam_changepermission_data)
add_fam_exe(fam_lookup_region)
add_fam_exe(fam_lookup_data)
add_fam_exe(fam_get_data)
add_fam_exe(fam_put_data)

#add testscripts
add_fam_test_script(t_region_perm_000)
add_fam_test_script(t_region_perm_400)
add_fam_test_script(t_region_perm_600)
add_fam_test_script(t_region_perm_440)
add_fam_test_script(t_region_perm_460)
add_fam_test_script(t_region_perm_666)
add_fam_test_script(t_data_perm_000)
add_fam_test_script(t_data_perm_440)
add_fam_test_script(t_data_perm_460)
add_fam_test_script(t_data_perm_666)
