#ifndef ZECTL_LIBZE_PLUGIN_SYSTEMDBOOT_H
#define ZECTL_LIBZE_PLUGIN_SYSTEMDBOOT_H

#include "libze/libze_plugin_manager.h"

char const *PLUGIN_SYSTEMDBOOT = "systemdboot";

libze_error
libze_plugin_systemdboot_init(libze_handle *lzeh);

libze_error
libze_plugin_systemdboot_pre_activate(libze_handle *lzeh);

libze_error
libze_plugin_systemdboot_mid_activate(libze_handle *lzeh, libze_activate_data *activate_data);

libze_error
libze_plugin_systemdboot_post_activate(libze_handle *lzeh, char const be_name[LIBZE_MAX_PATH_LEN]);

libze_error
libze_plugin_systemdboot_post_destroy(libze_handle *lzeh, char const be_name[LIBZE_MAX_PATH_LEN]);

libze_error
libze_plugin_systemdboot_post_create(libze_handle *lzeh, libze_create_data *create_data);

libze_error
libze_plugin_systemdboot_post_rename(libze_handle *lzeh, char const be_name_old[LIBZE_MAX_PATH_LEN],
                                     char const be_name_new[LIBZE_MAX_PATH_LEN]);

libze_plugin_fn_export const exported_plugin = {
    .plugin_init = libze_plugin_systemdboot_init,
    .plugin_pre_activate = libze_plugin_systemdboot_pre_activate,
    .plugin_mid_activate = libze_plugin_systemdboot_mid_activate,
    .plugin_post_activate = libze_plugin_systemdboot_post_activate,
    .plugin_post_destroy = libze_plugin_systemdboot_post_destroy,
    .plugin_post_create = libze_plugin_systemdboot_post_create,
    .plugin_post_rename = libze_plugin_systemdboot_post_rename};

#endif // ZECTL_LIBZE_PLUGIN_SYSTEMDBOOT_H
