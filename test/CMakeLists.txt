# enable GTest
#
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
  #  FIND_PACKAGE_ARGS NAMES GTest  # this would tell CMake to first look for an installed GTest, but requires CMake 3.28+
  #  EXCLUDE_FROM_ALL  # for CMake 3.28+ this can be used instead of the FetchContent_Properties below
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# FetchContent_MakeAvailable(googletest)  # for CMake 3.28+ use this instead of the block below
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
  target_compile_options(gtest PRIVATE "-w")
  target_compile_options(gtest_main PRIVATE "-w")
endif()

include(GoogleTest)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR} # test config header
  ${CMAKE_CURRENT_SOURCE_DIR} # util/
)

add_subdirectory(unit-test)
add_subdirectory(reg-test)
add_subdirectory(multi-memnode-test)
add_subdirectory(maxconfig)
add_subdirectory(coverage)
add_subdirectory(microbench)
add_subdirectory(apps)
add_subdirectory(permission-test)
add_subdirectory(services-test)
add_subdirectory(wrappers-test)
