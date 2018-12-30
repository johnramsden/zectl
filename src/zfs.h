//
// Created by john on 12/26/18.
//

#ifndef ZECTL_ZFS_H
#define ZECTL_ZFS_H

#include <sys/nvpair.h>
#include <libzfs_core.h>

#include "common.h"
#include "util.h"

ze_error_t zfs_run_channel_program(const char *zcp_file, const char *pool);

#endif //ZECTL_ZFS_H
