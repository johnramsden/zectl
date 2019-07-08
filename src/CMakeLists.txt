## Custom target will always cause its dependencies to be evaluated and is
## run by default
#add_custom_target(dummy_target ALL
#        DEPENDS
#        ${CMAKE_SOURCE_DIR}/src/zcp.h
#        ${CMAKE_SOURCE_DIR}/src/zcp.c
#        )
#
#add_custom_command(COMMAND
#        sh -c "${CMAKE_SOURCE_DIR}/scripts/file_to_lua_list.sh header ${CMAKE_SOURCE_DIR}/data/zcp > ${CMAKE_SOURCE_DIR}/src/zcp.h"
#        OUTPUT ${CMAKE_SOURCE_DIR}/src/zcp.h
#        DEPENDS always_rebuild
#        VERBATIM)
#
#add_custom_command(COMMAND
#        sh -c "${CMAKE_SOURCE_DIR}/scripts/file_to_lua_list.sh c ${CMAKE_SOURCE_DIR}/data/zcp > ${CMAKE_SOURCE_DIR}/src/zcp.c"
#        OUTPUT ${CMAKE_SOURCE_DIR}/src/zcp.c
#        DEPENDS always_rebuild
#        VERBATIM)
#
## Dummy output which is never actually produced. Anything that depends on
## this will always be rebuilt.
#add_custom_command(
#        OUTPUT always_rebuild
#        COMMAND cmake -E echo
#)

# zectl Source ------------------------------------------------------------------

set(SOURCE_FILES
        zectl.c zectl.h
        zectl_list.c zectl_create.c)

list(APPEND ZE_LINK_LIBRARIES libze util)

add_executable(zectl ${SOURCE_FILES})

target_link_libraries(zectl ${ZE_LINK_LIBRARIES})