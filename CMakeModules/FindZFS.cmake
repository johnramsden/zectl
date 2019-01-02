# ZFS Headers -----------------------------------------------------------------

# Find required ZFS include directories
message("Searching for required ZFS include directories")

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(ZFS QUIET libzfs)
    message("Found pkg-config")
else()
    find_path(ZFS_INCLUDE_DIR libzfs.h
            HINTS
            ENV ZFS_DIR
            PATH_SUFFIXES libzfs)
    message("Found zfs include dir in ${ZFS_INCLUDE_DIR}")
endif()

if(ZFS_INCLUDE_DIRS)
    message("Found zfs include directories in ${ZFS_INCLUDE_DIRS}")
    include_directories(AFTER ${ZFS_INCLUDE_DIRS})
elseif(ZFS_INCLUDE_DIR)
    message("Found zfs include directories in ${ZFS_INCLUDE_DIR}")
    include_directories(AFTER ${ZFS_INCLUDE_DIR})
else()
    message(FATAL_ERROR "Failed to find ZFS include directories")
endif()

# ZFS Libraries ---------------------------------------------------------------

# Find required ZFS libraries

list(APPEND ZE_LINK_LIBRARIES "")

message("\nSearching for ZFS libraries")

find_library(LIBZFS_LIB REQUIRED NAMES libzfs zfs)
find_library(LIBZPOOL_LIB REQUIRED NAMES libzpool zpool)
find_library(LIBZFS_CORE_LIB REQUIRED NAMES libzfs_core zfs_core)
find_library(LIBNVPAIR_LIB REQUIRED NAMES libnvpair nvpair)
find_library(LIBUUTIL_LIB REQUIRED NAMES libuutil uutil)
find_library(LIBSPL_LIB REQUIRED NAMES libspl spl)

if(LIBZFS_LIB)
    message("libzfs library found at: ${LIBZFS_LIB}")
    list(APPEND ZE_LINK_LIBRARIES "${LIBZFS_LIB}")
else()
    message(FATAL_ERROR "libzfs library not found")
endif()

if(LIBZPOOL_LIB)
    message("libzpool library found at: ${LIBZPOOL_LIB}")
    list(APPEND ZE_LINK_LIBRARIES "${LIBZPOOL_LIB}")
else()
    message(FATAL_ERROR "libzpool library not found")
endif()

if(LIBZFS_CORE_LIB)
    message("libzfs_core library found at: ${LIBZFS_CORE_LIB}")
    list(APPEND ZE_LINK_LIBRARIES "${LIBZFS_CORE_LIB}")
else()
    message(FATAL_ERROR "libzfs_core library not found")
endif()

if(LIBNVPAIR_LIB)
    message("libnvpair library found at: ${LIBNVPAIR_LIB}")
    list(APPEND ZE_LINK_LIBRARIES "${LIBNVPAIR_LIB}")
else()
    message(FATAL_ERROR "libnvpair library not found")
endif()

if(LIBUUTIL_LIB)
    message("libuutil library found at: ${LIBUUTIL_LIB}")
    list(APPEND ZE_LINK_LIBRARIES "${LIBUUTIL_LIB}")
else()
    message(FATAL_ERROR "libuutil library not found")
endif()