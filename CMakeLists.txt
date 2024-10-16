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

option(BUILD_TESTS "Build the unit tests (Default: ON)" ON)
# set(CMAKE_VERBOSE_MAKEFILE ON)

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

# Find all the source files in the flash and common directories
set(FLASH_SOURCES "libflash/fileio.cpp" "libflash/optimized_writer.cpp" "libflash/platformfs.cpp")
set(COMMON_SOURCES "common/io.cpp" "common/io.cpp" "common/error.cpp" "common/io.cpp" "common/common.cpp")


# Combine the source files from both directories into one list
set(LIB_SOURCES ${FLASH_SOURCES} ${COMMON_SOURCES})

# Create a static library from the combined source files
add_library(flashlib STATIC ${LIB_SOURCES})
target_include_directories(flashlib PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/libflash ${CMAKE_SOURCE_DIR}/vendor/expected/include)
target_link_libraries(flashlib PUBLIC crossplatform_compiler_flags)

if(BUILD_TESTS)
  add_subdirectory(tests)
endif()
