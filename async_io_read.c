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

void read_file_all_in_order_with_event(int *fds, int nums, int block, int event_max);
void read_file_all_in_order_with_events(int *fds, int nums, int block, int event_max);
void read_file_in_blocks_with_nums_events(int *fds, int nums, int block, int event_max);
void read_file_in_blocks_with_events(int *fds, int nums, int block, int event_max);

void read_file_all_in_order_with_event(int *fds, int nums, int block, int event_max) {
    struct iocb io;
    struct iocb *p=&io;
    char buf[block];
    struct io_event event;

    int file_size = get_file_size(fds[0]);
    int had_read = 0;
    int finished_file = 0;
    for(int i=0; i<nums; ++i) {
        while(had_read < file_size) {
            io_prep_pread(&io, fds[i], buf, block, had_read);
            int ret = io_submit(ctx, 1, &p);
            if(ret < 0)
                printf("io submit error\n");
            if(io_getevents(ctx,0,1,&event,NULL) == 1)
                had_read += 4096;
        }
        had_read = 0;
    }
}

void read_file_all_in_order_with_events(int *fds, int nums, int block, int event_max) {
    struct iocb iocbs[event_max];
    struct iocb *iocb_list[event_max];
    char buf[event_max][block];
    struct io_event events[event_max];

    int file_size = get_file_size(fds[0]);
    int had_read = 0;
    int finished_file = 0;
    for(int i=0; i<nums; ++i) {
        while(had_read < file_size) {
            for(int j=0; j<event_max; ++j) {
                io_prep_pread(&iocbs[j], fds[i], buf[j], block, had_read);
                iocb_list[j] = &iocbs[j];
            }
            int ret = io_submit(ctx, nums, iocb_list);
            if(ret < 0)
                printf("io submit error\n");
            int completed = io_getevents(ctx, 0, event_max, events, NULL);
            //printf("completed event %d\n", completed);
            //for(int k=0; k<completed;++k) {
            //    printf("read %d\n", events[k].res);
            //}
            had_read += completed * block;
        }
        had_read = 0;
    }
}

void read_file_in_blocks_with_nums_events(int *fds, int nums, int block, int event_max) {
    struct iocb iocbs[nums];
    struct iocb *iocb_list[nums];
    char buf[nums][block];
    struct io_event events[nums];

    int file_size = get_file_size(fds[0]);
    int had_read = 0;
    while(had_read < file_size) {
        for(int i=0; i<nums; ++i) {
            io_prep_pread(&iocbs[i], fds[i], buf[i], block, had_read);
            iocb_list[i] = &iocbs[i];
        }
        int ret = io_submit(ctx, nums, iocb_list);
        if(ret < 0)
            printf("io submit error\n");
        int completed = io_getevents(ctx, 0, nums, events, NULL);
        had_read += block;
    }
}

void read_file_in_blocks_with_events(int *fds, int nums, int block, int event_max) {
    struct iocb iocbs[event_max];
    struct iocb *iocb_list[event_max];
    char buf[event_max][block];
    struct io_event events[event_max];

    int file_size = get_file_size(fds[0]);
    int had_read = 0;
    int finished_file = 0;
    while(finished_file < nums) {
        int exec_events = (nums - finished_file) >= event_max ? event_max : (nums - finished_file);
        while(had_read < file_size) {
            for(int i=0; i<exec_events; ++i) {
                io_prep_pread(&iocbs[i], fds[i+finished_file], buf[i], block, had_read);
                iocb_list[i] = &iocbs[i];
            }
            int ret = io_submit(ctx, nums, iocb_list);
            if(ret < 0)
                printf("io submit error\n");
            int completed = io_getevents(ctx, 0, nums, events, NULL);
            had_read += block;
        }
        had_read = 0;
        finished_file += exec_events;
    }
}

void test_handle_time(int *fds, int nums, int block, int event_max, int mode) {
    long long start=0;
    long long end=0;
    void (*pfunc)(int *, int, int, int);

    switch(mode) {
        case 0 :
            event_max = 1;
            pfunc = read_file_all_in_order_with_event;
            break;
        case 1 :
            pfunc = read_file_all_in_order_with_events;
            break;
        case 2 :
            event_max = nums;
            pfunc = read_file_in_blocks_with_nums_events;
            break;
        case 3 :
            pfunc = read_file_in_blocks_with_events;
            break;
        default :
            return;
    }

    start = get_current_time_ms();
    pfunc(fds, nums, block, event_max);
    end = get_current_time_ms();

    printf("use %d block in %d mode with %d events, handle time %d us\n", block, mode, event_max, (int)(end-start));
}

int main(int argc, char *argv[]) {
    if(argc != 6) {
        printf("input: ./exec fd_nums flag block_size event_ mode(0,1,2,3)\n");
        return -1;
    }

    //nums : 测试打开的文件数量;
    //flags : 0代表在文件的路径在tmpfs下，1代码在当前目录的data下;
    //block : 设定的读取buf的大小，4k，8k，16k等等(换算为字节);
    //events : 设定的用于读取的events个数；
    //mode : 0代表用1个event顺序读文件，直到读完nums个文件;
    //       1代表用events个事件顺序读文件，直到读完nums个文件;
    //       2代表用nums相同个数的events分片读文件，每次读block大小(同时读多个文件);
    //       3代表用events个事件分片读，为测试events的个数是否对读取文件效率有影响;
    int nums = atoi(argv[1]);
    int flags = atoi(argv[2]);
    int block = atoi(argv[3]);
    int events = atoi(argv[4]);
    int mode = atoi(argv[5]);

    //int nums = 100;
    //int flags = 1;
    //int block = 4096;
    //int events = 100;
    //int mode = 2;

    int fds[nums];
    char path[20]={0};
    if (flags == 0) {
        strcpy(path, "/dev/shm/data/");
        printf("Read %d files from memery\n", nums);
    } else {
        strcpy(path, "./data/");
        printf("Read %d files from disk\n", nums);
    }

    //此版本适用于同步读取.
    //direct io获取数据会有问题，直接io_submit后立马io_getevents会导致数据不完全.
    //需要用thread或epoll监控事件，来进行事件监控;
    if (open_fds(fds, O_RDONLY, path, nums) < 0) return -1;
    //if (open_fds(fds, O_RDONLY | O_DIRECT, path, nums) < 0) return -1;

    io_setup(8192, &ctx);

    test_handle_time(fds, nums, block, events, mode);

    return 0;
}
