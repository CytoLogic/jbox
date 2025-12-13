#include "jbox.h"


int mkdir_main(int argc, char *argv[]) {

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
    // TODO: implement -p

    mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;
    char *mk_path;
    while (optind < argc) {
        mk_path=argv[optind];

        mkdir(mk_path, mode);

        optind++;
    }

    exit(EXIT_SUCCESS);
}
