cmake_minimum_required(VERSION 3.10)

project(zectl LANGUAGES C VERSION 0.1.6 DESCRIPTION "BE Manager")

set (CMAKE_C_STANDARD 11)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules/")

# Test these when checking for bugs
#add_compile_options(
#        -O2
#        -pipe
#        -fno-plt
#        -fexceptions
#        -Wp,-D_FORTIFY_SOURCE=3
#        -Wformat
#        -Werror=format-security
#        -fstack-clash-protection
#        -fcf-protection
#        -fno-omit-frame-pointer
#        -mno-omit-leaf-frame-pointer
#)
#
#add_link_options(
#        -O2
#        -pipe
#        -fno-plt
#        -fexceptions
#        -Wp,-D_FORTIFY_SOURCE=3
#        -Wformat
#        -Werror=format-security
#        -fstack-clash-protection
#        -fcf-protection
#        -fno-omit-frame-pointer
#        -mno-omit-leaf-frame-pointer
#)

include_directories(include)

include(FindZFS)

if (CMAKE_VERSION VERSION_LESS 3.12)
    add_definitions(-DDEBUG=0)
else()
    add_compile_definitions(DEBUG=0)
endif()

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(docs)
add_subdirectory(tests)

unset(PLUGINS_DIRECTORY CACHE)
