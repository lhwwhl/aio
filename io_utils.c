#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

#include "io_utils.h"

int open_fds(int *fds, int flags, const char *path, int nums) {
    char fullname[100]={0};
    char tempname[100]={0};

    strcat(tempname, path);
    strcat(tempname, "%d.data");
    for(int i=0; i<nums; ++i) {
        sprintf(fullname, tempname, i);
        if ((fds[i] = open(fullname, flags)) < 0) {
            printf("open file error %s\n", strerror(errno));
            return -1;
        }
    }

    return 0;
}

long long get_current_time_ms() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return 1000000 * time.tv_sec + time.tv_usec;
}

off_t get_file_size(int fd) {
    struct stat st;

    if (fstat(fd, &st) < 0) {
        printf("get file size error %s\n", strerror(errno));
        return -2;
    }

    return st.st_size;
}

