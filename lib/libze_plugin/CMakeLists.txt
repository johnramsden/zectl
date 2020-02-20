project(libze_plugin_systemdboot C)
set (CMAKE_C_STANDARD 11)

set(PLUGIN_SOURCE_FILES libze_plugin_systemdboot.c)

add_library(libze_plugin_systemdboot SHARED ${PLUGIN_SOURCE_FILES})
set_property(TARGET libze_plugin_systemdboot PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET libze_plugin_systemdboot PROPERTY PREFIX "")

set(PLUGINS_DIRECTORY "" CACHE STRING "Plugins directory")
if(NOT PLUGINS_DIRECTORY)
    set(PLUGINS_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../libze_plugin")
endif()

message("PLUGINS_DIRECTORY=${PLUGINS_DIRECTORY}")

set_target_properties(libze_plugin_systemdboot PROPERTIES
        IMPORTED_LOCATION_NOCONFIG "${PLUGINS_DIRECTORY}/libze_plugin_systemdboot.so"
        IMPORTED_SONAME_NOCONFIG "libze_plugin_systemdboot.so"
)

install(TARGETS libze_plugin_systemdboot DESTINATION ${PLUGINS_DIRECTORY})