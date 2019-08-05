#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/nvpair.h>
#include <libzfs_core.h>

#include "libze/libze_plugin_manager.h"
#include "libze/libze.h"
#include "libze/libze_util.h"

// Unsigned long long is 64 bits or more
#define ULL_SIZE 128

const char *ZE_PROP_NAMESPACE = "org.zectl";

/**
 * @brief Filter out boot environment properties based on name of program namespace
 * @param[in] unfiltered_nvl @p nvlist_t to filter based on namespace
 * @param[out] result_nvl Filtered @p nvlist_t continuing only properties matching namespace
 * @param namespace Prefix property to filter based on
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_UNKNOWN on failure.
 *
 * @pre @p unfiltered_nvl != NULL
 * @pre @p namespace != NULL
 */
static libze_error
libze_filter_be_props(nvlist_t *unfiltered_nvl, nvlist_t **result_nvl,
                      const char namespace[static 1]) {
    nvpair_t *pair = NULL;
    libze_error ret = LIBZE_ERROR_SUCCESS;

    for (pair = nvlist_next_nvpair(unfiltered_nvl, NULL); pair != NULL;
         pair = nvlist_next_nvpair(unfiltered_nvl, pair)) {
        char *nvp_name = nvpair_name(pair);
        char buf[LIBZE_MAXPATHLEN];

        if (libze_util_cut(nvp_name, LIBZE_MAXPATHLEN, buf, ':') != 0) {
            return LIBZE_ERROR_UNKNOWN;
        }

        if (strcmp(buf, namespace) == 0) {
            nvlist_add_nvpair(*result_nvl, pair);
        }
    }

    return ret;
}

/**
 * @brief Get all the ZFS properties which have been set with the @p namespace prefix
 *        and return them in @a result.
 *
 *        Properties in form:
 * @verbatim
   org.zectl:bootloader:
       value: 'systemdboot'
       source: 'zroot/ROOT'
   @endverbatim
 *
 * @param[in] lzeh Initialized lzeh libze handle
 * @param[out] result Properties of boot environment
 * @param[in] namespace ZFS property prefix
 * @return LIBZE_ERROR_SUCCESS on success, or
 *         LIBZE_ERROR_ZFS_OPEN, LIBZE_ERROR_UNKNOWN, LIBZE_ERROR_NOMEM
 *
 * @pre @p lzeh != NULL
 * @pre @p namespace != NULL
 */
libze_error
libze_get_be_props(libze_handle *lzeh, nvlist_t **result, const char namespace[static 1]) {
    nvlist_t *user_props = NULL;
    nvlist_t *filtered_user_props = NULL;
    libze_error ret = LIBZE_ERROR_SUCCESS;

    zfs_handle_t *zhp = zfs_open(lzeh->lzh, lzeh->be_root, ZFS_TYPE_FILESYSTEM);
    if (zhp == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed opening handle to %s.\n", lzeh->be_root);
    }

    if ((user_props = zfs_get_user_props(zhp)) == NULL) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed to retieve user properties for %s.\n", zfs_get_name(zhp));
        goto err;
    }

    if ((filtered_user_props = fnvlist_alloc()) == NULL) {
        ret = libze_error_nomem(lzeh);
        goto err;
    }

    if ((ret = libze_filter_be_props(user_props, &filtered_user_props, namespace))
        != LIBZE_ERROR_SUCCESS) {
        goto err;
    }

    zfs_close(zhp);
    *result = filtered_user_props;
    return ret;
err:
    zfs_close(zhp);
    fnvlist_free(user_props);
    fnvlist_free(filtered_user_props);
    return ret;
}

/**
 * @brief Set an error message to @p lzeh->libze_err and return the error type
 *        given in @p lze_err.
 * @param[in,out] initialized lzeh libze handle
 * @param[in] lze_err Error value returned
 * @param[in] lze_fmt Format specifier used by @p lzeh
 * @param ... Variable args used to format the error message saved in @p lzeh
 * @return @p lze_err
 *
 * @pre @p lzeh != NULL
 * @pre if @p lze_fmt == NULL, @p ... should have zero arguments.
 * @pre Length of formatted string < @p LIBZE_MAXPATHLEN
 */
