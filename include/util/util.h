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
copy_string(char *dst, const char *src, size_t size);

node_t *
create_node(prop_t *property, node_t *next);

node_t *
prepend_node(prop_t *property, node_t *head);

void
destroy_list(node_t *head);
void
destroy_property(prop_t *prop);

prop_t *
record_property(char *prop_name, char *prop_value);

int
get_hex_as_string();

#endif //ZECTL_UTIL_H
