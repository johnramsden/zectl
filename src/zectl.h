//
// Created by john on 12/25/18.
//

#ifndef ZECTL_ZECTL_H
#define ZECTL_ZECTL_H

#include "zcp.h"

typedef enum ze_error {
    ZE_ERROR_SUCCESS = 0,     /* Success */
    ZE_ERROR_LIBZFS,          /* libzfs error */
    ZE_ERROR_UNKNOWN,         /* Unknown error */
} ze_error_t;

#endif //ZECTL_ZECTL_H
