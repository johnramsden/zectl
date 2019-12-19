#include <string.h>
#include <sys/mount.h>

#include "libze/libze.h"
#include "system_linux.h"

/**
 * @brief Concatenate two strings with a separator
 * @param[in] prefix Prefix string
 * @param[in] separator Separator string
 * @param[in] suffix Suffix string
 * @param[in] buflen Length of buffer
 * @param[out] buf Resulting concatenated string
 * @return Nonzero if the resulting string is longer than the buffer length
 */
int
libze_util_concat(const char *prefix, const char *separator, const char *suffix,
                  size_t buflen, char buf[buflen]) {

    (void) strlcpy(buf, "", buflen);

    if ((strlcat(buf, prefix, buflen) >= buflen) ||
        (strlcat(buf, separator, buflen) >= buflen) ||
        (strlcat(buf, suffix, buflen) >= buflen)) {
        return -1;
    }

    return 0;
}

/**
 * @brief Cut a string at the last instance of a delimiter
 * @param[in] path String to cut
 * @param[in] buflen Length of buffer
 * @param[out] buf  Prefix before last instance of delimiter
 * @param[in] delimiter Delimiter to cut string at
 * @return Nonzero if buffer is too short, or there is no instance of delimiter
 */
int
libze_util_cut(const char path[static 1], size_t buflen, char buf[buflen], char delimiter) {
    char *slashp = NULL;

    if (strlcpy(buf, path, buflen) >= buflen) {
        return -1;
    }

    /* Get pointer to last instance of '/' */
    if ((slashp = strrchr(buf, delimiter)) == NULL) {
        return -1;
    }

    /* terminate string at '/' */
    *slashp = '\0';
    return 0;
}

/**
 * @brief Given a dataset, return just the portion after the root of boot environments
 * @param[in] root Root of boot environments
 * @param[in] dataset Full dataset to get suffix of
 * @param[in] buflen Length of buffer
 * @param[out] buf Buffer to place suffix in
 * @return Non-zero if there is no parent (path is just the name of the pool),
 *         or if the length of the buffer is exceeded
 */
int
libze_util_suffix_after_string(const char root[static 1], const char dataset[static 1], size_t buflen,
                               char buf[buflen]) {

    if (strlcpy(buf, dataset, buflen) >= buflen) {
        return -1;
    }

    size_t loc = strlen(root)+1;
    if (loc >= buflen) {
        return -1;
    }

    /* get substring after next '/' */
    if (strlcpy(buf, buf+loc, buflen) >= buflen) {
        return -1;
    }

    return 0;
}

/**
 * @brief Given a dataset, get the name of the boot environment
 * @param[in] dataset Dataset to get the boot environment of
 * @param[in] buflen Length of buffer
 * @param[out] buf Buffer to place boot environment in
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN if the length of the buffere exceeded,
 *         @p LIBZE_ERROR_UNKNOWN if no '/' is in the dataset name
 */
libze_error
libze_boot_env_name(libze_handle *lzeh, const char *dataset, size_t buflen, char buf[buflen]) {
    char *slashp = NULL;

    if (strlcpy(buf, dataset, buflen) >= buflen) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "The length of the buffer exceeded.\n");
    }

    /* Get pointer to last instance of '/' */
    if ((slashp = strrchr(buf, '/')) == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "There is no '/' in the name of the dataset (%s).\n", buf);
    }

    /* get substring after last '/' */
    if (strlcpy(buf, slashp+1, buflen) >= buflen) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "The length of the buffer exceeded.\n");
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief Check if the specified boot environment is currently set to active
 * @param[in] lzeh Initialized @p libze_handle
 * @param[in] be_dataset Dataset to check if active
 * @return @p B_TRUE if active else @p B_FALSE
 */
boolean_t
libze_is_active_be(libze_handle *lzeh, const char be_dataset[static 1]) {
    return ((strcmp(lzeh->env_activated_path, be_dataset) == 0) ? B_TRUE : B_FALSE);
}

