#include "jbox.h"

int cat_main(int argc, char *argv[]) {

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

    // iter over args and print to stdout
    char *src_path;
    while (optind < argc) {
        src_path=argv[optind];

        // open file
        FILE *src_fptr;
        src_fptr = fopen(src_path, "r");
        // get fd
        int src_fd; 
        src_fd = fileno(src_fptr);
        // get src file stat
        struct stat src_st;
        fstat(src_fd, &src_st);
        // get src file size
        off_t src_sz = src_st.st_size;
        // map memory
        void *src_map;
        src_map = mmap(NULL, src_sz, PROT_READ, MAP_PRIVATE, src_fd, 0);
        // print to stdout
        for (int i=0; i<src_sz; i++) {
            printf("%c", ((char *)src_map)[i]);
        }
        // cleanup
        munmap(src_map, src_sz);
        fclose(src_fptr);

        optind++;
    }

    exit(EXIT_SUCCESS);
}
