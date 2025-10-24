#include "jbox.h"


int mv_main(int argc, char *argv[]) {

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

    const char *src_path;
    const char *dst_path;
    src_path = argv[optind];
    dst_path = argv[optind+1];

    rename(src_path, dst_path);
    
    //TODO: handle cross-fs copy & delete
    //TODO: handle moving a file to a dir

    exit(EXIT_SUCCESS);
}
