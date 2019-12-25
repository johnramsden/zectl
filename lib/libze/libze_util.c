// Required for spl stat.h
#define __USE_LARGEFILE64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <string.h>
#include <sys/mount.h>
#include <libze/libze_util.h>
#include <dirent.h>
#include <sys/stat.h>

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
 * @return Non-zero if the length of the buffer is exceeded,
 *         or if there is no / contained in the data set
 */
int
libze_boot_env_name(const char *dataset, size_t buflen, char buf[buflen]) {
    char *slashp = NULL;

    if (strlcpy(buf, dataset, buflen) >= buflen) {
        return -1;
    }

    /* Get pointer to last instance of '/' */
    if ((slashp = strrchr(buf, '/')) == NULL) {
        return -1;
    }

    /* get substring after last '/' */
    if (strlcpy(buf, slashp+1, buflen) >= buflen) {
        return -1;
    }

    return 0;
}

/**
 * @brief Checks if the specified boot environment name is valid and exists
 * @param[in] lzeh Initialized @p libze_handle
 * @param[in] be Name of the boot environment
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_EEXIST if the dataset does not exist,
 *         @p LIBZE_ERROR_MAXPATHLEN if a name or path length exceeded,
 *         @p LIBZE_ERROR_ZFS_OPEN if the dataset exists but can't be opened
 */
libze_error
libze_validate_existing_be(libze_handle *lzeh, const char be[static 1]) {
    libze_error ret = LIBZE_ERROR_SUCCESS;
    zfs_handle_t *be_dataset_hndl = NULL;
    zfs_handle_t *be_dataset_bootpool_hndl = NULL;
    char be_dataset[ZFS_MAX_DATASET_NAME_LEN] = "";
    char be_dataset_bootpool[ZFS_MAX_DATASET_NAME_LEN] = "";

    /* Check dataset path */
    if (libze_util_concat(lzeh->env_root, "/", be, ZFS_MAX_DATASET_NAME_LEN, be_dataset) 
            != LIBZE_ERROR_SUCCESS) {
        return LIBZE_ERROR_MAXPATHLEN;
    }
    /* Check if dataset exists */
    if (!zfs_dataset_exists(lzeh->lzh, be_dataset, ZFS_TYPE_FILESYSTEM)) {
        return LIBZE_ERROR_EEXIST;
    }
    /* Check if dataset can be opened */
    be_dataset_hndl = zfs_open(lzeh->lzh, be_dataset, ZFS_TYPE_FILESYSTEM);
    if (be_dataset_hndl == NULL) {
        return LIBZE_ERROR_ZFS_OPEN;
    }
    zfs_close(be_dataset_hndl);

    if (lzeh->bootpool.pool_zhdl != NULL) {
        /* Check dataset path on bootpool */
        if (libze_util_concat(lzeh->bootpool.root_path_full, "", be, ZFS_MAX_DATASET_NAME_LEN, 
                be_dataset_bootpool) != LIBZE_ERROR_SUCCESS) {
            return LIBZE_ERROR_MAXPATHLEN;
        }
        /* Check if dataset on bootpool exists */
        if (!zfs_dataset_exists(lzeh->lzh, be_dataset_bootpool, ZFS_TYPE_FILESYSTEM)) {
            return LIBZE_ERROR_EEXIST;
        }
        /* Check if dataset on bootpool can be opened */
        be_dataset_bootpool_hndl = zfs_open(lzeh->lzh, be_dataset_bootpool, ZFS_TYPE_FILESYSTEM);
        if (be_dataset_bootpool_hndl == NULL) {
            return LIBZE_ERROR_ZFS_OPEN;
        }
        zfs_close(be_dataset_bootpool_hndl);
    }

    return ret;
}

/**
 * @brief Checks if the specified boot environment does not exist and is valid
 * @param[in] lzeh Initialized @p libze_handle
 * @param[in] be Name of a possible new boot environment
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_EEXIST if the dataset does exist,
 *         @p LIBZE_ERROR_MAXPATHLEN if a name or path length exceeded
 */
