#include "libze_plugin/libze_plugin_grub.h"

#include "libze/libze.h"
#include "libze/libze_util.h"

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REGEX_BUFLEN 512
#define GRUB_ENTRY_PREFIX "org.zectl"

#define NUM_GRUB_PROPERTY_VALUES 2
#define NUM_GRUB_PROPERTIES 2

/**
 * @brief list of grub plugin properties
 */
char const *systemdboot_properties[NUM_GRUB_PROPERTIES][NUM_GRUB_PROPERTY_VALUES] = {
    {"efi", "/efi"}, {"boot", "/boot"}};

/********************************************************************
 ************************** Miscellaneous ***************************
 ********************************************************************/

/**
 * @brief Check if a file is read-write accessible
 *
 * @param[in,out] lzeh  libze handle
 * @param[in] filename  File checked for r/w
 *
 * @return @p LIBZE_ERROR_SUCCESS if file accessible,
 *         @p LIBZE_ERROR_EPERM if file is not in read/write mode,
 *         @p LIBZE_ERROR_MAXPATHLEN if file exceeds max path length,
 *         @p LIBZE_ERROR_UNKNOWN for other access errors
 */
static libze_error
file_accessible(libze_handle *lzeh, char const filename[LIBZE_MAX_PATH_LEN]) {
    /* Check if filename is r/w */
    errno = 0;
    if (access(filename, R_OK | W_OK) != 0) {
        switch (errno) {
            case EACCES:
                return libze_error_set(lzeh, LIBZE_ERROR_EPERM,
                                       "File is not in read/write mode (%s).\n", filename);
            case ENAMETOOLONG:
                return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                                       "File exceeds max path length (%s).\n", filename);
            default:
                return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                                       "File could not be accessed (%s).\n", filename);
        }
    }

    return LIBZE_ERROR_SUCCESS;
}

/********************************************************************
 ********************** Plugin initialization ***********************
 ********************************************************************/

/**
 * @brief Initialize the grub plugin TODO
 *
 * @param[in,out] lzeh  libze handle
 *
 * @pre lzeh->ze_props is allocated
 *
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_UNKNOWN TODO
 */
libze_error
libze_plugin_grub_init(libze_handle *lzeh) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    return ret;
}

/********************************************************************
 ************************** Pre-activate ****************************
 ********************************************************************/

/**
 * @brief Pre-activate hook for plugin
 * @param[in] lzeh
 * @return @p LIBZE_ERROR_SUCCESS
 */
libze_error
libze_plugin_grub_pre_activate(libze_handle *lzeh) {
    return LIBZE_ERROR_SUCCESS;
}

/********************************************************************
 ************************** Mid-activate ****************************
 ********************************************************************/

/**
 * @brief Run mid-activate hook
 *
 * @param lzeh Initialized libze handle
 * @param activate_data New be to activate
 *
 * @return Non-zero on failure
 */
libze_error
libze_plugin_grub_mid_activate(libze_handle *lzeh, libze_activate_data *activate_data) {
    return LIBZE_ERROR_SUCCESS;
}

/********************************************************************
 ************************** Post-activate ****************************
 ********************************************************************/

/**
 * @brief Post-activate hook TODO
 *
 * @param[in,out] lzeh  Initialized libze handle
 * @param[in] be_name   Boot environment to activate
 *
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_UNKNOWN TODO
 */
libze_error
libze_plugin_grub_post_activate(libze_handle *lzeh, char const be_name[LIBZE_MAX_PATH_LEN]) {

    libze_error ret = LIBZE_ERROR_SUCCESS;

    char active_be[ZFS_MAX_DATASET_NAME_LEN];
    if (libze_boot_env_name(lzeh->env_activated_path, ZFS_MAX_DATASET_NAME_LEN, active_be) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Bootfs exceeds max path length.\n");
    }

    char efi_mountpoint[ZFS_MAXPROPLEN];
    char namespace_buf[ZFS_MAXPROPLEN];

    return ret;
}

/********************************************************************
 ************************** Post-create ****************************
 ********************************************************************/

/**
 * @brief Post-create hook TODO
 *        Edits loader entry
 *        Copies kernels from BE being cloned
 *
 * @param[in,out] lzeh  libze handle
 * @param[in] create_data  BE being created
 *
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_UNKNOWN TODO
 */
libze_error
libze_plugin_grub_post_create(libze_handle *lzeh, libze_create_data *create_data) {

    libze_error ret = LIBZE_ERROR_SUCCESS;
    int iret = 0;

    char active_be[ZFS_MAX_DATASET_NAME_LEN];
    if (libze_boot_env_name(lzeh->env_activated_path, ZFS_MAX_DATASET_NAME_LEN, active_be) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Bootfs exceeds max path length.\n");
    }

    char boot_mountpoint[ZFS_MAXPROPLEN];
    char efi_mountpoint[ZFS_MAXPROPLEN];

    char namespace_buf[ZFS_MAXPROPLEN];
    return ret;
}

/********************************************************************
 ************************** Post-destroy ****************************
 ********************************************************************/

/**
 * @brief Post-destroy hook
 *        Removes loader entry
 *        Deletes kernels from BE being destroyed
 *
 * @param[in,out] lzeh  libze handle
 * @param[in] be_name   BE being destroyed
 *
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN on buffer being exceeded,
 *         @p LIBZE_ERROR_UNKNOWN upon file deletion failure,
 *         @p LIBZE_ERROR_UNKNOWN if couldn't access a property,
 */
libze_error
libze_plugin_grub_post_destroy(libze_handle *lzeh, char const be_name[LIBZE_MAX_PATH_LEN]) {

    libze_error ret = LIBZE_ERROR_SUCCESS;

    char active_be[ZFS_MAX_DATASET_NAME_LEN];
    if (libze_boot_env_name(lzeh->env_activated_path, ZFS_MAX_DATASET_NAME_LEN, active_be) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Bootfs exceeds max path length.\n");
    }

    char efi_mountpoint[ZFS_MAXPROPLEN];
    char namespace_buf[ZFS_MAXPROPLEN];

    libze_plugin_manager_error per = libze_plugin_form_namespace(PLUGIN_GRUB, namespace_buf);
    if (per != LIBZE_PLUGIN_MANAGER_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Exceeded max property name length.\n");
    }

    ret = libze_be_prop_get(lzeh, efi_mountpoint, "efi", namespace_buf);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                               "Couldn't access grub:efi property.\n");
    }

    // ret = remove_kernels(lzeh, efi_mountpoint, be_name);

    return ret;
}