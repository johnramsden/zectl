#include "libze/libze.h"
#include "libze/libze_util.h"
#include "libze_plugin/libze_plugin_grub.h"

char const *const ZE_PROGRAM = "zectl grub_generator";

/**
 * @brief Helper function to set default properties
 * @param[in] lzeh Initialized lzeh libze handle
 * @return 0 on success, nonzero on failure
 *
 */
static int
define_default_props(libze_handle *lzeh) {
    nvlist_t *default_props = NULL;
    if ((default_props = fnvlist_alloc()) == NULL) {
        return -1;
    }
    if ((libze_default_prop_add(&default_props, "bootloader", "", ZE_PROP_NAMESPACE) != 0) ||
        (libze_default_prop_add(&default_props, "bootpool_root", "", ZE_PROP_NAMESPACE) != 0) ||
        (libze_default_prop_add(&default_props, "bootpool_prefix", "", ZE_PROP_NAMESPACE) != 0)) {
        return -1;
    }

    if (libze_default_props_set(lzeh, default_props, ZE_PROP_NAMESPACE) != 0) {
        nvlist_free(default_props);
        return -1;
    }

    return 0;
}

static int max(int x, int y) {
    return x ^ ((x ^ y) & -(x < y));
}

int main(int argc, char** argv) {
    int ret = EXIT_SUCCESS;

    libze_handle *lzeh = NULL;
    char be_name[ZFS_MAX_DATASET_NAME_LEN];

    if ((lzeh = libze_init()) == NULL) {
        fprintf(stderr,
                "%s: System may not be configured correctly "
                "for boot environments\n",
                ZE_PROGRAM);
        return EXIT_FAILURE;
    }

    ret = libze_bootloader_set(lzeh);
    if ((ret != LIBZE_ERROR_SUCCESS) && (ret != LIBZE_ERROR_PLUGIN_EEXIST)) {
        fputs(lzeh->libze_error_message, stderr);
        return EXIT_FAILURE;
    }

    // Clear any error messages
    if (ret == LIBZE_ERROR_PLUGIN_EEXIST) {
        char plugin[ZFS_MAXPROPLEN] = "";
        ret = libze_be_prop_get(lzeh, plugin, "bootloader", ZE_PROP_NAMESPACE);
        if (ret != LIBZE_ERROR_SUCCESS) {
            fputs(lzeh->libze_error_message, stderr);
            return EXIT_FAILURE;
        }
        fprintf(stderr,
                "WARNING: No bootloader plugin found under bootloader=%s.\n"
                "Continuing with no bootloader plugin.\n",
                plugin);
        (void) libze_error_clear(lzeh);
    }

    /* Initialize the root structure of a separate bootpool if available */
    if (libze_boot_pool_set(lzeh) != LIBZE_ERROR_SUCCESS) {
        fputs(lzeh->libze_error_message, stderr);
        ret = EXIT_FAILURE;
        goto fin;
    }

    /* Validate the running and activated boot environment */
    if (libze_validate_system(lzeh) != LIBZE_ERROR_SUCCESS) {
        fputs(lzeh->libze_error_message, stderr);
        ret = EXIT_FAILURE;
        goto fin;
    }

    if (define_default_props(lzeh) != 0) {
        fprintf(stderr, "%s: Failed to set default properties\n", ZE_PROGRAM);
        ret = EXIT_FAILURE;
        goto fin;
    }

    /* List */
    nvlist_t *list;
    if (libze_list(lzeh, &list) != LIBZE_ERROR_SUCCESS) {
        fprintf(stderr, "%s: Failed to get a list of boot environments\n", ZE_PROGRAM);
        ret = EXIT_FAILURE;
        goto fin;
    }
    nvpair_t *pair;
    nvlist_t *be_props;
    char *string_prop;
    for (pair = nvlist_next_nvpair(list, NULL); pair != NULL;
         pair = nvlist_next_nvpair(list, pair)) {

        nvpair_value_nvlist(pair, &be_props);

        if (nvlist_lookup_string(be_props, "name", &string_prop) != 0) {
            fprintf(stderr, "%s: Failed to get the name\n", ZE_PROGRAM);
            ret = EXIT_FAILURE;
            break;
        }
        libze_boot_env_name(string_prop, ZFS_MAX_DATASET_NAME_LEN, be_name);

        boolean_t active;
        if (nvlist_lookup_boolean_value(be_props, "active", &active) != 0) {
            fprintf(stderr, "%s: Failed to check if active (%s)\n", ZE_PROGRAM, be_name);
            ret = EXIT_FAILURE;
            break;
        }

        if (active) {
            printf("%s: Skip active boot environment (%s)\n", ZE_PROGRAM, be_name);
        }
        else {
            char mnt_path[LIBZE_MAX_PATH_LEN];
            if (libze_mount(lzeh, be_name, NULL, mnt_path) != LIBZE_ERROR_SUCCESS) {
                fprintf(stderr, "%s: Failed to mount boot environment (%s)\n", ZE_PROGRAM, be_name);
                ret = EXIT_FAILURE;
                break;
            }
            printf("%s: Mounted %s to %s!\n", ZE_PROGRAM, be_name, mnt_path);

            if (libze_unmount(lzeh, be_name) != LIBZE_ERROR_SUCCESS) {
                fprintf(stderr, "%s: Failed to unmount boot environment (%s)!\n", ZE_PROGRAM, be_name);
                ret = EXIT_FAILURE;
                break;
            }
        }
    }
    libze_list_free(list);

fin:
    libze_fini(lzeh);

    return 0;
}