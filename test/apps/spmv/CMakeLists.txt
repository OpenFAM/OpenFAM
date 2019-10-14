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
add_executable(csr_perload csr_perload.cpp)
target_link_libraries(csr_perload openfam)
add_executable(fam_spmv fam_spmv.cpp)
target_link_libraries(fam_spmv openfam)
add_executable(fam_dump_dataitem fam_dump_dataitem.cpp)
target_link_libraries(fam_dump_dataitem openfam)
add_executable(fam_spmv_multi_atomic fam_spmv_multi_atomic.cpp)
target_link_libraries(fam_spmv_multi_atomic openfam)

include_directories(${PROJECT_BINARY_DIR}/test) # private headers (test)
install (FILES "${PROJECT_SOURCE_DIR}/test/apps/spmv/config_3_4_1" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE DESTINATION ${PROJECT_BINARY_DIR}/test/apps/spmv/)
install (DIRECTORY "${PROJECT_SOURCE_DIR}/test/apps/spmv/out_3_4_1" DESTINATION  ${PROJECT_BINARY_DIR}/test/apps/spmv)

