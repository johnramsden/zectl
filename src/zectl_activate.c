#include "zectl.h"

/**
 * @brief Activate command main function
 * @param lzeh Initialized handle to libze object
 * @param argc As passed to main
 * @param argv As passed to main, contains boot env to activate
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p TODO comment error
 */
libze_error ze_activate(libze_handle *lzeh, int argc, char **argv) {
    libze_error            ret = LIBZE_ERROR_SUCCESS;
    int                    opt;
    libze_activate_options options = {.be_name = NULL, .noconfirm = B_FALSE};

    opterr = 0;

    while ((opt = getopt(argc, argv, "y")) != -1) {
        switch (opt) {
            case 'y':
                options.noconfirm = B_TRUE;
                break;
            default:
                fprintf(stderr, "%s activate: unknown option '-%c'\n", ZE_PROGRAM, optopt);
                ze_usage();
                return LIBZE_ERROR_UNKNOWN;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 1) {
        fprintf(stderr, "%s activate: wrong number of arguments.\n", ZE_PROGRAM);
        ze_usage();
        return LIBZE_ERROR_UNKNOWN;
    }

    options.be_name = argv[0];

    char be_ds_buff[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_util_concat(lzeh->env_root, "/", options.be_name, ZFS_MAX_DATASET_NAME_LEN, be_ds_buff) != 0) {
        fprintf(stderr, "Requested boot environment %s exceeds max length %d.\n", options.be_name,
                ZFS_MAX_DATASET_NAME_LEN);
        return LIBZE_ERROR_MAXPATHLEN;
    }

    if (libze_is_active_be(lzeh, be_ds_buff)) {
        fprintf(stderr, "Boot environment %s is already active.\n", options.be_name);
        return LIBZE_ERROR_UNKNOWN;
    }

err:
    return libze_activate(lzeh, &options);
}