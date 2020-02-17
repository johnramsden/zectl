#include "libze/libze_plugin_manager.h"

#include "libze/libze.h"
#include "libze/libze_util.h"

#include <dlfcn.h>
#include <string.h>

#define PLUGIN_MAX_PATHLEN 512

/**
 * @brief Open the handle to the specified plugin library.
 * @param[in] ze_plugin Name of the plugin
 * @return Handle to the library, or @p NULL if it doesn't exist
 */
libze_plugin_manager_error
libze_plugin_open(char const *ze_plugin, void **p_handle) {
#ifndef PLUGINS_DIRECTORY
    return LIBZE_PLUGIN_MANAGER_ERROR_PDIR_EEXIST;
#endif

    char plugin_path[PLUGIN_MAX_PATHLEN] = PLUGINS_DIRECTORY;
    if ((strlcat(plugin_path, "/libze_plugin_", PLUGIN_MAX_PATHLEN) >= PLUGIN_MAX_PATHLEN) ||
        (strlcat(plugin_path, ze_plugin, PLUGIN_MAX_PATHLEN) >= PLUGIN_MAX_PATHLEN) ||
        (strlcat(plugin_path, ".so", PLUGIN_MAX_PATHLEN) >= PLUGIN_MAX_PATHLEN)) {
        return LIBZE_PLUGIN_MANAGER_ERROR_MAXPATHLEN;
    }

    // NULL if plugin nonexistent
    *p_handle = dlopen(plugin_path, RTLD_NOW);

    if (*p_handle == NULL) {
        return LIBZE_PLUGIN_MANAGER_ERROR_EEXIST;
    }

    return LIBZE_PLUGIN_MANAGER_ERROR_SUCCESS;
}

/**
 * @brief Close handle to the plugin library
 * @param[in] libhandle Handle to the plugin library
 * @return Nonzero on error
 */
int
libze_plugin_close(void *libhandle) {
    return dlclose(libhandle);
}

/**
 * @brief Export a symbol, @p libze_plugin_fn_export , from the plugin
 * @param libhandle Handle to the plugin library
 * @param[out] ze_export Exported functions from the library, NULL on error
 * @return Non-zero on failure.
 *
 * @pre libhandle != NULL
 */
int
libze_plugin_export(void *libhandle, libze_plugin_fn_export **ze_export) {
    // Clear errors
    dlerror();
    *ze_export = (libze_plugin_fn_export *) dlsym(libhandle, "exported_plugin");
    if (dlerror() != NULL) {
        *ze_export = NULL;
        return -1;
    }

    return 0;
}

libze_plugin_manager_error
libze_plugin_form_namespace(char const plugin_name[], char buf[ZFS_MAXPROPLEN]) {
    if (libze_util_concat(ZE_PROP_NAMESPACE, ".", plugin_name, ZFS_MAXPROPLEN, buf) !=
        LIBZE_ERROR_SUCCESS) {
        return LIBZE_PLUGIN_MANAGER_ERROR_MAXPATHLEN;
    }
    return LIBZE_PLUGIN_MANAGER_ERROR_SUCCESS;
}

libze_plugin_manager_error
libze_plugin_form_property(char const plugin_prefix[], char const plugin_suffix[],
                           char buf[ZFS_MAXPROPLEN]) {
    if (libze_util_concat(plugin_prefix, ":", plugin_suffix, ZFS_MAXPROPLEN, buf) !=
        LIBZE_ERROR_SUCCESS) {
        return LIBZE_PLUGIN_MANAGER_ERROR_MAXPATHLEN;
    }
    return LIBZE_PLUGIN_MANAGER_ERROR_SUCCESS;
}