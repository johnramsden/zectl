#include "zectl.h"

typedef struct bootloader {
    nvlist_t *prop;
    boolean_t set;
} bootloader_t;

typedef struct create_options {
    boolean_t existing;
    boolean_t recursive;
} create_options_t;

typedef struct create_be_clone {
    char *be_clone_source;
    char be_clone_snap_suffix[ZFS_MAX_DATASET_NAME_LEN];
    char be_ds_created[ZFS_MAX_DATASET_NAME_LEN];
    create_options_t options;
} create_be_clone_t;

static void
gen_snap_suffix(size_t buflen, char buf[buflen]) {
    time_t now_time;
    time(&now_time);
    strftime(buf, buflen, "%F-%T", localtime(&now_time));
}

static ze_error_t
init_bootloader(libze_handle_t *lzeh, bootloader_t *bootloader) {
    nvlist_t *out_props = NULL;
    if (libze_get_be_props(lzeh, &out_props, ZE_PROP_NAMESPACE) != LIBZE_ERROR_SUCCESS) {
        return -1;
    }

    char prop[ZFS_MAXPROPLEN] = "";
    if (form_property_string(ZE_PROP_NAMESPACE, "bootloader", ZFS_MAXPROPLEN, prop) != 0) {
        return ZE_ERROR_UNKNOWN;
    }

    if (nvlist_lookup_nvlist(out_props, prop, &bootloader->prop) != 0) {
        bootloader->set = B_TRUE;
    }

    return ZE_ERROR_SUCCESS;
}

static ze_error_t
fini_bootloader(bootloader_t *bootloader) {
    if (bootloader->prop != NULL) {
        fnvlist_free(bootloader->prop);
    }
    // TODO: ???
    return ZE_ERROR_SUCCESS;
}

static ze_error_t
pre_create_be_clone(libze_handle_t *lzeh, create_be_clone_t *create_clone) {
    // Check length before calling 'create_be_clone'
    // +2 for '@' and '\0'
    if ((strlen(lzeh->bootfs)+strlen(create_clone->be_clone_snap_suffix)+2) > ZFS_MAX_DATASET_NAME_LEN) {
        fprintf(stderr, "%s%s%s%s", "Dataset name ", lzeh->bootfs, create_clone->be_clone_snap_suffix, " is too long");
        return ZE_ERROR_UNKNOWN;
    }

    if (zfs_dataset_exists(lzeh->lzh, create_clone->be_ds_created, ZFS_TYPE_FILESYSTEM)) {
        fprintf(stderr, "%s%s%s", "Dataset: ", create_clone->be_ds_created, " already exists.");
        return ZE_ERROR_EEXIST;
    }

    return ZE_ERROR_SUCCESS;
}

/**
 * @param lzeh Handle to initialized libze object
 * @param create_clone Clone object to create clone based on
 * @pre Length of dataset created already checked
 * @return
 */
static ze_error_t
create_be_clone(libze_handle_t *lzeh, create_be_clone_t *create_clone) {
    char *snap = NULL;
    char *source_ds = NULL;
    char buf[ZFS_MAX_DATASET_NAME_LEN] = "";
    ze_error_t ret = ZE_ERROR_SUCCESS;

    // Verify validity of options
    ze_error_t clone_ret = pre_create_be_clone(lzeh, create_clone);
    if (clone_ret != ZE_ERROR_SUCCESS) {
        return clone_ret;
    }

    if (!create_clone->options.existing) {
        snap = buf;
        source_ds = lzeh->bootfs;
        if (form_snapshot_string(source_ds, create_clone->be_clone_snap_suffix, ZFS_MAX_DATASET_NAME_LEN, snap) != 0) {
            return -1;
        }
        if (zfs_snapshot(lzeh->lzh, snap, create_clone->options.recursive, NULL) != 0) {
            return -1;
        }
    } else {
        // TODO: Existing
        snap = create_clone->be_clone_source;
        source_ds = create_clone->be_clone_source; // TODO: Fix if snap?
    }

    if (libze_clone(lzeh, source_ds, create_clone->be_clone_snap_suffix,
                    create_clone->be_ds_created) != LIBZE_ERROR_SUCCESS) {

    }


err:
    return ret;
}

/**
 * create command main function
 * @param lzeh Initialized handle to libze object
 * @param argc As passed to main
 * @param argv As passed to main, contains boot env to create
 * @return ZE_ERROR_SUCCESS upon success
 */
ze_error_t
ze_create(libze_handle_t *lzeh, int argc, char **argv) {

    bootloader_t bootloader;

    create_be_clone_t be_clone = {
            .be_clone_source = NULL,
            .be_clone_snap_suffix = "",
            .be_ds_created = "",
            .options = {B_FALSE}
    };

    ze_error_t ret = ZE_ERROR_SUCCESS;
    bootloader.set = B_FALSE;

    opterr = 0;
    int opt;
    while ((opt = getopt(argc, argv, "er")) != -1) {
        switch (opt) {
            case 'e':
                be_clone.be_clone_source = optarg;
                be_clone.options.existing = B_TRUE;
                break;
            case 'r':
                be_clone.options.recursive = B_TRUE;
                break;
            default:
                fprintf(stderr, "%s create: unknown option '-%c'\n", ZE_PROGRAM, optopt);
                ze_usage();
                return ZE_ERROR_UNKNOWN;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 1) {
        fprintf(stderr, "%s create: wrong number of arguments\n", ZE_PROGRAM);
        ze_usage();
        return ZE_ERROR_OPTIONS;
    }

    char *be_name = argv[0];

    if ((ret = init_bootloader(lzeh, &bootloader)) != ZE_ERROR_SUCCESS) {
        goto err;
    }

    // Setup clone object
    gen_snap_suffix(ZFS_MAX_DATASET_NAME_LEN, be_clone.be_clone_snap_suffix);
    if (form_dataset_string(lzeh->be_root, be_name,
                            ZFS_MAX_DATASET_NAME_LEN, be_clone.be_ds_created) != 0) {
        ret = ZE_ERROR_UNKNOWN;
        goto err;
    }

    ret = create_be_clone(lzeh, &be_clone);

err:
    fini_bootloader(&bootloader);
    return ret;
}