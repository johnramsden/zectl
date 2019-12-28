#include "zectl.h"

#include <stdio.h>
#include <unistd.h>

libze_error
ze_snapshot(libze_handle *lzeh, int argc, char **argv) {
    libze_error ret = LIBZE_ERROR_SUCCESS;
    int opt;

    opterr = 0;

    // Options may be added
    while ((opt = getopt(argc, argv, "")) != -1) {
        switch (opt) {
            default:
                fprintf(stderr, "%s snapshot: unknown option '-%c'\n", ZE_PROGRAM, optopt);
                ze_usage();
                return LIBZE_ERROR_UNKNOWN;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 1) {
        fprintf(stderr, "%s snapshot: wrong number of arguments\n", ZE_PROGRAM);
        ze_usage();
        return LIBZE_ERROR_UNKNOWN;
    }

    return libze_snapshot(lzeh, argv[0]);
}