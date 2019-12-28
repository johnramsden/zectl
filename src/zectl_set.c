#include "zectl.h"

#include <stdio.h>
#include <sys/nvpair.h>
#include <unistd.h>

/**
 * set command main function
 * @param lzeh Initialized handle to libze object
 * @param argc As passed to main
 * @param argv As passed to main, contains boot env to create
 * @return LIBZE_ERROR_SUCCESS upon success
 */
libze_error
ze_set(libze_handle *lzeh, int argc, char **argv) {

    libze_error ret = LIBZE_ERROR_SUCCESS;
    nvlist_t *properties = NULL;

    if (argc == 0) {
        fprintf(stderr, "No properties provided\n");
        ze_usage();
        return LIBZE_ERROR_UNKNOWN;
    }

    if (argc > 1 && argv[1][0] == '-') {
        fprintf(stderr, "%s set: unknown option '-%c'\n", ZE_PROGRAM, argv[1][1]);
        ze_usage();
        return LIBZE_ERROR_UNKNOWN;
    }

    properties = fnvlist_alloc();
    if (properties == NULL) {
        return LIBZE_ERROR_NOMEM;
    }

    for (int i = 1; i < argc; i++) {
        if ((ret = libze_add_set_property(properties, argv[i])) != LIBZE_ERROR_SUCCESS) {
            goto err;
        }
    }

    ret = libze_set(lzeh, properties);

err:
    fnvlist_free(properties);
    return ret;
}