//
// Created by john on 12/25/18.
//

#ifndef ZECTL_LIBZE_UTIL_H
#define ZECTL_LIBZE_UTIL_H

#include <stdio.h>
#include <string.h>
#include <stdio.h>

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
libze_util_suffix_after_string(const char root[static 1], const char dataset[static 1], size_t buflen, char buf[buflen]);

#endif //ZECTL_LIBZE_UTIL_H
