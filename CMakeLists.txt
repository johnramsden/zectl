cmake_minimum_required(VERSION 3.10)

project(zectl C)
set (CMAKE_C_STANDARD 11)

enable_testing()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules/")

include_directories(include)

include(FindZFS)
include(FindCheck)

if (CMAKE_VERSION VERSION_LESS 3.12)
    add_definitions(-DDEBUG=0)
else()
    add_compile_definitions(DEBUG=0)
endif()

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(tests)

unset(PLUGINS_DIRECTORY CACHE)
