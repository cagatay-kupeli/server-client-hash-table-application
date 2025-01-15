cmake_minimum_required(VERSION 3.26)
project(server)

set(CMAKE_CXX_STANDARD 23)

add_executable(server server.cpp)
add_executable(client client.cpp)