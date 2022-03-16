 #
 # CMakeLists.txt
 # Copyright (c) 2022 Hewlett Packard Enterprise Development, LP. All rights reserved.
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

function (create_text_files)
    file(READ ${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_volatile.txt.in CONTENTS)
    file(WRITE ${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_volatile.txt "")
    file(APPEND ${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_volatile.txt "${CONTENTS}")
    file(APPEND ${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_volatile.txt "${PROJECT_SOURCE_DIR}/\n")
    file(APPEND ${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_volatile.txt "${PROJECT_SOURCE_DIR}/build/test/config_files/services-test/volatile-multi-mem/\n")
    file(APPEND ${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_volatile.txt "${PROJECT_SOURCE_DIR}/build/\n")
    install (FILES "${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_volatile.txt" PERMISSIONS OWNER_READ OWNER_WRITE
        OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        DESTINATION ${PROJECT_BINARY_DIR}/test/services-test/)
    file(READ ${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_persistent.txt.in CONTENTS)
    file(WRITE ${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_persistent.txt "")
    file(APPEND ${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_persistent.txt "${CONTENTS}")
    file(APPEND ${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_persistent.txt "${PROJECT_SOURCE_DIR}/\n")
    file(APPEND ${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_persistent.txt "${PROJECT_SOURCE_DIR}/build/test/config_files/services-test/persistent-multi-mem/\n")
    file(APPEND ${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_persistent.txt "${PROJECT_SOURCE_DIR}/build/\n")
    install (FILES "${PROJECT_SOURCE_DIR}/test/services-test/multi_mem_persistent.txt" PERMISSIONS OWNER_READ OWNER_WRITE
        OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        DESTINATION ${PROJECT_BINARY_DIR}/test/services-test/)
endfunction()

enable_testing()
include_directories(${PROJECT_BINARY_DIR}/test) # private headers (test)
add_custom_target(services-test ${CMAKE_CTEST_COMMAND} --force-new-ctest-process)

#add executables
add_fam_exe(test_fam_create_region)

#add testscripts
add_fam_test_script(region_create_volatile_persistent)
create_text_files()
