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
int libze_plugin_systemdboot_mid_activate(libze_handle *lzeh) {
    puts("sd_mid_activate");

    return 0;
}
int libze_plugin_systemdboot_post_activate(libze_handle *lzeh) {
    puts("sd_post_activate");

    return 0;
}