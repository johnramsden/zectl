#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/nvpair.h>
#include <libzfs_core.h>
#include <errno.h>
#include <libze/libze_plugin_manager.h>

#include "libze/libze.h"
#include "libze/libze_util.h"
#include "system_linux.h"

// Unsigned long long is 64 bits or more
#define ULL_SIZE 128

const char *ZE_PROP_NAMESPACE = "org.zectl";

int
libze_get_root_dataset(libze_handle *lzeh) {
    zfs_handle_t *zh;
    int ret = 0;

    char rootfs[LIBZE_MAXPATHLEN];

    // Make sure type is ZFS
    if (libze_dataset_from_mountpoint("/", rootfs, LIBZE_MAXPATHLEN) != SYSTEM_ERR_SUCCESS) {
        return -1;
    }

    if ((zh = zfs_path_to_zhandle(lzeh->lzh, "/", ZFS_TYPE_FILESYSTEM)) == NULL) {
        return -1;
    }

    if (strlcpy(lzeh->rootfs, zfs_get_name(zh), LIBZE_MAXPATHLEN) >= LIBZE_MAXPATHLEN) {
        ret = -1;
    }

    zfs_close(zh);

    return ret;
}

void
libze_fini(libze_handle *lzeh) {
    if (lzeh == NULL) {
        return;
    }

    if (lzeh->lzh != NULL) {
        libzfs_fini(lzeh->lzh);
    }
    if (lzeh->lzph != NULL) {
        zpool_close(lzeh->lzph);
    }

    if (lzeh->ze_props != NULL) {
        fnvlist_free(lzeh->ze_props);
    }

    free(lzeh);
}

static int
check_for_bootloader(libze_handle *lzeh) {
    char *plugin = "systemdboot";

    char p_buff[ZFS_MAXPROPLEN] = "";
    if (libze_util_concat(ZE_PROP_NAMESPACE, ":", "bootloader", ZFS_MAXPROPLEN, p_buff) != 0) {
        return -1;
    }
    nvlist_t *nvl;
    if (nvlist_lookup_nvlist(lzeh->ze_props, p_buff, &nvl) != 0) {
        // Unset
        return 0;
    }

    void *p_handle = libze_plugin_open(plugin);
    if (p_handle == NULL) {
        fprintf(stderr, "Failed to open plugin %s\n", plugin);
    } else {
        if (libze_plugin_export(p_handle, &lzeh->lz_funcs) != 0) {
            fprintf(stderr, "Failed to open %s export table\n", plugin);
        } else {
            lzeh->lz_funcs->plugin_init(lzeh);
        }
        libze_plugin_close(p_handle);
    }

    return 0;
}

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

    (void) libze_error_set(lzeh, LIBZE_ERROR_SUCCESS, NULL);

    free(zpool);
    return lzeh;

err:
    libze_fini(lzeh);
    if (zpool != NULL) { free(zpool); }
    return NULL;
}

typedef struct libze_list_cbdata {
    nvlist_t **outnvl;
    libze_handle *lzeh;
} libze_list_cbdata_t;

