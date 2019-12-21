#include "zectl.h"
#include "zectl_util.h"

#define HEADER_PROPERTY "PROPERTY"
#define HEADER_VALUE "VALUE"

typedef struct get_value_widths {
    size_t property;
    size_t value;
} get_value_widths;

typedef struct get_options {
    boolean_t tab_delimited;
} get_options;

/**
 * @brief TODO comment
 */
static libze_error print_properties(libze_handle *lzeh, nvlist_t *properties, get_options *options) {
    nvpair_t *       pair       = NULL;
    nvlist_t *       prop       = NULL;
    char *           tab_suffix = "\t";
    get_value_widths widths     = {0};

    if (!options->tab_delimited) {
        widths.property = strlen(HEADER_PROPERTY);
        widths.value    = strlen(HEADER_VALUE);
        for (pair = nvlist_next_nvpair(properties, NULL); pair != NULL; pair = nvlist_next_nvpair(properties, pair)) {
            nvpair_value_nvlist(pair, &prop);

            if ((set_column_width_lookup(prop, &widths.value, "value") != 0) ||
                (set_column_width(&widths.property, nvpair_name(pair)))) {
                return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed getting property widths.\n");
            }
        }

        tab_suffix = "";
        widths.property += HEADER_SPACING;
        widths.value += HEADER_SPACING;
        printf("%-*s", (int)widths.property, HEADER_PROPERTY);
        printf("%-*s", (int)widths.value, HEADER_VALUE);
        fputs("\n", stdout);
    }

    for (pair = nvlist_next_nvpair(properties, NULL); pair != NULL; pair = nvlist_next_nvpair(properties, pair)) {
        nvpair_value_nvlist(pair, &prop);
        char *string_prop;
        printf("%-*s%s", (int)widths.property, nvpair_name(pair), tab_suffix);
        if (nvlist_lookup_string(prop, "value", &string_prop) == 0) {
            printf("%-*s%s", (int)widths.value, string_prop, tab_suffix);
        }
        fputs("\n", stdout);
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * Get command main function
 * @param lzeh Initialized handle to libze object
 * @param argc As passed to main
 * @param argv As passed to main, contains TODO
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p TODO
 */
libze_error ze_get(libze_handle *lzeh, int argc, char **argv) {

    libze_error ret        = LIBZE_ERROR_SUCCESS;
    nvlist_t *  properties = NULL;
    get_options options    = {.tab_delimited = B_FALSE};

    opterr = 0;
    int opt;
    while ((opt = getopt(argc, argv, "H")) != -1) {
        switch (opt) {
            case 'H':
                options.tab_delimited = B_TRUE;
                break;
            default:
                fprintf(stderr, "%s get: unknown option '-%c'\n", ZE_PROGRAM, optopt);
                ze_usage();
                return LIBZE_ERROR_UNKNOWN;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc > 2) {
        fprintf(stderr, "%s: Wrong number of arguments.\n", ZE_PROGRAM);
        return LIBZE_ERROR_UNKNOWN;
    }

    char *prop = argv[0];

    boolean_t dealloc_properties = B_FALSE;
    if ((argc == 0) || (strcmp(prop, "all") == 0)) {
        properties = lzeh->ze_props;
    }
    else {
        properties = fnvlist_alloc();
        if (properties == NULL) {
            return LIBZE_ERROR_NOMEM;
        }
        dealloc_properties = B_TRUE;
        if ((ret = libze_add_get_property(lzeh, &properties, prop)) != LIBZE_ERROR_SUCCESS) {
            goto err;
        }
    }

    ret = print_properties(lzeh, properties, &options);
err:
    if (dealloc_properties) {
        fnvlist_free(properties);
    }
    return ret;
}