#include "jbox.h"

int ls_main(int argc, char *argv[]) {

    // get args
    char opt;
    while ((opt = getopt(argc, argv, "t")) != -1) {
        switch (opt) {
            case 't':
                abort();
            case '?':
                abort();
            default:
                abort();
        }
    }

    // get directory stream
    if (argc == 1) {
    }

    DIR *ds = opendir("./");
    //ftw and nftw


    // output entry names
    struct dirent *dr;
    while ((dr = readdir(ds))) {
        printf("%s  ", dr->d_name);
    }
    printf("\n");

    //todo implement -t

    // cleanup
    closedir(ds);

    exit(EXIT_SUCCESS);
}