libze_error
libze_error_set(libze_handle *lzeh, libze_error lze_err, const char *lze_fmt, ...) {
    if (lzeh == NULL) {
        return lze_err;
    }

    if (lze_fmt == NULL) {
        strlcpy(lzeh->libze_err, "", LIBZE_MAXPATHLEN);
        return lze_err;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
    va_list argptr;
    va_start(argptr, lze_fmt);
    int length = vsnprintf(lzeh->libze_err, LIBZE_MAXPATHLEN, lze_fmt, argptr);
    va_end(argptr);
#pragma clang diagnostic pop

            assert(length < LIBZE_MAXPATHLEN);

    return lze_err;
}

/**
 * @brief Convenience function to set no memory error message
 * @param[in,out] initialized lzeh libze handle
 * @return @p LIBZE_ERROR_NOMEM
 *
 * @pre @p lzeh != NULL
 */
libze_error
libze_error_nomem(libze_handle *lzeh) {
    if (lzeh == NULL) {
        return LIBZE_ERROR_NOMEM;
    }
    return libze_error_set(lzeh, LIBZE_ERROR_NOMEM, "Failed to allocate memory.\n");
}

/**
 * @brief Check if a plugin is set, if it is initialize it.
 * @param lzeh Initialized @p libze_handle
 * @return @p LIBZE_ERROR_SUCCESS on success, @p LIBZE_ERROR_PLUGIN on failure
 *
 * @post if @a lzeh->ze_props contains an existing org.zectl:bootloader,
 *            the bootloader plugin is initialized.
 */
static libze_error
check_for_bootloader(libze_handle *lzeh) {
    char *plugin = NULL;
    libze_error ret = LIBZE_ERROR_SUCCESS;

    char p_buff[ZFS_MAXPROPLEN] = "";
    if (libze_util_concat(ZE_PROP_NAMESPACE, ":", "bootloader", ZFS_MAXPROPLEN, p_buff) != 0) {
        return -1;
    }
    nvlist_t *nvl;
    if (nvlist_lookup_nvlist(lzeh->ze_props, p_buff, &nvl) != 0) {
        // Unset
        return 0;
    }

    nvpair_t *pair = NULL;
    for (pair = nvlist_next_nvpair(nvl, NULL); pair != NULL;
         pair = nvlist_next_nvpair(nvl, pair)) {
        if (strcmp(nvpair_name(pair), "value") == 0) {
            plugin = fnvpair_value_string(pair);
        }
    }
    if (plugin == NULL) {
        return -1;
    }

    void *p_handle = libze_plugin_open(plugin);
    if (p_handle == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_PLUGIN,
                "Failed to open plugin %s\n", plugin);
    } else {
        if (libze_plugin_export(p_handle, &lzeh->lz_funcs) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_PLUGIN,
                    "Failed to open %s export table for plugin %s\n", plugin);
        } else {
            if (lzeh->lz_funcs->plugin_init(lzeh) != 0) {
                ret = libze_error_set(lzeh, LIBZE_ERROR_PLUGIN,
                        "Failed to initialize plugin %s\n", plugin);
            }
        }
        if (libze_plugin_close(p_handle) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_PLUGIN,
                    "Failed to close plugin %s\n", plugin);
        }
    }

    return ret;
}


/********************************************************
 ************** libze initialize / destroy **************
 ********************************************************/

/**
 * @brief Initialize libze handle.
 * @return Initialized handle, or NULL if unsuccessful.
 */
libze_handle *
libze_init(void) {
    libze_handle *lzeh = NULL;
    char *slashp = NULL;
    char *zpool = NULL;

    if ((lzeh = calloc(1, sizeof(libze_handle))) == NULL) {
        goto err;
    }
    if ((lzeh->lzh = libzfs_init()) == NULL) {
        goto err;
    }
    if (libze_get_root_dataset(lzeh) != 0) {
        goto err;
    }
    if (libze_util_cut(lzeh->rootfs, LIBZE_MAXPATHLEN, lzeh->be_root, '/') != 0) {
        goto err;
    }
    if ((slashp = strchr(lzeh->be_root, '/')) == NULL) {
        goto err;
    }

    size_t pool_length = (slashp)-(lzeh->be_root);
    zpool = calloc(1, pool_length+1);
    if (zpool == NULL) {
        goto err;
    }

    // Get pool portion of dataset
    if (strncpy(zpool, lzeh->be_root, pool_length) == NULL) {
        goto err;
    }
    zpool[pool_length] = '\0';

    if (strlcpy(lzeh->zpool, zpool, LIBZE_MAXPATHLEN) >= LIBZE_MAXPATHLEN) {
        goto err;
    }

    if ((lzeh->lzph = zpool_open(lzeh->lzh, lzeh->zpool)) == NULL) {
        goto err;
    }

    if (zpool_get_prop(lzeh->lzph, ZPOOL_PROP_BOOTFS, lzeh->bootfs,
            sizeof(lzeh->bootfs), NULL, B_TRUE) != 0) {
        goto err;
    }

    if (libze_get_be_props(lzeh, &lzeh->ze_props, ZE_PROP_NAMESPACE) != 0) {
        goto err;
    }

    if (check_for_bootloader(lzeh) != 0) {
        goto err;
    }

    // Clear any error messages
    (void)libze_error_set(lzeh, LIBZE_ERROR_SUCCESS, NULL);

    free(zpool);
    return lzeh;

err:
    libze_fini(lzeh);
    if (zpool != NULL) { free(zpool); }
    return NULL;
}

