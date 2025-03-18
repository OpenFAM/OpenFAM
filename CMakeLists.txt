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

cmake_minimum_required(VERSION 3.29)
project(OpenFAM
  LANGUAGES C;CXX)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

#
# build type: Release (default) or Debug
#
message(STATUS "====================")
if(CMAKE_BUILD_TYPE MATCHES Debug)
  message(STATUS "Build type: Debug")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
  message(STATUS "Build type: Release")
elseif(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
  message(STATUS "Build type: RelWithDebInfo")
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
option(ENABLE_FAM_PROFILING "FAM profiling" OFF)
option(ENABLE_LIBFABRIC_PROFILING "Libfabric profiling" OFF)
option(ENABLE_MEMSERVER_PROFILING "Memory Server profiling" OFF)

if(ENABLE_FAM_PROFILING)
  message(STATUS "FAM profiling enabled")
  add_compile_definitions(FAM_PROFILE)
endif()

if(ENABLE_LIBFABRIC_PROFILING)
  message(STATUS "LIBFABRIC profiling enabled")
  add_compile_definitions(LIBFABRIC_PROFILE)
endif()

if(ENABLE_MEMORYSERVER_PROFILING)
  message(STATUS "Memory Server profile enabled")
  add_compile_definitions(MEMSERVER_PROFILE)
endif()

# Region Registration requires offset checks to be enabled.
# However it is not mandatory for data item registrations.
# As a temporary fix, enabling CHECK_OFFSETS as a default
# for both PERMISSION_LEVELS.
# TODO: Change these in OpenFAM code as per permission
# levels in future releases.

option(ENABLE_CHECK_OFFSETS "Enable offset checks (default: off, on for Debug)" OFF)
if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(ENABLE_CHECK_OFFSETS ON)
endif()
if(NOT ENABLE_CHECK_OFFSETS)
  message(STATUS "Offset checks disabled")
else()
  message(STATUS "Offset checks enabled")
  add_compile_definitions(CHECK_OFFSETS)
endif()

option(ENABLE_THALLIUM "Enable RPC via Thallium" OFF)
if(ENABLE_THALLIUM)
  message(STATUS "Thallium RPC enabled")
  add_compile_definitions(USE_THALLIUM)
endif()

option(ENABLE_RESOURCE_RELEASE_ITEM_PERM "Enable resource relinquishment for region with DATAITEM permission" OFF)
if(ENABLE_RESOURCE_RELEASE_ITEM_PERM)
  message(STATUS "Enabling resource relinquishment for region with DATAITEM permission")
  add_compile_definitions(ENABLE_RESOURCE_RELEASE_ITEM_PERM)
endif()
message(STATUS "====================")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# Look for required packages
#
find_package(nvmm REQUIRED)
find_package(radixtree REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(LIBFABRIC 1.14.0 REQUIRED)
option(WITH_PMIX "Build OpenFAM with PMIx support" ON)
option(WITH_PMI2 "Build OpenFAM with PMI2 support" ON)
if(WITH_PMIX)
  find_package(PMIx REQUIRED)
endif()
if(WITH_PMI2)
  find_package(PMI2 REQUIRED)
endif()
set(boost_components system atomic context filesystem)
if(USE_BOOST_FIBER)
  set(boost_components ${boost_components} fiber)
endif()
find_package(Boost 1.65 REQUIRED COMPONENTS ${boost_components})
if(ENABLE_THALLIUM)
  find_package(thallium REQUIRED)
endif()

#
# common gcc flags
#

# To Be Fixed :
# -Wcast-qual : grpc headers throw up these warning during build
# of gRPC generated files
set(c_cxx_flags
  -Wall
  -W
  -Wextra
  -Wredundant-decls
  -Wunused
  -Wunused-macros
  -Wno-unused-parameter
  -Wcast-align
  -Wwrite-strings
  -Wmissing-field-initializers
  -Wendif-labels
  -Winit-self
  $<$<CXX_COMPILER_ID:GNU>:-Wlogical-op>
  -Wpacked
  -Wstack-protector
  -Wformat=2
  -Wswitch-enum
  -Wstrict-overflow=5
  -Wpointer-arith
  $<$<CXX_COMPILER_ID:GNU>:-Wnormalized=nfc>
  -Wno-long-long
  -Wconversion
  -Wunreachable-code
)
set(cxx_flags
  $<$<AND:$<BOOL:${ENABLE_THALLIUM}>,$<CXX_COMPILER_ID:GNU>>:-fconcepts>
)

set(debug_flags -g -O0 -fsanitize=address -fsanitize=leak)
set(coverage_flags --coverage)

add_compile_options(
  "$<$<COMPILE_LANGUAGE:C,CXX>:${c_cxx_flags}>"
  "$<$<COMPILE_LANGUAGE:CXX>:${cxx_flags}>"
  "$<$<CONFIG:Debug>:${debug_flags}>"
  "$<$<CONFIG:Coverage>:${debug_flags}>"
  "$<$<CONFIG:Coverage>:${coverage_flags}>"
)
add_link_options(
  "$<$<CONFIG:Debug>:${debug_flags}>"
  "$<$<CONFIG:Coverage>:${coverage_flags}>"
)
add_compile_definitions(
  "$<$<OR:$<CONFIG:Debug>,$<CONFIG:Coverage>>:DEBUG>"
  "$<$<CONFIG:Coverage>:COVERAGE>"
)

include(CheckPIESupported)
check_pie_supported()

#
# Set openFAM options to be used only by test. Some individual test cases might override these options.
# To use the following OpenFAM options set USE_FAM_OPTION flag
set(TEST_NPE 1)  # Number of PE used with mpirun
set(TEST_RUNTIME "PMIX")

option(SET_FAM_OPTION "Hard-code OpenFAM options for testing" OFF)
if(SET_FAM_OPTION)
  set(TEST_DEFAULT_REGION "Default")
  set(TEST_CIS_SERVER "127.0.0.1")
  set(TEST_GRPC_PORT "8787")
  set(TEST_LIBFABRIC_PROVIDER "sockets")
  set(TEST_FAM_THREAD_MODEL "FAM_THREAD_SERIALIZE")
  set(TEST_FAM_CONTEXT_MODEL "FAM_CONTEXT_DEFAULT")
  set(TEST_NUM_CONSUMER "1")
  message(STATUS "Using pre-defined test configuration")
else()
  set(USE_CONFIG_OPTIONS "1")
  message(STATUS "Tests use Fam options/config files")
endif()

set(TEST_SKIP_STATUS 77)

option(TEST_ENABLE_KNOWN_ISSUES "Enable testing of known issues" OFF)
if(ENABLE_KNOWN_ISSUES)
  add_compile_definitions(ENABLE_KNOWN_ISSUES)
endif()

option(USE_FAM_PERSIST "Use fam_persist" OFF)
if(USE_FAM_PERSIST)
  message(STATUS "Using FAM persist")
  add_compile_definitions(USE_FAM_PERSIST)
endif()

option(USE_FAM_INVALIDATE "Use fam_invalidate" OFF)
if(USE_FAM_INVALIDATE)
  message(STATUS "Using FAM invalidate")
  add_compile_definitions(USE_FAM_INVALIDATE)
endif()

option(USE_BOOST_FIBER "Use Boost synchronization primitives" OFF)
if(USE_BOOST_FIBER)
  message(STATUS "Using Boost Fiber")
  add_compile_definitions(USE_BOOST_FIBER)
endif()

# Use native atomics
add_compile_definitions(FAM_ATOMIC_NATIVE)

# Include fam_test_config.h in individual test cases and use the above variables as macros.
configure_file(
  ${PROJECT_SOURCE_DIR}/test/common/fam_test_config.h.in
  ${PROJECT_BINARY_DIR}/test/common/fam_test_config.h
)

# Read version number from VERSION file
file(STRINGS "VERSION" VERSION_STR)
add_compile_definitions(OPENFAM_VERSION="${VERSION_STR}")
string(REGEX REPLACE "^([0-9]+)\\..*" "\\1" MAJORVER "${VERSION_STR}")

#
# add source code
#
add_subdirectory(src)

option(WITH_TESTS "Build tests" ON)
if(WITH_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

option(WITH_EXAMPLES "Build examples" ON)
if(WITH_EXAMPLES)
  add_subdirectory(examples/api)
endif()

#
# Install targets
#

set(library_targets openfam famresmgr fammetadata_client fambitmap famcommon)
# the following gives an openfamTargets.cmake to help other projects directly use the build/ dir
export(TARGETS ${library_targets}
  NAMESPACE OpenFAM::
  FILE openfamTargets.cmake
)
export(PACKAGE OpenFAM)

include(GNUInstallDirs)
set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR}
  CACHE PATH "Location of header files")

install(TARGETS ${library_targets}
  EXPORT openfamTargets
)

# CMake 3.23+ would have FILE_SET instead
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
  DESTINATION "include"
  FILES_MATCHING
  PATTERN "*.h*"
)

install(TARGETS metadata_server memory_server cis_server
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
include(CMakePackageConfigHelpers)
install(EXPORT openfamTargets
  FILE openfamTargets.cmake
  NAMESPACE OpenFAM::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/OpenFAM"
)

configure_package_config_file(cmake/openfamConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/openfamConfig.cmake
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/OpenFAM"
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/openfamConfig.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/OpenFAM"
)
