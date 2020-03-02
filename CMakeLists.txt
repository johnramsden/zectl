cmake_minimum_required(VERSION 3.10)

project(zectl C)
set (CMAKE_C_STANDARD 11)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules/")

include_directories(include)

include(FindZFS)

if (CMAKE_VERSION VERSION_LESS 3.12)
    add_definitions(-DDEBUG=0)
    add_definitions(-DBUILD_TESTING=0)
else()
    add_compile_definitions(DEBUG=0)
    add_compile_definitions(BUILD_TESTING=0)
endif()

add_subdirectory(lib)
add_subdirectory(src)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif()

unset(PLUGINS_DIRECTORY CACHE)