static int
libze_list_cb(zfs_handle_t *zhdl, void *data) {
    libze_list_cbdata_t *cbd = data;
    char prop_buffer[LIBZE_MAXPATHLEN];
    char dataset[LIBZE_MAXPATHLEN];
    char be_name[LIBZE_MAXPATHLEN];
    char mountpoint[LIBZE_MAXPATHLEN];
    int ret = LIBZE_ERROR_SUCCESS;
    nvlist_t *props = NULL;

    if (((props = fnvlist_alloc()) == NULL)) {
        ret = LIBZE_ERROR_NOMEM;
        goto err;
    }

    // Name
    if (zfs_prop_get(zhdl, ZFS_PROP_NAME, dataset,
                     sizeof(dataset), NULL, NULL, 0, 1) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }
    fnvlist_add_string(props, "dataset", dataset);

    // Boot env name
    if (libze_boot_env_name(dataset, LIBZE_MAXPATHLEN, be_name) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }
    fnvlist_add_string(props, "name", be_name);

    // Mountpoint
    char mounted[LIBZE_MAXPATHLEN];
    if (zfs_prop_get(zhdl, ZFS_PROP_MOUNTED, mounted,
                     sizeof(mounted), NULL, NULL, 0, 1) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }

    int is_mounted = strcmp(mounted, "yes");
    if (is_mounted == 0) {
        if (zfs_prop_get(zhdl, ZFS_PROP_MOUNTPOINT, mountpoint,
                         sizeof(mountpoint), NULL, NULL, 0, 1) != 0) {
            ret = LIBZE_ERROR_UNKNOWN;
            goto err;
        }
    }
    fnvlist_add_string(props, "mountpoint", (is_mounted == 0) ? mountpoint : "-");

    // Creation
    if (zfs_prop_get(zhdl, ZFS_PROP_CREATION, prop_buffer,
                     sizeof(prop_buffer), NULL, NULL, 0, 1) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }

    char t_buf[ULL_SIZE];
    unsigned long long int formatted_time = strtoull(prop_buffer, NULL, 10);
    // ISO 8601 date format
    if (strftime(t_buf, ULL_SIZE, "%F %H:%M", localtime((time_t *)&formatted_time)) == 0) {
        ret = LIBZE_ERROR_UNKNOWN;
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

libze_error
libze_list(libze_handle *lzeh, nvlist_t **outnvl) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    if ((libzfs_core_init()) != 0) {
        ret = LIBZE_ERROR_LIBZFS;
        goto err;
    }

    // Get be root handle
    zfs_handle_t *zroot_hdl = NULL;
    if ((zroot_hdl = zfs_open(lzeh->lzh, lzeh->be_root, ZFS_TYPE_FILESYSTEM)) == NULL) {
        ret = LIBZE_ERROR_ZFS_OPEN;
        goto err;
    }

    // Out nvlist callback
    if ((*outnvl = fnvlist_alloc()) == NULL) {
        ret = LIBZE_ERROR_NOMEM;
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

static void
libze_free_children_nvl(nvlist_t *nvl) {
    nvpair_t *pair = NULL;
    for (pair = nvlist_next_nvpair(nvl, NULL); pair != NULL;
         pair = nvlist_next_nvpair(nvl, pair)) {
        nvlist_t *ds_props = NULL;
        nvpair_value_nvlist(pair, &ds_props);
        fnvlist_free(ds_props);
    }

    nvlist_free(nvl);
}

void
libze_list_free(nvlist_t *nvl) {
    libze_free_children_nvl(nvl);
}

typedef struct libze_clone_prop_cbdata {
    libze_handle *lzeh;
    zfs_handle_t *zhp;
    nvlist_t *props;
} libze_clone_prop_cbdata_t;

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

static int
libze_clone_cb(zfs_handle_t *zhdl, void *data) {
    libze_clone_cbdata *cbd = data;
    int ret = LIBZE_ERROR_SUCCESS;
    nvlist_t *props = NULL;

    if (((props = fnvlist_alloc()) == NULL)) {
        return LIBZE_ERROR_NOMEM;
    }

    libze_clone_prop_cbdata_t cb_data = {
            .lzeh = cbd->lzeh,
            .props = props,
            .zhp = zhdl
    };

    // Iterate over all props
    if (zprop_iter(clone_prop_cb, &cb_data,
                   B_FALSE, B_FALSE, ZFS_TYPE_FILESYSTEM) == ZPROP_INVAL) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }

    fnvlist_add_nvlist(*cbd->outnvl, zfs_get_name(zhdl), props);

    if (cbd->recursive) {
        if (zfs_iter_filesystems(zhdl, libze_clone_cb, cbd) != 0) {
            ret = LIBZE_ERROR_UNKNOWN;
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
 * @return
 */
libze_error
libze_clone(libze_handle *lzeh, char source_root[static 1], char source_snap_suffix[static 1], char be[static 1],
            boolean_t recursive) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    nvlist_t *cdata = NULL;
    if ((cdata = fnvlist_alloc()) == NULL) {
        return LIBZE_ERROR_NOMEM;
    }

    // Get be root handle
    zfs_handle_t *zroot_hdl = NULL;
    if ((zroot_hdl = zfs_open(lzeh->lzh, source_root, ZFS_TYPE_FILESYSTEM)) == NULL) {
        ret = LIBZE_ERROR_ZFS_OPEN;
        goto err;
    }

    libze_clone_cbdata cbd = {
            .outnvl = &cdata,
            .lzeh = lzeh,
            .recursive = recursive
    };

    // Get properties for bootfs and under bootfs
    if (libze_clone_cb(zroot_hdl, &cbd) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }

    if (recursive) {
        if (zfs_iter_filesystems(zroot_hdl, libze_clone_cb, &cbd) != 0) {
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
            DEBUG_PRINT("BE child");
            if (strlen(be_child_buf) > 0) {
                if (libze_util_concat(be, be_child_buf, "/", ZFS_MAX_DATASET_NAME_LEN, ds_child_buf) != 0) {
                    ret = LIBZE_ERROR_UNKNOWN;
                    goto err;
                }
            } else {
                // Child empty
                if (strlcpy(ds_child_buf, be, ZFS_MAX_DATASET_NAME_LEN) >= ZFS_MAX_DATASET_NAME_LEN) {
                    ret = LIBZE_ERROR_UNKNOWN;
                    goto err;
                }
            }
        }

        libze_util_concat(ds_name, "@", source_snap_suffix,
                          ZFS_MAX_DATASET_NAME_LEN, ds_snap_buf);
        DEBUG_PRINT("Cloning %s from %s", ds_name, ds_snap_buf);
        DEBUG_PRINT("DS child: %s", ds_child_buf);

        zfs_handle_t *snap_handle = NULL;
        if ((snap_handle = zfs_open(lzeh->lzh, ds_snap_buf, ZFS_TYPE_SNAPSHOT)) == NULL) {
            ret = LIBZE_ERROR_ZFS_OPEN;
            DEBUG_PRINT("LIBZE_ERROR_ZFS_OPEN %s", ds_snap_buf);
            goto err;
        }
        if (zfs_clone(snap_handle, ds_child_buf, ds_props) != 0) {
            DEBUG_PRINT("Clone error %s", ds_child_buf);
            zfs_close(snap_handle);
            ret = LIBZE_ERROR_UNKNOWN;
            goto err;
        }

        zfs_close(snap_handle);
    }

err:
    libze_free_children_nvl(cdata);
    zfs_close(zroot_hdl);
    return ret;
}

boolean_t
libze_is_active_be(libze_handle *lzeh, char be_dataset[static 1]) {
    return ((strcmp(lzeh->bootfs, be_dataset) == 0) ? B_TRUE : B_FALSE);
}

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

typedef struct libze_activate_cbdata {
    libze_handle *lzeh;
} libze_activate_cbdata;

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

static libze_error
mid_activate(libze_handle *lzeh, libze_activate_options *options, zfs_handle_t *be_zh) {
    libze_error ret = LIBZE_ERROR_SUCCESS;
    char *tmp_dirname = "/";
    const char *ds_name = zfs_get_name(be_zh);
    boolean_t is_root = B_TRUE;


    nvlist_t *props = fnvlist_alloc();
    if (props == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_NOMEM, "Couldn't allocate memory\n");
    }
    nvlist_add_string(props, "canmount", "noauto");

    if (strcmp(lzeh->rootfs, ds_name) != 0) {
        is_root = B_FALSE;
        nvlist_add_string(props, "mountpoint", "/");

        // Not currently mounted
        char temp_directory_template[LIBZE_MAXPATHLEN] = "";
        if (libze_util_concat("/tmp/ze.", options->be_name, ".XXXXXX",
                              LIBZE_MAXPATHLEN, temp_directory_template) != 0) {
            return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                                   "Could not create directory template\n");
        }

        // Create tmp mountpoint
        tmp_dirname = mkdtemp(temp_directory_template);
        if (tmp_dirname == NULL) {
            return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                    "Could not create tmp directory %s\n", temp_directory_template);
        }

        // AFTER here always goto err to cleanup

        if (zfs_prop_set(be_zh, "mountpoint", tmp_dirname) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                    "Failed to set mountpoint=%s for %s\n", temp_directory_template, ds_name);
            goto err;
        }

        if (zfs_mount(be_zh, NULL, 0) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                    "Failed to mount %s to %s\n", ds_name, temp_directory_template);
            goto err;
        }
    }

    // mid_activate
    if ((lzeh->lz_funcs != NULL) &&
        (lzeh->lz_funcs->plugin_mid_activate(lzeh, tmp_dirname) != 0)) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_PLUGIN, "Failed to run mid-activate hook\n");
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
 *
 * @invariant @p be_zh closed on exit
 */
