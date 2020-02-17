// Required for spl stat.h
#define __USE_LARGEFILE64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include "libze/libze_util.h"

#include "libze/libze.h"
#include "system_linux.h"

#include <dirent.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>

#define ASCII_OFFSET 48

/**
 * @brief Concatenate two strings with a separator
 *
 * @param[in] prefix     Prefix string
 * @param[in] separator  Separator string
 * @param[in] suffix     Suffix string
 * @param[in] buflen     Length of buffer
 * @param[out] buf       Resulting concatenated string
 *
 * @return Nonzero if the resulting string is longer than the buffer length
 */
int
libze_util_concat(char const *prefix, char const *separator, char const *suffix, size_t buflen,
                  char buf[buflen]) {

    (void) strlcpy(buf, "", buflen);

    if ((strlcat(buf, prefix, buflen) >= buflen) || (strlcat(buf, separator, buflen) >= buflen) ||
        (strlcat(buf, suffix, buflen) >= buflen)) {
        return -1;
    }

    return 0;
}

/**
 * @brief Cut a string at the last instance of a delimiter
 *
 * @param[in] path       String to cut
 * @param[in] buflen     Length of buffer
 * @param[out] buf       Prefix before last instance of delimiter
 * @param[in] delimiter  Delimiter to cut string at
 *
 * @return Non-zero if buffer is too short, or there is no instance of delimiter
 */
