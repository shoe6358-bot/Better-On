cmake_minimum_required(VERSION 3.22)
project(better_on_welcome VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

add_executable(better-on-welcome main.cpp)
target_link_libraries(better-on-welcome PRIVATE Qt6::Widgets)
target_compile_options(better-on-welcome PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS better-on-welcome RUNTIME DESTINATION bin)
