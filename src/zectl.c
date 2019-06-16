/*
 * Copyright (c) 2018 - 2019, John Ramsden.
 * MIT License, see LICENSE.md
 */

/*
 * Code for commandline application 'zectl'
 */

#include <stdio.h>
#include <string.h>

#include "zectl.h"
#include "libze/libze.h"
#include "util/util.h"

const char *ZE_PROGRAM = "zectl";
const char *ZE_PROP_NAMESPACE = "org.zectl";

/* Function pointer to command */
typedef ze_error_t (*command_func)(libze_handle_t *lzeh, int argc, char **argv);

/* Command name -> function map */
typedef struct {
    char *name;
    command_func command;
} command_map_t;

/* Print zectl command usage */
void ze_usage(void) {
    puts("\nUsage:");
    printf("%s activate <boot environment>\n", ZE_PROGRAM);
    printf("%s create <boot environment>\n", ZE_PROGRAM);
    printf("%s destroy <boot environment>\n", ZE_PROGRAM);
    printf("%s get <property>\n", ZE_PROGRAM);
    printf("%s list\n", ZE_PROGRAM);
    printf("%s mount <boot environment>\n", ZE_PROGRAM);
    printf("%s rename <boot environment> <boot environment>\n", ZE_PROGRAM);
    printf("%s set <property=value> <boot environment>\n", ZE_PROGRAM);
    printf("%s snapshot <boot environment>@<snap>\n", ZE_PROGRAM);
    printf("%s unmount <boot environment>\n", ZE_PROGRAM);
}

//libze_error_t
//ze_get_props(libze_handle_t *lzeh, nvlist_t *props, nvlist_t **out_props) {
//    if (!props) {
//        return LIBZE_ERROR_UNKNOWN;
//    }
//    return libze_channel_program(lzeh, zcp_get_props, props, out_props);
//}

/*
 * Check the command matches with one of the available options.
 * Return a function pointer to the requested command or NULL if no match
 */
static command_func
get_command(command_map_t *ze_command_map,
            int num_command_options, char input_name[static 1]){
    command_func command = NULL;

    for (int i = 0; i < num_command_options; i++) {
        if(strcmp(input_name,ze_command_map[i].name) == 0) {
            command = ze_command_map[i].command;
        }
    }
    return command;
}

#define NUM_COMMANDS 2 // Will be 9

int main(int argc, char *argv[]) {

    int ze_argc = argc-1;
    char *ze_argv[ze_argc];

    int ret = EXIT_SUCCESS;

    libze_handle_t *lzeh = NULL;

    /* Set up all commands */
    command_map_t ze_command_map[NUM_COMMANDS] = {
            /* If commands are added or removed, must modify 'NUM_COMMANDS' */
//            {"activate", ze_run_activate},
            {"create", ze_create},
//            {"destroy", ze_run_destroy},
//            {"get", ze_run_get},
            {"list", ze_list},
//            {"mount", ze_run_mount},
//            {"rename", ze_run_rename},
//            {"set", ze_run_set},
//            {"snapshot", ze_run_snapshot},
//            {"unmount", ze_run_unmount}
    };

    /* Check correct number of parameters were input */
    if(argc < 2){
        fprintf(stderr, "\n%s: Invalid input, please enter a command.\n", ZE_PROGRAM);
        ze_usage();
        ret = EXIT_FAILURE;
        goto fin;
    } else {
        /* Shift commandline arguments removing the program name. */
        for(int i = 0; i<ze_argc; i++) {
            ze_argv[i] = argv[i+1];
        }
    }

//    if (strcmp(ze_argv[0], "list") != 0) {
//        if(geteuid() != 0) {
//            fprintf(stderr, "Permission denied, try again as root.\n");
//            return EXIT_FAILURE;
//        }
//    }

    if((lzeh = libze_init()) == NULL) {
        printf("%s: System may not be configured correctly "
               "for boot environments\n", ZE_PROGRAM);
        ret = EXIT_FAILURE;
        goto fin;
    }

    // Get command requested
    command_func ze_command = get_command(ze_command_map,
                                          NUM_COMMANDS, ze_argv[0]);
    // Run command if valid
    if(!ze_command) {
        fprintf(stderr, "\n%s: Invalid input, no match found.\n", ZE_PROGRAM);
        ze_usage();
        ret = EXIT_FAILURE;
        goto fin;
    }

    ze_error_t ze_ret = ze_command(lzeh, ze_argc, ze_argv);
    if(ze_ret != ZE_ERROR_SUCCESS){
        fprintf(stderr, "%s: Failed to run '%s %s'.\n", ZE_PROGRAM, ZE_PROGRAM, ze_argv[0]);
        ret = EXIT_FAILURE;
        goto fin;
    }

fin:
    libze_fini(lzeh);
    return ret;
}
