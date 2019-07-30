cmake_minimum_required(VERSION 3.10)
project(zectl C)
set (CMAKE_C_STANDARD 11)

enable_testing()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules/")

include_directories(include)

include(FindZFS)
include(FindCheck)

execute_process(COMMAND
        sh -c "modinfo zfs | grep -iw version | awk '{print $2}' | cut -d '.' -f 2"
        OUTPUT_VARIABLE ZOL_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
string(STRIP ${ZOL_VERSION} ZOL_VERSION)
message(STATUS "ZOL_VERSION=${ZOL_VERSION}")

add_compile_definitions(ZOL_VERSION=${ZOL_VERSION})
add_compile_definitions(DEBUG=1)

add_subdirectory(src)
add_subdirectory(lib)
add_subdirectory(tests)