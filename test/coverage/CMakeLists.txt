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

#Add this target only for coverage build
if(CMAKE_BUILD_TYPE MATCHES Coverage)

# Check prereqs
find_program( GCOV_BIN gcov )
find_program( LCOV_BIN  lcov )
find_program( GENHTML_BIN genhtml )

if(NOT LCOV_BIN)
message(FATAL_ERROR "lcov not found! Aborting...")
endif() # NOT LCOV_BIN

if(NOT GENHTML_BIN)
message(FATAL_ERROR "genhtml not found! Aborting...")
endif() # NOT GENHTML_BIN


set(COVERAGE_REMOVE_PATHS
    '/usr/*'
    '${PROJECT_SOURCE_DIR}/third-party/*'
    '${PROJECT_SOURCE_DIR}/src/metadata/radixtree/*'
    '${PROJECT_SOURCE_DIR}/src/rpc/fam_rpc.grpc.pb.cc'
    '${PROJECT_SOURCE_DIR}/src/rpc/fam_rpc.grpc.pb.h'
    '${PROJECT_SOURCE_DIR}/src/rpc/fam_rpc.pb.cc'
    '${PROJECT_SOURCE_DIR}/src/rpc/fam_rpc.pb.h'
)

set(Coverage_GENHTML_ARGS --demangle-cpp)
set(COVERAGE_PATH ${PROJECT_BINARY_DIR}/test/coverage)
set(Coverage_NAME coverage)

#cov-clean
add_custom_target(cov-clean
# Cleanup lcov
COMMAND ${LCOV_BIN} --gcov-tool ${GCOV_BIN} -directory ${PROJECT_BINARY_DIR}/src --zerocounters

# Clean the coverage directory
COMMAND rm -rf ${COVERAGE_PATH}/*

# Create baseline.
COMMAND ${LCOV_BIN}  --gcov-tool ${GCOV_BIN} -c -i -d ${PROJECT_BINARY_DIR}/src -o ${COVERAGE_PATH}/${Coverage_NAME}.base

COMMENT "Resetting code coverage counters to zero."
)

#cov-run-tests
add_custom_target(cov-run-tests
# Run tests
WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
COMMAND make reg-test || echo "*** make reg-test failed. ***"
COMMAND make unit-test || echo "*** make unit-test failed. ***"
)

#cov-report
add_custom_target(cov-report
# Capture coverage data.
COMMAND ${LCOV_BIN} --gcov-tool ${GCOV_BIN} --directory ${PROJECT_BINARY_DIR}/src --capture --output-file ${COVERAGE_PATH}/${Coverage_NAME}.info

# add with baseline data.
COMMAND ${LCOV_BIN} ${Coverage_LCOV_ARGS} --gcov-tool ${GCOV_BIN} -a ${COVERAGE_PATH}/${Coverage_NAME}.base -a ${COVERAGE_PATH}/${Coverage_NAME}.info --output-file ${COVERAGE_PATH}/${Coverage_NAME}.total

# Generage final coverage info file afer removing coverage data of /usr/* and other third-party projects.
COMMAND ${LCOV_BIN} ${Coverage_LCOV_ARGS} --gcov-tool ${GCOV_BIN} --remove ${COVERAGE_PATH}/${Coverage_NAME}.total ${COVERAGE_REMOVE_PATHS} --output-file ${COVERAGE_PATH}/${Coverage_NAME}.info.final

# Generate HTML reports.
COMMAND ${GENHTML_BIN} ${Coverage_GENHTML_ARGS} -o ${COVERAGE_PATH} ${COVERAGE_PATH}/${Coverage_NAME}.info.final

# Display a short list of report on console.
COMMAND ${LCOV_BIN} -l ${COVERAGE_PATH}/${Coverage_NAME}.info.final

COMMAND echo "*** Lcov code coverage info report saved in ${COVERAGE_PATH}/${Coverage_NAME}.info ***"
COMMAND echo "*** Open ${COVERAGE_PATH}/index.html in your browser to view the coverage report ***"

WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
DEPENDS ${Coverage_DEPENDENCIES}
)

#coverage - Call all the targets to build the coverage
add_custom_target(coverage
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    COMMAND make cov-clean
    COMMAND make cov-run-tests
    COMMAND make cov-report
)

endif()
