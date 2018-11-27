#ifndef _IO_UTILS_H_
#define _IO_UTILS_H_
#endif

int open_fds(int *fds, int flags, const char *path, int nums);

long long get_current_time_ms();

off_t get_file_size(int fd);
