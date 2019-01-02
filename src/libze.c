
#include <string.h>
#include "system_linux.h"

#include "libze.h"
#include "util.h"

/*
 * Given a complete name, return just the portion that refers to the parent.
 * Will return -1 if there is no parent (path is just the name of the
 * pool).
 */
static int
parent_name(const char *path, char *buf, size_t buflen) {
    char *slashp;

    if(copy_string(buf, path, buflen) != 0) {
        DEBUG_PRINT("Failed to copy string");
        return -1;
    }
    DEBUG_PRINT("Getting parent of '%s'", buf);

    /* Get pointer to last instance of '/' */
    if (!(slashp = strrchr(buf, '/'))) {
        DEBUG_PRINT("Failed to terminate string at '/' for '%s'", buf);
        return -1;
    }

    /* terminate string at '/' */
    *slashp = '\0';

    return 0;
}

static int
get_root_dataset(libze_handle_t *lzeh) {
    zfs_handle_t *zh;

    char rootfs[ZE_MAXPATHLEN];

    // Make sure type is ZFS
    if (system_linux_get_dataset("/", rootfs, ZE_MAXPATHLEN) != SYSTEM_ERR_SUCCESS) {
        return -1;
    }

    zh = zfs_path_to_zhandle(lzeh->lzh, "/", ZFS_TYPE_FILESYSTEM);
    if (!zh) {
        return -1;
    }

    if (copy_string(lzeh->rootfs, zfs_get_name(zh), ZE_MAXPATHLEN) != 0) {
        return -1;
    }

    zfs_close(zh);

    return 0;
}

void
libze_fini(libze_handle_t *lzeh) {
    DEBUG_PRINT("Closing libze handle");
    if (lzeh) {
        if (lzeh->lzh) {
            libzfs_fini(lzeh->lzh);
        }
        if (lzeh->lzph) {
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

    if (!(lzeh = calloc(1, sizeof(libze_handle_t)))) {
        goto err;
    }

    if (!(lzeh->lzh = libzfs_init())) {
        goto err;
    }

    if (get_root_dataset(lzeh) != 0) {
        goto err;
    }

    if (parent_name(lzeh->rootfs, lzeh->be_root, ZE_MAXPATHLEN) != 0) {
        goto err;
    }

    if (!(slashp = strchr(lzeh->be_root, '/'))) {
        goto err;
    }

    pool_length = slashp - lzeh->be_root;
    zpool = malloc(pool_length + 1);
    if (!zpool) {
        goto err;
    }

    // Get pool portion of dataset
    if (!strncpy(zpool, lzeh->be_root, pool_length)) {
        goto err;
    }
    zpool[pool_length] = '\0';
    DEBUG_PRINT("POOL: %s", zpool);

    if (copy_string(lzeh->zpool, zpool, ZE_MAXPATHLEN) != 0) {
        goto err;
    }

    if (!(lzeh->lzph = zpool_open(lzeh->lzh, lzeh->zpool))) {
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
    if (lzeh) {
        if (lzeh->lzh) {
            libzfs_fini(lzeh->lzh);
        }
        if (lzeh->lzph) {
            zpool_close(lzeh->lzph);
        }
        free(lzeh);
    }
    if (zpool) {
        free(zpool);
    }
    return NULL;
}

