add_definitions(-DPLUGINS_DIRECTORY=\"${CMAKE_CURRENT_BINARY_DIR}/../libze_plugin\")

set(LIBZE_SOURCE_FILES
        libze.c system_linux.c system_linux.h
        libze_bootloader.c libze_plugin_manager.c libze_util.c)

add_library(libze ${LIBZE_SOURCE_FILES})
set_property(TARGET libze PROPERTY PREFIX "")
target_link_libraries(libze ${ZE_LINK_LIBRARIES} ${CMAKE_DL_LIBS})