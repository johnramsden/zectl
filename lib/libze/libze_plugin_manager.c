#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include "libze/libze_plugin_manager.h"

#define PLUGIN_MAX_PATHLEN 512

void *
libze_plugin_open(char *ze_plugin) {
#ifndef PLUGINS_DIRECTORY
    return LIBZE_PLUGIN_MANAGER_ERROR_PDIR_EEXIST;
#endif

    char plugin_path[PLUGIN_MAX_PATHLEN] = PLUGINS_DIRECTORY;
    if ((strlcat(plugin_path, "/libze_plugin_", PLUGIN_MAX_PATHLEN) >= PLUGIN_MAX_PATHLEN) ||
        (strlcat(plugin_path, ze_plugin, PLUGIN_MAX_PATHLEN) >= PLUGIN_MAX_PATHLEN) ||
        (strlcat(plugin_path, ".so", PLUGIN_MAX_PATHLEN) >= PLUGIN_MAX_PATHLEN)) {
        return NULL;
    }

    void* libhandle = dlopen(plugin_path, RTLD_NOW);
    if (libhandle == NULL) {
        fprintf(stderr, "%s", dlerror());
        return NULL;
    }

    return libhandle;
}

int
libze_plugin_close(void *libhandle) {
    return dlclose(libhandle);
}

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