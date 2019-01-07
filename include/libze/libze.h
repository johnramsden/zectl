//
// Created by john on 12/28/18.
//

#ifndef ZECTL_LIBZE_H
#define ZECTL_LIBZE_H

#include <libzfs/libzfs.h>

#define ZE_MAXPATHLEN    512

typedef struct libze_handle libze_handle_t;

typedef enum libze_error {
    LIBZE_ERROR_SUCCESS = 0,     /* Success */
    LIBZE_ERROR_LIBZFS,          /* libzfs error */
    LIBZE_ERROR_UNKNOWN,         /* Unknown error */
} libze_error_t;

struct libze_handle {
    libzfs_handle_t *lzh;
    zpool_handle_t *lzph;
    char be_root[ZE_MAXPATHLEN];
    char rootfs[ZE_MAXPATHLEN];
    char bootfs[ZE_MAXPATHLEN];
    char zpool[ZE_MAXPATHLEN];
    libze_error_t error;
};

libze_handle_t *libze_init();
void libze_fini(libze_handle_t *);


libze_error_t
libze_channel_program(libze_handle_t *lzeh, const char *zcp_file, nvlist_t *nvl, nvlist_t **out_nvl);

#endif //ZECTL_LIBZE_H
