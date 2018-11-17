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

    int file_size = get_file_size(fds[0]);
    while(file_size>0) {
        for(int i=0; i<nums; ++i) {
            read(fds[i], buf, block);
        }
        file_size -= block;
    }
}

int test_handle_time(int *fds, int nums, int block, int mode) {
    long long start=0;
    long long end=0;
    void (*p)(int *, int, int);

    p = mode == 0 ? read_file_in_all : read_file_in_blocks;

    start = get_current_time_ms();
    p(fds, nums, block);
    end = get_current_time_ms();

    return (int)(end-start);
}

int main(int argc, char *argv[]) {
    if(argc != 4) {
        printf("input: ./exec fd_nums block_size mode(0,1)\n");
    }
    int nums = atoi(argv[1]);
    int block = atoi(argv[2]);
    int mode = atoi(argv[3]);

    int fds[nums];
    if (open_fds(fds, nums) < 0) return -1;

    int interval = test_handle_time(fds, nums, block, mode);
    printf("Read %d files with %d block in %d mode, handle time %d\n", nums, block, mode, interval);

    return 0;
}