int
libze_util_cut(char const path[], size_t buflen, char buf[buflen], char delimiter) {
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
 * @brief Split a string at the last instance of a delimiter
 *
 * @param[in] path       String to cut
 * @param[in] buflen     Length of buffer
 * @param[out] buf_pre   Prefix before last instance of delimiter
 * @param[out] buf_post  Suffix after last instance of delimiter
 * @param[in] delimiter  Delimiter to cut string at
 *
 * @return Non-zero if buffer is too short, or there is no instance of delimiter
 */
int
libze_util_split(char const path[], size_t buflen, char buf_pre[buflen],
        char buf_post[buflen], char delimiter) {
    char *slashp = NULL;

    if (strlcpy(buf_pre, path, buflen) >= buflen) {
        return -1;
    }

    /* Get pointer to last instance of '/' */
    if ((slashp = strrchr(buf_pre, delimiter)) == NULL) {
        return -1;
    }

    /* terminate string at '/' */
    *slashp = '\0';
    slashp++;

    if (strlcpy(buf_post, slashp, buflen) >= buflen) {
        return -1;
    }

    return 0;
}

/**
 * @brief Given a dataset, return just the portion after the root of boot environments
 *
 * @param[in] root     Root of boot environments
 * @param[in] dataset  Full dataset to get suffix of
 * @param[in] buflen   Length of buffer
 * @param[out] buf     Buffer to place suffix in
 *
 * @return Non-zero if there is no parent (path is just the name of the pool),
 *         or if the length of the buffer is exceeded
 */
int
libze_util_suffix_after_string(char const root[], char const dataset[],
                               size_t buflen, char buf[buflen]) {

    if (strlcpy(buf, dataset, buflen) >= buflen) {
        return -1;
    }

    size_t loc = strlen(root) + 1;
    if (loc >= buflen) {
        return -1;
    }

    /* get substring after next '/' */
    if (strlcpy(buf, buf + loc, buflen) >= buflen) {
        return -1;
    }

    return 0;
}

/**
 * @brief Given a dataset, get the name of the boot environment
 *
 * @param[in] dataset  Dataset to get the boot environment of
 * @param[in] buflen   Length of buffer
 * @param[out] buf     Buffer to place boot environment in
 *
 * @return Non-zero if the length of the buffer is exceeded,
 *         or if there is no / contained in the data set
 */
int
libze_boot_env_name(char const *dataset, size_t buflen, char buf[buflen]) {
    char *slashp = NULL;

    if (strlcpy(buf, dataset, buflen) >= buflen) {
        return -1;
    }

    /* Get pointer to last instance of '/' */
    if ((slashp = strrchr(buf, '/')) == NULL) {
        return -1;
    }

    /* get substring after last '/' */
    if (strlcpy(buf, slashp + 1, buflen) >= buflen) {
        return -1;
    }

    return 0;
}

/**
 * @brief Check if the specified boot environment is set as active
 *
 * @param[in] lzeh  Initialized @p libze_handle
 * @param[in] be    Dataset or name of a boot environment to check
 *
 * @return @p B_TRUE if active, else @p B_FALSE
 */
boolean_t
libze_is_active_be(libze_handle *lzeh, char const be[]) {
    if (strchr(be, '/') == NULL) {
        return ((strcmp(lzeh->env_activated, be) == 0) ? B_TRUE : B_FALSE);
    }

    return ((strcmp(lzeh->env_activated_path, be) == 0) ? B_TRUE : B_FALSE);
}

/**
 * @brief Check if the specified boot environment is currently running.
 *
 * @param[in] lzeh  Initialized @p libze_handle
 * @param[in] be    Dataset or name of a boot environment to check
 *
 * @return @p B_TRUE if running, else @p B_FALSE
 */
boolean_t
libze_is_root_be(libze_handle *lzeh, char const be[]) {
    if (strchr(be, '/') == NULL) {
        return ((strcmp(lzeh->env_running, be) == 0) ? B_TRUE : B_FALSE);
    }

    return ((strcmp(lzeh->env_running_path, be) == 0) ? B_TRUE : B_FALSE);
}

/**
 * @brief Free an nvlist and one level down of it's children
 *
 * @param[in] nvl  nvlist to free
 */
void
libze_list_free(nvlist_t *nvl) {
    if (nvl == NULL) {
        return;
    }

    nvpair_t *pair = NULL;
    for (pair = nvlist_next_nvpair(nvl, NULL); pair != NULL; pair = nvlist_next_nvpair(nvl, pair)) {
        nvlist_t *ds_props = NULL;
        nvpair_value_nvlist(pair, &ds_props);
        fnvlist_free(ds_props);
    }

    nvlist_free(nvl);
}

/**
 * @brief Get the root dataset
 *
 * @param[in] lzeh  Initialized @p libze_handle
 *
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
    if (libze_dataset_from_mountpoint("/", ZFS_MAX_DATASET_NAME_LEN, rootfs) !=
        SYSTEM_ERR_SUCCESS) {
        return -1;
    }

    if ((zh = zfs_path_to_zhandle(lzeh->lzh, "/", ZFS_TYPE_FILESYSTEM)) == NULL) {
        return -1;
    }

    if (strlcpy(lzeh->env_running_path, zfs_get_name(zh), ZFS_MAX_DATASET_NAME_LEN) >=
        ZFS_MAX_DATASET_NAME_LEN) {
        strlcpy(lzeh->env_running_path, "", ZFS_MAX_DATASET_NAME_LEN);
        ret = -1;
    } else if (libze_boot_env_name(lzeh->env_running_path, ZFS_MAX_DATASET_NAME_LEN,
                                   lzeh->env_running) != 0) {
        ret = -1;
        strlcpy(lzeh->env_running, "", ZFS_MAX_DATASET_NAME_LEN);
        strlcpy(lzeh->env_running_path, "", ZFS_MAX_DATASET_NAME_LEN);
    }

    zfs_close(zh);
    return ret;
}

/**
 * @brief Returns the name of the ZFS pool from the specified dataset (everything to first '/')
 *
 * @param[in] buflen  Length of buffer
 * @param[out] buf    Buffer to place boot environment in
 *
 * @return Zero on success
 */
int
libze_get_zpool_name_from_dataset(char const dataset[], size_t buflen, char buf[buflen]) {
    if (buflen > 0) {
        if (dataset[0] == '/') {
            return -1;
        }
        for (size_t i = 1; i < buflen; ++i) {
            if (dataset[i] == '/') {
                (void) strlcpy(buf, dataset, i + 1);
                buf[i] = '\0';
                return 0;
            }
        }
    }
    return -1;
}

/**
 * @brief Mount dataset temporarily using zfsutil
 *
 * @param[in] dataset     Dataset to mount
 * @param[in] mountpoint  Mountpoint location
 *
 * @return @p LIBZE_ERROR_SUCCESS on success, @p LIBZE_ERROR_UNKNOWN on failure
 */
libze_error
libze_util_temporary_mount(char const dataset[ZFS_MAX_DATASET_NAME_LEN],
                           char const mountpoint[]) {
    char const *mount_settings = "zfsutil";
    char const *mount_type = "zfs";
    const unsigned long mount_flags = 0;

    if (mount(dataset, mountpoint, mount_type, mount_flags, mount_settings) != 0) {
        return LIBZE_ERROR_UNKNOWN;
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief Copy binary file into new binary file
 *
 * @param[in] file      Original file (rb)
 * @param[in] new_file  New file (wb)
 *
 * @return 0 on success else appropriate error as returned by @p errno
 */
static int
libze_util_copy_filepointer(FILE file[], FILE new_file[]) {
    errno = 0;
    char buf[COPY_BUFLEN];

    while (B_TRUE) {
        size_t r = fread(buf, 1, COPY_BUFLEN, file);
        if (r != COPY_BUFLEN) {
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
 *
 * @param file Original filename
 * @param new_file New filename
 *
 * @return 0 on success else appropriate error as returned by errno
 */
int
libze_util_copy_file(char const *filename, char const *new_filename) {
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
 * @brief Prototype for recursive_traverse callback
 */
typedef int (*traverse_cb)(char const dirname[LIBZE_MAX_PATH_LEN],
                           char const filename_suffix[LIBZE_MAX_PATH_LEN], struct stat *st,
                           void *data);

/**
 * @brief Recursive directory traversal function.
 *
 * @param[in] directory_path
 * @param[in] cb              Callback to run on each file and path.
 *                            Should call @p recursive_traverse if traversal should continue
 * @param[in] data            Callback data
 *
 * @return 0 on success, else appropriate error as returned by @p errno
 */
static int
recursive_traverse(char const directory_path[LIBZE_MAX_PATH_LEN], traverse_cb cb, void *data) {
    DIR *directory = NULL;
    char path_to_item[LIBZE_MAX_PATH_LEN];
    char *fin = path_to_item;
    struct dirent *de;
    struct stat st;

    int ret = 0;
    errno = 0;

    /* Copy current directory into path_to_item */
    if (strlcpy(path_to_item, directory_path, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) {
        return ENAMETOOLONG;
    }
    directory = opendir(directory_path);
    if (directory == NULL) {
        return errno;
    }

    /* Move the fin to end of string */
    fin += strlen(directory_path);

    while ((de = readdir(directory)) != NULL) {

        char buf[LIBZE_MAX_PATH_LEN];

        /* Copy filename into path */
        if ((strlcpy(buf, "/", LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
            (strlcat(buf, de->d_name, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN)) {
            ret = ENAMETOOLONG;
            goto done;
        }

        /* Append file to path */
        if (strlcpy(fin, buf, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) {
            ret = ENAMETOOLONG;
            goto done;
        }

        if (stat(path_to_item, &st) != 0) {
            ret = errno;
            goto done;
        }

        /* call cb */
        if ((ret = cb(directory_path, buf, &st, data) != 0)) {
            goto done;
        }

        /* Otherwise do nothing */
    }

done:
    errno = 0;
    /* Propagate errno if no previous error */
    if ((closedir(directory) != 0) && ret == 0) {
        return errno;
    }
    return ret;
}

struct copy_data {
    char const *dest;
};

/**
 * @brief Remove callback function for @p libze_util_rmdir
 *
 * @param[in] dirname          Name of parent of file or directory being removed
 * @param[in] filename_suffix  Name of file or directory being removed prefixed with '/'
 * @param[in] st               Stat buffer of dirname
 * @param[in] data             Callback data, unused
 *
 * @return 0 on success else appropriate error as returned by errno
 */
static int
rmdir_cb(char const dirname[LIBZE_MAX_PATH_LEN], char const filename_suffix[LIBZE_MAX_PATH_LEN],
         struct stat *st, void *data) {

    (void) data;

    int ret = 0;

    char path_to_item[LIBZE_MAX_PATH_LEN];

    /* Copy current path into path_to_item */
    if ((strlcpy(path_to_item, dirname, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
        (strlcat(path_to_item, filename_suffix, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN)) {
        return ENAMETOOLONG;
    }

    if (S_ISDIR(st->st_mode)) {
        if ((strcmp(filename_suffix, "/.") == 0) || (strcmp(filename_suffix, "/..") == 0)) {
            /* Skip entering "." or ".." */
            return 0;
        }

        /* path is directory, recurse */
        if ((ret = recursive_traverse(path_to_item, rmdir_cb, data)) != 0) {
            return ret;
        }

        errno = 0;
        if ((ret = rmdir(path_to_item)) != 0) {
            return errno;
        }

        return ret;
    }

    if (S_ISREG(st->st_mode)) {
        /* path is file, call remove */
        errno = 0;
        if (remove(path_to_item) != 0) {
            return errno;
        }

        return 0;
    }

    return 0;
}

/**
 * @brief Copy callback function for @p libze_util_copydir
 *
 * @param[in] dirname          Name of parent of file or directory being copied
 * @param[in] filename_suffix  Name of file or directory being copied prefixed with '/'
 * @param[in] st               Stat buffer of dirname
 * @param[in] data             Callback data of type 'struct copy_data'
 *
 * @return 0 on success else appropriate error as returned by errno
 */
static int
copy_cb(char const dirname[LIBZE_MAX_PATH_LEN], char const filename_suffix[LIBZE_MAX_PATH_LEN],
        struct stat *st, void *data) {

    struct copy_data *cd = data;
    struct stat newdir_st;

    char new_path[LIBZE_MAX_PATH_LEN];

    /* Create destination directory. Check error after for TOCTOU race condition */
    int err = mkdir(cd->dest, 0700);
    if (err != 0) {
        errno = 0;
        if (stat(cd->dest, &newdir_st) != 0) {
            return errno;
        }

        if (!S_ISDIR(newdir_st.st_mode)) {
            return ENOTDIR;
        }
    }

    char path_to_item[LIBZE_MAX_PATH_LEN];
    /* Copy current path into path */
    if ((strlcpy(path_to_item, dirname, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
        (strlcat(path_to_item, filename_suffix, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN)) {
        return ENAMETOOLONG;
    }

    if ((strlcpy(new_path, cd->dest, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
        (strlcat(new_path, filename_suffix, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN)) {
        return ENAMETOOLONG;
    }

    if (S_ISDIR(st->st_mode)) {
        if ((strcmp(filename_suffix, "/.") == 0) || (strcmp(filename_suffix, "/..") == 0)) {
            /* Skip entering "." or ".." */
            return 0;
        }

        /* path is directory, recurse */
        return recursive_traverse(path_to_item, copy_cb, &(struct copy_data){.dest = new_path});
    }

    if (S_ISREG(st->st_mode)) {
        /* path is file, copy */
        return libze_util_copy_file(path_to_item, new_path);
    }

    return 0;
}

/**
 * @brief Remove a directory recursively
 *
 * @param directory_path Directory to remove
 * @return 0 on success else appropriate error as returned by errno
 */
int
libze_util_rmdir(char const directory_path[LIBZE_MAX_PATH_LEN]) {

    if (access(directory_path, F_OK) != 0) {
        return ENOENT;
    }

    int ret = recursive_traverse(directory_path, rmdir_cb, NULL);
    if (ret != 0) {
        return ret;
    }

    errno = 0;
    if ((ret = rmdir(directory_path)) != 0) {
        return errno;
    }

    return ret;
}

/**
 * @brief Copy a directory recursively
 *
 * @param directory_path Directory to copy
 * @param new_directory_path Destination to copy to
 * @return 0 on success else appropriate error as returned by errno
 */
int
libze_util_copydir(char const directory_path[LIBZE_MAX_PATH_LEN],
                   char const new_directory_path[LIBZE_MAX_PATH_LEN]) {

    return recursive_traverse(directory_path, copy_cb,
                              &(struct copy_data){.dest = new_directory_path});
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
libze_util_replace_string(char const *to_replace, char const *replacement, size_t line_length,
                          char const line[line_length], size_t line_replaced_length,
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
        result_end =
            strncpy(result_end, tmp_line_loc, len_between_replacements) + len_between_replacements;

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

/**
 * @brief Replace a string based on a regular expression and sub expression.
 *        Does nothing on no match.
 *
 * @param[in] re              Regular expression to match
 * @param[in] replace_buflen  Length of @p replace
 * @param[in] replace         Sub expression to replace with
 * @param[in] input_buflen    Length of @p input
 * @param[in] input           Input string to apply regular expression to
 * @param[in] output_buflen   Length of @p output
 * @param[out] output         Buffer containing modified string (or input on no match)
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN if @p output_buflen exceeded,
 *         @p LIBZE_ERROR_UNKNOWN if replacing subexpression fails
 */
libze_error
libze_util_regex_subexpr_replace(regex_t *re, size_t replace_buflen,
                                 char const replace[replace_buflen], size_t input_buflen,
                                 char const input[input_buflen], size_t output_buflen,
                                 char output[output_buflen]) {
    char *pos;
    int start_offset;
    int len;

    if (strlcpy(output, replace, replace_buflen) >= output_buflen) {
        return LIBZE_ERROR_MAXPATHLEN;
    }

    regmatch_t pmatch[LIBZE_UTIL_MAX_REGEX_GROUPS];
    if (regexec(re, input, LIBZE_UTIL_MAX_REGEX_GROUPS, pmatch, 0) == REG_NOMATCH) {
        return LIBZE_ERROR_SUCCESS;
    }

    /* Replace subexpression */
    for (pos = output; *pos != 0; pos++) {
        if (*pos != '\\') {
            continue;
        }
        char next_char = *(pos + 1);
        if (next_char > '0' && next_char <= '9') {
            int match_index = next_char - ASCII_OFFSET;
            start_offset = pmatch[match_index].rm_so;
            len = pmatch[match_index].rm_eo - start_offset;
            if (start_offset < 0) {
                return LIBZE_ERROR_UNKNOWN;
            }
            if ((strlen(output) + len - 1) > output_buflen) {
                return LIBZE_ERROR_MAXPATHLEN;
            }
            memmove(pos + len, pos + 2, strlen(pos) - 1);
            memmove(pos, input + start_offset, len);
            pos = pos + len - 2;
        }
    }

    return LIBZE_ERROR_SUCCESS;
}