#include "util/util.h"

#include <stdlib.h>
#include <string.h>
#include <libzfs.h>

static int
concat_with_separator(const char *prefix, const char *separator, const char *suffix,
                      size_t buflen, char buf[buflen]) {
    if ((strlcat(buf, prefix, buflen) >= buflen) ||
        (strlcat(buf, separator, buflen) >= buflen) ||
        (strlcat(buf, suffix, buflen) >= buflen)) {
        return -1;
    }

    return 0;
}

int
form_property_string(const char namespace[static 1], const char property[static 1],
        size_t buflen, char buf[buflen]) {
    return concat_with_separator(namespace, ":", property, buflen, buf);
}

int
form_dataset_string(const char root[static 1], const char boot_env[static 1],
        size_t buflen, char buf[buflen]) {
    return concat_with_separator(root, "/", boot_env, buflen, buf);
}

int
form_snapshot_string(const char dataset[static 1], const char snap_name[static 1],
        size_t buflen, char buf[buflen]) {
    return concat_with_separator(dataset, "@", snap_name, buflen, buf);
}

char *file_contents(const char *file) {
    char *buffer = NULL;
    FILE *fp = fopen(file, "rb");

    if (fp == NULL) {
        return NULL;
    }

    fseek (fp, 0, SEEK_END);
    size_t length = (size_t) ftell(fp);
    fseek (fp, 0, SEEK_SET);
    buffer = malloc(length);
    if (buffer != NULL) {
        fread (buffer, 1, length, fp);
    }
    fclose (fp);

    return buffer;
}
