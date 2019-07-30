#include <stdio.h>
#include "libze_plugin/libze_plugin_systemdboot.h"

int libze_plugin_systemdboot_init(libze_handle *lzeh) {
    puts("sd_init");

    return 0;
}
int libze_plugin_systemdboot_pre_activate(libze_handle *lzeh) {
    puts("sd_pre_activate");

    return 0;
}
int libze_plugin_systemdboot_mid_activate(libze_handle *lzeh, char be_mountpoint[static 2]) {
    fputs("sd_mid_activate: ", stdout);
    puts(be_mountpoint);

    return 0;
}
int libze_plugin_systemdboot_post_activate(libze_handle *lzeh) {
    puts("sd_post_activate");

    return 0;
}
int libze_plugin_systemdboot_post_destroy(libze_handle *lzeh, char be_name[static 1]) {
    puts("sd_post_destroy");

    return 0;
}