cmake_minimum_required(VERSION 3.10)
project(zectl C)

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
set(DATA_DIR data)
set(ZCP_SCRIPTS
        ${CMAKE_SOURCE_DIR}/${DATA_DIR}/zcp/list.lua
)

# LUA Scripts ------------------------------------------------------------------

foreach(zcp ${ZCP_SCRIPTS})

    # copy zcps to build dir
    if(${zcp} MATCHES "(lua)$")
        file(COPY ${zcp} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endif()

endforeach()

add_subdirectory(src)
add_subdirectory(lib)
add_subdirectory(tests)