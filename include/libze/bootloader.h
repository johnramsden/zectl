#ifndef ZECTL_BOOTLOADER_H
#define ZECTL_BOOTLOADER_H

#include "libze.h"

libze_error_t
libze_bootloader_systemd_pre(libze_handle_t *lzeh);

/* Function pointer to command */
typedef libze_error_t (*bootloader_func)(libze_handle_t *lzeh);

/* Command name -> function map */
typedef struct {
    char *name;
    bootloader_func command;
} bootloader_map_t;

#endif //ZECTL_BOOTLOADER_H
