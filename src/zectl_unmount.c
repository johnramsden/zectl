#include "zectl.h"

libze_error
ze_unmount(libze_handle *lzeh, int argc, char **argv) {
    libze_error ret = LIBZE_ERROR_SUCCESS;
    int opt;

    opterr = 0;

    while ((opt = getopt(argc, argv, "")) != -1) {
        switch (opt) {
            default:
                fprintf(stderr, "%s destroy: unknown option '-%c'\n", ZE_PROGRAM, optopt);
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

    if ((ret = libze_unmount(lzeh, argv[0])) != LIBZE_ERROR_SUCCESS) {
        fputs(lzeh->libze_error_message, stderr);
    }

err:
    return ret;
}