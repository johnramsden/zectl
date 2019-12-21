#ifndef ZECTL_LIBZE_H
#define ZECTL_LIBZE_H

#include <libzfs/libzfs.h>

#define LIBZE_MAX_ERROR_LEN 1024

// 255 in case mounted on non-ZFS
#define LIBZE_MAX_PATH_LEN 255

extern const char *ZE_PROP_NAMESPACE;

typedef enum libze_error {
    LIBZE_ERROR_SUCCESS = 0,
    LIBZE_ERROR_EEXIST, /* Dataset/fs/snapshot doesn't exist */
    LIBZE_ERROR_EPERM,
    LIBZE_ERROR_LIBZFS,
    LIBZE_ERROR_INVALID_CONFIG,
    LIBZE_ERROR_MAXPATHLEN, /* Dataset/fs/snapshot exceeds LIBZE_MAXPATHLEN */
    LIBZE_ERROR_MKDIR,
    LIBZE_ERROR_MOUNT,
    LIBZE_ERROR_NOMEM,
    LIBZE_ERROR_NOT_IMPLEMENTED,
    LIBZE_ERROR_PLUGIN,
    LIBZE_ERROR_PLUGIN_EEXIST,
    LIBZE_ERROR_UNKNOWN,
    LIBZE_ERROR_ZFS_OPEN
} libze_error;

typedef struct libze_handle           libze_handle;
typedef struct libze_plugin_fn_export libze_plugin_fn_export;

/**
 * @struct
 * @brief A struct that stores the zfs handle to a separate boot pool and the user specified
 * properties of the root path and prefix in case that the system is setup to use a separate boot pool.
 *
 * @invariant If bootpool exists: pool_zhdl != NULL && all strings not empty (strlen >= 1)
 *            Else:               pool_zhdl == NULL and all strings empty (strlen == 0)
 */
typedef struct bootpool {
    zpool_handle_t *pool_zhdl; /**< A handle to the boot zpool                                           */
    char zpool_name[ZFS_MAX_DATASET_NAME_LEN]; /**< ZFS Pool name for all boot datasets of all boot environments     */
    char root_path[ZFS_MAX_DATASET_NAME_LEN];  /**< Dataset root path (e.g. "bpool/boot/env")      */
    char root_path_full[ZFS_MAX_DATASET_NAME_LEN];     /**< Dataset root path with prefix (e.g. "bpool/boot/env/ze-" or
                                                            "bpool/boot/env/" if no prefix is set) */
    char dataset_prefix[ZFS_MAX_DATASET_NAME_LEN];     /**< Dataset prefix (e.g. "ze" for "ROOT_PATH/ze-ENV") */
    char env_activated_path[ZFS_MAX_DATASET_NAME_LEN]; /**< Path to the boot dataset of the currently activated boot
                                                            environment */
    char env_running_path[ZFS_MAX_DATASET_NAME_LEN];   /**< Path to the boot dataset of the currently running boot
                                                            environment   */
} bootpool_t;

/**
 * @struct
 * @brief A struct that holds a handle and the path to a ZFS dataset.
 *
 * @invariant if linked boot dataset available: ((dataset_zhdl != NULL) && (strlen(dataset_path) >= 1))
 *            else                              ((dataset_zhdl == NULL) && (strlen(dataset_path) == 0))
 */
typedef struct zfs_dataset_t {
    zfs_handle_t *dataset_zhdl;                           /**< Handle to opened ZFS datset */
    char          dataset_path[ZFS_MAX_DATASET_NAME_LEN]; /**< Actual path to the dataset  */
} zfs_dataset_t;

/**
 * @struct libze_handle
 * @brief Used for majority of libze functions.
 *
 * @invariant Initialized with libze_init:
 * @invariant (lzh != NULL) && (lzph != NULL)
 * @invariant ze_props != NULL
 * @invariant strlen(env_pool) >= 1
 * @invariant strlen(env_root) >= 1
 * @invariant strlen(env_activated_path) >= 3
 * @invariant strlen(env_running_path) >= 3
 * @invariant strlen(libze_error_message) == 0
 * @invariant libze_error == LIBZE_ERROR_SUCCESS
 *
 * @invariant Closed with libze_fini:
 * @invariant lzh, pool_zhdl are closed and NULL
 * @invariant ze_props has been freed and is NULL
 */
