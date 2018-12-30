//
// Created by john on 12/28/18.
//

#ifndef ZECTL_ZE_H
#define ZECTL_ZE_H

#include <libzfs/libzfs.h>
#include "common.h"

#define ZE_MAXPATHLEN    512

typedef struct libze_handle libze_handle_t;

struct libze_handle {
    libzfs_handle_t *lzh;
    zpool_handle_t *lzph;
    char be_root[ZE_MAXPATHLEN];
    char rootfs[ZE_MAXPATHLEN];
    char bootfs[ZE_MAXPATHLEN];
    ze_error_t error;
};

libze_handle_t *libze_init();
void libze_fini(libze_handle_t *);

#endif //ZECTL_ZE_H
