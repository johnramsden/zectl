#include "zectl.h"

/**
 * @brief Rename command main function
 * @param lzeh Initialized handle to libze object
 * @param argc As passed to main
 * @param argv As passed to main, contains boot env to rename
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p TODO comment error
 */
libze_error ze_rename(libze_handle *lzeh, int argc, char **argv) {
    int opt;
    opterr = 0;

    while ((opt = getopt(argc, argv, "")) != -1) {
        switch (opt) {
            default:
                fprintf(stderr, "%s rename: unknown option '-%c'\n", ZE_PROGRAM, optopt);
                ze_usage();
                return LIBZE_ERROR_UNKNOWN;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 2) {
        fprintf(stderr, "%s rename: wrong number of arguments.\n", ZE_PROGRAM);
        ze_usage();
        return LIBZE_ERROR_UNKNOWN;
    }
    if (strchr(argv[1], '/') != NULL) {
        fprintf(stderr, "%s rename: Boot environment name can't contain '/'\n", ZE_PROGRAM);
        return LIBZE_ERROR_UNKNOWN;
    }

    return libze_rename(lzeh, argv[0], argv[1]);
}