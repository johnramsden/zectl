#include "zectl.h"

libze_error
ze_activate(libze_handle *lzeh, int argc, char **argv) {
    libze_error ret = LIBZE_ERROR_SUCCESS;
    int opt;
    libze_activate_options options = {
            .be_name = NULL,
            .noconfirm = B_FALSE
    };

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
        fprintf(stderr, "%s activate: wrong number of arguments\n", ZE_PROGRAM);
        ze_usage();
        return LIBZE_ERROR_UNKNOWN;
    }

    options.be_name = argv[0];

    libze_activate(lzeh, &options);

err:

    return ret;
}