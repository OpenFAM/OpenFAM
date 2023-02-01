 #
 # CMakeLists.txt
 # Copyright (c) 2019-2023 Hewlett Packard Enterprise Development, LP. All rights reserved.
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

 # Setting LD_LIBRARY_PATH here for the benefit of protoc, protoc command we are using is dependent on libgrpc_plugin_support.so
 # Adding the path of so files into LD_LIBRARY_PATH. 
 # In OpenFAM 2.0, this was not needed as shared libraries where not build for libgrpc_plugin_support.
 # TODO: Get rid of setting LD_LIBRARY_PATH. Check if we can remove building shared library for libgrpc_plugin_support.
set(ENV{LD_LIBRARY_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/build/lib:${CMAKE_CURRENT_SOURCE_DIR}/../third-party/build/lib64:$ENV{LD_LIBRARY_PATH}")

add_subdirectory(memory_service)
add_subdirectory(fam-api)
add_subdirectory(allocator)
add_subdirectory(common)
add_subdirectory(memory-node)
add_subdirectory(metadata_service)
add_subdirectory(bitmap-manager)
add_subdirectory(cis)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(fammetadata SHARED ${METADATA_CLIENT_SRC})
add_definitions(-DFAM_ATOMIC_NATIVE)

target_link_libraries(fammetadata yaml-cpp nvmm radixtree grpc grpc++ grpc++_reflection boost_system grpc_plugin_support gpr protobuf)

add_library(openfam SHARED ${LIBOPENFAM_SRC})

if(USE_BOOST_FIBER)
	target_link_libraries(openfam fabric fammetadata grpc grpc++ grpc++_reflection gpr yaml-cpp nvmm boost_fiber boost_context pmix pmi2 fambitmap)
else()
	target_link_libraries(openfam fabric fammetadata grpc grpc++ grpc++_reflection gpr yaml-cpp nvmm boost_context pmix pmi2 fambitmap)
endif()


string(FIND ${VERSION_STR} "." dotloc)
string(SUBSTRING ${VERSION_STR} 0 ${dotloc} MAJORVER)
set_target_properties(openfam PROPERTIES 
	VERSION ${VERSION_STR}
	SOVERSION ${MAJORVER})

add_executable(cis_server ${CIS_SERVER_SRC})

if(USE_BOOST_FIBER)
	target_link_libraries(cis_server fabric grpc grpc++ grpc++_reflection gpr protobuf yaml-cpp nvmm boost_system boost_thread boost_fiber boost_context fambitmap radixtree)
else()
	target_link_libraries(cis_server fabric grpc grpc++ grpc++_reflection gpr protobuf yaml-cpp nvmm boost_system boost_thread boost_context fambitmap radixtree)
endif()

add_executable (memory_server ${MEMORY_SERVER_SRC})

if(USE_BOOST_FIBER)
	target_link_libraries(memory_server fabric radixtree grpc grpc++ grpc++_reflection gpr protobuf yaml-cpp nvmm boost_system boost_thread boost_fiber boost_context fambitmap)
else()
	target_link_libraries(memory_server fabric radixtree grpc grpc++ grpc++_reflection gpr protobuf yaml-cpp nvmm boost_system boost_thread boost_context fambitmap)
endif()

add_executable (metadata_server ${METADATA_SERVER_SRC})

target_link_libraries(metadata_server yaml-cpp nvmm radixtree grpc grpc++ grpc++_reflection gpr protobuf boost_system fambitmap)

add_library(openfam_c SHARED ${LIBOPENFAM_C_SRC})

target_link_libraries(openfam_c openfam)
