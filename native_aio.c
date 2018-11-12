#include<stdio.h>
#include<fcntl.h>
#include<string.h>
#include<stdlib.h>
#include<libaio.h>
#include<errno.h>
#include<unistd.h>

int main(void){
    int output_fd;
    const char *content="hello world!";
    const char *outputfile="hello.txt";
    io_context_t ctx;
    struct iocb io,*p=&io;
    struct io_event e;
    struct timespec timeout;

    memset(&ctx,0,sizeof(ctx));

    if(io_setup(1024, &ctx)!=0) {
        printf("io_setup error\n");
        return -1;
    }

    if((output_fd=open(outputfile, O_CREAT|O_WRONLY, 0644))<0) {
        perror("open error");
        io_destroy(ctx);
        return -1;
    }

    io_prep_pwrite(&io, output_fd, content, strlen(content), 0);
    io.data=content;
    if(io_submit(ctx,1,&p)!=1) {
        io_destroy(ctx);
        printf("io_submit error\n");
        return -1;
    }

    while(1) {
        timeout.tv_sec=0;
        timeout.tv_nsec=5;//0.5s
        if(io_getevents(ctx, 0, 1024, &e, &timeout)==1) {
            printf("have done\n");
            close(output_fd);
            break;
        }
        printf("haven't done\n");
        sleep(1);
    }

    io_destroy(ctx);
    return 0;
}
