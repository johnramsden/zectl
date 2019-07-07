//
// Created by john on 12/28/18.
//

#ifndef ZECTL_LIBZE_H
#define ZECTL_LIBZE_H

#include <libzfs/libzfs.h>

#define ZE_MAXPATHLEN    512

typedef enum libze_error {
    LIBZE_ERROR_SUCCESS = 0,
    LIBZE_ERROR_LIBZFS,
    LIBZE_ERROR_ZFS_OPEN,
    LIBZE_ERROR_UNKNOWN,
    LIBZE_ERROR_EPERM,
    LIBZE_ERROR_NOMEM,
} libze_error_t;

typedef struct libze_handle {
    libzfs_handle_t *lzh;
    zpool_handle_t *lzph;
    char be_root[ZE_MAXPATHLEN];
    char rootfs[ZE_MAXPATHLEN];
    char bootfs[ZE_MAXPATHLEN];
    char zpool[ZE_MAXPATHLEN];
    libze_error_t error;
} libze_handle_t;

typedef struct libze_clone_cbdata {
    nvlist_t **outnvl;
    libze_handle_t *lzeh;

} libze_clone_cbdata_t;

libze_handle_t *libze_init();
void libze_fini(libze_handle_t *);


libze_error_t
libze_list(libze_handle_t *lzeh, nvlist_t **outnvl);
libze_error_t
libze_channel_program(libze_handle_t *lzeh, const char *zcp_file, nvlist_t *nvl, nvlist_t **out_nvl);
libze_error_t
libze_clone(libze_handle_t *lzeh, char source_root[static 1], char source_snap_suffix[static 1], char be[static 1]);

int
boot_env_name(const char dataset[static 1], size_t buflen, char buf[buflen]);
int
boot_env_name_children(const char root[static 1], const char dataset[static 1], size_t buflen, char buf[buflen]);

int
libze_prop_prefix(const char path[static 1], size_t buflen, char buf[buflen]);
libze_error_t
libze_get_be_props(libze_handle_t *lzeh, nvlist_t **result, const char namespace[static 1]);

libze_error_t
libze_get_be_props(libze_handle_t *lzeh, nvlist_t **result, const char *namespace);

#endif //ZECTL_LIBZE_H
