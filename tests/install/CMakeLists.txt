cmake_minimum_required(VERSION 3.18)
project(glns_install_test LANGUAGES C CXX)

find_package(glns 0.1 CONFIG REQUIRED)

add_executable(glns_cpp_consumer main.cpp)
target_link_libraries(glns_cpp_consumer PRIVATE glns::core)

add_executable(glns_c_consumer main.c)
target_link_libraries(glns_c_consumer PRIVATE glns::core)

enable_testing()
add_test(NAME cpp_consumer COMMAND glns_cpp_consumer)
add_test(NAME c_consumer COMMAND glns_c_consumer)
