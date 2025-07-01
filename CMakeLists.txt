cmake_minimum_required(VERSION 3.15)

project(mender-flash)

# set(CMAKE_VERBOSE_MAKEFILE ON)

option(COVERAGE "Turn coverage instrumentation on (Default: OFF)" OFF)
if($CACHE{COVERAGE})
  set(CMAKE_C_FLAGS "--coverage $CACHE{CMAKE_C_FLAGS}")
endif()

add_compile_options(-Wall -Wextra -Werror)
add_executable(mender-flash main.c)

install(TARGETS mender-flash
  DESTINATION bin
  COMPONENT mender-flash
)

enable_testing()
add_test(NAME tests
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests.sh" "${CMAKE_CURRENT_BINARY_DIR}"
)
add_custom_target(check
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  DEPENDS mender-flash
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
  DEPENDS coverage_enabled check
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
