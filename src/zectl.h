#ifndef ZECTL_ZECTL_H
#define ZECTL_ZECTL_H

#include "libze/libze.h"
#include "libze/libze_plugin_manager.h"

#define HEADER_SPACING 2

extern char const * const ZECTL_VERSION;

extern char const * const ZE_PROGRAM;

void
ze_usage(void);

libze_error
ze_activate(libze_handle *lzeh, int argc, char **argv);

libze_error
ze_create(libze_handle *lzeh, int argc, char **argv);

libze_error
ze_destroy(libze_handle *lzeh, int argc, char **argv);

libze_error
ze_get(libze_handle *lzeh, int argc, char **argv);

libze_error
ze_list(libze_handle *lzeh, int argc, char **argv);

libze_error
ze_mount(libze_handle *lzeh, int argc, char **argv);

libze_error
ze_rename(libze_handle *lzeh, int argc, char **argv);

libze_error
ze_set(libze_handle *lzeh, int argc, char **argv);

libze_error
ze_snapshot(libze_handle *lzeh, int argc, char **argv);

libze_error
ze_unmount(libze_handle *lzeh, int argc, char **argv);

#endif // ZECTL_ZECTL_H
