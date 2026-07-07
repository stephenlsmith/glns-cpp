cmake_minimum_required(VERSION 3.16)
project(glns VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(glns_core STATIC
  src/utilities.cpp
  src/powers.cpp
  src/parameters.cpp
  src/insertion_deletion.cpp
  src/tour_optimizations.cpp
  src/solver.cpp
  src/parser.cpp
  src/printer.cpp
)
target_include_directories(glns_core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
# no fused multiply-add: TSPLIB coordinate distances must round identically
# to the Julia reference implementation (floor(x + 0.5) sits on ulp boundaries)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(glns_core PRIVATE -ffp-contract=off)
endif()

add_executable(glns_cli cli/main.cpp)
target_link_libraries(glns_cli PRIVATE glns_core)
set_target_properties(glns_cli PROPERTIES OUTPUT_NAME glns)

enable_testing()
add_executable(glns_tests tests/test_basic.cpp)
target_link_libraries(glns_tests PRIVATE glns_core)
add_test(NAME basic COMMAND glns_tests ${CMAKE_CURRENT_SOURCE_DIR}/examples)
