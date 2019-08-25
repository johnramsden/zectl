#include "zectl_util.h"

int
set_column_width(size_t *width_column, const char *string_prop) {
    size_t item_width;
    item_width = strlen(string_prop);
    if (item_width > *width_column) {
        *width_column = item_width;
    }
    return 0;
}

int
set_column_width_lookup(nvlist_t *be_props, size_t *width_column, char *property) {
    char *string_prop;
    if (nvlist_lookup_string(be_props, property, &string_prop) != 0) {
        return -1;
    }

    return set_column_width(width_column, string_prop);
}