libze_error
libze_activate(libze_handle *lzeh, libze_activate_options *options) {
    /*
     * Steps:
     *   * Get bootloader
     *   * pre_activate
     *   * Check if BE exists
     *   * If BE exists and not active, activate, else return
     *      * if not currently mounted at '/'
     *          * if mounted, unmount
     *          * set canmount=noauto before mount
     *          * if plugin
     *              * mount to tmpdir
     *              * mid_activate
     *              * unmount
     *      * set post props, canmount=noauto
     *      * set bootfs=BE
     *   * Set required child settings
     *      * canmount=noauto
     *      * promote if clone
     *   * post_activate
     */
    libze_error ret = LIBZE_ERROR_SUCCESS;

    char be_ds_buff[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_util_concat(lzeh->be_root, "/", options->be_name, ZFS_MAX_DATASET_NAME_LEN, be_ds_buff) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Requested boot environment %s exceeds max length %d\n",
                               options->be_name, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (!zfs_dataset_exists(lzeh->lzh, be_ds_buff, ZFS_TYPE_DATASET)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                "Boot environment %s does not exist\n", options->be_name);
    }


    if ((lzeh->lz_funcs != NULL) && (lzeh->lz_funcs->plugin_pre_activate(lzeh) != 0)) {
        return LIBZE_ERROR_PLUGIN;
    }

    zfs_handle_t *be_zh = zfs_open(lzeh->lzh, be_ds_buff, ZFS_TYPE_DATASET);
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

    libze_activate_cbdata cbd = { lzeh };

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

// References:
//  nvlist: github.com/zfsonlinux/zfs/blob/master/module/nvpair/fnvpair.c
//  lzc:    github.com/zfsonlinux/zfs/blob/master/lib/libzfs_core/libzfs_core.c#L1229

libze_error
libze_channel_program(libze_handle *lzeh, const char *zcp, nvlist_t *nvl, nvlist_t **outnvl) {

    libze_error ret = LIBZE_ERROR_SUCCESS;

    if ((libzfs_core_init()) != 0) {
        ret = LIBZE_ERROR_LIBZFS;
        goto err;
    }

    uint64_t instrlimit = 10*1000*1000; // 10 million is default
    uint64_t memlimit = 10*1024*1024;   // 10MB is default

    DEBUG_PRINT("Running zcp: \n%s\n", zcp);

// TODO: Remove after 0.8 released
#if defined(ZOL_VERSION) && ZOL_VERSION >= 8
    int err = lzc_channel_program(lzeh->zpool, zcp, instrlimit, memlimit, nvl, outnvl);
    if (err != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_LIBZFS,
                "Failed to run channel program\n");
    }
#else
#if defined(ZOL_VERSION)
    DEBUG_PRINT("Wrong ZFS version %d", ZOL_VERSION);
#endif

DEBUG_PRINT("Can't run channel program");
ret = LIBZE_ERROR_LIBZFS;
#endif

err:
    libzfs_core_fini();
    return ret;
}

