cmake_minimum_required(VERSION 3.10)
project(KernelSim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

include_directories(include)

file(GLOB SOURCES "src/*.cpp")

add_executable(kernel_sim ${SOURCES})

target_link_libraries(kernel_sim PRIVATE Threads::Threads)
