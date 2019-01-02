add_compile_definitions(DEBUG=1)

# ZFS Version

execute_process(COMMAND
        sh -c "modinfo zfs | grep -iw version | awk '{print $2}' | cut -d '.' -f 2"
        OUTPUT_VARIABLE ZOL_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
string(STRIP ${ZOL_VERSION} ZOL_VERSION)
message(STATUS "ZOL_VERSION=${ZOL_VERSION}")
add_compile_definitions(ZOL_VERSION=${ZOL_VERSION})

# zectl Source ------------------------------------------------------------------

set(SOURCE_FILES
        zectl.c zectl.h)

list(APPEND ZE_LINK_LIBRARIES libze util)

add_executable(zectl ${SOURCE_FILES})

target_link_libraries(zectl ${ZE_LINK_LIBRARIES})