/**
 * @brief @p libze_handle cleanup.
 * @param lzeh @p libze_handle to de-allocate and close resources on.
 *
 * @post @p lzeh->lzh is closed
 * @post @p lzeh->lzph is closed
 * @post @p lzeh->ze_props is free'd
 * @post @p lzeh is free'd
 */
void
libze_fini(libze_handle *lzeh) {
    if (lzeh == NULL) {
        return;
    }

    if (lzeh->lzh != NULL) {
        libzfs_fini(lzeh->lzh);
        lzeh->lzh = NULL;
    }

    if (lzeh->lzph != NULL) {
        zpool_close(lzeh->lzph);
        lzeh->lzph = NULL;
    }

    if (lzeh->ze_props != NULL) {
        fnvlist_free(lzeh->ze_props);
        lzeh->ze_props = NULL;
    }

    free(lzeh);
    lzeh = NULL;
}

/**********************************
 ************** list **************
 **********************************/

typedef struct libze_list_cbdata {
    nvlist_t **outnvl;
    libze_handle *lzeh;
} libze_list_cbdata_t;

/**
 * @brief Callback for each boot environment.
 * @param[in] zhdl Initialized zfs handle for boot environment being acted on.
 * @param[in,out] data Pointer to initialized @p libze_list_cbdata_t struct.
 * @return Non-zero on failure.
 *
 * @pre @p zhdl != NULL
 * @pre @p data != NULL
 */
static int
libze_list_cb(zfs_handle_t *zhdl, void *data) {
    libze_list_cbdata_t *cbd = data;
    char prop_buffer[LIBZE_MAXPATHLEN];
    char dataset[LIBZE_MAXPATHLEN];
    char be_name[LIBZE_MAXPATHLEN];
    char mountpoint[LIBZE_MAXPATHLEN];
    int ret = LIBZE_ERROR_SUCCESS;
    nvlist_t *props = NULL;

    const char *handle_name = zfs_get_name(zhdl);

    if (((props = fnvlist_alloc()) == NULL)) {
        return libze_error_set(cbd->lzeh, LIBZE_ERROR_NOMEM,
                "Failed to allocate nvlist.\n");
    }

    // Name
    if (zfs_prop_get(zhdl, ZFS_PROP_NAME, dataset,
            sizeof(dataset), NULL, NULL, 0, 1) != 0) {
        ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed get 'name' property for %s.\n", handle_name);
        goto err;
    }
    fnvlist_add_string(props, "dataset", dataset);

    // Boot env name
    if (libze_boot_env_name(dataset, LIBZE_MAXPATHLEN, be_name) != 0) {
        ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed get boot environment for %s.\n", handle_name);
        goto err;
    }
    fnvlist_add_string(props, "name", be_name);

    // Mountpoint
    char mounted[LIBZE_MAXPATHLEN];
    if (zfs_prop_get(zhdl, ZFS_PROP_MOUNTED, mounted,
            sizeof(mounted), NULL, NULL, 0, 1) != 0) {
        ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed get 'mounted' for %s.\n", handle_name);
        goto err;
    }

    int is_mounted = strcmp(mounted, "yes");
    if (is_mounted == 0) {
        if (zfs_prop_get(zhdl, ZFS_PROP_MOUNTPOINT, mountpoint,
                sizeof(mountpoint), NULL, NULL, 0, 1) != 0) {
            ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                    "Failed get 'mountpoint' for %s.\n", handle_name);
            goto err;
        }
    }
    fnvlist_add_string(props, "mountpoint", (is_mounted == 0) ? mountpoint : "-");

    // Creation
    if (zfs_prop_get(zhdl, ZFS_PROP_CREATION, prop_buffer,
            sizeof(prop_buffer), NULL, NULL, 0, 1) != 0) {
        ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed get 'creation' for %s.\n", handle_name);
        goto err;
    }

    char t_buf[ULL_SIZE];
    unsigned long long int formatted_time = strtoull(prop_buffer, NULL, 10);
    // ISO 8601 date format
    if (strftime(t_buf, ULL_SIZE, "%F %H:%M", localtime((time_t *)&formatted_time)) == 0) {
        ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed get time from creation for %s.\n", handle_name);
        goto err;
    }
    fnvlist_add_string(props, "creation", t_buf);

    // Nextboot
    boolean_t is_nextboot = (strcmp(cbd->lzeh->bootfs, dataset) == 0);
    fnvlist_add_boolean_value(props, "nextboot", is_nextboot);

    // Active
    boolean_t is_active = (is_mounted == 0) && (strcmp(mountpoint, "/") == 0);
    fnvlist_add_boolean_value(props, "active", is_active);

    fnvlist_add_nvlist(*cbd->outnvl, prop_buffer, props);

    return ret;
