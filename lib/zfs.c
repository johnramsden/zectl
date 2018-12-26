//
// Created by john on 12/25/18.
//

#include "zfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
//#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

long fsize(int fd) {
    // Reference: https://stackoverflow.com/a/8384
//    struct stat st;
//
//    if (fstat(fd, &st) == 0) {
//        return st.st_size;
//    }

    FILE *fp = fdopen(fd, "r");
    if (fp == NULL) {
        return -1;
    }

    fseek(fp, 0L, SEEK_END);
    return ftell(fp);
}

int map_file(const char *file) {

    char *buffer;

    int fd = open(file, O_RDONLY);
    if(fd < 0) {
        return -1;
    }

    long file_size = fsize(fd);
    if (file_size == -1) {
        return -1;
    }

    // Map file

    char *region = mmap(0L, (size_t) file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (region == MAP_FAILED) {
        return -1;
    }

    printf("Contents of region: %s\n", region);

    if (munmap(region, (size_t) file_size) != 0) {
        return -1;
    }

    return 0;

}