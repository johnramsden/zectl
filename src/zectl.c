/*
 * Copyright (c) 2018 - 2019, John Ramsden.
 * MIT License, see LICENSE.md
 */

/*
 * Code for commandline application 'zectl'
 */

#include "zectl.h"

#include "libze/libze.h"

#include <stdio.h>
#include <string.h>

char const *const ZE_PROGRAM = "zectl";
char const *const ZECTL_VERSION = "0.1.6";

/* Function pointer to command */
typedef libze_error (*command_func)(libze_handle *lzeh, int argc, char **argv);

/* Command name -> function map */
typedef struct {
    char *name;
    command_func command;
} command_map_t;

/* Print zectl command usage */
void
ze_usage(void) {
    puts("\nUsage:");
    printf("%s activate <boot environment>\n", ZE_PROGRAM);
    printf("%s create [ -e <existing-dataset> | <existing-dataset@snapshot> ] [ -r ] "
           "<boot-environment>\n",
           ZE_PROGRAM);
    printf("%s destroy [ -F ] <boot-environment>\n", ZE_PROGRAM);
    printf("%s get [ -H ] [ property ]\n", ZE_PROGRAM);
    printf("%s list\n", ZE_PROGRAM);
    printf("%s mount <boot environment>\n", ZE_PROGRAM);
    printf("%s rename <boot-environment> <boot-environment-new>\n", ZE_PROGRAM);
    printf("%s set <property>=<value>\n", ZE_PROGRAM);
    printf("%s snapshot <boot-environment>@<snapshot>\n", ZE_PROGRAM);
    printf("%s unmount <boot-environment>\n", ZE_PROGRAM);
    printf("%s version\n", ZE_PROGRAM);
}

/*
 * Check the command matches with one of the available options.
 * Return a function pointer to the requested command or NULL if no match
 */
static command_func
get_command(command_map_t *ze_command_map, int num_command_options, char input_name[static 1]) {
    command_func command = NULL;

    for (int i = 0; i < num_command_options; i++) {
        if (strcmp(input_name, ze_command_map[i].name) == 0) {
            command = ze_command_map[i].command;
        }
    }
    return command;
}

/**
 * @brief Helper function to set default properties
 * @param[in] lzeh Initialized lzeh libze handle
 * @return 0 on success, nonzero on failure
 *
 */
static int
define_default_props(libze_handle *lzeh) {
    nvlist_t *default_props = NULL;
    if ((default_props = fnvlist_alloc()) == NULL) {
        return -1;
    }
    if ((libze_default_prop_add(&default_props, "bootloader", "", ZE_PROP_NAMESPACE) != 0) ||
        (libze_default_prop_add(&default_props, "bootpoolroot", "", ZE_PROP_NAMESPACE) != 0) ||
        (libze_default_prop_add(&default_props, "bootpoolprefix", "", ZE_PROP_NAMESPACE) != 0)) {
        return -1;
    }

    if (libze_default_props_set(lzeh, default_props, ZE_PROP_NAMESPACE) != 0) {
        nvlist_free(default_props);
        return -1;
    }

    return 0;
}

#define NUM_COMMANDS 10

int
main(int argc, char *argv[]) {

    int ze_argc = argc - 1;
    char *ze_argv[ze_argc];

    int ret = EXIT_SUCCESS;

    libze_handle *lzeh = NULL;

    /* Set up all commands */
    command_map_t ze_command_map[NUM_COMMANDS] = {
        /* If commands are added or removed, must modify 'NUM_COMMANDS' */
        {"activate", ze_activate}, {"create", ze_create},  {"destroy", ze_destroy}, {"get", ze_get},
        {"list", ze_list},         {"mount", ze_mount},    {"rename", ze_rename},   {"set", ze_set},
        {"snapshot", ze_snapshot}, {"unmount", ze_unmount}};

    /* Check correct number of parameters were input */
    if (argc < 2) {
        fprintf(stderr, "\n%s: Invalid input, please enter a command.\n", ZE_PROGRAM);
        ze_usage();
        ret = EXIT_FAILURE;
        goto fin;
    } else {
        /* Shift commandline arguments removing the program name. */
        for (int i = 0; i < ze_argc; i++) {
            ze_argv[i] = argv[i + 1];
        }
    }

    // TODO: Add back after testing
    //    if (strcmp(ze_argv[0], "list") != 0) {
    //        if(geteuid() != 0) {
    //            fprintf(stderr, "Permission denied, try again as root.\n");
    //            return EXIT_FAILURE;
    //        }
    //    }

    if (strcmp(ze_argv[0], "version") == 0) {
        puts(ZECTL_VERSION);
        return EXIT_SUCCESS;
    }

    if ((lzeh = libze_init()) == NULL) {
        fprintf(stderr,
                "%s: System may not be configured correctly "
                "for boot environments\n",
                ZE_PROGRAM);
        return EXIT_FAILURE;
    }

    ret = libze_bootloader_set(lzeh);
    if ((ret != LIBZE_ERROR_SUCCESS) && (ret != LIBZE_ERROR_PLUGIN_EEXIST)) {
        fputs(lzeh->libze_error_message, stderr);
        return EXIT_FAILURE;
    }

    // Clear any error messages
    if (ret == LIBZE_ERROR_PLUGIN_EEXIST) {
        char plugin[ZFS_MAXPROPLEN] = "";
        ret = libze_be_prop_get(lzeh, plugin, "bootloader", ZE_PROP_NAMESPACE);
        if (ret != LIBZE_ERROR_SUCCESS) {
            fputs(lzeh->libze_error_message, stderr);
            return EXIT_FAILURE;
        }
        fprintf(stderr,
                "WARNING: No bootloader plugin found under bootloader=%s.\n"
                "Continuing with no bootloader plugin.\n",
                plugin);
        (void) libze_error_clear(lzeh);
    }

    /* Initialize the root structure of a separate bootpool if available */
    if (libze_boot_pool_set(lzeh) != LIBZE_ERROR_SUCCESS) {
        fputs(lzeh->libze_error_message, stderr);
        ret = EXIT_FAILURE;
        goto fin;
    }

    /* Validate the running and activated boot environment */
    if (libze_validate_system(lzeh) != LIBZE_ERROR_SUCCESS) {
        fputs(lzeh->libze_error_message, stderr);
        ret = EXIT_FAILURE;
        goto fin;
    }

    if (define_default_props(lzeh) != 0) {
        fprintf(stderr, "%s: Failed to set default properties\n", ZE_PROGRAM);
        ret = EXIT_FAILURE;
        goto fin;
    }

    // Get command requested
    command_func ze_command = get_command(ze_command_map, NUM_COMMANDS, ze_argv[0]);
    // Run command if valid
    if (!ze_command) {
        fprintf(stderr, "\n%s: Invalid input, no match found.\n", ZE_PROGRAM);
        ze_usage();
        ret = EXIT_FAILURE;
        goto fin;
    }

    libze_error ze_ret = ze_command(lzeh, ze_argc, ze_argv);
    if (ze_ret != LIBZE_ERROR_SUCCESS) {
        fprintf(stderr, "%s: Failed to run '%s %s'.\n", ZE_PROGRAM, ZE_PROGRAM, ze_argv[0]);
        fputs(lzeh->libze_error_message, stderr);
        ret = EXIT_FAILURE;
    }

fin:
    libze_fini(lzeh);
    return ret;
}
