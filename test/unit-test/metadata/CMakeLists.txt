 #
 # CMakeLists.txt
 # Copyright (c) 2019-2020 Hewlett Packard Enterprise Development, LP. All rights reserved.
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
    target_link_libraries(${file_name} fammetadata pmix pmi2 openfam gtest)
    
    add_test(NAME ${file_name} COMMAND ${CMAKE_COMMAND} -DFILE_NAME=${file_name} -P ${PROJECT_SOURCE_DIR}/run_test.cmake)
    IF (${CMAKE_MAJOR_VERSION} GREATER 2)
        set_tests_properties(${file_name} PROPERTIES SKIP_RETURN_CODE ${TEST_SKIP_STATUS})
    ENDIF()
    
	add_definitions(-DFAM_ATOMIC_NATIVE)

endfunction()

#add tests
#add_fam_test(fam_metadata_test)
add_fam_test(fam_metadata_rpc_test)
#add_fam_test(fam_metadata_regress_test)
