#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define TIME_MEASSURE
// #define RETURN_BY_BUF

int main()
{
    long long sz;

    char buf[150];
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

#ifdef TIME_MEASSURE
    FILE *outputFile = fopen("data.txt", "w");
    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        struct timespec tstart = {0, 0}, tend = {0, 0};
        clock_gettime(CLOCK_MONOTONIC, &tstart);
        sz = read(fd, buf, 2);
        clock_gettime(CLOCK_MONOTONIC, &tend);
        long long usertime = (1e9 * tend.tv_sec + tend.tv_nsec) -
                             (1e9 * tstart.tv_sec + tstart.tv_nsec);
        long long kerneltime = write(fd, write_buf, strlen(write_buf));
        fprintf(outputFile, "%d %lld %lld %lld\n", i, kerneltime, usertime,
                usertime - kerneltime);
#ifdef RETURN_BY_BUF
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
#else
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
#endif
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n",
               kerneltime);
    }
#endif
#ifndef TIME_MEASSURE
    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 0);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 0);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }
#endif

    close(fd);
    return 0;
}
