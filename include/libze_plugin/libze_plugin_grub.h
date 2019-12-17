//
// Created by markus on 12/16/19.
//

#ifndef ZECTL_LIBZE_PLUGIN_GRUB_H
#define ZECTL_LIBZE_PLUGIN_GRUB_H

#include "libze/libze_plugin_manager.h"

const char *PLUGIN_GRUB = "grub";

libze_error
libze_plugin_grub_init(libze_handle *lzeh);
libze_error
libze_plugin_grub_pre_activate(libze_handle *lzeh);
libze_error
libze_plugin_grub_mid_activate(libze_handle *lzeh, libze_activate_data *activate_data);
libze_error
libze_plugin_grub_post_activate(libze_handle *lzeh, const char be_name[LIBZE_MAX_PATH_LEN]);
libze_error
libze_plugin_grub_post_destroy(libze_handle *lzeh, const char be_name[LIBZE_MAX_PATH_LEN]);

libze_plugin_fn_export exported_plugin = {
        .plugin_init = libze_plugin_grub_init,
        .plugin_pre_activate = libze_plugin_grub_pre_activate,
        .plugin_mid_activate = libze_plugin_grub_mid_activate,
        .plugin_post_activate = libze_plugin_grub_post_activate,
        .plugin_post_destroy = libze_plugin_grub_post_destroy
};

#endif //ZECTL_LIBZE_PLUGIN_GRUB_H
