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

add_executable(mender-flash main.cpp lib.cpp)
install(TARGETS mender-flash
  DESTINATION bin
  COMPONENT mender-flash
)

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
  DEPENDS main_test
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

add_executable(main_test EXCLUDE_FROM_ALL main_test.cpp lib.cpp)
target_link_libraries(main_test PRIVATE GTest::gtest_main)
gtest_discover_tests(main_test ${MENDER_TEST_FLAGS})
