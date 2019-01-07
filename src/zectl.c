/*
 * Copyright (c) 2018, John Ramsden.
 * MIT License, see:
 * https://github.com/johnramsden/zectl/blob/master/LICENSE.md
 */

/*
 * Code for commandline application 'zectl'
 */

#include <stdio.h>
#include <string.h>

#include "zectl.h"
#include "libze/libze.h"
#include "util/util.h"

/* Function pointer to command */
typedef ze_error_t (*command_func)(libze_handle_t *lzeh, int argc, char **argv);

/* Command name -> function map */
typedef struct {
    char *name;
    command_func command;
} command_map_t;

/* Print zectl command usage */
static void ze_usage(void){
    puts("\nUsage:");
    puts("zectl activate <boot environment>");
    puts("zectl create <boot environment>");
    puts("zectl destroy <boot environment>");
    puts("zectl get <property>");
    puts("zectl list");
    puts("zectl mount <boot environment>");
    puts("zectl rename <boot environment> <boot environment>");
    puts("zectl set <property=value> <boot environment>");
    puts("zectl snapshot <boot environment>@<snap>");
    puts("zectl unmount <boot environment>");
}

/* TODO Implement */
static ze_error_t
ze_list(libze_handle_t *lzeh, int argc, char **argv) {
    ze_error_t ret = ZE_ERROR_SUCCESS;

    boolean_t spaceused = B_FALSE;
    boolean_t origin = B_FALSE;

    DEBUG_PRINT("Running list");

    nvlist_t *nvl = fnvlist_alloc();
    fnvlist_add_string(nvl, "dataset", lzeh->rootfs);

    nvlist_t *props_requested = fnvlist_alloc();
    fnvlist_add_string(props_requested, "name", "name");

    // TODO
    if (spaceused) {
        fnvlist_add_string(props_requested, "used", "used");
        fnvlist_add_string(props_requested, "usedds", "usedds");
        fnvlist_add_string(props_requested, "usedbysnapshots", "usedbysnapshots");
        fnvlist_add_string(props_requested, "usedrefreserv", "usedrefreserv");
        fnvlist_add_string(props_requested, "refer", "refer");
    }

    if (origin) {
        fnvlist_add_string(props_requested, "origin", "origin");
    }

    fnvlist_add_string(props_requested, "creation", "creation");
    fnvlist_add_nvlist(nvl, "columns", props_requested);

    nvlist_t *outnvl;
    libze_channel_program(lzeh, zcp_list, nvl, &outnvl);

    fflush(stderr);
    fflush(stdout);
//
//    if (outnvl) {
//        dump_nvlist(outnvl, 0);
//    }

    return ret;
}

/*
 * Check the command matches with one of the available options.
 * Return a function pointer to the requested command or NULL if no match
 */
static command_func
get_command(command_map_t *ze_command_map,
            int num_command_options, char *input_name){
    command_func command = NULL;

    for (int i = 0; i < num_command_options; i++) {
        if(strcmp(input_name,ze_command_map[i].name) == 0) {
            command = ze_command_map[i].command;
        }
    }
    return command;
}

#define NUM_COMMANDS 1 // Will be 9

int main(int argc, char **argv) {

    int ze_argc = argc-1;
    char *ze_argv[ze_argc];

    int ret = EXIT_SUCCESS;

    libze_handle_t *lzeh = NULL;

    /* Set up all commands */
    command_map_t ze_command_map[NUM_COMMANDS] = {
            /* If commands are added or removed, must modify 'NUM_COMMANDS' */
//            {"activate", ze_run_activate},
//            {"create", ze_run_create},
//            {"destroy", ze_run_destroy},
//            {"get", ze_run_get},
            {"list", ze_list},
//            {"mount", ze_run_mount},
//            {"rename", ze_run_rename},
//            {"set", ze_run_set},
//            {"snapshot", ze_run_snapshot},
//            {"unmount", ze_run_unmount}
    };

    fputs("ZE: Boot Environment Manager for ZFS\n\n", stdout);

    if(!(lzeh = libze_init())) {
        fputs("zectl: System may not be configured correctly for boot environments\n", stderr);
        ret = EXIT_FAILURE;
        goto fin;
    }

    /* Check correct number of parameters were input */
    if(argc < 2){
        fprintf(stderr, "\nzectl: Invalid input, please enter a command.\n");
        ze_usage();
        ret = EXIT_FAILURE;
        goto fin;
    } else {
        /* Shift commandline arguments removing the program name. */
        for(int i = 0; i<ze_argc; i++) {
            ze_argv[i] = argv[i+1];
        }
    }

    // Get command requested
    command_func ze_command = get_command(ze_command_map,
                                           NUM_COMMANDS, ze_argv[0]);
    // Run command if valid
    if(!ze_command) {
        fprintf(stderr, "\nzectl: Invalid input, no match found.");
        ze_usage();
        ret = EXIT_FAILURE;
        goto fin;
    }

    if(ze_command(lzeh, ze_argc, ze_argv) != ZE_ERROR_SUCCESS){
        fprintf(stderr, "zectl: Failed to run 'zectl %s'.\n", ze_argv[0]);
        ret = EXIT_FAILURE;
        goto fin;
    }

fin:
    if (lzeh) {
        libze_fini(lzeh);
    }
    return ret;
}
