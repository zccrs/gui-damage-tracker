cmake_minimum_required(VERSION 3.16)
project(gui-damage-tracker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)

add_library(gdt STATIC
    src/gdtnode.cpp
    src/gdttracker.cpp
)
target_include_directories(gdt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
target_link_libraries(gdt PUBLIC Qt6::Core Qt6::Gui)
target_compile_options(gdt PRIVATE -Wall -Wextra -Wpedantic)

option(GDT_BUILD_TESTS "Build unit tests" ON)
option(GDT_BUILD_EXAMPLES "Build examples" ON)

if(GDT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(GDT_BUILD_EXAMPLES)
    add_subdirectory(examples/damage-demo)
endif()
