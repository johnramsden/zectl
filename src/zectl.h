#ifndef ZECTL_ZECTL_H
#define ZECTL_ZECTL_H

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "libze/libze.h"
#include "ze_util/ze_util.h"

extern const char *ZE_PROGRAM;
extern const char *ZE_PROP_NAMESPACE;

typedef enum ze_error {
    ZE_ERROR_SUCCESS = 0,     /* Success */
    ZE_ERROR_LIBZFS,          /* libzfs error */
    ZE_ERROR_OPTIONS,         /* Options error */
    ZE_ERROR_NOMEM,         /* Options error */
    ZE_ERROR_UNKNOWN,         /* Unknown error */
    ZE_ERROR_EPERM,         /* Permission error */
    ZE_ERROR_EEXIST         /* Dataset/fs/snapshot exists */
} ze_error_t;

void
ze_usage(void);

ze_error_t
ze_list(libze_handle_t *lzeh, int argc, char **argv);

ze_error_t
ze_create(libze_handle_t *lzeh, int argc, char **argv);

//libze_error_t
//ze_get_props(libze_handle_t *lzeh, nvlist_t *props, nvlist_t **out_props);
//
//libze_error_t
//ze_get_setting(libze_handle_t *lzeh, char *setting, char *result);

#endif //ZECTL_ZECTL_H
