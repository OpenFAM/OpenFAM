 #
 # CMakeLists.txt
 # Copyright (c) 2019-2020,2023 Hewlett Packard Enterprise Development, LP. All rights reserved.
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
#compile .proto filr using protoc compiler
execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../third-party/build/bin/protoc --grpc_out=${CMAKE_CURRENT_SOURCE_DIR} --proto_path=${CMAKE_CURRENT_SOURCE_DIR} 
	--plugin=protoc-gen-grpc=${CMAKE_CURRENT_SOURCE_DIR}/../../third-party/build/bin/grpc_cpp_plugin ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_rpc.proto
                COMMAND  ${CMAKE_CURRENT_SOURCE_DIR}/../../third-party/build/bin/protoc --cpp_out=${CMAKE_CURRENT_SOURCE_DIR} --proto_path=${CMAKE_CURRENT_SOURCE_DIR}                       ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_rpc.proto)

if(ENABLE_THALLIUM)
	set(METADATA_THALLIUM_CLIENT ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_service_thallium_client.cpp)
	set(METADATA_THALLIUM_SERVER ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_service_thallium_server.cpp)
endif()

set(CIS_SERVER_SRC
  ${CIS_SERVER_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_rpc.grpc.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_rpc.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_service_direct.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_service_client.cpp
  ${METADATA_THALLIUM_CLIENT}
  PARENT_SCOPE
  )

set(MEMORY_SERVER_SRC
  ${MEMORY_SERVER_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_rpc.grpc.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_rpc.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_service_direct.cpp  
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_service_client.cpp
  ${METADATA_THALLIUM_CLIENT}
  PARENT_SCOPE
  )

set(METADATA_CLIENT_SRC
  ${METADATA_CLIENT_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_rpc.grpc.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_rpc.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_service_direct.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_service_client.cpp
  ${METADATA_THALLIUM_CLIENT}
  PARENT_SCOPE
  )

set(METADATA_SERVER_SRC
  ${METADATA_SERVER_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_rpc.grpc.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_rpc.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_service_direct.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_metadata_service_server.cpp
  ${METADATA_THALLIUM_SERVER}
  PARENT_SCOPE
  )
