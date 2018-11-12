#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/timeb.h>

#include <libaio.h>

#define AIO_BLKSIZE (4*1024)
#define AIO_MAX 200
int tonums=0;

long long get_system_time() {
    struct timeb t;
    ftime(&t);
    return 1000 * t.time + t.millitm;
}

static void rd_done(io_context_t ctx, struct iocb *iocb, long res, long res2) {
    /* library needs accessors to look at iocb? */
    int iosize = iocb->u.c.nbytes;
    char *buf = iocb->u.c.buf;
    off_t offset = iocb->u.c.offset;

    if (res2 != 0)
        printf("aio read %d\n", res2);
    if (res != iosize) {
        fprintf(stderr, "read missing bytes expect %d got %d", 0, iocb->u.c.nbytes, res);
        exit(1);
    }
    --tonums;

    //printf("fd %d read %d bytes\n", iocb->aio_fildes, iosize);
    free(iocb->u.c.buf);
    memset(iocb, 0xff, sizeof(iocb));   // paranoia
    free(iocb);
}

int calc_blocks(int length, int block) {
    return length%block==0 ? length/block : length/block+1;
}

int main(int argc, char *argv[]) {
    int fd[AIO_MAX];
    char name[30];
    struct stat st;
    off_t length = 0, offset = 0;
    io_context_t myctx;

    int temp_fd;
    for(int i=1; i<=AIO_MAX; ++i) {
        sprintf(name, "./data/%d.data", i);
        if ((temp_fd = open(name, O_RDONLY)) < 0) {
            printf("open failed\n");
            exit(1);
        }
        fd[i-1] = temp_fd;
    }

    if (fstat(fd[0], &st) < 0) {
        perror("fstat");
        exit(1);
    }
    length = st.st_size;

    /* initialize state machine */
    memset(&myctx, 0, sizeof(myctx));
    io_queue_init(AIO_MAX, &myctx);

    tonums = calc_blocks(length, AIO_BLKSIZE)*AIO_MAX;

    struct iocb *ioq[AIO_MAX];
    int iosize=0;
    long long start, end;

    start = get_system_time();
    while(tonums>0) {
        int rc;
        for(int i=0; i<AIO_MAX; ++i) {
            struct iocb *io = (struct iocb *)malloc(sizeof(struct iocb));
            char *buf = (char *)malloc(4096);
            iosize = MIN(length - offset, AIO_BLKSIZE);

            if (NULL == buf || NULL == io) {
                fprintf(stderr, "out of memory", 0);
                exit(1);
            }

            io_prep_pread(io, fd[i], buf, iosize, offset);
            io_set_callback(io, rd_done);
            ioq[i] = io;
        }
        offset += iosize;

        rc = io_submit(myctx, AIO_MAX, ioq);
        if (rc < 0) {
            printf("io_submit %s\n", strerror(rc));
        }

        rc = io_queue_run(myctx);
        if (rc < 0) {
            printf("io_queue_run %d\n", rc);
        }

        //if (busy == 1024) {
        //    rc = io_queue_wait(myctx, NULL);
        //    if (rc < 0) {
        //        io_error("io_queue_wait", rc);
        //    }
        //}
    }
    end = get_system_time();
    printf("interval %d\n", end-start);
    return 0;
}