libze_error
libze_validate_new_be(libze_handle *lzeh, const char be[static 1]) {
    libze_error ret = LIBZE_ERROR_SUCCESS;
    zfs_handle_t *be_dataset_hndl = NULL;
    zfs_handle_t *be_dataset_bootpool_hndl = NULL;
    char be_dataset[ZFS_MAX_DATASET_NAME_LEN] = "";
    char be_dataset_bootpool[ZFS_MAX_DATASET_NAME_LEN] = "";

    /* Check dataset path */
    if (libze_util_concat(lzeh->env_root, "/", be, ZFS_MAX_DATASET_NAME_LEN, be_dataset) 
            != LIBZE_ERROR_SUCCESS) {
        return LIBZE_ERROR_MAXPATHLEN;
    }
    /* Check if dataset doesn't exists */
    if (zfs_dataset_exists(lzeh->lzh, be_dataset, ZFS_TYPE_FILESYSTEM)) {
        return LIBZE_ERROR_EEXIST;
    }

    if (lzeh->bootpool.pool_zhdl != NULL) {
        /* Check dataset path on bootpool */
        if (libze_util_concat(lzeh->bootpool.root_path_full, "", be, ZFS_MAX_DATASET_NAME_LEN, 
                be_dataset_bootpool) != LIBZE_ERROR_SUCCESS) {
            return LIBZE_ERROR_MAXPATHLEN;
        }
        /* Check if dataset on bootpool doesn't exists */
        if (zfs_dataset_exists(lzeh->lzh, be_dataset_bootpool, ZFS_TYPE_FILESYSTEM)) {
            return LIBZE_ERROR_EEXIST;
        }
    }

    return ret;
}

/**
 * @brief Check if the specified boot environment is set as active
 * @param[in] lzeh Initialized @p libze_handle
 * @param[in] be Dataset or name of a boot environment to check
 * @return @p B_TRUE if active, else @p B_FALSE
 */
boolean_t
libze_is_active_be(libze_handle *lzeh, const char be[static 1]) {
    if (strchr(be, '/') == NULL) {
        return ((strcmp(lzeh->env_activated, be) == 0) ? B_TRUE : B_FALSE);
    }
    else {
        return ((strcmp(lzeh->env_activated_path, be) == 0) ? B_TRUE : B_FALSE);
    }
}

/**
 * @brief Check if the specified boot environment is currently running.
 * @param[in] lzeh Initialized @p libze_handle
 * @param[in] be Dataset or name of a boot environment to check
 * @return @p B_TRUE if running, else @p B_FALSE
 */
