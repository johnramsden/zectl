#include "zectl.h"

/**
 * get command main function
 * @param lzeh Initialized handle to libze object
 * @param argc As passed to main
 * @param argv As passed to main, contains boot env to create
 * @return LIBZE_ERROR_SUCCESS upon success
 */
libze_error
ze_get(libze_handle *lzeh, int argc, char **argv) {

    libze_error ret = LIBZE_ERROR_SUCCESS;

    opterr = 0;
    int opt;
    while ((opt = getopt(argc, argv, "")) != -1) {
        switch (opt) {
            default:
                fprintf(stderr, "%s get: unknown option '-%c'\n", ZE_PROGRAM, optopt);
                ze_usage();
                return LIBZE_ERROR_UNKNOWN;
        }
    }

    return ret;
}