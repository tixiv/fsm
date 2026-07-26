#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int main () {
    int fd = open("/tmp/audio.pipe", O_RDWR);

    printf("size is %d\n", fcntl(fd, F_GETPIPE_SZ));

    fcntl(fd, F_SETPIPE_SZ, 1024);

    printf("size is %d\n", fcntl(fd, F_GETPIPE_SZ));

    while(1) usleep(50000);

    close(fd);
}
