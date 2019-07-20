project(libze_plugin_systemdboot C)
set (CMAKE_C_STANDARD 11)

set(PLUGIN_SOURCE_FILES libze_plugin_systemdboot.c)

add_library(libze_plugin_systemdboot SHARED ${PLUGIN_SOURCE_FILES})
set_property(TARGET libze_plugin_systemdboot PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET libze_plugin_systemdboot PROPERTY PREFIX "")