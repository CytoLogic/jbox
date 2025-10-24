#include "jbox.h"


int pwd_main(int argc, char *argv[]) {

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

    char *cwd = getcwd(NULL, 0);
    printf("%s\n", cwd);
    free(cwd);

    exit(EXIT_SUCCESS);
}
