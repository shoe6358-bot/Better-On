cmake_minimum_required(VERSION 3.22)
project(better_on_hub VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network)

add_executable(better-on-hub main.cpp)
target_link_libraries(better-on-hub PRIVATE Qt6::Widgets Qt6::Network)
target_compile_options(better-on-hub PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS better-on-hub RUNTIME DESTINATION bin)
