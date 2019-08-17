#ifndef ZECTL_LIBZE_PLUGIN_MANAGER_H
#define ZECTL_LIBZE_PLUGIN_MANAGER_H

#include "libze/libze.h"

typedef enum libze_plugin_manager_error {
    LIBZE_PLUGIN_MANAGER_ERROR_SUCCESS = 0,
    LIBZE_PLUGIN_MANAGER_ERROR_UNKNOWN,
    LIBZE_PLUGIN_MANAGER_ERROR_EEXIST,
    LIBZE_PLUGIN_MANAGER_ERROR_MAXPATHLEN,
    LIBZE_PLUGIN_MANAGER_ERROR_PDIR_EEXIST,  /**< Plugin directory @p PLUGINS_DIRECTORY doesn't exist */
} libze_plugin_manager_error;

typedef int (*plugin_fn_init)(libze_handle *lzeh);
typedef int (*plugin_fn_pre_activate)(libze_handle *lzeh);
typedef int (*plugin_fn_mid_activate)(libze_handle *lzeh, char be_mountpoint[static 2]);
typedef int (*plugin_fn_post_activate)(libze_handle *lzeh);
typedef int (*plugin_fn_post_destroy)(libze_handle *lzeh, char be_name[static 1]);
typedef struct libze_plugin_fn_export {
    plugin_fn_init plugin_init;
    plugin_fn_pre_activate plugin_pre_activate;
    plugin_fn_mid_activate plugin_mid_activate;
    plugin_fn_post_activate plugin_post_activate;
    plugin_fn_post_destroy plugin_post_destroy;
} libze_plugin_fn_export;

libze_plugin_manager_error
libze_plugin_open(char *ze_plugin, void **p_handle);

int
libze_plugin_close(void *libhandle);

int
libze_plugin_export(void *libhandle, libze_plugin_fn_export **ze_export);

#endif //ZECTL_LIBZE_PLUGIN_MANAGER_H
