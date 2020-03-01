#include "zectl.h"

#include <stdio.h>
#include <unistd.h>

libze_error
ze_unmount(libze_handle *lzeh, int argc, char **argv) {
    int opt;
    opterr = 0;

    while ((opt = getopt(argc, argv, "")) != -1) {
        switch (opt) {
            default:
                fprintf(stderr, "%s unmount: unknown option '-%c'\n", ZE_PROGRAM, optopt);
                ze_usage();
                return LIBZE_ERROR_UNKNOWN;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 1) {
        fprintf(stderr, "%s unmount: wrong number of arguments\n", ZE_PROGRAM);
        ze_usage();
        return LIBZE_ERROR_UNKNOWN;
    }

    return libze_unmount(lzeh, argv[0]);
}
