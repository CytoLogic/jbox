#ifndef JSHELL_CMD_REGISTRY_H
#define JSHELL_CMD_REGISTRY_H

#include <stdio.h>

typedef enum {
  CMD_BUILTIN,
  CMD_EXTERNAL
} jshell_cmd_type_t;

typedef struct jshell_cmd_spec {
    const char *name;
    const char *summary;
    const char *long_help;
    jshell_cmd_type_t type;
    int (*run)(int argc, char **argv);
    void (*print_usage)(FILE *out);
} jshell_cmd_spec_t;

void jshell_register_command(const jshell_cmd_spec_t *spec);
const jshell_cmd_spec_t *jshell_find_command(const char *name);
void jshell_for_each_command(void (*callback)(const jshell_cmd_spec_t *spec, void *userdata),
                              void *userdata);

#endif