err:
    fnvlist_free(props);
    return ret;
}

/**
 * @brief Prepare a listing with valid properies
 * @param[in] lzeh Initialized @p libze_handle
 * @param[in,out] outnvl Reference to an @p nvlist_t*, populated with valid 'list properties'
 * @return @p LIBZE_ERROR_SUCCESS on success, @p LIBZE_ERROR_LIBZFS,
 *         @p LIBZE_ERROR_ZFS_OPEN, or @p LIBZE_ERROR_NOMEM on failure.
 */
libze_error
libze_list(libze_handle *lzeh, nvlist_t **outnvl) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    if ((libzfs_core_init()) != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_LIBZFS, "Failed to initialize libzfs_core.\n");
        goto err;
    }

    // Get be root handle
    zfs_handle_t *zroot_hdl = NULL;
    if ((zroot_hdl = zfs_open(lzeh->lzh, lzeh->be_root, ZFS_TYPE_FILESYSTEM)) == NULL) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_LIBZFS,
                "Failed to open handle to %s.\n", lzeh->be_root);
        goto err;
    }

    // Out nvlist callback
    if ((*outnvl = fnvlist_alloc()) == NULL) {
        ret = libze_error_nomem(lzeh);
        goto err;
    }
    libze_list_cbdata_t cbd = {
            .outnvl = outnvl,
            .lzeh = lzeh
    };

    zfs_iter_filesystems(zroot_hdl, libze_list_cb, &cbd);

err:
    zfs_close(zroot_hdl);
    libzfs_core_fini();
    return ret;
}

/***********************************
 ************** clone **************
 ***********************************/

typedef struct libze_clone_prop_cbdata {
    libze_handle *lzeh;
    zfs_handle_t *zhp;
    nvlist_t *props;
} libze_clone_prop_cbdata_t;

/**
 * @brief Callback to run on each property
 * @param prop Current property
 * @param[in,out] data Initialized @p libze_clone_prop_cbdata_t to save props into.
 * @return @p ZPROP_CONT to continue iterating.
 */
static int
clone_prop_cb(int prop, void *data) {
    libze_clone_prop_cbdata_t *pcbd = data;

    zprop_source_t src;
    char propbuf[LIBZE_MAXPATHLEN];
    char statbuf[LIBZE_MAXPATHLEN];
    const char *prop_name;

    // Skip if readonly or canmount
    if (zfs_prop_readonly(prop)) {
        return ZPROP_CONT;
    }

    prop_name = zfs_prop_to_name(prop);

    // Always set canmount=noauto
    if (prop == ZFS_PROP_CANMOUNT) {
        fnvlist_add_string(pcbd->props, prop_name, "noauto");
        return ZPROP_CONT;
    }

    if (zfs_prop_get(pcbd->zhp, prop, propbuf, sizeof(propbuf), &src, statbuf,
            sizeof(statbuf), B_FALSE) != 0) {
        return ZPROP_CONT;
    }

    // Skip if not LOCAL and not RECEIVED
    if ((src != ZPROP_SRC_LOCAL) && (src != ZPROP_SRC_RECEIVED)) {
        return ZPROP_CONT;
    }

    if (nvlist_add_string(pcbd->props, prop_name, propbuf) != 0) {
        return ZPROP_CONT;
    }

    return ZPROP_CONT;
}

/**
 * @brief Callback run recursively on a dataset
 * @param[in] zhdl Initialized @p zfs_handle_t representing current dataset
 * @param data Initialized @p libze_clone_cbdata to save properties into.
 * @return Non-zero on success.
 */
static int
libze_clone_cb(zfs_handle_t *zhdl, void *data) {
    libze_clone_cbdata *cbd = data;
    int ret = LIBZE_ERROR_SUCCESS;
    nvlist_t *props = NULL;

    if (((props = fnvlist_alloc()) == NULL)) {
        return libze_error_nomem(cbd->lzeh);
    }

    libze_clone_prop_cbdata_t cb_data = {
            .lzeh = cbd->lzeh,
            .props = props,
            .zhp = zhdl
    };

    // Iterate over all props
    if (zprop_iter(clone_prop_cb, &cb_data,
            B_FALSE, B_FALSE, ZFS_TYPE_FILESYSTEM) == ZPROP_INVAL) {
        ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed to iterate over properties for top level dataset.\n");
        goto err;
    }

    fnvlist_add_nvlist(*cbd->outnvl, zfs_get_name(zhdl), props);

    if (cbd->recursive) {
        if (zfs_iter_filesystems(zhdl, libze_clone_cb, cbd) != 0) {
            ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                    "Failed to iterate over child datasets.\n");
            goto err;
        }
    }

    return ret;
