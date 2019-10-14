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


#compile .proto filr using protoc compiler
execute_process(COMMAND protoc --grpc_out=${CMAKE_CURRENT_SOURCE_DIR} --proto_path=${CMAKE_CURRENT_SOURCE_DIR} --plugin=protoc-gen-grpc=${CMAKE_CURRENT_SOURCE_DIR}/../../third-party/build/bin/grpc_cpp_plugin ${CMAKE_CURRENT_SOURCE_DIR}/fam_rpc.proto
                COMMAND  protoc --cpp_out=${CMAKE_CURRENT_SOURCE_DIR} --proto_path=${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/fam_rpc.proto)

#Add the .pb.cc and .grpc.pb.cc file path to LIBOPENFAM_SRC 
set(LIBOPENFAM_SRC
  ${LIBOPENFAM_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_rpc.grpc.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_rpc.pb.cc
  PARENT_SCOPE
  )

set(MEMORYSERVER_SRC
  ${MEMORYSERVER_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_rpc.grpc.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_rpc.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_rpc_service_impl.cpp
  PARENT_SCOPE
  )
