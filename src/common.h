//
// Created by john on 12/26/18.
//

#ifndef ZECTL_COMMON_H
#define ZECTL_COMMON_H

typedef enum ze_error {
    ZE_ERROR_SUCCESS = 0,     /* Success */
    ZE_ERROR_LIBZFS,          /* libzfs error */
    ZE_ERROR_UNKNOWN,         /* Unknown error */
} ze_error_t;

#endif //ZECTL_COMMON_H