err:
    fnvlist_free(props);
    return ret;
}

/**
 * @brief Create a recursive clone from a snapshot given the dataset and snapshot separately.
 *        The snapshot suffix should be the same for all nested datasets.
 * @param lzeh Initialized libze handle
 * @param source_root Top level dataset for clone.
 * @param source_snap_suffix Snapshot name.
 * @param be Name for new boot environment
 * @param recursive Do recursive clone
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_ZFS_OPEN, @p LIBZE_ERROR_UNKNOWN,
 *         or @p LIBZE_ERROR_MAXPATHLEN on failure.
 *
 * @pre lzeh != NULL
 * @pre source_root != NULL
 * @pre source_snap_suffix != NULL
 * @pre be != NULL
 */
libze_error
libze_clone(libze_handle *lzeh, char source_root[static 1], char source_snap_suffix[static 1], char be[static 1],
            boolean_t recursive) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    nvlist_t *cdata = NULL;
    if ((cdata = fnvlist_alloc()) == NULL) {
        return libze_error_nomem(lzeh);
    }

    // Get be root handle
    zfs_handle_t *zroot_hdl = NULL;
    if ((zroot_hdl = zfs_open(lzeh->lzh, source_root, ZFS_TYPE_FILESYSTEM)) == NULL) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN,
                "Error opening %s", source_root);
        goto err;
    }

    libze_clone_cbdata cbd = {
            .outnvl = &cdata,
            .lzeh = lzeh,
            .recursive = recursive
    };

    // Get properties for bootfs and under bootfs
    if (libze_clone_cb(zroot_hdl, &cbd) != 0) {
        // libze_clone_cb sets error message.
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }

    if (recursive) {
        if (zfs_iter_filesystems(zroot_hdl, libze_clone_cb, &cbd) != 0) {
            // libze_clone_cb sets error message.
            ret = LIBZE_ERROR_UNKNOWN;
            goto err;
        }
    }

    nvpair_t *pair = NULL;
    for (pair = nvlist_next_nvpair(cdata, NULL); pair != NULL;
         pair = nvlist_next_nvpair(cdata, pair)) {
        nvlist_t *ds_props = NULL;
        nvpair_value_nvlist(pair, &ds_props);

        // Recursive clone
        char *ds_name = nvpair_name(pair);
        char ds_snap_buf[ZFS_MAX_DATASET_NAME_LEN] = "";
        char be_child_buf[ZFS_MAX_DATASET_NAME_LEN] = "";
        char ds_child_buf[ZFS_MAX_DATASET_NAME_LEN] = "";
        if (libze_util_suffix_after_string(source_root, ds_name, ZFS_MAX_DATASET_NAME_LEN, be_child_buf) == 0) {
            if (strlen(be_child_buf) > 0) {
                if (libze_util_concat(be, be_child_buf, "/", ZFS_MAX_DATASET_NAME_LEN, ds_child_buf) != 0) {
                    ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                            "Requested child clone exceeds max length %d\n", ZFS_MAX_DATASET_NAME_LEN);
                    goto err;
                }
            } else {
                // Child empty
                if (strlcpy(ds_child_buf, be, ZFS_MAX_DATASET_NAME_LEN) >= ZFS_MAX_DATASET_NAME_LEN) {
                    ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                            "No child clone found\n");
                    goto err;
                }
            }
        }

        if (libze_util_concat(ds_name, "@", source_snap_suffix,
                ZFS_MAX_DATASET_NAME_LEN, ds_snap_buf) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                    "Requested snapshot exceeds max length %d\n", ZFS_MAX_DATASET_NAME_LEN);
            goto err;
        }

        zfs_handle_t *snap_handle = NULL;
        if ((snap_handle = zfs_open(lzeh->lzh, ds_snap_buf, ZFS_TYPE_SNAPSHOT)) == NULL) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN,
                    "Error opening %s", ds_snap_buf);
            goto err;
        }
        if (zfs_clone(snap_handle, ds_child_buf, ds_props) != 0) {
            zfs_close(snap_handle);
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Clone error %s", ds_child_buf);
            goto err;
        }

        zfs_close(snap_handle);
    }

err:
    libze_list_free(cdata);
    zfs_close(zroot_hdl);
    return ret;
}

/*************************************
 ************** destroy **************
 *************************************/

typedef struct libze_destroy_cbdata {
    libze_handle *lzeh;
    libze_destroy_options *options;
} libze_destroy_cbdata;

