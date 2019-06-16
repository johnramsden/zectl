//
// Created by john on 12/25/18.
//

#ifndef ZECTL_UTIL_H
#define ZECTL_UTIL_H

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

char *file_contents(const char *file);

typedef struct node node_t;
typedef struct prop prop_t;

struct prop {
    char *name;
    char *value;
};

struct node {
    node_t *next;
    prop_t *property;
};


int
form_property_string(const char namespace[static 1], const char property[static 1],
                     size_t buflen, char buf[buflen]);
int
form_dataset_string(const char root[static 1], const char boot_env[static 1],
                    size_t buflen, char buf[buflen]);
int
form_snapshot_string(const char dataset[static 1], const char snap_name[static 1],
                     size_t buflen, char buf[buflen]);
#endif //ZECTL_UTIL_H
