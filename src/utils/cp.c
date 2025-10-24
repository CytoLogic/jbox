#include "jbox.h"

int cp_main(int argc, char *argv[]) {

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

    // open files
    FILE *src_fptr, *dst_fptr;
    box_chk_null_ptr(src_fptr = fopen(src_path, "r"));
    box_chk_null_ptr(dst_fptr = fopen(dst_path, "w+"));

    // get fds
    int src_fd, dst_fd; 
    box_chk_neg1_ret(src_fd = fileno(src_fptr));
    box_chk_neg1_ret(dst_fd = fileno(dst_fptr));

    // get src file stats
    struct stat src_st;
    box_chk_nonzero_ret(fstat(src_fd, &src_st));

    // src_sz = dst_sz
    off_t src_sz = src_st.st_size;
    box_chk_nonzero_ret(ftruncate(dst_fd, src_sz));

    // map memory
    void *src_map, *dst_map;
    box_chk_map_fail(src_map = mmap(NULL, src_sz, PROT_READ, MAP_PRIVATE, src_fd, 0));
    box_chk_map_fail(dst_map = mmap(NULL, src_sz, PROT_WRITE, MAP_SHARED, dst_fd, 0));

    // perform copy
    memcpy(dst_map, src_map, src_sz);

    // sync & cleanup
    box_chk_nonzero_ret(msync(dst_map, src_sz, MS_SYNC));
    box_chk_nonzero_ret(munmap(src_map, src_sz));
    box_chk_nonzero_ret(munmap(dst_map, src_sz));

    box_chk_nonzero_ret(fclose(src_fptr));
    box_chk_nonzero_ret(fclose(dst_fptr));

    exit(EXIT_SUCCESS);
}