static libze_error
libze_filter_be_props(nvlist_t *unfiltered_nvl, nvlist_t **result_nvl, const char namespace[static 1]) {
    nvpair_t *pair;
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
 *        and return them in @a result
 * @param[in] lzeh Initialized lzeh libze handle
 * @param[out] result Properties of boot environment
 * @param[in] namespace ZFS property prefix
 * @return LIBZE_ERROR_SUCCESS on success, or
 *         LIBZE_ERROR_ZFS_OPEN, LIBZE_ERROR_UNKNOWN, LIBZE_ERROR_NOMEM
 *
 * @invariant @p lzeh != NULL
 * @invariant @p namespace != NULL
 * @invariant @p zhp is closed on exit
 * @invariant @p filtered_user_props is free'd on error, or allocated is success
 */
libze_error
libze_get_be_props(libze_handle *lzeh, nvlist_t **result, const char namespace[static 1]) {
    nvlist_t *user_props = NULL;
    nvlist_t *filtered_user_props = NULL;
    libze_error ret = LIBZE_ERROR_SUCCESS;
    zfs_handle_t *zhp = zfs_open(lzeh->lzh, lzeh->be_root, ZFS_TYPE_FILESYSTEM);

    if (zhp == NULL) {
        return LIBZE_ERROR_ZFS_OPEN;
    }

    if ((user_props = zfs_get_user_props(zhp)) == NULL) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }

    if ((filtered_user_props = fnvlist_alloc()) == NULL) {
        ret = LIBZE_ERROR_NOMEM;
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
 * @invariant @p lzeh != NULL
 * @invariant if @p lze_fmt == NULL, @p ... should have zero arguments.
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

    va_list argptr;
    va_start(argptr, lze_fmt);
    // TODO: Check too long?
    int length = vsnprintf(lzeh->libze_err, LIBZE_MAXPATHLEN, lze_fmt, argptr);
    va_end(argptr);

    assert(length < LIBZE_MAXPATHLEN);

    return lze_err;
}
