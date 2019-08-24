#include "zectl.h"

/**
 * create command main function
 * @param lzeh Initialized handle to libze object
 * @param argc As passed to main
 * @param argv As passed to main, contains boot env to create
 * @return LIBZE_ERROR_SUCCESS upon success
 */
libze_error
ze_create(libze_handle *lzeh, int argc, char **argv) {

    libze_bootloader bootloader;

    libze_create_options be_clone = {
            .existing = B_FALSE,
            .recursive = B_FALSE
    };

    libze_error ret = LIBZE_ERROR_SUCCESS;
    bootloader.set = B_FALSE;

    char *be_existing = NULL;

    opterr = 0;
    int opt;
    while ((opt = getopt(argc, argv, "e:r")) != -1) {
        switch (opt) {
            case 'e':
                be_existing = optarg;
                be_clone.existing = B_TRUE;
                break;
            case 'r':
                be_clone.recursive = B_TRUE;
                break;
            default:
                fprintf(stderr, "%s create: unknown option '-%c'\n", ZE_PROGRAM, optopt);
                ze_usage();
                return LIBZE_ERROR_UNKNOWN;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 1) {
        fprintf(stderr, "%s create: wrong number of arguments\n", ZE_PROGRAM);
        ze_usage();
        return LIBZE_ERROR_UNKNOWN;
    }

    if (strlcpy(be_clone.be_name, argv[0], ZFS_MAX_DATASET_NAME_LEN) >= ZFS_MAX_DATASET_NAME_LEN) {
        fprintf(stderr, "Boot environment name exceeds max dataset length.\n");
        return LIBZE_ERROR_MAXPATHLEN;
    }

    if ((ret = libze_bootloader_init(lzeh, &bootloader, ZE_PROP_NAMESPACE)) != LIBZE_ERROR_SUCCESS) {
        goto err;
    }

    if (be_clone.existing) {
        if (strlcpy(be_clone.be_source, be_existing, ZFS_MAX_DATASET_NAME_LEN) >= ZFS_MAX_DATASET_NAME_LEN) {
            fprintf(stderr, "Existing boot environment source exceeds max dataset length.\n");
            return LIBZE_ERROR_MAXPATHLEN;
        }
    }

    ret = libze_create(lzeh, &be_clone);

err:
    libze_bootloader_fini(&bootloader);
    return ret;
}