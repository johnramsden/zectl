//
// Created by john on 12/25/18.
//

#include "util.h"

#include <stdio.h>
#include <stdlib.h>

char *file_contents(const char *file) {
    char *buffer = NULL;
    FILE *fp = fopen(file, "rb");

    if (fp) {
        fseek (fp, 0, SEEK_END);
        size_t length = (size_t) ftell(fp);
        fseek (fp, 0, SEEK_SET);
        buffer = malloc(length);
        if (buffer) {
            fread (buffer, 1, length, fp);
        }
        fclose (fp);
    }

    return buffer;
}
