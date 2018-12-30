
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
    DEBUG_PRINT("Getting parent of '%s'\n", buf);

    /* Get pointer to last instance of '/' */
    if (!(slashp = strrchr(buf, '/'))) {
        DEBUG_PRINT("bez: Failed to terminate string at '/' for '%s'\n", buf);
        return -1;
    }

    /* terminate string at '/' */
    *slashp = '\0';

    return 0;
}

libze_handle_t *
libze_init() {

    libze_handle_t *lzeh = NULL;
    char *zpool;

    if (!(lzeh = calloc(1, sizeof(libze_handle_t)))) {
        goto err;
    }

    if (!(lzeh->lzh = libzfs_init())) {
        goto err;
    }

    if (system_linux_get_dataset("/", lzeh) != SYSTEM_ERR_SUCCESS) {
        goto err;
    }

    if (parent_name(lzeh->rootfs, lzeh->be_root, ZE_MAXPATHLEN) != 0) {
        goto err;
    }

    return lzeh;

    /* Error occurred */
err:
    if (lzeh) {
        if (lzeh->lzh) {
            libzfs_fini(lzeh->lzh);
        }
        if (lzeh->lzph) {
            zpool_close(lzeh->lzph);
        }
        free(lzeh);
    }
    return NULL;
}