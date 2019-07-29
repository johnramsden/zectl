#include "libze/libze.h"
#include "libze/libze_util.h"

libze_error
libze_bootloader_init(libze_handle *lzeh, libze_bootloader *bootloader, const char ze_namespace[static 1]) {
    nvlist_t *out_props = NULL;
    if (libze_get_be_props(lzeh, &out_props, ze_namespace) != LIBZE_ERROR_SUCCESS) {
        return -1;
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

libze_error
libze_bootloader_fini(libze_bootloader *bootloader) {
    if (bootloader->prop != NULL) {
        fnvlist_free(bootloader->prop);
    }
    // TODO: ???
    return LIBZE_ERROR_SUCCESS;
}