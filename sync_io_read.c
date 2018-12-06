#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "io_utils.h"

void read_file_in_all(int *fds, int nums, int block);
void read_file_in_blocks(int *fds, int nums, int block);

void read_file_in_all(int *fds, int nums, int block) {
    char buf[block];

    for(int i=0; i<nums; ++i) {
        while(1) {
            if(read(fds[i], buf, block) != block) break;
        }
    }
}

void read_file_in_blocks(int *fds, int nums, int block) {
    char buf[block];

    long long start=0, end=0;
    long long s_start=0, s_end=0;
    int file_size = get_file_size(fds[0]);
    int ret;
    int offset=0;
    while(file_size>offset) {
        start = get_current_time_ms();
        for(int i=0; i<nums; ++i) {
            s_start = get_current_time_ms();
            ret = read(fds[i], buf, block);
            s_end = get_current_time_ms();
            printf("single read time %d us\n", (int)(s_end-s_start));
        }
        end = get_current_time_ms();
        printf("fd %d read time %d us\n", nums, (int)(end-start));
        offset += block;
    }
}

void test_handle_time(int *fds, int nums, int block, int mode) {
    long long start=0;
    long long end=0;
    void (*p)(int *, int, int);

    p = mode == 0 ? read_file_in_all : read_file_in_blocks;

    start = get_current_time_ms();
    p(fds, nums, block);
    end = get_current_time_ms();

    printf("use %d block in %d mode, handle time %d\n", block, mode, (int)(end-start));
}

int main(int argc, char *argv[]) {
    if(argc != 5) {
        printf("input: ./exec fd_nums flag block_size mode(0,1)\n");
        return -1;
    }
    int nums = atoi(argv[1]);
    int flags = atoi(argv[2]);
    int block = atoi(argv[3]);
    int mode = atoi(argv[4]);

    //int nums = 255;
    //int flags = 1;
    //int block = 4096;
    //int mode = 1;

    int fds[nums];
    char path[20]={0};
    if (flags == 0) {
        strcpy(path, "/dev/shm/data/");
        printf("Read %d files from memery\n", nums);
    }
    else {
        strcpy(path, "./data/");
        printf("Read %d files from disk\n", nums);
    }

    if (open_fds(fds, O_RDONLY, path, nums) < 0)
    //if (open_fds(fds, O_RDONLY | O_NONBLOCK, path, nums) < 0)
        return -1;

    test_handle_time(fds, nums, block, mode);

    return 0;
}
