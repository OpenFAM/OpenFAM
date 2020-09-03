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

cmake_minimum_required(VERSION 2.8.11)
project(OpenFAM)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

#
# build type: Release (default) or Debug
#
if(CMAKE_BUILD_TYPE MATCHES Debug)
  message(STATUS "Build type: Debug")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
  message(STATUS "Build type: Release")
elseif(CMAKE_BUILD_TYPE MATCHES Coverage)
  message(STATUS "Build type: Coverage")
else()
  message(STATUS "Invalid build type: ${CMAKE_BUILD_TYPE}")
  message(STATUS "Default to : Release")
  set(CMAKE_BUILD_TYPE "Release")
endif()

#
# add any other dependency checks
#

#
# enable profiling
#
if(ENABLE_FAM_PROFILING)
  message(STATUS "FAM profiling enabled")
  add_definitions(-DFAM_PROFILE)
endif()

if(ENABLE_LIBFABRIC_PROFILING)
        message(STATUS "LIBFABRIC profiling enabled")
        add_definitions(-DLIBFABRIC_PROFILE)
endif()

if(ENABLE_MEMORYSERVER_PROFILING)
        message(STATUS "Memory Server profile enabled")
        add_definitions(-DMEMSERVER_PROFILE)
endif()

message(STATUS "====================")

#
# common gcc flags
#
# To Be Fixed :
# -Wcast-qual : grpc headers throw up these warning during build
# of gRPC generated files
set(CMAKE_CXX_FLAGS "-std=c++11 -pthread")
set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -Wall -W -Wextra -Wredundant-decls -Wunused -Wunused-macros -Wno-unused-parameter -Wcast-align -Wwrite-strings -Wmissing-field-initializers -Wendif-labels -Winit-self -Wlogical-op -Wpacked -Wstack-protector -Wformat=2 -Wswitch-enum -Wstrict-overflow=5 -Wpointer-arith -Wnormalized=nfc -Wno-long-long -Wconversion -Wunreachable-code")
# Convert all warnings to errors
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")


#
# gcc flags for release build
#
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -g -O2")

#
# gcc flags for debug build
#
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -DDEBUG -O0 -g")

#
# gcc flags for debug build
#
set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS} -DDEBUG -DCOVERAGE -Og -g -fprofile-arcs -ftest-coverage")

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

#
# third-party libraries
#
set(GTEST_ROOT ${PROJECT_SOURCE_DIR}/third-party/googletest)

add_subdirectory(third-party)
include_directories(${PROJECT_SOURCE_DIR}/third-party/googletest/include)

#
# include directories
#
include_directories(${PROJECT_SOURCE_DIR}/include) # public headers (include)
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/third-party/build/include) # third-party headers (include)
include_directories(${PROJECT_SOURCE_DIR}/test/util) # utility functions
include_directories(${PROJECT_SOURCE_DIR}/src) # private headers (src)
include_directories(${PROJECT_SOURCE_DIR}/src/pmi) # private headers (src)
#
# library directories
#
link_directories(${PROJECT_SOURCE_DIR}/third-party/build/lib) # third-party libraries

#
# default system include and link directories
#
link_directories(/usr/lib)
include_directories(/usr/include)
link_directories(/usr/local/lib)
include_directories(/usr/local/include)

#
# Set openFAM options to be used only by test. Some individual test cases might override these options.
#

set( TEST_DEFAULT_REGION "Default" )
set( TEST_OPENFAM_MODEL "memory_server" )
set( TEST_CIS_INTERFACE_TYPE "rpc" )
set( TEST_CIS_SERVER "127.0.0.1" )
set( TEST_GRPC_PORT "8787" )
set( TEST_LIBFABRIC_PROVIDER "sockets" )
set( TEST_FAM_THREAD_MODEL "FAM_THREAD_SERIALIZE" )
set( TEST_FAM_CONTEXT_MODEL "FAM_CONTEXT_DEFAULT" )
set( TEST_RUNTIME "PMIX" )
set( TEST_NUM_CONSUMER "1" )

