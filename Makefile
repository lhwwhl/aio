all : aio raio rwaio sio

aio : native_aio.c
	gcc -o aio native_aio.c -laio

raio : aio_rfile_in_blocks.c
	gcc -std=c99 -o raio aio_rfile_in_blocks.c -laio

rwaio : aio_rwfile_in_blocks.c
	gcc -std=c99 -o rwaio aio_rwfile_in_blocks.c -laio

sio : sync_io_rfile.c
	gcc -std=c99 -o sio sync_io_rfile.c

clean :
	rm -rf aio raio rwaio sio hello.txt
