cmake_minimum_required(VERSION 3.10)

project(zectl C)
set (CMAKE_C_STANDARD 11)

enable_testing()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules/")

include_directories(include)

include(FindZFS)
include(FindCheck)

add_compile_definitions(DEBUG=0)

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(tests)

unset(PLUGINS_DIRECTORY CACHE)