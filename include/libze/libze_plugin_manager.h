#ifndef ZECTL_LIBZE_PLUGIN_MANAGER_H
#define ZECTL_LIBZE_PLUGIN_MANAGER_H

#include "libze/libze.h"

typedef enum libze_plugin_manager_error {
    LIBZE_PLUGIN_MANAGER_ERROR_SUCCESS = 0,
    LIBZE_PLUGIN_MANAGER_ERROR_UNKNOWN,
    LIBZE_PLUGIN_MANAGER_ERROR_EEXIST,
    LIBZE_PLUGIN_MANAGER_ERROR_MAXPATHLEN,
    /**< Plugin directory @p PLUGINS_DIRECTORY doesn't exist */
    LIBZE_PLUGIN_MANAGER_ERROR_PDIR_EEXIST,
} libze_plugin_manager_error;

typedef struct libze_activate_data {
    char const *const be_mountpoint;
    char const *const be_name;
} libze_activate_data;

typedef struct libze_create_data {
    char const *const be_mountpoint;
    char const *const be_name;
} libze_create_data;

typedef libze_error (*plugin_fn_init)(libze_handle *lzeh);

typedef libze_error (*plugin_fn_pre_activate)(libze_handle *lzeh);

typedef libze_error (*plugin_fn_mid_activate)(libze_handle *lzeh,
                                              libze_activate_data *activate_data);

typedef libze_error (*plugin_fn_post_activate)(libze_handle *lzeh,
                                               char const be_name[LIBZE_MAX_PATH_LEN]);

typedef libze_error (*plugin_fn_post_destroy)(libze_handle *lzeh,
                                              char const be_name[LIBZE_MAX_PATH_LEN]);

typedef libze_error (*plugin_fn_post_create)(libze_handle *lzeh, libze_create_data *create_data);

typedef libze_error (*plugin_fn_post_rename)(libze_handle *lzeh,
                                             char const be_name_old[LIBZE_MAX_PATH_LEN],
                                             char const be_name_new[LIBZE_MAX_PATH_LEN]);

typedef struct libze_plugin_fn_export {
    plugin_fn_init plugin_init;
    plugin_fn_pre_activate plugin_pre_activate;
    plugin_fn_mid_activate plugin_mid_activate;
    plugin_fn_post_activate plugin_post_activate;
    plugin_fn_post_destroy plugin_post_destroy;
    plugin_fn_post_create plugin_post_create;
    plugin_fn_post_rename plugin_post_rename;
} libze_plugin_fn_export;

libze_plugin_manager_error
libze_plugin_open(char const *ze_plugin, void **p_handle);

int
libze_plugin_close(void *libhandle);

int
libze_plugin_export(void *libhandle, libze_plugin_fn_export **ze_export);

libze_plugin_manager_error
libze_plugin_form_namespace(char const plugin_name[], char buf[ZFS_MAXPROPLEN]);

libze_plugin_manager_error
libze_plugin_form_property(char const plugin_name[], char const plugin_suffix[],
                           char buf[ZFS_MAXPROPLEN]);

#endif // ZECTL_LIBZE_PLUGIN_MANAGER_H
