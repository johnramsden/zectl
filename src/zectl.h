//
// Created by john on 12/25/18.
//

#ifndef ZECTL_ZECTL_H
#define ZECTL_ZECTL_H

#include "zcp.h"
#include "libze/libze.h"

typedef enum ze_error {
    ZE_ERROR_SUCCESS = 0,     /* Success */
    ZE_ERROR_LIBZFS,          /* libzfs error */
    ZE_ERROR_OPTIONS,         /* Options error */
    ZE_ERROR_NOMEM,         /* Options error */
    ZE_ERROR_UNKNOWN,         /* Unknown error */
    ZE_ERROR_EPERM,         /* Permission error */
} ze_error_t;

void ze_usage(void);
ze_error_t
ze_list(libze_handle_t *lzeh, int argc, char **argv);

libze_error_t
ze_get_props(libze_handle_t *lzeh, nvlist_t *props, nvlist_t **out_props);

#endif //ZECTL_ZECTL_H
