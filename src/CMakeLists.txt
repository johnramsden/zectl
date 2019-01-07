execute_process(COMMAND
        sh -c "${CMAKE_SOURCE_DIR}/scripts/file_to_header.sh ${CMAKE_SOURCE_DIR}/data/zcp"
        OUTPUT_FILE ${CMAKE_SOURCE_DIR}/src/zcp.h)

# zectl Source ------------------------------------------------------------------

set(SOURCE_FILES
        zectl.c zectl.h zcp.h)

list(APPEND ZE_LINK_LIBRARIES libze util)

add_executable(zectl ${SOURCE_FILES})

target_link_libraries(zectl ${ZE_LINK_LIBRARIES})