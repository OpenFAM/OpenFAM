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

add_subdirectory(memory_service)
add_subdirectory(fam-api)
add_subdirectory(allocator)
add_subdirectory(common)
add_subdirectory(memory-node)
add_subdirectory(metadata_service)
add_subdirectory(bitmap-manager)
add_subdirectory(cis)

add_library(fammetadata SHARED ${METADATA_CLIENT_SRC})
add_definitions(-DFAM_ATOMIC_NATIVE)

target_link_libraries(fammetadata yaml-cpp nvmm radixtree grpc grpc++ grpc++_reflection boost_system)

add_library(openfam SHARED ${LIBOPENFAM_SRC})

if(USE_BOOST_FIBER)
	target_link_libraries(openfam fabric fammetadata grpc grpc++ grpc++_reflection yaml-cpp nvmm boost_fiber boost_context pmix pmi2 fambitmap)
else()
	target_link_libraries(openfam fabric fammetadata grpc grpc++ grpc++_reflection yaml-cpp nvmm boost_context pmix pmi2 fambitmap)
endif()

set_target_properties(openfam PROPERTIES SOVERSION ${VERSION_STR})

add_executable(cis_server ${CIS_SERVER_SRC})

if(USE_BOOST_FIBER)
	target_link_libraries(cis_server fabric grpc grpc++ grpc++_reflection yaml-cpp nvmm boost_system boost_thread boost_fiber boost_context fambitmap radixtree)
else()
	target_link_libraries(cis_server fabric grpc grpc++ grpc++_reflection yaml-cpp nvmm boost_system boost_thread boost_context fambitmap radixtree)
endif()

add_executable (memory_server ${MEMORY_SERVER_SRC})

if(USE_BOOST_FIBER)
	target_link_libraries(memory_server fabric radixtree grpc grpc++ grpc++_reflection yaml-cpp nvmm boost_system boost_thread boost_fiber boost_context fambitmap)
else()
	target_link_libraries(memory_server fabric radixtree grpc grpc++ grpc++_reflection yaml-cpp nvmm boost_system boost_thread boost_context fambitmap)
endif()

add_executable (metadata_server ${METADATA_SERVER_SRC})

target_link_libraries(metadata_server yaml-cpp nvmm radixtree grpc grpc++ grpc++_reflection boost_system fambitmap)
