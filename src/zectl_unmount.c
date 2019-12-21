#include "zectl.h"

/**
 * @brief Unmount command main function
 * @param lzeh Initialized handle to libze object
 * @param argc As passed to main
 * @param argv As passed to main, contains boot env to unmount
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p TODO comment error
 */
libze_error ze_unmount(libze_handle *lzeh, int argc, char **argv) {
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
        fprintf(stderr, "%s unmount: wrong number of arguments.\n", ZE_PROGRAM);
        ze_usage();
        return LIBZE_ERROR_UNKNOWN;
    }

    return libze_unmount(lzeh, argv[0]);
}