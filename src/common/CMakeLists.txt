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

set(LIBOPENFAM_SRC
  ${LIBOPENFAM_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_context.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_libfabric.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_async_qhandler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_exception.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_internal_exception.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_config_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/atomic_queue.cpp
  PARENT_SCOPE
  )

set(CIS_SERVER_SRC
  ${CIS_SERVER_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_context.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_libfabric.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_async_qhandler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_exception.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_internal_exception.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_config_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/atomic_queue.cpp
  PARENT_SCOPE
)

set(MEMORY_SERVER_SRC
  ${MEMORY_SERVER_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_context.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_libfabric.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_async_qhandler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_exception.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_internal_exception.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_config_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/atomic_queue.cpp
  PARENT_SCOPE
  )

set(METADATA_CLIENT_SRC
  ${METADATA_CLIENT_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_exception.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_internal_exception.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_config_info.cpp
  PARENT_SCOPE
  )

set(METADATA_SERVER_SRC
  ${METADATA_SERVER_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_exception.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_internal_exception.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fam_config_info.cpp
  PARENT_SCOPE
  )

set(LIBOPENFAM_C_SRC
  ${LIBOPENFAM_C_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/c_api.cpp
  PARENT_SCOPE
  )

