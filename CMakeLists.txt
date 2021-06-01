# Check cmake version
cmake_minimum_required(VERSION 3.16)

# Project info (version may be edited)
project(dexpo CXX)
include(cmake/StandardProjectSettings.cmake)
include(cmake/PreventInSourceBuilds.cmake)

# Link this 'library' to set the c++ standard / compile-time options requested
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_17)

# Link this 'library' to use the warnings specified in CompilerWarnings.cmake
add_library(project_warnings INTERFACE)

# Snable cache system
include(cmake/Cache.cmake)

# Standard compiler warnings
include(cmake/CompilerWarnings.cmake)
set_project_warnings(project_warnings)

# Sanitizer options if supported by compiler
include(cmake/Sanitizers.cmake)
enable_sanitizers(project_options)

# Switch to ON when tests are made
option(ENABLE_TESTING "Enable Test Builds" OFF)

# Find xcb somehow (FIXME not 100% reliable)
find_library(xcb xcb REQUIRED)
if(!xcb)
  message("The xcb library can't be found in standart locations.")
  message("Consider moving it to folders like root, lib, lib64 etc.")
  message(
    "If you are familiar with CMake, you may want to add a hint to find_library."
  )
endif()

if(ENABLE_TESTING)
  enable_testing()
  message(
    "Building Tests. Be sure to check out test/constexpr_tests for constexpr testing"
  )
  add_subdirectory(test)
endif()

add_subdirectory(src)
