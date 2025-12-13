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
    jbox_chk_null_ptr_err(src_fptr = fopen(src_path, "r"));
    jbox_chk_null_ptr_err(dst_fptr = fopen(dst_path, "w+"));

    // get fds
    int src_fd, dst_fd; 
    jbox_chk_neg1_ret_err(src_fd = fileno(src_fptr));
    jbox_chk_neg1_ret_err(dst_fd = fileno(dst_fptr));

    // get src file stats
    struct stat src_st;
    jbox_chk_nonzero_ret_err(fstat(src_fd, &src_st));

    // src_sz = dst_sz
    off_t src_sz = src_st.st_size;
    jbox_chk_nonzero_ret_err(ftruncate(dst_fd, src_sz));

    // map memory
    void *src_map, *dst_map;
    jbox_chk_map_fail_err(src_map = mmap(NULL, src_sz, PROT_READ, MAP_PRIVATE, src_fd, 0));
    jbox_chk_map_fail_err(dst_map = mmap(NULL, src_sz, PROT_WRITE, MAP_SHARED, dst_fd, 0));

    // perform copy
    memcpy(dst_map, src_map, src_sz);

    // sync & cleanup
    jbox_chk_nonzero_ret_err(msync(dst_map, src_sz, MS_SYNC));
    jbox_chk_nonzero_ret_err(munmap(src_map, src_sz));
    jbox_chk_nonzero_ret_err(munmap(dst_map, src_sz));

    jbox_chk_nonzero_ret_err(fclose(src_fptr));
    jbox_chk_nonzero_ret_err(fclose(dst_fptr));

    exit(EXIT_SUCCESS);
}
