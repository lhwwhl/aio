all : async_io_read sync_io_read

async_io_read : io_utils.o async_io_read.c
	gcc -std=c99 -o async_io_read async_io_read.c io_utils.o -laio

sync_io_read : io_utils.o sync_io_read.c 
	gcc -std=c99 -o sync_io_read sync_io_read.c io_utils.o

io_utils.o : io_utils.h io_utils.c
	gcc -std=c99 -c io_utils.c -o io_utils.o

.PHONY:clean
clean :
	rm -rf sync_io_read async_io_read io_utils.o
