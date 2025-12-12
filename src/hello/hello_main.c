#include "cmd_spec.h"

extern cmd_spec_t cmd_hello_spec;

int main(int argc, char **argv) {
    return cmd_hello_spec.run(argc, argv);
}

