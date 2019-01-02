cmake_minimum_required(VERSION 3.10)
project(zectl C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules/")

include_directories(include)

include(FindZFS)

set(DATA_DIR data)
set(ZCP_SCRIPTS
        ${DATA_DIR}/zcp/list.lua
)

# LUA Scripts ------------------------------------------------------------------

foreach(zcp ${ZCP_SCRIPTS})

    # copy zcps to build dir
    if(${zcp} MATCHES "(lua)$")
        file(COPY ${zcp} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endif()

endforeach()

add_subdirectory(src)
#link_directories(lib/libze lib/util)
add_subdirectory(lib)