/**
 * @brief Check if the specified boot environment is the currently running boot environment
 * @param[in] lzeh Initialized @p libze_handle
 * @param[in] be_dataset Dataset to be checked if is currently running
 * @return @p B_TRUE if dataset is running else @p B_FALSE
 */
boolean_t
libze_is_root_be(libze_handle *lzeh, const char be_dataset[static 1]) {
    return ((strcmp(lzeh->env_running_path, be_dataset) == 0) ? B_TRUE : B_FALSE);
}

/**
 * @brief Free an nvlist and one level down of it's children
 * @param[in] nvl nvlist to free
 */
void
libze_list_free(nvlist_t *nvl) {
    if (nvl == NULL) {
        return;
    }

    nvpair_t *pair = NULL;
    for (pair = nvlist_next_nvpair(nvl, NULL); pair != NULL;
         pair = nvlist_next_nvpair(nvl, pair)) {
        nvlist_t *ds_props = NULL;
        nvpair_value_nvlist(pair, &ds_props);
        fnvlist_free(ds_props);
    }

    nvlist_free(nvl);
}

/**
 * @brief Returns the name of the ZFS pool from the specified dataset (everything to first '/')
 * @param[in] buflen Length of buffer
 * @param[out] buf Buffer to place boot environment in
 * @return @p B_TRUE
 */
boolean_t
libze_get_zpool_name_from_dataset(const char dataset[static 3], size_t buflen, char buf[buflen]) {
    char *zpool_name = NULL;
    if ((zpool_name = strchr(dataset, '/')) == NULL) {
        return (strlcpy(buf, zpool_name, buflen) < buflen);
    }
    else {
        return B_FALSE;
    }
}

/**
 * @brief Get the root dataset
 * @param[in] lzeh Initialized @p libze_handle
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_INVALID_CONFIG if no ZFS dataset is mounted at root ('/'),
 *         @p LIBZE_ERROR_LIBZFS if the running dataset can't be determined,
 *         @p LIBZE_ERROR_MAXPATHLEN if the name of the running dataset exceeds the max path len
 *
 * @pre lzeh != NULL
 */
libze_error
libze_get_root_dataset(libze_handle *lzeh) {
    zfs_handle_t *zh;
    libze_error ret = LIBZE_ERROR_SUCCESS;

    char rootfs[ZFS_MAX_DATASET_NAME_LEN];

    // Make sure type is ZFS
    if (libze_dataset_from_mountpoint("/", ZFS_MAX_DATASET_NAME_LEN, rootfs) != SYSTEM_ERR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_LIBZFS,
                "Can't retrieve the ZFS dataset which is mounted at '/'.\n");
    }

    if ((zh = zfs_path_to_zhandle(lzeh->lzh, "/", ZFS_TYPE_FILESYSTEM)) == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_INVALID_CONFIG,
                "No dataset is mounted at root ('/').\n");
    }
    if (strlcpy(lzeh->env_running_path, zfs_get_name(zh), ZFS_MAX_DATASET_NAME_LEN) >= ZFS_MAX_DATASET_NAME_LEN) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "The name of the running dataset (%s) exceeds max length (%d).",
                zfs_get_name(zh), ZFS_MAX_DATASET_NAME_LEN);
    }

    zfs_close(zh);
    return ret;
}

/**
 * @brief Mounts a specified ZFS dataset at a given mountpoint
 * @param[in] dataset The datset which should be mounted
 * @param[in] mountpoint An existing directory where the dataset should be mounted to
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MOUNT if mount failed
 */
libze_error
libze_util_temporary_mount(const char dataset[ZFS_MAX_DATASET_NAME_LEN], const char mountpoint[static 2]) {
    const char *mount_settings = "zfsutil";
    const char *mount_type = "zfs";
    const unsigned long mount_flags = 0;

    if (mount(dataset, mountpoint, mount_type, mount_flags, mount_settings) != 0) {
        return LIBZE_ERROR_MOUNT;
    }

    return LIBZE_ERROR_SUCCESS;
}