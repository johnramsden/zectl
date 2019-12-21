#include "libze_plugin/libze_plugin_grub.h"
#include <stdio.h>

/**
 * @brief TODO comment
 */
libze_error libze_plugin_grub_init(libze_handle *lzeh) {
    return LIBZE_ERROR_SUCCESS; // Pass
}

/**
 * @brief TODO comment
 */
libze_error libze_plugin_grub_pre_activate(libze_handle *lzeh) {
    return LIBZE_ERROR_SUCCESS; // Pass
}

/**
 * @brief TODO comment
 */
libze_error libze_plugin_grub_mid_activate(libze_handle *lzeh, libze_activate_data *activate_data) {
    // TODO Replace pattern?
    // puts(be_mountpoint);

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief TODO comment
 */
libze_error libze_plugin_grub_post_activate(libze_handle *lzeh, const char be_name[LIBZE_MAX_PATH_LEN]) {
    // puts("Creating Temporary working directory.");
    // puts("No changes will be made until the end of the GRUB configuration.");

    // ** Setup boot env tree **
    // const char* tmpdir_env = "/tmp/ze.env/";
    // char *tmp_dirname = "/";

    // Create tmp mountpoint
    // tmp_dirname = mkdtemp(tmpdir_env);
    // if (tmp_dirname == NULL) {
    //     return libze_error_set(lzeh, LIBZE_PLUGIN_MANAGER_ERROR_UNKNOWN,
    //             "Could not create tmp directory %s.\n", tmpdir_env);
    // }

    // zfs_dataset_exists
    // libze_is_root_be

    // Call grub_mkconfig if not skipped

    // ** Tear down boot env tree **
    // err:

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief TODO comment
 */
libze_error libze_plugin_grub_post_destroy(libze_handle *lzeh, const char be_name[LIBZE_MAX_PATH_LEN]) {
    return LIBZE_ERROR_SUCCESS;
}