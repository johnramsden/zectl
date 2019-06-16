#include <string.h>
#include <sys/nvpair.h>
#include <libzfs_core.h>
#include <errno.h>

#include "libze/libze.h"
#include "util/util.h"
#include "system_linux.h"

/*
 * Given a complete name, return just the portion that refers to the parent.
 * Will return -1 if there is no parent (path is just the name of the
 * pool).
 */
static int
cut_at_delimiter(const char path[static 1], size_t buflen, char buf[buflen], char delimiter) {
    char *slashp = NULL;

    if(copy_string(buf, path, buflen) != 0) {
        DEBUG_PRINT("Failed to copy string");
        return -1;
    }
    DEBUG_PRINT("Getting parent of '%s'", buf);

    /* Get pointer to last instance of '/' */
    if ((slashp = strrchr(buf, '/')) == NULL) {
        DEBUG_PRINT("Failed to terminate string at '/' for '%s'", buf);
        return -1;
    }

    /* terminate string at '/' */
    *slashp = '\0';
    return 0;
}
/*
 * Given a complete name, return just the portion that refers to the parent.
 * Will return -1 if there is no parent (path is just the name of the
 * pool).
 */
static int
parent_name(const char path[static 1], size_t buflen, char buf[buflen]) {
    return cut_at_delimiter(path, buflen, buf, '/');
}

int
libze_prop_prefix(const char path[static 1], size_t buflen, char buf[buflen]) {
    return cut_at_delimiter(path, buflen, buf, ':');
}
/*
 * Given a complete name, return just the portion that refers to the be.
 * Will return -1 if there is no parent (path is just the name of the
 * pool).
 */
int
boot_env_name_children(const char root[static 1], const char dataset[static 1], size_t buflen, char buf[buflen]) {


    if(strlcpy(buf, dataset, buflen) >= buflen) {
        return -1;
    }

    size_t loc = strlen(root)+1;
    if (loc >= buflen) {
        return -1;
    }

    /* get substring after last '/' */
    if(strlcpy(buf, buf+loc, buflen) >= buflen) {
        return -1;
    }

    return parent_name(buf, buflen, buf);
}

int
boot_env_name(const char dataset[static 1], size_t buflen, char buf[buflen]) {
    char *slashp;

    if(copy_string(buf, dataset, buflen) != 0) {
        DEBUG_PRINT("Failed to copy string");
        return -1;
    }
    DEBUG_PRINT("Getting parent of '%s'", buf);

    /* Get pointer to last instance of '/' */
    if ((slashp = strrchr(buf, '/')) == NULL) {
        DEBUG_PRINT("Failed to terminate string at '/' for '%s'", buf);
        return -1;
    }

    /* get substring after last '/' */
    if(copy_string(buf, slashp+1, buflen) != 0) {
        DEBUG_PRINT("Failed to copy string");
        return -1;
    }

    return 0;
}

static int
get_root_dataset(libze_handle_t *lzeh) {
    zfs_handle_t *zh;
    int ret;

    char rootfs[ZE_MAXPATHLEN];

    // Make sure type is ZFS
    if (system_linux_get_dataset("/", rootfs, ZE_MAXPATHLEN) != SYSTEM_ERR_SUCCESS) {
        return -1;
    }

    if ((zh = zfs_path_to_zhandle(lzeh->lzh, "/", ZFS_TYPE_FILESYSTEM)) == NULL) {
        return -1;
    }

    ret = copy_string(lzeh->rootfs, zfs_get_name(zh), ZE_MAXPATHLEN);

    zfs_close(zh);

    return ret;
}

void
libze_fini(libze_handle_t *lzeh) {
    DEBUG_PRINT("Closing libze handle");
    if (lzeh != NULL) {
        if (lzeh->lzh != NULL) {
            libzfs_fini(lzeh->lzh);
        }
        if (lzeh->lzph != NULL) {
            zpool_close(lzeh->lzph);
        }
        free(lzeh);
    }
}

