cmake_minimum_required(VERSION 3.14)
project(fpscounter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Header-only INTERFACE library
add_library(fpscounter INTERFACE)
target_include_directories(
  fpscounter INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                       $<INSTALL_INTERFACE:include>
)
target_compile_features(fpscounter INTERFACE cxx_std_17)

# Tests (disabled by default for downstream consumers)
option(FPSCOUNTER_BUILD_TESTS "Build tests" OFF)
if(FPSCOUNTER_BUILD_TESTS)
  include(CTest)
  add_subdirectory(test)
endif()