struct libze_handle {
    libzfs_handle_t *lzh;                         /**< Handle to libzfs                                           */
    zpool_handle_t * pool_zhdl;                   /**< Handle to current zpool                                    */
    char env_pool[ZFS_MAX_DATASET_NAME_LEN];      /**< ZFS pool name of all boot environments                     */
    char env_root[ZFS_MAX_DATASET_NAME_LEN];      /**< Dataset root path of all boot environments                 */
    char env_activated[ZFS_MAX_DATASET_NAME_LEN]; /**< Currently activated boot environment                       */
    char env_activated_path[ZFS_MAX_DATASET_NAME_LEN]; /**< Path of the currently activated boot environment */
    char env_running[ZFS_MAX_DATASET_NAME_LEN];      /**< Currently running boot environment                         */
    char env_running_path[ZFS_MAX_DATASET_NAME_LEN]; /**< Path to the currently running boot environment             */
    bootpool_t              bootpool;                /**< Stores information about an additional bootpool if present */
    libze_plugin_fn_export *lz_funcs;                /**< Pointer to bootloader plugin                               */
    nvlist_t *              ze_props;                /**< User org.zectl properties                                  */
    char libze_error_message[LIBZE_MAX_ERROR_LEN];   /**< Last error buffer                                          */
    libze_error libze_error;                         /**< Last error buffer                                          */
};

typedef struct libze_clone_cbdata {
    nvlist_t **   outnvl;
    libze_handle *lzeh;
    boolean_t     recursive;
} libze_clone_cbdata;

/* Function pointer to command */
typedef libze_error (*bootloader_func)(libze_handle *lzeh);

/* Command name -> function map */
typedef struct {
    char *          name;
    bootloader_func command;
} bootloader_map;

typedef struct libze_bootloader {
    nvlist_t *prop;
    boolean_t set;
} libze_bootloader;

typedef struct libze_activate_options {
    char *    be_name;
    boolean_t noconfirm;
} libze_activate_options;

typedef struct libze_destroy_options {
    char *    be_name;
    boolean_t noconfirm;
    boolean_t destroy_origin;
    boolean_t force;
} libze_destroy_options;

typedef struct libze_create_options {
    boolean_t existing;
    boolean_t recursive;
    char      be_name[ZFS_MAX_DATASET_NAME_LEN];
    char      be_source[ZFS_MAX_DATASET_NAME_LEN];
} libze_create_options;

libze_error libze_activate(libze_handle *lzeh, libze_activate_options *options);
libze_error libze_clone(libze_handle *lzeh, char source_root[static 1], char source_snap_suffix[static 1],
                        char be[static 1], boolean_t recursive);
libze_error libze_create(libze_handle *lzeh, libze_create_options *options);
libze_error libze_destroy(libze_handle *lzeh, libze_destroy_options *options);
libze_error libze_list(libze_handle *lzeh, nvlist_t **outnvl);
libze_error libze_mount(libze_handle *lzeh, const char boot_environment[static 1], const char *mountpoint,
                        char mountpoint_buffer[LIBZE_MAX_PATH_LEN]);
libze_error libze_rename(libze_handle *lzeh, const char boot_environment[static 1],
                         const char new_boot_environment[static 1]);
libze_error libze_set(libze_handle *lzeh, nvlist_t *properties);
libze_error libze_snapshot(libze_handle *lzeh, const char boot_environment[static 1]);
libze_error libze_unmount(libze_handle *lzeh, const char boot_environment[static 1]);

libze_handle *libze_init(void);
void libze_fini(libze_handle *lzeh);

libze_error libze_boot_pool_set(libze_handle *lzeh);
libze_error libze_boot_env_name(libze_handle *lzeh, const char *dataset, size_t buflen, char *buf);

libze_error libze_bootloader_init(libze_handle *lzeh, libze_bootloader *bootloader, const char ze_namespace[static 1]);
libze_error libze_bootloader_fini(libze_bootloader *bootloader);
libze_error libze_bootloader_set(libze_handle *lzeh);

libze_error libze_error_set(libze_handle *lzeh, libze_error lze_err, const char *lze_fmt, ...);
libze_error libze_error_nomem(libze_handle *lzeh);
libze_error libze_error_clear(libze_handle *lzeh);

libze_error libze_default_props_set(libze_handle *lzeh, nvlist_t *default_prop, const char *namespace);
libze_error libze_default_prop_add(nvlist_t **prop_out, const char *name, const char *value, const char *namespace);

libze_error libze_add_set_property(nvlist_t *properties, const char *property);
libze_error libze_add_get_property(libze_handle *lzeh, nvlist_t **properties, const char *property);

libze_error libze_be_props_get(libze_handle *lzeh, nvlist_t **result, const char *namespace);
libze_error libze_be_prop_get(libze_handle *lzeh, char *result_prop, const char *property, const char *namespace);

#endif // ZECTL_LIBZE_H
