cmake_minimum_required(VERSION 3.10)
project(zenoh_cpp_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(zenohcxx REQUIRED)

add_executable(zenoh_cpp_pub zenoh_cpp_pub.cpp)
target_link_libraries(zenoh_cpp_pub PRIVATE zenohcxx::zenohcxx)

add_executable(zenoh_cpp_sub zenoh_cpp_sub.cpp)
target_link_libraries(zenoh_cpp_sub PRIVATE zenohcxx::zenohcxx)

