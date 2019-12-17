#ifndef ZECTL_LIBZE_H
#define ZECTL_LIBZE_H

#include <libzfs/libzfs.h>

#define LIBZE_MAX_ERROR_LEN    1024

// 255 in case mounted on non-ZFS
#define LIBZE_MAX_PATH_LEN    255

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
    LIBZE_ERROR_PLUGIN,
    LIBZE_ERROR_PLUGIN_EEXIST
} libze_error;

typedef struct libze_handle libze_handle;
typedef struct libze_plugin_fn_export libze_plugin_fn_export;

/**
 * @struct
 * @brief
 *
 * @invariant if bootpool: ((boot_pool.lzbph != NULL) && (strlen(boot_pool_root) >= 1))
 * @invariant if no bootpool: ((boot_pool.lzbph == NULL) && (strlen(boot_pool_root) == 0))
 */
typedef struct boot_pool {
    zfs_handle_t *lzbph;                              /**< Handle to current boot dataset   */
    char boot_pool_root[ZFS_MAX_DATASET_NAME_LEN];    /**< Dataset root for boot dataset(s) */
    char boot_pool_dataset[ZFS_MAX_DATASET_NAME_LEN]; /**< Actual dataset with boot files   */
} boot_pool;

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
 * @invariant strlen(libze_error_message) == 0
 * @invariant libze_error == LIBZE_ERROR_SUCCESS
 *
 * @invariant Closed with libze_fini:
 * @invariant lzh, lzph are closed and NULL
 * @invariant ze_props has been freed and is NULL
 */
struct libze_handle {
    libzfs_handle_t *lzh;                           /**< Handle to libzfs                         */
    zpool_handle_t *lzph;                           /**< Handle to current zpool                  */
    char be_root[ZFS_MAX_DATASET_NAME_LEN];         /**< Dataset root of boot environments        */
    char rootfs[ZFS_MAX_DATASET_NAME_LEN];          /**< Root dataset (current mounted '/')       */
    char bootfs[ZFS_MAX_DATASET_NAME_LEN];          /**< Activated boot dataset                   */
    char zpool[ZFS_MAX_DATASET_NAME_LEN];           /**< ZFS pool name                            */
    boot_pool bootpool;                             /**< boot pool                                */
    libze_plugin_fn_export *lz_funcs;               /**< Pointer to bootloader plugin             */
    nvlist_t *ze_props;                             /**< User org.zectl properties                */
    char libze_error_message[LIBZE_MAX_ERROR_LEN];  /**< Last error buffer                        */
    libze_error libze_error;                        /**< Last error buffer                        */
};

typedef struct libze_clone_cbdata {
    nvlist_t **outnvl;
    libze_handle *lzeh;
    boolean_t recursive;
} libze_clone_cbdata;

libze_handle *
libze_init(void);

void
libze_fini(libze_handle *lzeh);

libze_error
libze_boot_pool_set(libze_handle *lzeh);

libze_error
libze_clone(libze_handle *lzeh, char source_root[static 1], char source_snap_suffix[static 1], char be[static 1],
            boolean_t recursive);

int
libze_boot_env_name(const char *dataset, size_t buflen, char *buf);

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

typedef struct libze_create_options {
    boolean_t existing;
    boolean_t recursive;
    char be_name[ZFS_MAX_DATASET_NAME_LEN];
    char be_source[ZFS_MAX_DATASET_NAME_LEN];
} libze_create_options;

libze_error
libze_activate(libze_handle *lzeh, libze_activate_options *options);

libze_error
libze_create(libze_handle *lzeh, libze_create_options *options);

libze_error
libze_destroy(libze_handle *lzeh, libze_destroy_options *options);

libze_error
libze_list(libze_handle *lzeh, nvlist_t **outnvl);

libze_error
libze_mount(libze_handle *lzeh, const char boot_environment[static 1],
        const char *mountpoint, char mountpoint_buffer[LIBZE_MAX_PATH_LEN]);

libze_error
libze_rename(libze_handle *lzeh, const char boot_environment[static 1],
             const char new_boot_environment[static 1]);

libze_error
libze_set(libze_handle *lzeh, nvlist_t *properties);

libze_error
libze_snapshot(libze_handle *lzeh, const char boot_environment[static 1]);

libze_error
libze_unmount(libze_handle *lzeh, const char boot_environment[static 1]);

libze_error
libze_bootloader_init(libze_handle *lzeh, libze_bootloader *bootloader, const char ze_namespace[static 1]);
libze_error
libze_bootloader_fini(libze_bootloader *bootloader);

libze_error
libze_error_set(libze_handle *lzeh, libze_error lze_err, const char *lze_fmt, ...);
libze_error
libze_error_nomem(libze_handle *lzeh);
libze_error
libze_error_clear(libze_handle *lzeh);

libze_error
libze_default_props_set(libze_handle *lzeh, nvlist_t *default_prop, const char *namespace);
libze_error
libze_default_prop_add(nvlist_t **prop_out, const char *name, const char *value,
                       const char *namespace);

libze_error
libze_add_set_property(nvlist_t *properties, const char *property);
libze_error
libze_add_get_property(libze_handle *lzeh, nvlist_t **properties, const char *property);

libze_error
libze_bootloader_set(libze_handle *lzeh);

libze_error
libze_be_props_get(libze_handle *lzeh, nvlist_t **result, const char *namespace);
libze_error
libze_be_prop_get(libze_handle *lzeh, char *result_prop, const char *property,
                  const char *namespace);

#endif //ZECTL_LIBZE_H
