set(LIBZE_SOURCE_FILES
        libze.c system_linux.c system_linux.h
        bootloader/bootloader.c bootloader/systemdboot.c)

add_library(libze ${LIBZE_SOURCE_FILES})
target_link_libraries(libze util ${ZE_LINK_LIBRARIES})