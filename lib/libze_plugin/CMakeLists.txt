project(libze_plugin_systemdboot C)
set (CMAKE_C_STANDARD 11)

set(PLUGIN_SOURCE_GRUB_FILES libze_plugin_grub.c)
set(PLUGIN_SOURCE_SYSTEMDBOOT_FILES libze_plugin_systemdboot.c)

add_definitions(-DSYSTEMD_SYSTEM_UNIT_PATH="/etc/systemd/system")

add_library(libze_plugin_grub SHARED ${PLUGIN_SOURCE_GRUB_FILES})
set_property(TARGET libze_plugin_grub PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET libze_plugin_grub PROPERTY PREFIX "")

add_library(libze_plugin_systemdboot SHARED ${PLUGIN_SOURCE_SYSTEMDBOOT_FILES})
set_property(TARGET libze_plugin_systemdboot PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET libze_plugin_systemdboot PROPERTY PREFIX "")