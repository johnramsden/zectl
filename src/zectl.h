#ifndef ZECTL_ZECTL_H
#define ZECTL_ZECTL_H

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "libze/libze.h"
#include "libze/libze_util.h"
#include "libze/libze_plugin_manager.h"

extern const char *ZE_PROGRAM;

void
ze_usage(void);

libze_error
ze_activate(libze_handle *lzeh, int argc, char **argv);

libze_error
ze_create(libze_handle *lzeh, int argc, char **argv);

libze_error
ze_destroy(libze_handle *lzeh, int argc, char **argv);

libze_error
ze_list(libze_handle *lzeh, int argc, char **argv);

libze_error
ze_set(libze_handle *lzeh, int argc, char **argv);

#endif //ZECTL_ZECTL_H
