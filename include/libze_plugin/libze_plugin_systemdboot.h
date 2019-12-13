#ifndef ZECTL_LIBZE_PLUGIN_SYSTEMDBOOT_H
#define ZECTL_LIBZE_PLUGIN_SYSTEMDBOOT_H

#include "libze/libze_plugin_manager.h"

const char *PLUGIN_SYSTEMDBOOT = "systemdboot";

libze_error
libze_plugin_systemdboot_init(libze_handle *lzeh);

libze_error
libze_plugin_systemdboot_pre_activate(libze_handle *lzeh);

libze_error
libze_plugin_systemdboot_mid_activate(libze_handle *lzeh, char be_mountpoint[static 2],
                                      char be_name[ZFS_MAX_DATASET_NAME_LEN]);

libze_error
libze_plugin_systemdboot_post_activate(libze_handle *lzeh);

libze_error
libze_plugin_systemdboot_post_destroy(libze_handle *lzeh, char be_name[static 1]);

libze_plugin_fn_export exported_plugin = {
        .plugin_init = libze_plugin_systemdboot_init,
        .plugin_pre_activate = libze_plugin_systemdboot_pre_activate,
        .plugin_mid_activate = libze_plugin_systemdboot_mid_activate,
        .plugin_post_activate = libze_plugin_systemdboot_post_activate,
        .plugin_post_destroy = libze_plugin_systemdboot_post_destroy
};

#endif //ZECTL_LIBZE_PLUGIN_SYSTEMDBOOT_H
