#ifndef ZECTL_ZECTL_UTIL_H
#define ZECTL_ZECTL_UTIL_H

#include "zectl.h"

#include <stddef.h>
#include <sys/nvpair.h>

int
set_column_width_lookup(nvlist_t *be_props, size_t *width_column, char *property);

int
set_column_width(size_t *width_column, const char *string_prop);

#endif // ZECTL_ZECTL_UTIL_H
