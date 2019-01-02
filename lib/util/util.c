//
// Created by john on 12/25/18.
//

#include "util/util.h"

#include <stdlib.h>
#include <string.h>

int
copy_string(char *dst, const char *src, size_t size) {
    int ret = 0;
    if (strlcpy(dst, src, size) == 0) {
        ret = -1;
    }
    return ret;
//    int n = snprintf(dst, size, "%s", src);
//    if (n >= size) {
//        ret = -1;
//    }
//    return ret;
}

char *file_contents(const char *file) {
    char *buffer = NULL;
    FILE *fp = fopen(file, "rb");

    if (!fp) {
        return NULL;
    }

    fseek (fp, 0, SEEK_END);
    size_t length = (size_t) ftell(fp);
    fseek (fp, 0, SEEK_SET);
    buffer = malloc(length);
    if (buffer) {
        fread (buffer, 1, length, fp);
    }
    fclose (fp);

    return buffer;
}

/*
 * Simple linked list
 */

/*
 * create_node:
 * Create a node for property list.
 */
node_t *
create_node(prop_t *property, node_t *next) {
    node_t *new_node = malloc(sizeof(node_t));

    if (new_node != NULL) {
        new_node->property = property;
        new_node->next = next;
    } else {
        fprintf(stderr, "Error allocating memory while creating node.\n");
    }

    return new_node;
}

node_t *
prepend_node(prop_t *property, node_t *head) {
    node_t *new_node = create_node(property, head);
    if (new_node != NULL) {
        head = new_node;
    } else {
        fprintf(stderr, "Error prepend node due to failure to create node.\n");
    }

    return head;
}

//void
//destroy_list(node_t *head) {
//    node_t *temp;
//
//    while (head != NULL) {
//        temp = head;
//        head = head->next;
//        destroy_property(temp->property);
//        free(temp);
//    }
//}