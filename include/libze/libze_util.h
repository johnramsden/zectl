#ifndef ZECTL_LIBZE_UTIL_H
#define ZECTL_LIBZE_UTIL_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>

#include "libze/libze.h"

#define COPY_BUFLEN 4096

#if defined(DEBUG) && DEBUG > 0
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define DEBUG_PRINT(fmt, args...) do { \
    fprintf(stderr, "DEBUG: %s:%d:%s(): " \
                    fmt, __FILENAME__, __LINE__, __func__, ##args); \
    fprintf(stderr, "\n"); \
    } while(0)
#else
#define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
#endif

int
libze_util_concat(const char *prefix, const char *separator, const char *suffix,
                  size_t buflen, char buf[buflen]);

int
libze_util_cut(const char path[static 1], size_t buflen, char buf[buflen], char delimiter);

int
libze_util_suffix_after_string(const char root[static 1], const char dataset[static 1],
                               size_t buflen, char buf[buflen]);

int
libze_get_root_dataset(libze_handle *lzeh);

int
libze_get_zpool_name_from_dataset(const char dataset[static 3], size_t buflen, char buf[buflen]);

boolean_t
libze_is_active_be(libze_handle *lzeh, const char be[static 1]);
boolean_t
libze_is_root_be(libze_handle *lzeh, const char be[static 1]);

libze_error
libze_util_open_boot_environment(libze_handle* lzeh, const char be[static 1],
                                 zfs_handle_t **be_zh,
                                 char be_ds[ZFS_MAX_DATASET_NAME_LEN],
                                 zfs_handle_t **be_bpool_zh,
                                 char be_bpool_ds[ZFS_MAX_DATASET_NAME_LEN]);

libze_error
libze_util_temporary_mount(const char dataset[ZFS_MAX_DATASET_NAME_LEN], const char mountpoint[static 2]);

void
libze_list_free(nvlist_t *nvl);

int
libze_util_copy_file(const char *filename, const char *new_filename);

int
libze_util_copydir(const char directory_path[LIBZE_MAX_PATH_LEN],
                   const char new_directory_path[LIBZE_MAX_PATH_LEN]);

libze_error
libze_util_replace_string(const char *to_replace, const char *replacement,
                          size_t line_length,
                          const char line[line_length],
                          size_t line_replaced_length,
                          char line_replaced[line_replaced_length]);

#endif //ZECTL_LIBZE_UTIL_H
