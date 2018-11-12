#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <errno.h>

#include <libaio.h>

#define AIO_BLKSIZE (64*1024)
#define AIO_MAXIO   32

static int busy = 0;
static int tocopy = 0;
static int dstfd = -1;
static const char *dstname = NULL;
static const char *srcname = NULL;

static void io_error(const char *func, int rc) {
    if (rc == ENOSYS)
        fprintf(stderr, "AIO not in this kernel");
    else if (rc < 0)
        fprintf(stderr, "%s: %s", func, strerror(rc));
    else
        fprintf(stderr, "%s: error %d", func, rc);

    if (dstfd > 0)
        close(dstfd);
    if (dstname)
        unlink(dstname);
    exit(1);
}

static void wr_done(io_context_t ctx, struct iocb *iocb, long res, long res2) {
    if (res2 != 0) {
        io_error("aio write", res2);
    }
    if (res != iocb->u.c.nbytes) {
        fprintf(stderr, "write missed bytes expect %d got %d", 0, iocb->u.c.nbytes, res2);
        exit(1);
    }
    --tocopy;
    --busy;
    free(iocb->u.c.buf);

    memset(iocb, 0xff, sizeof(iocb));   // paranoia
    free(iocb);
    write(2, "w", 1);
}

static void rd_done(io_context_t ctx, struct iocb *iocb, long res, long res2) {
    /* library needs accessors to look at iocb? */
    int iosize = iocb->u.c.nbytes;
    char *buf = iocb->u.c.buf;
    off_t offset = iocb->u.c.offset;

    if (res2 != 0)
        io_error("aio read", res2);
    if (res != iosize) {
        fprintf(stderr, "read missing bytes expect %d got %d", 0, iocb->u.c.nbytes, res);
        exit(1);
    }

    /* turn read into write */
    io_prep_pwrite(iocb, dstfd, buf, iosize, offset);
    io_set_callback(iocb, wr_done);
    if (1 != (res = io_submit(ctx, 1, &iocb)))
        io_error("io_submit write", res);
    write(2, "r", 1);
}

int calc_blocks(int length, int block) {
    if(length % block == 0)
        return length / block;
    else
        return length / block + 1;
}

int main(int argc, char *argv[]) {
    int srcfd;
    struct stat st;
    off_t length = 0, offset = 0;
    io_context_t myctx;

    if (argc != 3 || argv[1][0] == '-') {
        fprintf(stderr, "Usage: aiocp SOURCE DEST");
        exit(1);
    }
    if ((srcfd = open(srcname = argv[1], O_RDONLY)) < 0) {
        perror(srcname);
        exit(1);
    }
    if (fstat(srcfd, &st) < 0) {
        perror("fstat");
        exit(1);
    }
    length = st.st_size;
    printf("length %d\n", length);

    if ((dstfd = open(dstname = argv[2], O_WRONLY | O_CREAT, 0666)) < 0) {
        close(srcfd);
        perror(dstname);
        exit(1);
    }

    /* initialize state machine */
    memset(&myctx, 0, sizeof(myctx));
    io_queue_init(AIO_MAXIO, &myctx);

    tocopy = calc_blocks(length, AIO_BLKSIZE);
    printf("tocopy %d\n", tocopy);

    while (tocopy > 0) {
        int i, rc;
        /* Submit as many reads as once as possible upto AIO_MAXIO */
        int n = MIN(MIN(AIO_MAXIO - busy, AIO_MAXIO / 2),
                    calc_blocks(length - offset, AIO_BLKSIZE));

        if (n > 0) {
            struct iocb *ioq[n];
            for (i = 0; i < n; i++) {
                struct iocb *io = (struct iocb *)malloc(sizeof(struct iocb));
                int iosize = MIN(length - offset, AIO_BLKSIZE);
                char *buf = (char *)malloc(iosize);

                if (NULL == buf || NULL == io) {
                    fprintf(stderr, "out of memory", 0);
                    exit(1);
                }

                io_prep_pread(io, srcfd, buf, iosize, offset);
                io_set_callback(io, rd_done);
                ioq[i] = io;
                offset += iosize;
            }

            rc = io_submit(myctx, n, ioq);
            if (rc < 0) {
                io_error("io_submit", rc);
            }
            busy += n;
        }

        rc = io_queue_run(myctx);
        if (rc < 0) {
            io_error("io_queue_run", rc);
        }

        if (busy == AIO_MAXIO) {
            rc = io_queue_wait(myctx, NULL);
            if (rc < 0) {
                io_error("io_queue_wait", rc);
            }
        }
    }

    close(srcfd);
    close(dstfd);
    exit(0);
}
