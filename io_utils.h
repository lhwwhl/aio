#ifndef _IO_UTILS_H_
#define _IO_UTILS_H_
#endif

#define IO_BLOCK_SIZE 4096
#define IO_MAX_NUMS 32

int open_fds(int *fds, int nums);

long long get_current_time_ms();

off_t get_file_size(int fd);
