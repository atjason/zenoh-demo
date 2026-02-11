cmake_minimum_required(VERSION 3.10)
project(zenoh_cpp_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(zenohc REQUIRED)
find_package(zenohcxx REQUIRED)

add_executable(bench_echo_ack
  src/bench_echo_ack.cpp
)
target_link_libraries(bench_echo_ack PRIVATE zenohcxx::zenohc)

add_executable(bench_pub_rtt
  src/bench_pub_rtt.cpp
)
target_link_libraries(bench_pub_rtt PRIVATE zenohcxx::zenohc)

