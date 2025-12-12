#ifndef JSHELL_CMD_SPEC_H
#define JSHELL_CMD_SPEC_H

#include <stdio.h>

typedef struct jshell_cmd_spec {
    const char *name;
    const char *summary;
    const char *long_help;
    int (*run)(int argc, char **argv);
    void (*print_usage)(FILE *out);
} jshell_cmd_spec_t;

void jshell_register_command(const cmd_spec_t *spec);
const jshell_cmd_spec_t *find_command(const char *name);
void jshell_for_each_command(void (*callback)(const cmd_spec_t *spec, void *userdata),
                      void *userdata);

#endif

