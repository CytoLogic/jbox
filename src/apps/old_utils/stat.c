#include "jbox.h"


int stat_main(int argc, char *argv[]) {

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

    //struct stat *f_stats;
    char * f_name;
    while (optind < argc) {
        f_name = argv[optind];
        //stat(f_name, f_stats);
        
        printf("File: %s\n", f_name);
        printf("Size: \tBlocks: \tIO Block: \t");
        printf("Device: \tInode: \tLinks: \t");
        printf("Access: Uid: Gid:");
        printf("Access: \n");
        printf("Modify: \n");
        printf("Change: \n");
        printf("Birth: \n");

        optind++;
    }


    exit(EXIT_SUCCESS);
}
