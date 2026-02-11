cmake_minimum_required(VERSION 3.10)
project(zenoh_cpp_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(zenohc CONFIG QUIET)
if(NOT zenohc_FOUND)
  message(FATAL_ERROR
    "zenoh-c (CMake package: zenohc) not found.\n"
    "Ubuntu/Debian: sudo apt install libzenohc-dev\n"
    "macOS/Homebrew: brew install libzenohc\n"
    "If installed to a custom prefix, set -DCMAKE_PREFIX_PATH=/path/to/prefix")
endif()

find_package(zenohcxx CONFIG QUIET)
if(NOT zenohcxx_FOUND)
  message(FATAL_ERROR
    "zenoh-cpp (CMake package: zenohcxx) not found.\n"
    "Ubuntu/Debian package name is libzenohcpp-dev (note: apt package != CMake package).\n"
    "Ubuntu/Debian: sudo apt install libzenohcpp-dev\n"
    "macOS/Homebrew: brew install libzenohcpp\n"
    "If installed to a custom prefix, set -DCMAKE_PREFIX_PATH=/path/to/prefix")
endif()

add_executable(zenoh_cpp_pub zenoh_cpp_pub.cpp)
target_link_libraries(zenoh_cpp_pub PRIVATE zenohcxx::zenohc)

add_executable(zenoh_cpp_sub zenoh_cpp_sub.cpp)
target_link_libraries(zenoh_cpp_sub PRIVATE zenohcxx::zenohc)

