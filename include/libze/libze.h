//
// Created by john on 12/28/18.
//

#ifndef ZECTL_LIBZE_H
#define ZECTL_LIBZE_H

#include <libzfs/libzfs.h>

#define LIBZE_MAXPATHLEN    512

extern const char *ZE_PROP_NAMESPACE;

typedef enum libze_error {
    LIBZE_ERROR_SUCCESS = 0,
    LIBZE_ERROR_LIBZFS,
    LIBZE_ERROR_ZFS_OPEN,
    LIBZE_ERROR_UNKNOWN,
    LIBZE_ERROR_EPERM,
    LIBZE_ERROR_NOMEM,
    LIBZE_ERROR_EEXIST,         /* Dataset/fs/snapshot doesn't exist */
    LIBZE_ERROR_MAXPATHLEN,     /* Dataset/fs/snapshot exceeds LIBZE_MAXPATHLEN */
    LIBZE_ERROR_PLUGIN
} libze_error;

typedef struct libze_handle libze_handle;
typedef struct libze_plugin_fn_export libze_plugin_fn_export;

/**
 * @struct libze_handle
 * @brief Used for majority of libze functions.
 *
 * @invariant Initialized with libze_init:
 * @invariant (lzh != NULL) && (lzph != NULL)
 * @invariant ze_props != NULL
 * @invariant strlen(be_root) >= 1
 * @invariant strlen(rootfs) >= 3
 * @invariant strlen(bootfs) >= 3
 * @invariant strlen(zpool) >= 1
 *
 * @invariant Closed with libze_fini:
 * @invariant lzh, lzph are closed and NULL
 * @invariant ze_props has been free'd and is NULL
 */
struct libze_handle {
    libzfs_handle_t *lzh;              /**< Handle to libzfs                   */
    zpool_handle_t *lzph;              /**< Handle to current zpool            */
    char be_root[LIBZE_MAXPATHLEN];    /**< Dataset root of boot environments  */
    char rootfs[LIBZE_MAXPATHLEN];     /**< Root dataset (current mounted '/') */
    char bootfs[LIBZE_MAXPATHLEN];     /**< Dataset set to bootfs              */
    char zpool[LIBZE_MAXPATHLEN];      /**< ZFS pool name                      */
    libze_plugin_fn_export *lz_funcs;  /**< Pointer to bootloader plugin       */
    nvlist_t *ze_props;                /**< User org.zectl properties          */
    char libze_err[LIBZE_MAXPATHLEN];  /**< Last error buffer                  */
};

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

typedef struct libze_destroy_options {
    char *be_name;
    boolean_t noconfirm;
    boolean_t destroy_origin;
    boolean_t force;
} libze_destroy_options;

libze_error
libze_activate(libze_handle *lzeh, libze_activate_options *options);
libze_error
libze_destroy(libze_handle *lzeh, libze_destroy_options *options);

libze_error
libze_bootloader_init(libze_handle *lzeh, libze_bootloader *bootloader, const char ze_namespace[static 1]);
libze_error
libze_bootloader_fini(libze_bootloader *bootloader);

libze_error
libze_error_set(libze_handle *lzeh, libze_error lze_err, const char *lze_fmt, ...);
libze_error
libze_error_nomem(libze_handle *lzeh);

libze_error
libze_set_default_props(libze_handle *lzeh, nvlist_t *default_prop, const char namespace[static 1]);
libze_error
libze_add_default_prop(nvlist_t **prop_out, const char name[static 3], const char value[static 1],
                       const char *namespace);
#endif //ZECTL_LIBZE_H