libze_handle_t *
libze_init() {

    libze_handle_t *lzeh = NULL;
    char *slashp = NULL;
    char *zpool = NULL;
    size_t pool_length;

    if ((lzeh = calloc(1, sizeof(libze_handle_t))) == NULL) {
        goto err;
    }
    if ((lzeh->lzh = libzfs_init()) == NULL) {
        goto err;
    }
    if (get_root_dataset(lzeh) != 0) {
        goto err;
    }
    if (parent_name(lzeh->rootfs, ZE_MAXPATHLEN, lzeh->be_root) != 0) {
        goto err;
    }
    if ((slashp = strchr(lzeh->be_root, '/')) == NULL) {
        goto err;
    }

    pool_length = slashp - lzeh->be_root;
    zpool = malloc(pool_length + 1);
    if (!zpool) {
        goto err;
    }

    // Get pool portion of dataset
    if (strncpy(zpool, lzeh->be_root, pool_length) == NULL) {
        goto err;
    }
    zpool[pool_length] = '\0';
    DEBUG_PRINT("POOL: %s", zpool);

    if (copy_string(lzeh->zpool, zpool, ZE_MAXPATHLEN) != 0) {
        goto err;
    }

    if ((lzeh->lzph = zpool_open(lzeh->lzh, lzeh->zpool)) == NULL) {
        goto err;
    }

    if (zpool_get_prop(lzeh->lzph, ZPOOL_PROP_BOOTFS, lzeh->bootfs,
                       sizeof(lzeh->bootfs), NULL, B_TRUE) != 0) {
        goto err;
    }

    free(zpool);
    return lzeh;

    /* Error occurred */
err:
    DEBUG_PRINT("Error occurred");
    if (lzeh != NULL) {
        if (lzeh->lzh != NULL) {
            libzfs_fini(lzeh->lzh);
        }
        if (lzeh->lzph != NULL) {
            zpool_close(lzeh->lzph);
        }
        free(lzeh);
    }
    if (zpool != NULL) {
        free(zpool);
    }
    return NULL;
}

typedef struct libze_list_cbdata {
    nvlist_t **outnvl;
    libze_handle_t *lzeh;
} libze_list_cbdata_t;

int
libze_list_cb(zfs_handle_t *zhdl, void *data) {
    libze_list_cbdata_t *cbd = data;
    char prop_buffer[ZE_MAXPATHLEN];
    char dataset[ZE_MAXPATHLEN];
    char be_name[ZE_MAXPATHLEN];
    char mountpoint[ZE_MAXPATHLEN];
    int ret = LIBZE_ERROR_SUCCESS;
    nvlist_t *props = NULL;
    int is_mounted;

    if(((props = fnvlist_alloc()) == NULL)) {
        ret = LIBZE_ERROR_NOMEM;
        goto err;
    }

    // Name
    if(zfs_prop_get(zhdl, ZFS_PROP_NAME, dataset,
                    sizeof(dataset), NULL, NULL, 0, 1) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }
    fnvlist_add_string(props, "dataset", dataset);

    // Boot env name
    if (boot_env_name(dataset, ZE_MAXPATHLEN, be_name) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }
    fnvlist_add_string(props, "name", be_name);

    // Mountpoint
    char mounted[ZE_MAXPATHLEN];
    if(zfs_prop_get(zhdl, ZFS_PROP_MOUNTED, mounted,
                    sizeof(mounted), NULL, NULL, 0, 1) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }

    if((is_mounted = strcmp(mounted, "yes")) == 0) {
        if(zfs_prop_get(zhdl, ZFS_PROP_MOUNTPOINT, mountpoint,
                        sizeof(mountpoint), NULL, NULL, 0, 1) != 0) {
            ret = LIBZE_ERROR_UNKNOWN;
            goto err;
        }
    }
    fnvlist_add_string(props, "mountpoint", (is_mounted == 0) ? mountpoint : "-");

    // Creation
    if(zfs_prop_get(zhdl, ZFS_PROP_CREATION, prop_buffer,
            sizeof(prop_buffer), NULL, NULL, 0, 1) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }
    fnvlist_add_string(props, "creation", prop_buffer);

    // Nextboot
    boolean_t is_nextboot = (strcmp(cbd->lzeh->bootfs, dataset) == 0);
    fnvlist_add_boolean_value(props, "nextboot", is_nextboot);

    // Active
    boolean_t is_active = (is_mounted == 0) && (strcmp(mountpoint, "/") == 0);
    fnvlist_add_boolean_value(props, "active", is_active);

    fnvlist_add_nvlist(*(cbd->outnvl), prop_buffer, props);

    return ret;
err:
    fnvlist_free(props);
    return ret;
}

