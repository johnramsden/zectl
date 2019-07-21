#ifndef ZECTL_LIBZE_PLUGIN_MANAGER_H
#define ZECTL_LIBZE_PLUGIN_MANAGER_H

#include "libze/libze.h"

typedef enum libze_plugin_manager_error {
    LIBZE_PLUGIN_MANAGER_ERROR_SUCCESS = 0,
    LIBZE_PLUGIN_MANAGER_ERROR_UNKNOWN,
    LIBZE_PLUGIN_MANAGER_ERROR_EEXIST,
} libze_plugin_manager_error;

typedef int (*plugin_fn_init)(libze_handle *lzeh);
typedef int (*plugin_fn_pre_activate)(libze_handle *lzeh);
typedef int (*plugin_fn_mid_activate)(libze_handle *lzeh, char be_mountpoint[static 2]);
typedef int (*plugin_fn_post_activate)(libze_handle *lzeh);
typedef struct libze_plugin_fn_export {
    plugin_fn_init plugin_init;
    plugin_fn_pre_activate plugin_pre_activate;
    plugin_fn_mid_activate plugin_mid_activate;
    plugin_fn_post_activate plugin_post_activate;
} libze_plugin_fn_export;

void *
libze_plugin_open();

int
libze_plugin_close(void *libhandle);

int
libze_plugin_export(void *libhandle, libze_plugin_fn_export **ze_export);

#endif //ZECTL_LIBZE_PLUGIN_MANAGER_H
