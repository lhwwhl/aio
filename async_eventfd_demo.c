#define _GNU_SOURCE
#define __STDC_FORMAT_MACROS

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

#define NUM_EVENTS  200
#define ALIGN_SIZE  4096
#define RD_WR_SIZE  4096

void aio_callback(io_context_t ctx, struct iocb *iocb, long res, long res2) {
    char *data = iocb->u.c.buf;
    printf("buf %c\n", data[0]);
    printf("request_type: %s, offset: %lld, length: %lu, res: %ld, res2: %ld\n",
            (iocb->aio_lio_opcode == IO_CMD_PREAD) ? "READ" : "WRITE",
            iocb->u.c.offset, iocb->u.c.nbytes, res, res2);
}

int main(int argc, char *argv[]) {
    int efd, fd, epfd;
    io_context_t ctx;
    struct timespec tms = {0, 0};
    struct io_event event;
    struct iocb iocbs[NUM_EVENTS];
    struct iocb *iocbps[NUM_EVENTS];
    void *buf[NUM_EVENTS];
    struct epoll_event epevent;

    efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd == -1) {
        perror("eventfd");
        return 2;
    }

    int fds[NUM_EVENTS];
    int flags = 1;
    char path[20]={0};
    if (flags == 0) {
        strcpy(path, "/dev/shm/data/");
        printf("Read %d files from memery\n", NUM_EVENTS);
    } else {
        strcpy(path, "./data/");
        printf("Read %d files from disk\n", NUM_EVENTS);
    }

    //if (open_fds(fds, O_RDWR, path, NUM_EVENTS) < 0) return -1;
    //if (open_fds(fds, O_RDWR | O_DIRECT , path, NUM_EVENTS) < 0) return -1;
    //if (open_fds(fds, O_RDWR | O_NONBLOCK, path, NUM_EVENTS) < 0) return -1;
    if (open_fds(fds, O_RDWR | O_NONBLOCK | O_DIRECT, path, NUM_EVENTS) < 0) return -1;

    ctx = 0;
    if (io_setup(8192, &ctx)) {
        perror("io_setup");
        return 3;
    }

    epfd = epoll_create(1);
    if (epfd == -1) {
        perror("epoll_create");
        return 4;
    }

    epevent.events = EPOLLIN | EPOLLET;
    epevent.data.ptr = NULL;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, efd, &epevent)) {
        perror("epoll_ctl");
        return 5;
    }

    long long start = get_current_time_ms();
    for (int i=0; i<NUM_EVENTS; ++i) {
        if (posix_memalign(&buf[i], ALIGN_SIZE, RD_WR_SIZE)) {
            perror("posix_memalign");
            return 6;
        }
        //printf("buf: %p\n", buf);

        io_prep_pread(&iocbs[i], fds[i], buf[i], RD_WR_SIZE, 0);
        io_set_eventfd(&iocbs[i], efd);
        //io_set_callback(&iocbs[i], aio_callback);
        iocbs[i].data = buf[i];
        iocbps[i] = &iocbs[i];
    }

    if (io_submit(ctx, NUM_EVENTS, iocbps) != NUM_EVENTS) {
        perror("io_submit");
        return 7;
    }
    long long end = get_current_time_ms();
    printf("iocb submit %d us\n", (int)(end - start));

    int total_completed_events = 0;
    while (total_completed_events < NUM_EVENTS) {
        uint64_t active_events;

        if (epoll_wait(epfd, &epevent, 1, -1) != 1) {
            perror("epoll_wait");
            return 8;
        }

        if (read(efd, &active_events, sizeof(active_events)) != sizeof(active_events)) {
            perror("read");
            return 9;
        }
        int pending_events = active_events;
        while(pending_events > 0) {
            int ret = io_getevents(ctx, 1, 1, &event, &tms);
            if (ret > 0) {
                //((io_callback_t)(event.data))(ctx, event.obj, event.res, event.res2);
                char *data = event.data;
                //printf("buf %c\n", data[0]);
                //struct iocb *iocb = event.obj;
                //char *iocbbuf = (char *)iocb->u.c.buf;
                //printf("iocb buf %p\n", iocbbuf);
                total_completed_events += ret;
                pending_events -= ret;
            }
        }
    }

    end = get_current_time_ms();
    printf("iocb finished %d us\n", (int)(end - start));

    close(epfd);
    //free(buf);
    io_destroy(ctx);
    close(fd);
    close(efd);

    return 0;
}