boolean_t
libze_is_root_be(libze_handle *lzeh, const char be[static 1]) {
    if (strchr(be, '/') == NULL) {
        return ((strcmp(lzeh->env_running, be) == 0) ? B_TRUE : B_FALSE);
    }
    else {
        return ((strcmp(lzeh->env_running_path, be) == 0) ? B_TRUE : B_FALSE);
    }
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
 * @brief Get the root dataset
 * @param[in] lzeh Initialized @p libze_handle
 * @return Zero on success
 *
 * @pre lzeh != NULL
 */
int
libze_get_root_dataset(libze_handle *lzeh) {
    zfs_handle_t *zh;
    int ret = 0;

    char rootfs[ZFS_MAX_DATASET_NAME_LEN];

    // Make sure type is ZFS
    if (libze_dataset_from_mountpoint("/", ZFS_MAX_DATASET_NAME_LEN, rootfs) != SYSTEM_ERR_SUCCESS) {
        return -1;
    }

    if ((zh = zfs_path_to_zhandle(lzeh->lzh, "/", ZFS_TYPE_FILESYSTEM)) == NULL) {
        return -1;
    }

    if (strlcpy(lzeh->env_running_path, zfs_get_name(zh), ZFS_MAX_DATASET_NAME_LEN) >= ZFS_MAX_DATASET_NAME_LEN) {
        strlcpy(lzeh->env_running_path, "", ZFS_MAX_DATASET_NAME_LEN);
        ret = -1;
    }
    else if (libze_boot_env_name(lzeh->env_running_path, ZFS_MAX_DATASET_NAME_LEN, lzeh->env_running) != 0) {
        ret = -1;
        strlcpy(lzeh->env_running, "", ZFS_MAX_DATASET_NAME_LEN);
        strlcpy(lzeh->env_running_path, "", ZFS_MAX_DATASET_NAME_LEN);
    }

    zfs_close(zh);
    return ret;
}

/**
 * @brief Returns the name of the ZFS pool from the specified dataset (everything to first '/')
 * @param[in] buflen Length of buffer
 * @param[out] buf Buffer to place boot environment in
 * @return Zero on success
 */
int
libze_get_zpool_name_from_dataset(const char dataset[static 3], size_t buflen, char buf[buflen]) {
    if (buflen > 0) {
        if (dataset[0] == '/') {
            return -1;
        }
        for (size_t i = 1; i < buflen; ++i) {
            if (dataset[i] == '/') {
                (void)strlcpy(buf, dataset, i+1);
                buf[i] = '\0';
                return 0;
            }
        }
    }
    return -1;
}

/**
 * @brief Open and return the handle(s) from the dataset(s) corresponding to the specified boot
 *        environment. In case that no bootpool is available, @p be_bpool_zh will be set to NULL.
 *        In case that an error occurred, both handles will be set to NULL and an error is returned.
 * @param[in] lzeh Initialized @p libze_handle
 * @param[in] be Is the name of a boot environment
 * @param[out] be_zh Used to return the opened dataset handle of the specified boot environment
 * @param[out] be_bpool_zh Used to return the opened dataset handle of the bootpool of the specified
 *             boot environment
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_EEXIST if the dataset does not exist,
 *         @p LIBZE_ERROR_MAXPATHLEN if a name or path length exceeded,
 *         @p LIBZE_ERROR_ZFS_OPEN if the dataset exists but can't be opened
 * 
 * @post @p lzeh->bootpool.pool_zhdl == NULL:
 *       on success, @p be_zh != NULL && @p be_bpool_zh == NULL
 *       on failure, @p be_zh == NULL && @p be_bpool_zh == NULL
 * 
 * @post @p lzeh->bootpool.pool_zhdl != NULL:
 *       on success, @p be_zh != NULL && @p be_bpool_zh != NULL
 *       on failure, @p be_zh == NULL && @p be_bpool_zh == NULL
 */
libze_error
libze_util_open_boot_environment(libze_handle* lzeh, const char be[static 1], zfs_handle_t **be_zh,
                                 zfs_handle_t **be_bpool_zh) {
    libze_error ret = LIBZE_ERROR_SUCCESS;
    char be_dataset[ZFS_MAX_DATASET_NAME_LEN] = "";
    char be_dataset_bpool[ZFS_MAX_DATASET_NAME_LEN] = "";
    *be_zh = NULL;
    *be_bpool_zh = NULL;

    if ((ret = libze_validate_existing_be(lzeh, be)) != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    (void)libze_util_concat(lzeh->env_root, "/", be, ZFS_MAX_DATASET_NAME_LEN, be_dataset);
    *be_zh = zfs_open(lzeh->lzh, be_dataset, ZFS_TYPE_FILESYSTEM);

    if (lzeh->bootpool.pool_zhdl != NULL) {
        (void)libze_util_concat(lzeh->bootpool.root_path_full, "", be, ZFS_MAX_DATASET_NAME_LEN,
                                be_dataset_bpool);
        *be_bpool_zh = zfs_open(lzeh->lzh, be_dataset_bpool, ZFS_TYPE_FILESYSTEM);

        if (*be_bpool_zh == NULL) {
            zfs_close(*be_zh);
            *be_zh = NULL;
        }
    }

    return (*be_zh != NULL ? LIBZE_ERROR_SUCCESS : LIBZE_ERROR_ZFS_OPEN);
}

libze_error
libze_util_temporary_mount(const char dataset[ZFS_MAX_DATASET_NAME_LEN], const char mountpoint[static 2]) {
    const char *mount_settings = "zfsutil";
    const char *mount_type = "zfs";
    const unsigned long mount_flags = 0;

    if (mount(dataset, mountpoint, mount_type, mount_flags, mount_settings) != 0) {
        return LIBZE_ERROR_UNKNOWN;
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief Copy binary file into new binary file
 * @param file Original file (rb)
 * @param new_file New file (wb)
 * @return 0 on success else appropriate error as returned by errno
 */
static int
libze_util_copy_filepointer(FILE *file, FILE *new_file)
{
    assert(file != NULL);
    assert(new_file != NULL);

    errno = 0;
    char buf[COPY_BUFLEN];

    while(B_TRUE) {
        size_t r = fread(buf, 1, COPY_BUFLEN, file);
        if(r != COPY_BUFLEN) {
            if (ferror(file) != 0) {
                return errno;
            }
            if (r == 0) {
                fflush(new_file);
                return 0; /* EOF */
            }
        }

        fwrite(buf, 1, r, new_file);
        if (ferror(new_file) != 0) {
            return errno;
        }
    }
}

/**
 * @brief Copy binary file into new file
 * @param file Original filename
 * @param new_file New filename
 * @return 0 on success else appropriate error as returned by errno
 */
int
libze_util_copy_file(const char *filename, const char *new_filename)
{
    FILE *file = NULL;
    FILE *new_file = NULL;

    libze_error ret = 0;
    errno = 0;

    file = fopen(filename, "rb");
    if (file == NULL) {
        return errno;
    }

    new_file = fopen(new_filename, "w+b");
    if (new_file == NULL) {
        ret = errno;
        goto err;
    }

    ret = libze_util_copy_filepointer(file, new_file);

err:
    fclose(file);
    if (new_file != NULL) {
        fclose(new_file);
    }

    return ret;
}

/**
 * @brief Copy a directory recursively
 * @param directory_path Directory to copy
 * @param new_directory_path Destination to copy to
 * @return 0 on success else appropriate error as returned by errno
 */
int
libze_util_copydir(const char directory_path[LIBZE_MAX_PATH_LEN],
                   const char new_directory_path[LIBZE_MAX_PATH_LEN]) {
    DIR *directory = NULL;
    DIR *new_directory = NULL;
    char path[LIBZE_MAX_PATH_LEN];
    char new_path[LIBZE_MAX_PATH_LEN];
    char *fin = path;
    char *new_fin = new_path;
    struct dirent *de;
    struct stat st;

    int ret = 0;
    errno = 0;

    directory = opendir(directory_path);
    if (directory == NULL) {
        return errno;
    }

    /* Check error after for TOCTOU race condition */
    int err = mkdir(new_directory_path, 0700);
    if (err != 0) {
        errno = 0;
        if (stat(new_directory_path, &st) != 0) {
            return errno;
        }

        if(!S_ISDIR(st.st_mode)) {
            return ENOTDIR;
        }
    }

    errno = 0;
    new_directory = opendir(new_directory_path);
    if (new_directory == NULL) {
        return errno;
    }

    /* Copy current path into path */
    if (strlcpy(path, directory_path, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) {
        return ENAMETOOLONG;
    }
    if (strlcpy(new_path, new_directory_path, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) {
        return ENAMETOOLONG;
    }

    /* Move the fin to end of string */
    fin += strlen(directory_path);
    new_fin += strlen(new_directory_path);

    while((de = readdir(directory)) != NULL) {

        char buf[LIBZE_MAX_PATH_LEN];

        /* Copy filename into path */
        if ((strlcpy(buf, "/", LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
            (strlcat(buf, de->d_name, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN)) {
            return ENAMETOOLONG;
        }

        if ((strlcpy(fin, buf, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
            (strlcpy(new_fin, buf, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN)) {
            return ENAMETOOLONG;
        }

        if (stat(path, &st) != 0) {
            return errno;
        }

        if(S_ISDIR(st.st_mode)) {
            if ((strcmp(fin, "/.") == 0) || (strcmp(fin, "/..") == 0)) {
                /* Skip entering "." or ".." */
                continue;
            }

            /* path is directory, recurse */
            if ((ret = libze_util_copydir(path, new_directory_path)) != 0) {
                return ret;
            }
        }

        if(S_ISREG(st.st_mode)) {
            /* path is file, copy */
            if ((ret = libze_util_copy_file(path, new_path) != 0)) {
                return ret;
            }
        }

        /* Otherwise do nothing */
    }

    return 0;
}

/**
 * @brief Global string search and replace
 * @param to_replace String to replace
 * @param replacement Replacement string
 * @param line_length Line length
 * @param line Line to search
 * @param line_replaced_length line_replaced length
 * @param line_replaced Modified line
 * @return @p LIBZE_ERROR_SUCCESS on success
 *         @p LIBZE_ERROR_MAXPATHLEN When LIBZE_MAX_PATH_LEN exceeded
 *         @p LIBZE_ERROR_NOMEM
 */
libze_error
libze_util_replace_string(const char *to_replace, const char *replacement,
                          size_t line_length,
                          const char line[line_length],
                          size_t line_replaced_length,
                          char line_replaced[line_replaced_length]) {

    libze_error ret = LIBZE_ERROR_SUCCESS;
    char *result_buf = NULL;
    char *insert_location = NULL;

    /* Distance between replacement and last replacement */
    size_t len_between_replacements;

    /* Number of occurrences of to_replace */
    int count;

    size_t len_rep = strlen(to_replace);
    // Empty replacement, skip
    if (len_rep == 0) {
        if (strlcpy(line_replaced, line, line_replaced_length) >= line_replaced_length) {
            return LIBZE_ERROR_MAXPATHLEN;
        }
        return LIBZE_ERROR_SUCCESS;
    }

    size_t len_with;
    if (replacement == NULL) {
        replacement = "";
    }
    len_with = strlen(replacement);

    char tmp_line[line_length];
    if (strlcpy(tmp_line, line, line_length) >= line_length) {
        return LIBZE_ERROR_MAXPATHLEN;
    }
    /* remainder of line after last replacement */
    char *tmp_line_loc = tmp_line;

    /* Count the number of replacements */
    char *replace_loc;
    insert_location = tmp_line;
    for (count = 0; (replace_loc = strstr(insert_location, to_replace)); ++count) {
        insert_location = replace_loc + len_rep;
    }

    size_t buf_size = strlen(tmp_line) + (len_with - len_rep) * count + 1;
    size_t buf_left = buf_size;

    result_buf = malloc(buf_size);
    if (result_buf == NULL) {
        return LIBZE_ERROR_NOMEM;
    }

    /* end of the result string */
    char *result_end = result_buf;
    while ((count--) > 0) {
        /* next occurrence of to_replace in line */
        insert_location = strstr(tmp_line_loc, to_replace);
        len_between_replacements = insert_location - tmp_line_loc;

        /* Copy len_between_replacements to result_end */
        result_end = strncpy(result_end, tmp_line_loc, len_between_replacements) + len_between_replacements;

        buf_left = buf_size - (result_end - result_buf);
        if (strlcpy(result_end, replacement, buf_left) >= buf_left) {
            ret = LIBZE_ERROR_MAXPATHLEN;
            goto err;
        }

        result_end += len_with;

        /* Go to next replacement */
        tmp_line_loc += len_between_replacements + len_rep;
    }

    buf_left = buf_size - (result_end - result_buf);
    if (strlcpy(result_end, tmp_line_loc, buf_left) >= buf_left) {
        ret = LIBZE_ERROR_MAXPATHLEN;
        goto err;
    }

    if (strlcpy(line_replaced, result_buf, line_replaced_length) >= line_replaced_length) {
        ret = LIBZE_ERROR_MAXPATHLEN;
    }

err:
    free(result_buf);
    return ret;
}