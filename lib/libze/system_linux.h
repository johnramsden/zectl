/*
 * Copyright (c) 2018, John Ramsden.
 * https://github.com/johnramsden/zectl/blob/master/LICENSE.md
 */

#ifndef ZE_SYSTEM_LINUX_H
#define ZE_SYSTEM_LINUX_H

#include "libze/libze.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum system_fs_error {
    SYSTEM_ERR_SUCCESS = 0,
    SYSTEM_ERR_MNT_FILE, // Non-existent or invalid path to mnt file
    SYSTEM_ERR_NOT_FOUND,
    SYSTEM_ERR_WRONG_FSTYPE,
    SYSTEM_ERR_UNKNOWN
} system_fs_error;

system_fs_error
libze_dataset_from_mountpoint(char mountpoint[], size_t buflen, char dataset_buf[buflen]);

#endif // ZE_SYSTEM_LINUX_H
