// Make sure libspl mnttab.h isn't imported, creates getmnttent conflict
#define _SYS_MNTTAB_H

#include "system_linux.h"

#include "libze/libze_util.h"

#include <mntent.h>

/**
 * @brief Given a mountpoint get the dataset mounted
 * @param[in] mountpoint Mountpoint to get dataset from
 * @param[in] buflen Length of buffer
 * @param[out] dataset_buf Buffer to place a dataset in
 * @return @p SYSTEM_ERR_SUCCESS on success.
 *         @p SYSTEM_ERR_MNT_FILE if no mntfile exists.
 *         @p SYSTEM_ERR_NOT_FOUND if no dataset found.
 *         @p SYSTEM_ERR_WRONG_FSTYPE if non-zfs.
 *         @p SYSTEM_ERR_UNKNOWN if buflen exceeded.
 */
system_fs_error
libze_dataset_from_mountpoint(char mountpoint[], size_t buflen, char dataset_buf[buflen]) {

    struct mntent *ent = NULL;
    system_fs_error ret = SYSTEM_ERR_SUCCESS;

    char const *mnt_location_file = "/proc/mounts";
    FILE *mnt_file = setmntent(mnt_location_file, "r");

    if (mnt_file == NULL) {
        return SYSTEM_ERR_MNT_FILE;
    }

    do { /* Loop until mountpoint found or EOF */
        ent = getmntent(mnt_file);
    } while (ent != NULL && (strcmp(ent->mnt_dir, mountpoint) != 0));

    if (ent == NULL) {
        ret = SYSTEM_ERR_NOT_FOUND;
        goto fin;
    }

    /* Found root, checking if zfs */
    if (strcmp(ent->mnt_type, "zfs") != 0) {
        ret = SYSTEM_ERR_WRONG_FSTYPE;
        goto fin;
    }

    if (strlcpy(dataset_buf, ent->mnt_fsname, buflen) >= buflen) {
        ret = SYSTEM_ERR_UNKNOWN;
        goto fin;
    }

fin:
    endmntent(mnt_file);
    return ret;
}