project(grub_generator C CXX)

set(CMAKE_CXX_STANDARD 11)
set(SOURCES grub_generator.cpp)

set(PLUGINS_DIRECTORY "" CACHE STRING "Plugins directory")
if(NOT PLUGINS_DIRECTORY)
    set(PLUGINS_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../libze_plugin")
endif()

add_executable(grub_generator ${SOURCES})

target_link_libraries(grub_generator libze_plugin_grub libze)

install(TARGETS grub_generator DESTINATION ${CMAKE_INSTALL_PREFIX}/grub.d)