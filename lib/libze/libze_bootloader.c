#include "libze/libze.h"
#include "libze/libze_util.h"

/**
 * @brief Initialize bootloader and gather it's relevant properties
 * @param lzeh Initialized @p libze_handle
 * @param bootloader Initialize @p libze_bootloader
 * @param ze_namespace ZFS property namespace
 * @return @p LIBZE_ERROR_SUCCESS on success, @p LIBZE_ERROR_UNKNOWN on failure
 *
 * @pre lzeh != NULL
 * @pre bootloader != NULL
 * @pre (ze_namespace != NULL) && (strlen(ze_namespace) >= 1)
 */
libze_error
libze_bootloader_init(libze_handle *lzeh, libze_bootloader *bootloader,
                      char const ze_namespace[]) {
    nvlist_t *out_props = NULL;
    if (libze_be_props_get(lzeh, &out_props, ze_namespace) != LIBZE_ERROR_SUCCESS) {
        return LIBZE_ERROR_UNKNOWN;
    }

    char prop[ZFS_MAXPROPLEN] = "";
    if (libze_util_concat(ze_namespace, ":", "bootloader", ZFS_MAXPROPLEN, prop) != 0) {
        return LIBZE_ERROR_UNKNOWN;
    }

    if (nvlist_lookup_nvlist(out_props, prop, &bootloader->prop) != 0) {
        bootloader->set = B_TRUE;
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief Close bootloader
 * @param[in] bootloader Initialized bootloader
 * @return LIBZE_ERROR_SUCCESS on success
 */
libze_error
libze_bootloader_fini(libze_bootloader *bootloader) {
    if ((bootloader != NULL) && (bootloader->prop != NULL)) {
        fnvlist_free(bootloader->prop);
    }
    // TODO: ???
    return LIBZE_ERROR_SUCCESS;
}