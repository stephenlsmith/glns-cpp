cmake_minimum_required(VERSION 3.18)
project(glns VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GLNS_BUILD_PYTHON "Build the Python extension module" OFF)
option(GLNS_BUILD_CLI "Build the command-line solver" ON)
option(GLNS_BUILD_TESTS "Build the tests" ON)

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
  src/c_api.cpp
)
target_include_directories(glns_core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(glns_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
# no fused multiply-add: TSPLIB coordinate distances must round identically
# to the Julia reference implementation (floor(x + 0.5) sits on ulp boundaries)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(glns_core PRIVATE -ffp-contract=off)
endif()

if(GLNS_BUILD_CLI)
  add_executable(glns_cli cli/main.cpp)
  target_link_libraries(glns_cli PRIVATE glns_core)
  set_target_properties(glns_cli PROPERTIES OUTPUT_NAME glns)
endif()

if(GLNS_BUILD_TESTS)
  enable_testing()
  add_executable(glns_tests tests/test_basic.cpp)
  target_link_libraries(glns_tests PRIVATE glns_core)
  add_test(NAME basic COMMAND glns_tests ${CMAKE_CURRENT_SOURCE_DIR}/examples)

  add_executable(glns_c_api_test tests/test_c_api.c)
  target_link_libraries(glns_c_api_test PRIVATE glns_core)
  set_target_properties(glns_c_api_test PROPERTIES LINKER_LANGUAGE CXX)
  add_test(NAME c_api COMMAND glns_c_api_test ${CMAKE_CURRENT_SOURCE_DIR}/examples)

  add_executable(glns_checksum tools/checksum.cpp)
  target_link_libraries(glns_checksum PRIVATE glns_core)
endif()

if(GLNS_BUILD_PYTHON)
  find_package(Python 3.9 COMPONENTS Interpreter Development.Module REQUIRED)
  execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
  find_package(nanobind CONFIG REQUIRED)
  nanobind_add_module(_core STABLE_ABI bindings/python/glns_module.cpp)
  target_link_libraries(_core PRIVATE glns_core)
  install(TARGETS _core LIBRARY DESTINATION glns)
endif()
