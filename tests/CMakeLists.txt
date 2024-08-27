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

include(GoogleTest)
set(MENDER_TEST_FLAGS EXTRA_ARGS --gtest_output=xml:${CMAKE_SOURCE_DIR}/reports/)
include_directories(${CMAKE_SOURCE_DIR}/ ${CMAKE_SOURCE_DIR}/tests/)
set(TESTLIB_SOURCES "common/testing.cpp")
add_library(testlib STATIC ${TESTLIB_SOURCES})
target_include_directories(testlib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(testlib PUBLIC platform_compiler_flags)

add_executable(mender-flash_test EXCLUDE_FROM_ALL "main_test.cpp" "libflash/optimized_writer_test.cpp" "libflash/platformfs_test.cpp")
target_include_directories(mender-flash_test PUBLIC ${CMAKE_SOURCE_DIR} libflash common)
target_link_libraries(mender-flash_test PRIVATE GTest::gtest_main flashlib testlib)
gtest_discover_tests(mender-flash_test ${MENDER_TEST_FLAGS})