libze_error_t
libze_list(libze_handle_t *lzeh, nvlist_t **outnvl) {
    libze_error_t ret = LIBZE_ERROR_SUCCESS;

    if((libzfs_core_init()) != 0) {
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
    if((*outnvl = fnvlist_alloc()) == NULL) {
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


typedef struct libze_clone_prop_cbdata {
    libze_handle_t *lzeh;
    zfs_handle_t *zhp;
    nvlist_t *props;
} libze_clone_prop_cbdata_t;

static int
clone_prop_cb(int prop, void *data) {
    libze_clone_prop_cbdata_t *pcbd = data;

    zprop_source_t src;
    char propbuf[ZE_MAXPATHLEN];
    char statbuf[ZE_MAXPATHLEN];
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

int
libze_clone_cb(zfs_handle_t *zhdl, void *data) {
    libze_clone_cbdata_t *cbd = data;
    char prop_buffer[ZE_MAXPATHLEN];
    int ret = LIBZE_ERROR_SUCCESS;
    nvlist_t *props = NULL;

    if(((props = fnvlist_alloc()) == NULL)) {
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

    fnvlist_add_nvlist(*(cbd->outnvl), zfs_get_name(zhdl), props);

    return ret;
err:
    fnvlist_free(props);
    return ret;
}

libze_error_t
libze_clone(libze_handle_t *lzeh, char source_root[static 1], char source_snap_suffix[static 1], char be[static 1]) {
    libze_error_t ret = LIBZE_ERROR_SUCCESS;

    nvlist_t *cdata = NULL;
    if((cdata = fnvlist_alloc()) == NULL) {
        return LIBZE_ERROR_NOMEM;
    }

    // Get be root handle
    zfs_handle_t *zroot_hdl = NULL;
    if ((zroot_hdl = zfs_open(lzeh->lzh, source_root, ZFS_TYPE_FILESYSTEM)) == NULL) {
        ret = LIBZE_ERROR_ZFS_OPEN;
        goto err;
    }

    libze_clone_cbdata_t cbd = {
            .outnvl = &cdata,
            .lzeh = lzeh
    };

    // Get properties for bootfs and under bootfs
    if ((libze_clone_cb(zroot_hdl, &cbd) != 0) ||
        (zfs_iter_filesystems(zroot_hdl, libze_clone_cb, &cbd) != 0)) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }

    nvpair_t *pair = NULL;
    for (pair = nvlist_next_nvpair(cdata, NULL); pair != NULL;
         pair = nvlist_next_nvpair(cdata, pair)) {
        nvlist_t *ds_props = NULL;
        nvpair_value_nvlist(pair, &ds_props);

        // TODO: Recursive clone
        char *ds_name = nvpair_name(pair);
        char ds_snap_buf[ZFS_MAX_DATASET_NAME_LEN] = "";
//        char be_child_buf[ZFS_MAX_DATASET_NAME_LEN] = "";
//        char ds_child_buf[ZFS_MAX_DATASET_NAME_LEN] = "";
//        if (boot_env_name_children(source_root, ds_name, be_child_buf, ZFS_MAX_DATASET_NAME_LEN) == 0) {
//            if (form_dataset_string(be, be_child_buf, ds_child_buf, ZFS_MAX_DATASET_NAME_LEN) != 0) {
//                ret = LIBZE_ERROR_UNKNOWN;
//                goto err;
//            }
//        }

        form_snapshot_string(ds_name, source_snap_suffix,
                             ZFS_MAX_DATASET_NAME_LEN, ds_snap_buf);
        DEBUG_PRINT("Cloning %s from %s", ds_name, ds_snap_buf);

        zfs_handle_t *snap_handle = NULL;
        if ((snap_handle = zfs_open(lzeh->lzh, ds_snap_buf, ZFS_TYPE_SNAPSHOT)) == NULL) {
            ret = LIBZE_ERROR_ZFS_OPEN;
            goto err;
        }
        if (zfs_clone(snap_handle, be, ds_props) != 0) {
            zfs_close(snap_handle);
            ret = LIBZE_ERROR_UNKNOWN;
            goto err;
        }

        zfs_close(snap_handle);
    }

err:
    // TODO: Free children nvlists?
    fnvlist_free(cdata);
    zfs_close(zroot_hdl);
    return ret;
}

// References:
//  nvlist: github.com/zfsonlinux/zfs/blob/master/module/nvpair/fnvpair.c
//  lzc:    github.com/zfsonlinux/zfs/blob/master/lib/libzfs_core/libzfs_core.c#L1229

libze_error_t
libze_channel_program(libze_handle_t *lzeh, const char *zcp, nvlist_t *nvl, nvlist_t **outnvl) {

    libze_error_t ret = LIBZE_ERROR_SUCCESS;

    if((libzfs_core_init()) != 0) {
        ret = LIBZE_ERROR_LIBZFS;
        goto err;
    }

    uint64_t instrlimit = 10 * 1000 * 1000; // 10 million is default
    uint64_t memlimit = 10 * 1024 * 1024;   // 10MB is default

    DEBUG_PRINT("Running zcp: \n%s\n", zcp);

// TODO: Remove after 0.8 released
#if defined(ZOL_VERSION) && ZOL_VERSION >= 8
    int err = lzc_channel_program(lzeh->zpool, zcp, instrlimit, memlimit, nvl, outnvl);
    if (err != 0) {
        fprintf(stderr, "Failed to run channel program\n");
        ret = LIBZE_ERROR_LIBZFS;
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

static libze_error_t
libze_filter_be_props(nvlist_t *unfiltered_nvl, nvlist_t **result_nvl, const char namespace[static 1]) {
    nvpair_t *pair;
    libze_error_t ret = LIBZE_ERROR_SUCCESS;

    for (pair = nvlist_next_nvpair(unfiltered_nvl, NULL); pair != NULL;
         pair = nvlist_next_nvpair(unfiltered_nvl, pair)) {
        char *nvp_name = nvpair_name(pair);
        char buf[ZE_MAXPATHLEN];

        if (libze_prop_prefix(nvp_name, ZE_MAXPATHLEN, buf) != 0) {
            return LIBZE_ERROR_UNKNOWN;
        }

        if (strcmp(buf, namespace) == 0) {
            nvlist_add_nvpair(*result_nvl, pair);
        }
    }

    return ret;
}

libze_error_t
libze_get_be_props(libze_handle_t *lzeh, nvlist_t **result, const char namespace[static 1]) {
    nvlist_t *user_props = NULL;
    nvlist_t *filtered_user_props = NULL;
    libze_error_t ret = LIBZE_ERROR_SUCCESS;
    zfs_handle_t *zhp = zfs_open(lzeh->lzh, lzeh->be_root, ZFS_TYPE_FILESYSTEM);

    if (zhp == NULL) {
        return LIBZE_ERROR_ZFS_OPEN;
    }

    if((user_props = zfs_get_user_props(zhp)) == NULL) {
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

//    dump_nvlist(filtered_user_props, 0);

    *result = filtered_user_props;
    return ret;
err:
    zfs_close(zhp);
    fnvlist_free(filtered_user_props);
    return ret;
}