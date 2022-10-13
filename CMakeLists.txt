 #
 # CMakeLists.txt
 # Copyright (c) 2019-2022 Hewlett Packard Enterprise Development, LP. All rights reserved.
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

if(ENABLE_CHECK_OFFSETS)
	message(STATUS "Offset checks enabled")
	add_definitions(-DCHECK_OFFSETS)
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
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -DDEBUG -DCHECK_OFFSETS -O0 -g")

#
# gcc flags for debug build
#
set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS} -DDEBUG -DCOVERAGE -O0 -g -fprofile-arcs -ftest-coverage")

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
link_directories(${PROJECT_SOURCE_DIR}/third-party/build/lib64) # third-party libraries

#
# default system include and link directories
#
link_directories(/usr/lib)
include_directories(/usr/include)
link_directories(/usr/local/lib)
include_directories(/usr/local/include)

#
# Set openFAM options to be used only by test. Some individual test cases might override these options.
# To use the following OpenFAM options set USE_FAM_OPTION flag
set( TEST_NPE 1 ) #Number of pe used with mpirun
set( TEST_RUNTIME "PMIX" )

if (SET_FAM_OPTION)
       set( TEST_DEFAULT_REGION "Default" )
       set( TEST_CIS_SERVER "127.0.0.1" )
       set( TEST_GRPC_PORT "8787" )
       set( TEST_LIBFABRIC_PROVIDER "sockets" )
       set( TEST_FAM_THREAD_MODEL "FAM_THREAD_SERIALIZE" )
       set( TEST_FAM_CONTEXT_MODEL "FAM_CONTEXT_DEFAULT" )
       set( TEST_NUM_CONSUMER "1" )
       message(STATUS "Using pre-defined test configuratin")
else()
       set (USE_CONFIG_OPTIONS "1")
       message(STATUS "Tests use Fam options/config files")
endif()
	
IF (${CMAKE_MAJOR_VERSION} GREATER 2)
        set( TEST_SKIP_STATUS 77 )
ELSE ()
        set( TEST_SKIP_STATUS 0 )
ENDIF()
set( TEST_ENABLE_KNOWN_ISSUES "no" )
if (ENABLE_KNOWN_ISSUES)
        set( TEST_ENABLE_KNOWN_ISSUES "yes" )
        add_definitions(-DENABLE_KNOWN_ISSUES)
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
add_subdirectory(test/services-test)
add_subdirectory(examples/api)

#install openfam_adm in bin directory
install (SCRIPT openfam_adm_install.cmake)
#install template config files in install path
install (DIRECTORY "${PROJECT_SOURCE_DIR}/config" USE_SOURCE_PERMISSIONS
		DESTINATION ${PROJECT_BINARY_DIR}/)

install (FILES "${PROJECT_SOURCE_DIR}/test/unit-test/config/sample_config" PERMISSIONS OWNER_READ OWNER_WRITE
        GROUP_READ WORLD_READ
        DESTINATION ${PROJECT_BINARY_DIR}/test/unit-test/config/)

#
# find src include -regex '.*\.\(c\|cpp\|h\)$' -print | xargs clang-format -i -style="{BasedOnStyle: llvm, IndentWidth: 4}"
#
