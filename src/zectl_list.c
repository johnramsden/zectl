#include "libze/libze_util.h"
#include "zectl.h"
#include "zectl_util.h"

#include <stdio.h>
#include <string.h>
#include <sys/nvpair.h>
#include <unistd.h>

#define HEADER_NAME "Name"
#define HEADER_ACTIVE "Active"
#define HEADER_MOUNTPOINT "Mountpoint"
#define HEADER_SPACEUSED "Space"
#define HEADER_CREATION "Creation"

typedef struct list_value_widths {
    size_t name;
    size_t active;
    size_t mountpoint;
    size_t spaceused;
    size_t creation;
} list_value_widths_t;

typedef struct list_options {
    boolean_t spaceused;
    boolean_t snapshots;
    boolean_t all;
    boolean_t tab_delimited;
} list_options_t;

static int
compute_column_widths(nvlist_t *be_props, list_options_t *options, list_value_widths_t *widths) {
    int active_width = 0;

    if ((set_column_width_lookup(be_props, &widths->name, "name") != 0) ||
        (set_column_width_lookup(be_props, &widths->creation, "creation") != 0) ||
        (set_column_width_lookup(be_props, &widths->mountpoint, "mountpoint") != 0)) {
        return -1;
    }

    if (options->tab_delimited) {
        boolean_t nextboot;
        if (nvlist_lookup_boolean_value(be_props, "nextboot", &nextboot) != 0) {
            return -1;
        }
        if (nextboot) {
            active_width++;
        }
        boolean_t active;
        if (nvlist_lookup_boolean_value(be_props, "active", &active) != 0) {
            return -1;
        }
        if (active) {
            active_width++;
        }
        widths->active = active_width;
    }

    // TODO: ??

    return 0;
}

static void
print_bes(nvlist_t **bootenvs, list_options_t *options) {
    nvpair_t *pair;
    nvlist_t *be_props;
    const char *string_prop;
    char *tab_suffix = "\t";

    list_value_widths_t widths = {0};

    if (!options->tab_delimited) {
        tab_suffix = "";
        widths.name = strlen(HEADER_NAME);
        widths.active = strlen(HEADER_ACTIVE);
        widths.mountpoint = strlen(HEADER_MOUNTPOINT);
        widths.spaceused = strlen(HEADER_SPACEUSED);
        widths.creation = strlen(HEADER_CREATION);
        for (pair = nvlist_next_nvpair(*bootenvs, NULL); pair != NULL;
             pair = nvlist_next_nvpair(*bootenvs, pair)) {
            nvpair_value_nvlist(pair, &be_props);
            compute_column_widths(be_props, options, &widths);
        }
        widths.name += HEADER_SPACING;
        widths.active += HEADER_SPACING;
        widths.mountpoint += HEADER_SPACING;
        widths.spaceused += HEADER_SPACING;
        widths.creation += HEADER_SPACING;
        printf("%-*s", (int) widths.name, HEADER_NAME);
        printf("%-*s", (int) widths.active, HEADER_ACTIVE);
        printf("%-*s", (int) widths.mountpoint, HEADER_MOUNTPOINT);
        printf("%-*s", (int) widths.creation, HEADER_CREATION);
        fputs("\n", stdout);
    }

    for (pair = nvlist_next_nvpair(*bootenvs, NULL); pair != NULL;
         pair = nvlist_next_nvpair(*bootenvs, pair)) {
        nvpair_value_nvlist(pair, &be_props);

        if (nvlist_lookup_string(be_props, "name", &string_prop) == 0) {
            char buf[ZFS_MAX_DATASET_NAME_LEN];
            libze_boot_env_name(string_prop, ZFS_MAX_DATASET_NAME_LEN, buf);
            printf("%-*s%s", (int) widths.name, buf, tab_suffix);
        }

        char active_buff[3] = "";
        boolean_t active;
        if (nvlist_lookup_boolean_value(be_props, "active", &active) == 0) {
            if (active) {
                strcat(active_buff, "N");
            }
        }
        boolean_t nextboot;
        if (nvlist_lookup_boolean_value(be_props, "nextboot", &nextboot) == 0) {
            if (nextboot) {
                strcat(active_buff, "R");
            }
        }
        printf("%-*s%s", (int) widths.active, active_buff, tab_suffix);

        if (nvlist_lookup_string(be_props, "mountpoint", &string_prop) == 0) {
            printf("%-*s%s", (int) widths.creation, string_prop, tab_suffix);
        }

        if (nvlist_lookup_string(be_props, "creation", &string_prop) == 0) {
            printf("%-*s%s", (int) widths.creation, string_prop, tab_suffix);
        }

        fputs("\n", stdout);
    }
}

libze_error
ze_list(libze_handle *lzeh, int argc, char **argv) {
    libze_error ret = LIBZE_ERROR_SUCCESS;
    int opt;
    list_options_t options = {B_FALSE};
    nvlist_t *outnvl;

    opterr = 0;

    // TODO: aDs
    while ((opt = getopt(argc, argv, "H")) != -1) {
        switch (opt) {
                //            case 'a':
                //                options.all = B_TRUE;
                //                break;
            case 'D':
                options.spaceused = B_TRUE;
                break;
            case 'H':
                options.tab_delimited = B_TRUE;
                break;
                //            case 's':
                //                options.snapshots = B_TRUE;
                //                break;
            default:
                fprintf(stderr, "%s list: unknown option '-%c'\n", ZE_PROGRAM, optopt);
                ze_usage();
                return LIBZE_ERROR_UNKNOWN;
        }
    }

    if ((ret = libze_list(lzeh, &outnvl)) == LIBZE_ERROR_SUCCESS) {
        print_bes(&outnvl, &options);
    }

    libze_list_free(outnvl);
    return ret;
}
