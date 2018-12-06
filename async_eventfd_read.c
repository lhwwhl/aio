#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <libaio.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>

#include "io_utils.h"

#define ALIGN_SIZE  4096
#define RD_WR_SIZE  4096

void aio_callback(io_context_t ctx, struct iocb *iocb, long res, long res2) {
    char *data = iocb->u.c.buf;
    printf("buf %c\n", data[0]);
    printf("request_type: %s, offset: %lld, length: %lu, res: %ld, res2: %ld\n",
            (iocb->aio_lio_opcode == IO_CMD_PREAD) ? "READ" : "WRITE",
            iocb->u.c.offset, iocb->u.c.nbytes, res, res2);
}

int async_eventfd_read(io_context_t ctx, int *fds, int nums) {
    int efd, epfd;
    struct timespec tms = {0, 0};
    struct io_event event;
    struct iocb iocbs[nums];
    struct iocb *iocbp;
    struct iocb *iocbps[nums];
    void *buf[nums];
    struct epoll_event epevent;

    efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd == -1) {
        perror("eventfd");
        return -3;
    }

    epfd = epoll_create(1);
    if (epfd == -1) {
        perror("epoll_create");
        return -4;
    }

    epevent.events = EPOLLIN | EPOLLET;
    epevent.data.ptr = NULL;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, efd, &epevent)) {
        perror("epoll_ctl");
        return -5;
    }

    for (int i=0; i<nums; ++i) {
        if (posix_memalign(&buf[i], ALIGN_SIZE, RD_WR_SIZE)) {
            perror("posix_memalign");
            return -6;
        }
    }

    int file_size = get_file_size(fds[0]);
    int had_read = 0;
    while(had_read < file_size) {
        long long start = get_current_time_ms();
        long long s_submit = get_current_time_ms();
        for (int i=0; i<nums; ++i) {
            io_prep_pread(&iocbs[i], fds[i], buf[i], RD_WR_SIZE, had_read);
            io_set_eventfd(&iocbs[i], efd);
            //io_set_callback可以设置事件的回调函数;
            //io_set_callback(&iocbs[i], aio_callback);
            iocbs[i].data = buf[i];
            iocbps[i] = &iocbs[i];
            //测试单步io_submit执行的时间;
            //iocbp = &iocbs[i];
            //long long s_submit = get_current_time_ms();
            //if (io_submit(ctx, 1, &iocbp) != 1) {
            //    perror("io_submit");
            //    return -7;
            //}
            //long long e_submit = get_current_time_ms();
            //printf("iocb submit %d us\n", (int)(e_submit - s_submit));
        }

        if (io_submit(ctx, nums, iocbps) != nums) {
            perror("io_submit");
            return -7;
        }
        long long e_submit = get_current_time_ms();
        printf("iocb submit %d us\n", (int)(e_submit - s_submit));

        int total_completed_events = 0;
        while (total_completed_events < nums) {
            uint64_t active_events;
            if (epoll_wait(epfd, &epevent, 1, -1) != 1) {
                perror("epoll_wait");
                return -8;
            }

            if (read(efd, &active_events, sizeof(active_events)) != sizeof(active_events)) {
                perror("read");
                return -9;
            }
            int pending_events = active_events;
            while(pending_events > 0) {
                int ret = io_getevents(ctx, 1, 1, &event, &tms);
                if (ret > 0) {
                    //((io_callback_t)(event.data))(ctx, event.obj, event.res, event.res2);
                    char *data = event.data;
                    //printf("buf %c\n", data[0]);
                    total_completed_events += ret;
                    pending_events -= ret;
                }
            }
        }
        long long end = get_current_time_ms();
        printf("finished %d us\n\n", (int)(end - start));
        had_read +=  RD_WR_SIZE;
    }

    close(epfd);
    for (int i=0; i<nums; ++i) {
        free(buf[i]);
    }
    close(efd);
}

int main(int argc, char *argv[]) {
    if(argc != 5) {
        printf("input: ./exec fd_nums flag block_size mode(0,1,2,3)\n");
        return -1;
    }

    //nums : 测试打开的文件数量;
    //flags : 0代表在文件的路径在tmpfs下，1代码在当前目录的data下;
    //block : 设定的读取buf的大小，4k，8k，16k等等(换算为字节);
    //mode : 0代表fd O_RDWR;
    //       1代表fd O_RDWR O_DIRECT;
    //       2代表fd O_RDWR O_NONBLOCK;
    //       3代表fd O_RDWR O_NONBLOCK O_DIRECT;
    int nums = atoi(argv[1]);
    int flags = atoi(argv[2]);
    int block = atoi(argv[3]);
    int mode = atoi(argv[4]);

    int fds[nums];
    char path[20]={0};
    if (flags == 0) {
        strcpy(path, "/dev/shm/data/");
        printf("Read %d files from memery\n", nums);
    } else {
        strcpy(path, "./data/");
        printf("Read %d files from disk\n", nums);
    }

    switch(mode) {
        case 0 :
            if (open_fds(fds, O_RDWR, path, nums) < 0) return -2;
            break;
        case 1 :
            if (open_fds(fds, O_RDWR | O_DIRECT, path, nums) < 0) return -2;
            break;
        case 2 :
            if (open_fds(fds, O_RDWR | O_NONBLOCK, path, nums) < 0) return -2;
            break;
        case 3 :
            if (open_fds(fds, O_RDWR | O_NONBLOCK | O_DIRECT, path, nums) < 0) return -2;
            break;
        default :
            return -2;
    }

    io_context_t ctx = 0;

    io_setup(8192, &ctx);

    int ret = async_eventfd_read(ctx, fds, nums);
    if(ret < 0) {
        return ret;
    }

    io_destroy(ctx);

    return 0;
}
