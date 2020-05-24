#ifndef ZECTL_LIBZE_UTIL_H
#define ZECTL_LIBZE_UTIL_H

#include "libze/libze.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define COPY_BUFLEN 4096
#define LIBZE_UTIL_MAX_REGEX_GROUPS 10

#if defined(DEBUG) && DEBUG > 0
#    define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#    define DEBUG_PRINT(fmt, args...)                                                              \
        do {                                                                                       \
            fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, __FILENAME__, __LINE__, __func__, ##args);  \
            fprintf(stderr, "\n");                                                                 \
        } while (0)
#else
#    define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
#endif

int
libze_util_concat(char const *prefix, char const *separator, char const *suffix, size_t buflen,
                  char buf[buflen]);

int
libze_util_cut(char const path[static 1], size_t buflen, char buf[buflen], char delimiter);

int
libze_util_split(char const path[static 1], size_t buflen, char buf_pre[buflen],
                 char buf_post[buflen], char delimiter);

int
libze_util_suffix_after_string(char const root[static 1], char const dataset[static 1],
                               size_t buflen, char buf[buflen]);

int
libze_get_root_dataset(libze_handle *lzeh);

int
libze_get_zpool_name_from_dataset(char const dataset[static 3], size_t buflen, char buf[buflen]);

boolean_t
libze_is_active_be(libze_handle *lzeh, char const be[static 1]);

boolean_t
libze_is_root_be(libze_handle *lzeh, char const be[static 1]);

libze_error
libze_util_temporary_mount(char const dataset[ZFS_MAX_DATASET_NAME_LEN],
                           char const mountpoint[static 2]);

void
libze_list_free(nvlist_t *nvl);

int
libze_util_copy_file(char const *filename, char const *new_filename);

int
libze_util_copydir(char const directory_path[LIBZE_MAX_PATH_LEN],
                   char const new_directory_path[LIBZE_MAX_PATH_LEN]);

int
libze_util_rmdir(char const directory_path[LIBZE_MAX_PATH_LEN]);

int
libze_util_mkdir(char const directory_path[LIBZE_MAX_PATH_LEN], mode_t mode);

libze_error
libze_util_replace_string(char const *to_replace, char const *replacement, size_t line_length,
                          char const line[line_length], size_t line_replaced_length,
                          char line_replaced[line_replaced_length]);

libze_error
libze_util_regex_subexpr_replace(regex_t *re, size_t replace_buflen,
                                 char const replace[replace_buflen], size_t input_buflen,
                                 char const input[input_buflen], size_t output_buflen,
                                 char output[output_buflen]);

#endif // ZECTL_LIBZE_UTIL_H
