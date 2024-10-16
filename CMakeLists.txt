cmake_minimum_required(VERSION 3.15)

# fail hard when some include doesn't 100% work
if (POLICY CMP0111)
  cmake_policy(SET CMP0111 NEW)
endif (POLICY CMP0111)

# update timestamps of downloaded files after extraction instead of keeping the timestamps from the archive
if (POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif (POLICY CMP0135)

project(mender-flash)

# set(CMAKE_VERBOSE_MAKEFILE ON)
enable_testing()

option(COVERAGE "Turn coverage instrumentation on (Default: OFF)" OFF)
if($CACHE{COVERAGE})
  set(CMAKE_CXX_FLAGS "--coverage $CACHE{CMAKE_CXX_FLAGS}")
endif()

set(GTEST_VERSION 1.12.1)

option(MENDER_DOWNLOAD_GTEST "Download google test if it is not found (Default: ON)" ON)

if (MENDER_DOWNLOAD_GTEST)

  ### BEGIN taken from https://google.github.io/googletest/quickstart-cmake.html
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/release-${GTEST_VERSION}.zip
  )

  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  ### END

  set(BUILD_GMOCK ON)
  set(INSTALL_GTEST OFF)
  FetchContent_MakeAvailable(googletest)

else()
  find_package(GTest REQUIRED)
endif()

# TODO: proper platform detection
set(PLATFORM linux_x86)

#---------------------------------------------------------------------------------------------------
# When deciding which one(s) of these to include, the general rule is this:
#
# If it's a cross platform component, add this:
#   target_link_libraries(<TARGET> PUBLIC crossplatform_compiler_flags)
#
# If it's a platform specific component, with a cross platform interface, add this:
#   target_link_libraries(<TARGET> PRIVATE platform_compiler_flags)
#   target_link_libraries(<TARGET> INTERFACE crossplatform_compiler_flags)

add_library(crossplatform_compiler_flags INTERFACE)
target_compile_features(crossplatform_compiler_flags INTERFACE cxx_std_11)
target_compile_options(crossplatform_compiler_flags INTERFACE -std=c++11 -Werror -Wall -Wconversion)

add_library(platform_compiler_flags INTERFACE)
target_compile_features(platform_compiler_flags INTERFACE cxx_std_17)
target_compile_options(platform_compiler_flags INTERFACE -std=c++17 -Werror -Wall -Wconversion)
#---------------------------------------------------------------------------------------------------

add_executable(mender-flash "main.cpp")
install(TARGETS mender-flash
  DESTINATION bin
  COMPONENT mender-flash
)
target_include_directories(mender-flash PUBLIC .)
target_link_libraries(mender-flash flashlib)

if($CACHE{COVERAGE})
  add_custom_target(coverage_enabled COMMAND true)
else()
  add_custom_target(coverage_enabled
    COMMAND echo 'Please run `cmake -D COVERAGE=ON .` first!'
    COMMAND false
  )
endif()

add_custom_target(coverage
  COMMAND lcov --capture --quiet --directory .
               --output-file coverage.lcov
               --exclude '/usr/*'
               --exclude '*/googletest/*'
               --exclude '*_test.*'
               --exclude '*/googlemock/*'
               --exclude '*/vendor/*'
  DEPENDS coverage_enabled check
)

# CMake is not clever enough to build the tests before running them so we use
# the 'check' target below that does both.
add_custom_target(check
  COMMAND ${CMAKE_CTEST_COMMAND}
  DEPENDS mender-flash_test
)

# CMake doesn't generate the 'uninstall' target.
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

add_custom_target(uninstall
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)

add_custom_target(install-bin
  COMMAND ${CMAKE_COMMAND} --install . --component=mender-flash
)
add_custom_target(uninstall-bin
  COMMAND ${CMAKE_COMMAND} -D CMAKE_INSTALL_COMPONENT=mender-flash -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

include(GoogleTest)
set(MENDER_TEST_FLAGS EXTRA_ARGS --gtest_output=xml:${CMAKE_SOURCE_DIR}/reports/)

# Find all the source files in the flash and common directories
set(FLASH_SOURCES "libflash/fileio.cpp" "libflash/optimized_writer.cpp" "libflash/platformfs.cpp")
set(COMMON_SOURCES "common/io.cpp" "common/io.cpp" "common/error.cpp" "common/io.cpp" "common/common.cpp")
set(TESTLIB_SOURCES "common/testing.cpp")

# Combine the source files from both directories into one list
set(LIB_SOURCES ${FLASH_SOURCES} ${COMMON_SOURCES})

# Create a static library from the combined source files
add_library(flashlib STATIC ${LIB_SOURCES})
target_include_directories(flashlib PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/libflash ${CMAKE_SOURCE_DIR}/vendor/expected/include)
target_link_libraries(flashlib PUBLIC crossplatform_compiler_flags)

add_library(testlib STATIC ${TESTLIB_SOURCES})
target_include_directories(testlib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(testlib PUBLIC platform_compiler_flags)

add_executable(mender-flash_test EXCLUDE_FROM_ALL "main_test.cpp" "flash_test/optimized_writer_test.cpp" "flash_test/platformfs_test.cpp")
target_include_directories(mender-flash_test PUBLIC ${CMAKE_SOURCE_DIR} libflash common)
target_link_libraries(mender-flash_test PRIVATE GTest::gtest_main flashlib testlib)
gtest_discover_tests(mender-flash_test ${MENDER_TEST_FLAGS})
