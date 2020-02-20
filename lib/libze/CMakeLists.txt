cmake_minimum_required(VERSION 3.10)

project(libze VERSION 0.0.0 DESCRIPTION "ZE SFS library")

set(LIBZE_SOURCE_FILES
        libze.c system_linux.c system_linux.h
        libze_bootloader.c libze_plugin_manager.c libze_util.c)

add_library(libze SHARED ${LIBZE_SOURCE_FILES})
set_property(TARGET libze PROPERTY PREFIX "")

set(LIBZE_PUBLIC_HEADERS
        ../../include/libze/libze.h
        ../../include/libze/libze_util.h
        ../../include/libze/libze_plugin_manager.h)

set_target_properties(libze PROPERTIES
        PUBLIC_HEADER "${LIBZE_PUBLIC_HEADERS}")

target_include_directories(libze PRIVATE .)

set(PLUGINS_DIRECTORY "" CACHE STRING "Plugins directory")

if(NOT PLUGINS_DIRECTORY)
    set(PLUGINS_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../libze_plugin")
endif()
if(NOT CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR "${PLUGINS_DIRECTORY}")
endif()
if(NOT CMAKE_INSTALL_INCLUDEDIR)
    set(CMAKE_INSTALL_INCLUDEDIR "include")
endif()

message("PLUGINS_DIRECTORY=${PLUGINS_DIRECTORY}")

target_compile_definitions(libze PUBLIC PLUGINS_DIRECTORY=\"${PLUGINS_DIRECTORY}\")

target_link_libraries(libze ${ZE_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

install(TARGETS libze
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
