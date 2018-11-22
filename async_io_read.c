#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<libaio.h>
#include<errno.h>

#include "io_utils.h"

void read_file_all_in_order_with_event(io_context_t ctx, int *fds, int nums, int block, int event_max);
void read_file_all_in_order_with_events(io_context_t ctx, int *fds, int nums, int block, int event_max);
void read_file_in_blocks_with_nums_events(io_context_t ctx, int *fds, int nums, int block, int event_max);
void read_file_in_blocks_with_events(io_context_t ctx, int *fds, int nums, int block, int event_max);

void read_file_all_in_order_with_event(io_context_t ctx, int *fds, int nums, int block, int event_max) {
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

void read_file_all_in_order_with_events(io_context_t ctx, int *fds, int nums, int block, int event_max) {
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

void read_file_in_blocks_with_nums_events(io_context_t ctx, int *fds, int nums, int block, int event_max) {
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

void read_file_in_blocks_with_events(io_context_t ctx, int *fds, int nums, int block, int event_max) {
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
                io_prep_pread(&iocbs[i], fds[i], buf[i], block, had_read);
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

void test_handle_time(io_context_t ctx, int *fds, int nums, int block, int event_max, int mode) {
    long long start=0;
    long long end=0;
    void (*pfunc)(io_context_t, int *, int, int, int);

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
    pfunc(ctx, fds, nums, block, event_max);
    end = get_current_time_ms();

    printf("use %d block in %d mode with %d events, handle time %d ms\n", block, mode, event_max, (int)(end-start));
}

int main(int argc, char *argv[]) {
    if(argc != 6) {
        printf("input: ./exec fd_nums flag block_size event_ mode(0,1,2,3)\n");
        return -1;
    }
    int nums = atoi(argv[1]);
    int flag = atoi(argv[2]);
    int block = atoi(argv[3]);
    int events = atoi(argv[4]);
    int mode = atoi(argv[5]);

    int fds[nums];
    if (open_fds(fds, nums, flag) < 0) return -1;
    if (flag == 0)
        printf("Read %d files from memery\n", nums);
    else
        printf("Read %d files from disk\n", nums);

    io_context_t ctx=0;
    io_setup(nums, &ctx);

    test_handle_time(ctx, fds, nums, block, events, mode);

    return 0;
}
