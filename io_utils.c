#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/timeb.h>
#include <errno.h>

#include "io_utils.h"

int open_fds(int *fds, int nums) {
    char filename[30];

    for(int i=0; i<nums; ++i) {
        sprintf(filename, "./data/%d.data", i);
        if ((fds[i] = open(filename, O_RDONLY)) < 0) {
            printf("open file error %s\n", strerror(errno));
            return -1;
        }
    }

    return 0;
}

long long get_current_time_ms() {
    struct timeb t;
    ftime(&t);
    return 1000 * t.time + t.millitm;
}

off_t get_file_size(int fd) {
    struct stat st;

    if (fstat(fd, &st) < 0) {
        printf("get file size error %s\n", strerror(errno));
        return -1;
    }

    return st.st_size;
}

