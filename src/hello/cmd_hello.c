#include <stdio.h>

#include "argtable3.h"
#include "cmd_spec.h"

static struct arg_lit *hello_help_flag;
static struct arg_str *hello_name_arg;
static struct arg_end *hello_end;
static void *hello_argtable[4];

static void build_hello_argtable(void) {
    hello_help_flag =
        arg_lit0("h", "help", "show help and exit");
    hello_name_arg =
        arg_str0("n",
                 "name",
                 "NAME",
                 "name to greet");
    hello_end = arg_end(10);

    hello_argtable[0] = hello_help_flag;
    hello_argtable[1] = hello_name_arg;
    hello_argtable[2] = hello_end;
    hello_argtable[3] = NULL;
}

int hello_run(int argc, char **argv) {
    int parse_errors;
    const char *name_value;

    build_hello_argtable();

    parse_errors = arg_parse(argc, argv, hello_argtable);

    if (hello_help_flag->count > 0) {
        hello_print_usage(stdout);
        arg_freetable(hello_argtable, 3);
        return 0;
    }

    if (parse_errors > 0) {
        arg_print_errors(stdout, hello_end, "hello");
        hello_print_usage(stdout);
        arg_freetable(hello_argtable, 3);
        return 1;
    }

    if (hello_name_arg->count > 0) {
        name_value = hello_name_arg->sval[0];
        if (name_value != NULL) {
            printf("Hello, %s!\n", name_value);
        } else {
            printf("Hello!\n");
        }
    } else {
        printf("Hello!\n");
    }

    arg_freetable(hello_argtable, 3);
    return 0;
}

void hello_print_usage(FILE *output) {
    build_hello_argtable();

    fprintf(output, "Usage: hello ");
    arg_print_syntax(output, hello_argtable, "\n");
    fprintf(output, "\nOptions:\n");
    arg_print_glossary(output, hello_argtable, "  %-20s %s\n");

    arg_freetable(hello_argtable, 3);
}

cmd_spec_t cmd_hello_spec = {
    .name = "hello",
    .summary = "print a friendly greeting",
    .long_help = "Print a greeting, optionally addressing a specific NAME.",
    .run = hello_run,
    .print_usage = hello_print_usage,
};

void register_hello_command(void) {
    register_command(&cmd_hello_spec);
}

