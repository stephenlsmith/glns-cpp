cmake_minimum_required(VERSION 3.18)
project(glns VERSION 0.1.0 LANGUAGES CXX C)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(GLNS_IS_TOP_LEVEL ON)
else()
  set(GLNS_IS_TOP_LEVEL OFF)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GLNS_BUILD_PYTHON "Build the Python extension module" OFF)
option(GLNS_BUILD_CLI "Build the command-line solver" ${GLNS_IS_TOP_LEVEL})
option(GLNS_BUILD_TESTS "Build the tests" ${GLNS_IS_TOP_LEVEL})
option(GLNS_INSTALL_CPP "Install the C/C++ library and CMake package" ${GLNS_IS_TOP_LEVEL})
option(GLNS_ENABLE_WARNINGS "Enable recommended compiler warnings" ${GLNS_IS_TOP_LEVEL})

function(glns_enable_warnings target)
  if(NOT GLNS_ENABLE_WARNINGS)
    return()
  endif()
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

if(GLNS_IS_TOP_LEVEL)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

if(GLNS_IS_TOP_LEVEL AND NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(glns_core PUBLIC cxx_std_17)
set_target_properties(glns_core PROPERTIES
  EXPORT_NAME core
  POSITION_INDEPENDENT_CODE ON
)
add_library(glns::core ALIAS glns_core)
glns_enable_warnings(glns_core)
# no fused multiply-add: TSPLIB coordinate distances must round identically
# to the Julia reference implementation (floor(x + 0.5) sits on ulp boundaries)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(glns_core PRIVATE -ffp-contract=off)
elseif(MSVC)
  target_compile_options(glns_core PRIVATE /fp:strict)
endif()

if(GLNS_BUILD_CLI)
  add_executable(glns_cli cli/main.cpp)
  target_link_libraries(glns_cli PRIVATE glns_core)
  set_target_properties(glns_cli PROPERTIES OUTPUT_NAME glns)
  glns_enable_warnings(glns_cli)
endif()

if(GLNS_BUILD_TESTS)
  enable_testing()
  add_executable(glns_tests tests/test_basic.cpp)
  target_link_libraries(glns_tests PRIVATE glns_core)
  add_test(NAME basic COMMAND glns_tests ${CMAKE_CURRENT_SOURCE_DIR}/examples
                                      ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
                                      ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(basic PROPERTIES TIMEOUT 120)

  add_executable(glns_c_api_test tests/test_c_api.c)
  target_link_libraries(glns_c_api_test PRIVATE glns_core)
  set_target_properties(glns_c_api_test PROPERTIES LINKER_LANGUAGE CXX)
  add_test(NAME c_api COMMAND glns_c_api_test ${CMAKE_CURRENT_SOURCE_DIR}/examples)

  add_executable(glns_checksum tools/checksum.cpp)
  target_link_libraries(glns_checksum PRIVATE glns_core)

  if(GLNS_BUILD_CLI)
    add_test(NAME cli_help COMMAND glns_cli --help)
    set_tests_properties(cli_help PROPERTIES
      PASS_REGULAR_EXPRESSION "Usage:  glns")

    add_test(NAME cli_no_input COMMAND glns_cli)
    add_test(NAME cli_unknown_flag
      COMMAND glns_cli ${CMAKE_CURRENT_SOURCE_DIR}/examples/tiny.gtsp -bogus=1)
    add_test(NAME cli_extra_positional
      COMMAND glns_cli ${CMAKE_CURRENT_SOURCE_DIR}/examples/tiny.gtsp extra.gtsp)
    set_tests_properties(cli_no_input cli_unknown_flag cli_extra_positional
      PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli_numeric_options
      COMMAND ${CMAKE_COMMAND}
        -DGLNS_CLI=$<TARGET_FILE:glns_cli>
        -DGLNS_INSTANCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/tiny.gtsp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_numbers.cmake)
    set_tests_properties(cli_numeric_options PROPERTIES TIMEOUT 30)
  endif()
endif()

if(GLNS_BUILD_PYTHON)
  find_package(Python 3.9 COMPONENTS Interpreter Development.Module REQUIRED)
  execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
  find_package(nanobind CONFIG REQUIRED)
  # Build for the selected interpreter. Nanobind's linked stable ABI starts
  # at Python 3.12, while this package supports Python 3.9 and newer.
  nanobind_add_module(_core bindings/python/glns_module.cpp)
  target_link_libraries(_core PRIVATE glns_core)
  install(TARGETS _core LIBRARY DESTINATION glns)
endif()

if(GLNS_INSTALL_CPP)
  install(TARGETS glns_core
    EXPORT glnsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
  install(DIRECTORY include/glns DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  if(GLNS_BUILD_CLI)
    install(TARGETS glns_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

  set(GLNS_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/glns)
  configure_package_config_file(
    cmake/glnsConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/glnsConfig.cmake
    INSTALL_DESTINATION ${GLNS_INSTALL_CMAKEDIR}
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/glnsConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )
  install(EXPORT glnsTargets
    FILE glnsTargets.cmake
    NAMESPACE glns::
    DESTINATION ${GLNS_INSTALL_CMAKEDIR}
  )
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/glnsConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/glnsConfigVersion.cmake
    DESTINATION ${GLNS_INSTALL_CMAKEDIR}
  )
endif()
