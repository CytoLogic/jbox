#include "jbox.h"


int touch_main(int argc, char *argv[]) {

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

    char *f_path;
    while (optind < argc) {
        f_path=argv[optind];

        if (utime(f_path, NULL) != 0) {
            if (errno == ENOENT) {
                fclose(fopen(f_path, "w"));
            } else {
                perror(NULL);
            }
        }

        optind++;
    }

    exit(EXIT_SUCCESS);
}