static int
libze_destroy_cb(zfs_handle_t *zh, void *data) {
    int ret = 0;
    libze_destroy_cbdata *cbd = data;

    const char *ds = zfs_get_name(zh);
    if (zfs_is_mounted(zh, NULL) && cbd->options->force) {
        zfs_unmount(zh, NULL, 0);
    } else {
        return libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                "Dataset %s is mounted, run with force or unmount dataset\n", ds);
    }

    // Check if clone, origin snapshot saved to buffer
    char buf[LIBZE_MAXPATHLEN];
    if (zfs_prop_get(zh, ZFS_PROP_ORIGIN, buf, sizeof(buf), NULL, NULL, 0, 1) == 0) {
        // Is a clone, continue
        if (cbd->options->destroy_origin) {
            // Destroy origin snapshot
            zfs_handle_t *origin_h = zfs_open(cbd->lzeh->lzh, buf, ZFS_TYPE_SNAPSHOT);
            if (origin_h != NULL) {
                if (zfs_destroy(origin_h, B_FALSE) != 0) {
                    zfs_close(origin_h);
                    return libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                            "Failed to destroy origin snapshot %s\n", buf);
                }
            }
            zfs_close(origin_h);
        }
        // TODO: If dependent clones exist, should we promote them? Not sure if that ever occurs.
    }

    // Destroy children recursively
    if (zfs_iter_children(zh, libze_destroy_cb, cbd) != 0) {
        return libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed to iterate over children of %s\n", ds);
    }
    // Destroy dataset
    if (zfs_destroy(zh, B_FALSE) != 0) {
        return libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed to destroy dataset %s\n", ds);
    }

    return ret;
}

/**
 * @brief Destroy a boot environment clone or dataset
 * @param lzeh Initialized @p libze_handle
 * @param options Destroy options
 * @param filesystem Clone or boot environment full name
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_ZFS_OPEN if @p filesystem can't be opened,
 *         @p LIBZE_ERROR_EEXIST if @p filesystem doesnt exist,
 */
static libze_error
destroy_filesystem(libze_handle *lzeh, libze_destroy_options *options,
                 const char filesystem[ZFS_MAX_DATASET_NAME_LEN]) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    if (!zfs_dataset_exists(lzeh->lzh, filesystem, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                "Dataset %s does not exist\n", filesystem);
    }
    zfs_handle_t *be_zh = NULL;
    be_zh = zfs_open(lzeh->lzh, filesystem, ZFS_TYPE_FILESYSTEM);
    if (be_zh == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN,
                "Failed opening dataset %s\n", filesystem);
    }

    libze_destroy_cbdata cbd = {
            .lzeh = lzeh,
            .options = options
    };

    if (libze_destroy_cb(be_zh, &cbd) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
    }
    zfs_close(be_zh);

    return ret;
}

/**
 * @brief Destroy a boot environment snapshot
 * @param lzeh Initialized @p libze_handle
 * @param options Destroy options
 * @param snapshot Snapshot full name
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_ZFS_OPEN if @p snapshot dataset can't be opened,
 *         @p LIBZE_ERROR_EEXIST if @p snapshot doesnt exist, or isnt a BE,
 *         @p LIBZE_ERROR_MAXPATHLEN if snapshot name is too long
 */
static libze_error
destroy_snapshot(libze_handle *lzeh, libze_destroy_options *options,
        const char snapshot[ZFS_MAX_DATASET_NAME_LEN]) {
    if (!zfs_dataset_exists(lzeh->lzh, snapshot, ZFS_TYPE_SNAPSHOT)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                "Snapshot %s does not exist\n", snapshot);
    }
    char be_snap_ds_buff[ZFS_MAX_DATASET_NAME_LEN] = "";
    char be_full_ds[ZFS_MAX_DATASET_NAME_LEN] = "";

    // Get boot environment name, we know ZFS_MAX_DATASET_NAME_LEN wont be exceeded
    (void) libze_util_cut(options->be_name, ZFS_MAX_DATASET_NAME_LEN, be_snap_ds_buff, '@');

    // Join BE name with BE root to verify requested snap is from a BE
    if (libze_util_concat(lzeh->be_root, "/", be_snap_ds_buff,
            ZFS_MAX_DATASET_NAME_LEN, be_full_ds) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Requested boot environment %s exceeds max length %d\n",
                options->be_name, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (!zfs_name_valid(be_full_ds, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                "Invalid dataset %s\n", be_full_ds);
    }

    if (!zfs_dataset_exists(lzeh->lzh, be_full_ds, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                "Dataset %s does not exist\n", be_full_ds);
    }

    zfs_handle_t *be_zh = zfs_open(lzeh->lzh, snapshot, ZFS_TYPE_SNAPSHOT);
    if (be_zh == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN,
                "Failed opening dataset %s\n", snapshot);
    }

    if (zfs_destroy(be_zh, B_FALSE) != 0) {
        zfs_close(be_zh);
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                "Failed to destroy snapshot %s\n", snapshot);
    }
    zfs_close(be_zh);

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief Destroy a boot environment
 * @param lzeh Initialized @p libze_handle
 * @param options Destroy options
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN If the requested environment to destroy is too long,
 *         @p LIBZE_ERROR_UNKNOWN If the environment is active,
 *         @p LIBZE_ERROR_EEXIST If the environment doesn't exist,
 *         @p LIBZE_ERROR_ZFS_OPEN If the dataset couldn't be opened,
 *         @p LIBZE_ERROR_PLUGIN If the plugin hook failed
 */
