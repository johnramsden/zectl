cmake_minimum_required(VERSION 3.10)

project(zectl LANGUAGES C VERSION 0.1.3 DESCRIPTION "BE Manager")

set (CMAKE_C_STANDARD 11)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules/")

include_directories(include)

include(FindZFS)

if (CMAKE_VERSION VERSION_LESS 3.12)
    add_definitions(-DDEBUG=0)
else()
    add_compile_definitions(DEBUG=0)
endif()

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(docs)
add_subdirectory(tests)

unset(PLUGINS_DIRECTORY CACHE)
