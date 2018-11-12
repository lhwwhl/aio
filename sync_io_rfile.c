#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/timeb.h>

#define IO_MAX 200
long long get_system_time() {
    struct timeb t;
    ftime(&t);
    return 1000 * t.time + t.millitm;
}

int main() {
    int fd[IO_MAX];
    char name[30];
    int temp=0;

    for(int i=1; i<=IO_MAX; ++i) {
        sprintf(name, "./data/%d.data", i);
        if ((temp = open(name, O_RDONLY)) < 0) {
            printf("open failed\n");
            exit(1);
        }
        fd[i-1] = temp;
    }

    char buf[4096];
    int ret;

    long long start = get_system_time();
    for(int i=0; i<IO_MAX; ++i) {
        ret = read(fd[i], buf, 4096);
        while(1) {
            ret = read(fd[i], buf, 4096);
            //printf("fd %d have read bytes %d\n", fd[i], ret);
            if(ret != 4096)
                break;
        }
    }
    long long end = get_system_time();
    printf("interval %d\n", end-start);
}
