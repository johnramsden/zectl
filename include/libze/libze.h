//
// Created by john on 12/28/18.
//

#ifndef ZECTL_LIBZE_H
#define ZECTL_LIBZE_H

#include <libzfs/libzfs.h>

#define LIBZE_MAXPATHLEN    512

typedef enum libze_error {
    LIBZE_ERROR_SUCCESS = 0,
    LIBZE_ERROR_LIBZFS,
    LIBZE_ERROR_ZFS_OPEN,
    LIBZE_ERROR_UNKNOWN,
    LIBZE_ERROR_EPERM,
    LIBZE_ERROR_NOMEM,
    LIBZE_ERROR_EEXIST         /* Dataset/fs/snapshot doesn't exist */
} libze_error;

typedef struct libze_handle {
    libzfs_handle_t *lzh;
    zpool_handle_t *lzph;
    char be_root[LIBZE_MAXPATHLEN];
    char rootfs[LIBZE_MAXPATHLEN];
    char bootfs[LIBZE_MAXPATHLEN];
    char zpool[LIBZE_MAXPATHLEN];
} libze_handle;

typedef struct libze_clone_cbdata {
    nvlist_t **outnvl;
    libze_handle *lzeh;
    boolean_t recursive;
} libze_clone_cbdata;

libze_handle *
libze_init(void);

void
libze_fini(libze_handle *);

libze_error
libze_list(libze_handle *lzeh, nvlist_t **outnvl);

void
libze_list_free(nvlist_t *nvl);

libze_error
libze_channel_program(libze_handle *lzeh, const char *zcp_file, nvlist_t *nvl, nvlist_t **out_nvl);

libze_error
libze_clone(libze_handle *lzeh, char source_root[static 1], char source_snap_suffix[static 1], char be[static 1],
            boolean_t recursive);

int
libze_boot_env_name(const char *dataset, size_t buflen, char *buf);

int
libze_prop_prefix(const char path[static 1], size_t buflen, char buf[buflen]);

libze_error
libze_get_be_props(libze_handle *lzeh, nvlist_t **result, const char namespace[static 1]);

/* Function pointer to command */
typedef libze_error (*bootloader_func)(libze_handle *lzeh);

/* Command name -> function map */
typedef struct {
    char *name;
    bootloader_func command;
} bootloader_map;

typedef struct libze_bootloader {
    nvlist_t *prop;
    boolean_t set;
} libze_bootloader;

typedef struct libze_activate_options {
    char *be_name;
    boolean_t noconfirm;
} libze_activate_options;

libze_error
libze_activate(libze_handle *lzeh, libze_activate_options *options);

libze_error
libze_bootloader_init(libze_handle *lzeh, libze_bootloader *bootloader, const char ze_namespace[static 1]);
libze_error
libze_bootloader_fini(libze_bootloader *bootloader);
libze_error
libze_bootloader_systemd_pre(libze_handle *lzeh);

#endif //ZECTL_LIBZE_H
