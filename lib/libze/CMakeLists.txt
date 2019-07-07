set(LIBZE_SOURCE_FILES libze.c system_linux.c system_linux.h ../bootloader/systemdboot.c ../../include/libze/bootloader.h)

add_library(libze ${LIBZE_SOURCE_FILES})
target_link_libraries(libze util ${ZE_LINK_LIBRARIES} )