if (ARG_OPENFAM_MODEL)
        set( TEST_OPENFAM_MODEL ${ARG_OPENFAM_MODEL})
endif()

if (${TEST_OPENFAM_MODEL} STREQUAL "shared_memory")
        message(STATUS "Building shared memory model")
        add_definitions(-DSHM)
endif()

add_definitions(-DSERVER_LIST="0:127.0.0.1")
add_definitions(-DMEMSERVER_LIBFABRIC_PORT="7500")
add_definitions(-DMEMSERVER_LIBFABRIC_PROVIDER="sockets")
add_definitions(-DMEMSERVER_ADDR="127.0.0.1")

if (ENABLE_METADATA_SERVICE_RPC)
	if(${TEST_OPENFAM_MODEL} STREQUAL "shared_memory")
		message(STATUS "Error : Can not enable RPC in shared memory model")
	else()
        message(STATUS "Using Metadata service RPC interface")
		add_definitions(-DMETADATA_SERVICE_RPC)
		add_definitions(-DMETASERVER_RPC_PORT=8788)
	endif()
endif()

if (ENABLE_MEMORY_SERVICE_RPC)
	if(${TEST_OPENFAM_MODEL} STREQUAL "shared_memory")
        message(STATUS "Error : Can not enable RPC in shared memory model")
    else()
        message(STATUS "Using memory service RPC interface")
		add_definitions(-DMEMORY_SERVICE_RPC)
		add_definitions(-DMEMSERVER_RPC_PORT=8789)
	endif()
endif()

if (USE_FAM_PERSIST)
        message(STATUS "Using FAM persist")
        add_definitions(-DUSE_FAM_PERSIST)
endif()

if (USE_FAM_INVALIDATE)
        message(STATUS "Using FAM invalidate")
        add_definitions(-DUSE_FAM_INVALIDATE)
endif()

if (USE_BOOST_FIBER)
        message(STATUS "Using Boost Fiber")
        add_definitions(-DUSE_BOOST_FIBER)
endif()

# Use native atomics
add_definitions(-DFAM_ATOMIC_NATIVE)

# Runtime options to be used to invoke test cases
set( TEST_NPE 1 ) #Number of pe used with mpirun
set( TEST_RUNTIME_BIN ${PROJECT_SOURCE_DIR}/third-party/build/bin/mpirun ) # path to mpirun/prun
set( TEST_RUNTIME_OPTS --allow-run-as-root -n ${TEST_NPE} ) # path to mpirun/prun

# Include fam_test_config.h in individual test cases and use the above variables as macros.
configure_file( ${PROJECT_SOURCE_DIR}/test/common/fam_test_config.h.in ${PROJECT_BINARY_DIR}/test/common/fam_test_config.h )

# Read version number from VERSION file
file(STRINGS "VERSION" VERSION_STR)
add_definitions(-DOPENFAM_VERSION="${VERSION_STR}")

#
# add source code
#
add_subdirectory(src)
add_subdirectory(test/unit-test)
add_subdirectory(test/reg-test)
add_subdirectory(test/multi-memnode-test)
add_subdirectory(test/maxconfig)
add_subdirectory(test/coverage)
add_subdirectory(test/microbench)
add_subdirectory(test/apps)
add_subdirectory(test/permission-test)
add_subdirectory(examples/api)
#
# find src include -regex '.*\.\(c\|cpp\|h\)$' -print | xargs clang-format -i -style="{BasedOnStyle: llvm, IndentWidth: 4}"
#
install (FILES "${PROJECT_SOURCE_DIR}/test/setup.sh" PERMISSIONS OWNER_READ OWNER_WRITE
        OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        DESTINATION ${PROJECT_BINARY_DIR})

install (FILES "${PROJECT_SOURCE_DIR}/test/unit-test/config/sample_config" PERMISSIONS OWNER_READ OWNER_WRITE
        GROUP_READ WORLD_READ
        DESTINATION ${PROJECT_BINARY_DIR}/test/unit-test/config/)

install (DIRECTORY "${PROJECT_SOURCE_DIR}/test/config_files" DESTINATION "${PROJECT_BINARY_DIR}/test")
