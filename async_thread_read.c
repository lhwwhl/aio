#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <libaio.h>
#include <pthread.h>
#include <errno.h>

#include "io_utils.h"

io_context_t ctx=0;
long long start = 0;
long long end = 0;

void submit_requests(int *fds, int nums, int block) {
    struct iocb iocbs[nums];
    struct iocb *iocb_list[nums];

    char *buf[nums];
    for(int i=0; i<nums; ++i){
        buf[i] = (char *)calloc(0, 4096);
        //direct io读取时buf必须与page对齐，否则数据写不到buf里;
        posix_memalign((void**)&buf[i], 4096, 4096);
    }

    int file_size = get_file_size(fds[0]);
    int had_read = 0;
    long long total_size = 0;
    long long s_submit, e_submit;
    while(had_read < file_size) {
        start = get_current_time_ms();

        //测试malloc分配iocb与buf，同时测单此iosubmit执行时间，留下当个简单的例子吧;
        //for(int i=0; i<nums; ++i) {
        //    struct iocb *io = (struct iocb *)malloc(sizeof(struct iocb));
        //    char *buf = (char *)malloc(4096);
        //    posix_memalign((void**)&buf, 4096, 4096);
        //    io_prep_pread(io, fds[i], buf, block, had_read);
        //    io->data = (void *)buf;
        //    s_submit = get_current_time_ms();
        //    int ret = io_submit(ctx, 1, &io);
        //    if(ret < 0)
        //        printf("io submit error\n");
        //    e_submit = get_current_time_ms();
        //    printf("single iocb submit %d us\n", (int)(e_submit - s_submit));
        //}

        for(int i=0; i<nums; ++i) {
            io_prep_pread(&iocbs[i], fds[i], buf[i], block, had_read);
            iocbs[i].data = (void *)buf;
            iocb_list[i] = &iocbs[i];
        }
        s_submit = get_current_time_ms();
        int ret = io_submit(ctx, nums, iocb_list);
        if(ret < 0)
            printf("io submit error\n");
        e_submit = get_current_time_ms();
        printf("iocb submit %d us\n", (int)(e_submit - s_submit));
        had_read += block;
    }
}

void *reap_events(void *dummy){
    int nums = *(int *)dummy;
    struct io_event events[nums];
    int counts = 0;
    struct timespec timeout = {1, 0};
    while(1) {
        int completed = io_getevents(ctx, 0, nums, events, &timeout);
        for(int i=0; i<completed; ++i){
            char *data=(char *)events[i].data;
            //printf("%ld %c\n", events[i].res, data[0]);
            //如果使用malloc分配iocb或者buf需要在使用后free;
        }
        counts += completed;
        if(counts >= nums) {
            end = get_current_time_ms();
            printf("iocb finished %d us\n\n", (int)(end - start));
            counts = 0;
        }
    }
}

int main(int argc, char *argv[]) {
    if(argc != 4) {
        printf("input: ./exec fd_nums flag block_size\n");
        return -1;
    }

    //nums : 测试打开的文件数量;
    //flags : 0代表在文件的路径在tmpfs下，1代码在当前目录的data下;
    //block : 设定的读取buf的大小，4k，8k，16k等等(换算为字节);
    int nums = atoi(argv[1]);
    int flags = atoi(argv[2]);
    int block = atoi(argv[3]);

    //int nums = 100;
    //int flags = 1;
    //int block = 4096;

    int fds[nums];
    char path[20]={0};
    if (flags == 0) {
        strcpy(path, "/dev/shm/data/");
        printf("Read %d files from memery\n", nums);
    } else {
        strcpy(path, "./data/");
        printf("Read %d files from disk\n", nums);
    }

    if (open_fds(fds, O_RDONLY | O_DIRECT, path, nums) < 0) return -1;
    //if (open_fds(fds, O_RDONLY, path, nums) < 0) return -1;

    io_setup(8192, &ctx);

    pthread_t reaper_thread;

    pthread_create(&reaper_thread, NULL, reap_events, (void *)&nums);
    submit_requests(fds, nums, block);
    pthread_join(reaper_thread, NULL);

    return 0;
}
