#include "jbox.h"


int rm_main(int argc, char *argv[]) {

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

    char *rm_path;
    while (optind < argc) {
        rm_path=argv[optind];

        remove(rm_path);

        optind++;
    }

    exit(EXIT_SUCCESS);
}