libze_error
libze_destroy(libze_handle *lzeh, libze_destroy_options *options) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    char be_ds_buff[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_util_concat(lzeh->be_root, "/", options->be_name, ZFS_MAX_DATASET_NAME_LEN, be_ds_buff) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Requested boot environment %s exceeds max length %d\n",
                options->be_name, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (libze_is_active_be(lzeh, be_ds_buff)) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Cannot destroy active boot environment %s\n", options->be_name);
    }
    if (libze_is_root_be(lzeh, be_ds_buff)) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Cannot destroy root boot environment %s\n", options->be_name);
    }

    if ((strchr(be_ds_buff, '@') == NULL)) {
        if ((ret = destroy_filesystem(lzeh, options, be_ds_buff)) != LIBZE_ERROR_SUCCESS) {
            return ret;
        }
    } else {
        if ((ret = destroy_snapshot(lzeh, options, be_ds_buff)) != LIBZE_ERROR_SUCCESS) {
            return ret;
        }
    }

    if ((lzeh->lz_funcs != NULL) && (lzeh->lz_funcs->plugin_post_destroy(lzeh, options->be_name) != 0)) {
        return LIBZE_ERROR_PLUGIN;
    }

    return ret;
}

/**************************************
 ************** activate **************
 **************************************/

typedef struct libze_activate_cbdata {
    libze_handle *lzeh;
} libze_activate_cbdata;

/**
 * @brief Callback run for ever sub dataset of @p zhdl
 * @param[in] zhdl Initialed @p zfs_handle_t to recurse based on.
 * @param[in,out] data @p libze_activate_cbdata to activate based on.
 * @return Non zero on failure.
 *
 * @pre zhdl != NULL
 * @pre data != NULL
 */
static int
libze_activate_cb(zfs_handle_t *zhdl, void *data) {
    char buf[LIBZE_MAXPATHLEN];
    libze_activate_cbdata *cbd = data;

    if (zfs_prop_set(zhdl, "canmount", "noauto") != 0) {
        return libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed setting canmount=noauto for %s\n", zfs_get_name(zhdl));
    }

    // Check if clone
    if (zfs_prop_get(zhdl, ZFS_PROP_ORIGIN, buf,
            sizeof(buf), NULL, NULL, 0, 1) != 0) {
        // Not a clone, continue
        return 0;
    }

    if (zfs_promote(zhdl) != 0) {
        return libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed promoting %s\n", zfs_get_name(zhdl));
    }

    if (zfs_iter_filesystems(zhdl, libze_activate_cb, cbd) != 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief Function run mid-activate, execute plugin if it exists.
 * @param[in] lzeh Initialized @p libze_handle
 * @param[in] options Options to set properties based on
 * @param[in] be_zh Initialized @p zfs_handle_t to run mid activate upon
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_UNKNOWN, or @p LIBZE_ERROR_PLUGIN on failure.
 *
 * @pre lzeh != NULL
 * @pre be_zh != NULL
 * @pre options != NULL
 * @post if be_zh != root dataset, be_zh unmounted on exit
 */
static libze_error
mid_activate(libze_handle *lzeh, libze_activate_options *options, zfs_handle_t *be_zh) {
    libze_error ret = LIBZE_ERROR_SUCCESS;
    char *tmp_dirname = "/";
    const char *ds_name = zfs_get_name(be_zh);
    boolean_t is_root = libze_is_root_be(lzeh, ds_name);

    nvlist_t *props = fnvlist_alloc();
    if (props == NULL) {
        return libze_error_nomem(lzeh);
    }
    nvlist_add_string(props, "canmount", "noauto");

    if (is_root) {
        nvlist_add_string(props, "mountpoint", "/");

        // Not currently mounted
        char tmpdir_template[LIBZE_MAXPATHLEN] = "";
        if (libze_util_concat("/tmp/ze.", options->be_name, ".XXXXXX",
                LIBZE_MAXPATHLEN, tmpdir_template) != 0) {
            return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                    "Could not create directory template\n");
        }

        // Create tmp mountpoint
        tmp_dirname = mkdtemp(tmpdir_template);
        if (tmp_dirname == NULL) {
            return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                    "Could not create tmp directory %s\n", tmpdir_template);
        }

        // AFTER here always goto err to cleanup

        if (zfs_prop_set(be_zh, "mountpoint", tmp_dirname) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                    "Failed to set mountpoint=%s for %s\n", tmpdir_template, ds_name);
            goto err;
        }

        if (zfs_mount(be_zh, NULL, 0) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                    "Failed to mount %s to %s\n", ds_name, tmpdir_template);
            goto err;
        }
    }

    // mid_activate
    if ((lzeh->lz_funcs != NULL) &&
        (lzeh->lz_funcs->plugin_mid_activate(lzeh, tmp_dirname) != 0)) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_PLUGIN,
                "Failed to run mid-activate hook\n");
        goto err;
    }

