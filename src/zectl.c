/*
 * Copyright (c) 2018, John Ramsden.
 * MIT License, see:
 * https://github.com/johnramsden/zectl/blob/master/LICENSE.md
 */

/*
 * Code for commandline application 'zectl'
 */

#include <stdio.h>
#include "zectl.h"


/* Function pointer to command */
typedef ze_error (*command_func)(int argc, char **argv);

/* Command name -> function map */
typedef struct {
    char *name;
    command_func command;
} command_map_t;

/* Print zectl command usage */
static void zectl_usage(void){
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

ze_error list() {
    const char *f = "list.lua";

    run_channel_program(f, "zroot");

    return ZE_SUCCESS;
}

int main() {
    list();

    return 0;
}
