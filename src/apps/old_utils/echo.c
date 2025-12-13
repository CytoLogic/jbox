#include "jbox.h"

int echo_main(int argc, char *argv[]) {

    // get args
    char opt;
    while ((opt = getopt(argc, argv, "")) != -1) {
        switch (opt) {
            case '?':
                abort();
            default:
                abort();
        }
    }

    // echo args
    char *str;
    while (optind < argc) {
        str = argv[optind];
        fputs(str, stdout);
        fputc(' ', stdout);
        optind++;
    }
    fputc('\n', stdout);

    exit(EXIT_SUCCESS);
}
