#
# CMakeLists.txt
# Copyright (c) 2019-2024 Hewlett Packard Enterprise Development, LP. All rights reserved.
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

# NOTE:
# - the shared libraries exposed herein are standalone, meaning that linking against them is
#   possible without having any undefined symbols
# TODO:
# - the CMake project visibility expressed by PRIVATE/PUBLIC for target_link_libraries
#   is likely not completely accurate: we are building auxiliary libraries (all except libopenfam)
#   which are not intended to be used directly by any consumer of OpenFAM, but they may
#   show up as targets and add OpenFAM-internal paths and wrapped dependencies to the include paths.
# - the openfam library currently also exports the libfabric header dir (amongst other things),
#   which is unnecessary as the public headers should not depend on it

function(add_rpc_obj target protofile)
  get_filename_component(protodir ${protofile} DIRECTORY)
  add_library(${target} OBJECT ${protofile})
  target_link_libraries(${target} PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf-lite
  )
  protobuf_generate(
    TARGET ${target}
    APPEND_PATH
  )
  protobuf_generate(
    TARGET ${target}
    LANGUAGE grpc
    PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    APPEND_PATH
  )
  set_property(TARGET ${target} PROPERTY POSITION_INDEPENDENT_CODE ON)
endfunction()

add_library(fambitmap SHARED bitmap-manager/bitmap.cpp)
target_include_directories(fambitmap PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>  # for building tests
)
target_link_libraries(fambitmap PRIVATE nvmm::nvmm)
set_target_properties(fambitmap PROPERTIES
  VERSION ${VERSION_STR}
  SOVERSION ${MAJORVER}
)

# some common sets of objects and their dependencies
add_library(famcommon SHARED
  common/fam_exception.cpp
  common/fam_internal_exception.cpp
  common/fam_config_info.cpp
  $<$<BOOL:${USE_THALLIUM}>:common/fam_thallium_engine_helper.cpp>
)
target_include_directories(famcommon PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)
target_link_libraries(famcommon PUBLIC
  ${YAML_CPP_LIBRARIES}
  $<$<BOOL:${USE_THALLIUM}>:thallium>
)
set_target_properties(famcommon PROPERTIES
  VERSION ${VERSION_STR}
  SOVERSION ${MAJORVER}
)

#
# Metadata RPC obj, client lib and server
add_rpc_obj(metadata_rpc_obj metadata_service/fam_metadata_rpc.proto)
add_library(fammetadata_client SHARED
  metadata_service/fam_metadata_service_direct.cpp
  metadata_service/fam_metadata_service_client.cpp
)
target_link_libraries(fammetadata_client
  PRIVATE
  metadata_rpc_obj
  PUBLIC
  famcommon
  fambitmap
  radixtree::radixtree
)
set_target_properties(fammetadata_client PROPERTIES
  VERSION ${VERSION_STR}
  SOVERSION ${MAJORVER}
)
add_executable(metadata_server
  metadata_service/fam_metadata_service_server.cpp
  metadata_service/metadata_server_main.cpp
  $<$<BOOL:${ENABLE_THALLIUM}>:metadata_service/fam_metadata_service_thallium_server.cpp>
)
target_include_directories(metadata_server PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(metadata_server
  famcommon
  fambitmap
  fammetadata_client  # server would not need the _service_client.cpp, but avoids yet another lib
  gRPC::grpc++
  $<$<BOOL:${USE_THALLIUM}>:thallium>
)

# an extended set of common libraries, but only used by famresmgr and following
add_library(ext_common_objs OBJECT
  common/fam_context.cpp
  common/fam_libfabric.cpp
  common/fam_async_qhandler.cpp
  common/atomic_queue.cpp
  allocator/memserver_allocator.cpp  # for atomic_queue
)
target_include_directories(ext_common_objs PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  # async_qhandler needs access to memory_service headers:
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(ext_common_objs PUBLIC
  famcommon
  radixtree::radixtree
  libfabric::libfabric
  Boost::atomic  # async_qhandler
  Boost::thread  # async_qhandler
  $<$<BOOL:${USE_BOOST_FIBER}>:Boost::fiber>  # async_qhandler
)
set_property(TARGET ext_common_objs PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(famresmgr SHARED
  memory_service/fam_server_resource_manager.cpp
  fam-api/fam_ops_libfabric.cpp  # from resource_manager
  allocator/fam_allocator_client.cpp  # from ops_libfabric
  allocator/fam_client_resource_manager.cpp  # from allocator_client
  fam-api/fam_descriptor.cpp  # from allocator_client
  cis/fam_cis_client.cpp  # from allocator_client
  cis/fam_cis_direct.cpp  # from allocator_client
  memory_service/fam_memory_service_direct.cpp
  memory_service/fam_memory_service_client.cpp  # from fam_cis_direct
)
target_link_libraries(famresmgr
  PRIVATE
  memory_rpc_obj
  cis_rpc_obj  # cis_client
  ext_common_objs
  PUBLIC
  fambitmap
  fammetadata_client   # from fam_cis_direct
  nvmm::nvmm
  radixtree::radixtree
  libfabric::libfabric
)
target_include_directories(famresmgr PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  # when building against the tree, add the current source dir for includes,
  # building against the installed variant of famresmgr is not supported
)

#
# Memory RPC obj and server
add_rpc_obj(memory_rpc_obj memory_service/fam_memory_service_rpc.proto)
add_executable(memory_server
  memory_service/fam_memory_service_server.cpp
  memory_service/memory_server_main.cpp
)
target_link_libraries(memory_server PRIVATE
  memory_rpc_obj
  famresmgr
  $<$<BOOL:${USE_THALLIUM}>:thallium>
)

#
# CIS RPC obj and server
add_rpc_obj(cis_rpc_obj cis/fam_cis_rpc.proto)
add_executable(cis_server
  cis/fam_cis_direct.cpp
  cis/fam_cis_server.cpp
  cis/cis_server_main.cpp
  $<$<BOOL:${ENABLE_THALLIUM}>:cis/fam_cis_thallium_server.cpp>
)
target_link_libraries(cis_server PRIVATE
  cis_rpc_obj
  famresmgr
  $<$<BOOL:${USE_THALLIUM}>:thallium>
)

#
# OpenFAM client libraries
add_library(openfam SHARED
  fam-api/fam.cpp
  fam-api/fam_ops_shm.cpp
)
target_link_libraries(openfam
  PRIVATE
  $<$<BOOL:${PMIx_FOUND}>:PMIx::libpmix>
  $<$<BOOL:${PMI2_FOUND}>:PMI2::libpmi2>
  $<$<BOOL:${USE_THALLIUM}>:thallium>
  PUBLIC
  famcommon
  famresmgr
)
target_compile_definitions(openfam PRIVATE
  $<$<BOOL:${PMIx_FOUND}>:FAM_HAVE_PMIx>
  $<$<BOOL:${PMI2_FOUND}>:FAM_HAVE_PMI2>
)
target_include_directories(openfam PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(openfam PROPERTIES
  VERSION ${VERSION_STR}
  SOVERSION ${MAJORVER}
)

add_library(openfam_c SHARED common/c_api.cpp)
target_include_directories(openfam_c PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(openfam_c PRIVATE
  openfam
)
set_target_properties(openfam_c PROPERTIES
  VERSION ${VERSION_STR}
  SOVERSION ${MAJORVER}
)