err:
    if (!is_root && zfs_is_mounted(be_zh, NULL)) {
        // Retain existing error if occurrred
        if ((zfs_unmount(be_zh, NULL, 0) != 0) && (ret != LIBZE_ERROR_SUCCESS)) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                    "Failed to unmount %s", ds_name);
        } else {
            rmdir(tmp_dirname);
            if ((zfs_prop_set_list(be_zh, props) != 0) && (ret != LIBZE_ERROR_SUCCESS)) {
                ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                        "Failed to unset mountpoint for %s:\n", ds_name);
            }
        }
    }
    nvlist_free(props);
    return ret;
}

/**
 * @brief Based on @p options, activate a boot environment
 * @param[in] lzeh Initialized lzeh libze handle
 * @param options The options based on which the boot environment is activated
 * @return LIBZE_ERROR_SUCCESS on success,
 *         or LIBZE_ERROR_EEXIST, LIBZE_ERROR_PLUGIN, LIBZE_ERROR_ZFS_OPEN,
 *         LIBZE_ERROR_UNKNOWN
 */
libze_error
libze_activate(libze_handle *lzeh, libze_activate_options *options) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    char be_ds_buff[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_util_concat(lzeh->be_root, "/", options->be_name, ZFS_MAX_DATASET_NAME_LEN, be_ds_buff) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Requested boot environment %s exceeds max length %d\n",
                options->be_name, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (!zfs_dataset_exists(lzeh->lzh, be_ds_buff, ZFS_TYPE_DATASET)) { // NOLINT(hicpp-signed-bitwise)
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                "Boot environment %s does not exist\n", options->be_name);
    }


    if ((lzeh->lz_funcs != NULL) && (lzeh->lz_funcs->plugin_pre_activate(lzeh) != 0)) {
        return LIBZE_ERROR_PLUGIN;
    }

    zfs_handle_t *be_zh = zfs_open(lzeh->lzh, be_ds_buff, ZFS_TYPE_DATASET); // NOLINT(hicpp-signed-bitwise)
    if (be_zh == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN,
                "Failed opening dataset %s\n", be_ds_buff);
    }

    if (mid_activate(lzeh, options, be_zh) != LIBZE_ERROR_SUCCESS) {
        goto err;
    }

    if (zpool_set_prop(lzeh->lzph, "bootfs", be_ds_buff) != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed setting bootfs=%s\n", be_ds_buff);
        goto err;
    }

    libze_activate_cbdata cbd = {lzeh};

    // Set for top level dataset
    if (libze_activate_cb(be_zh, &cbd) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }

    // Set for all child datasets and promote
    if (zfs_iter_filesystems(be_zh, libze_clone_cb, &cbd) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }

    if ((lzeh->lz_funcs != NULL) && (lzeh->lz_funcs->plugin_post_activate(lzeh) != 0)) {
        goto err;
    }

err:
    zfs_close(be_zh);
    return ret;
}

#ifdef UNUSED
/**
 * @brief set be required props
 * @param lzeh Open libze_handle
 * @param be_zh Open zfs handle
 * @return libze_error
 */
static libze_error
set_be_props(libze_handle *lzeh, zfs_handle_t *be_zh, const char be_name[static 1]) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    nvlist_t *props = fnvlist_alloc();
    if (props == NULL) {
        return LIBZE_ERROR_NOMEM;
    }
    nvlist_add_string(props, "canmount", "noauto");
    if (strcmp(lzeh->rootfs, be_name) != 0) {
        nvlist_add_string(props, "mountpoint", "/");
    }

    if (zfs_prop_set_list(be_zh, props) != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed to set properties for %s:\n"
                "\tcanmount=noauto\n\tmountpoint=/\n\n", be_name);
        goto err;
    }

    if (zpool_set_prop(lzeh->lzph, "bootfs", be_name) != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed setting bootfs=%s\n", be_name);
        goto err;
    }

err:
    nvlist_free(props);
    return ret;
}
#endif
