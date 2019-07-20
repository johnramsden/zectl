#include "libze/libze_util.h"

#include <stdlib.h>
#include <string.h>
#include <libzfs.h>

int
libze_util_concat(const char *prefix, const char *separator, const char *suffix,
                  size_t buflen, char buf[buflen]) {
    if ((strlcat(buf, prefix, buflen) >= buflen) ||
        (strlcat(buf, separator, buflen) >= buflen) ||
        (strlcat(buf, suffix, buflen) >= buflen)) {
        return -1;
    }

    return 0;
}

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

char *
libze_util_file_contents(const char file[static 1]) {
    char *buffer = NULL;
    FILE *fp = fopen(file, "rb");

    if (fp == NULL) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size_t length = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buffer = malloc(length);
    if (buffer != NULL) {
        fread(buffer, 1, length, fp);
    }
    fclose(fp);

    return buffer;
}

/*
* Given a complete name, return just the portion that refers to the suffix.
* Will return -1 if there is no parent (path is just the name of the
* pool).
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

int
libze_boot_env_name(const char *dataset, size_t buflen, char *buf) {
    char *slashp;

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

