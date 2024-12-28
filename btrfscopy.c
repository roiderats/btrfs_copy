/*

  vers. 2018-12-06 19:06 Try to seek back and wait when in error. Known not to work
  
  TODO: try few times, seek forward

*/

/* Code optimization and readability improvements for btrfscopy.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <linux/fs.h>
#include <linux/btrfs.h>

#define ERROR_EXIT(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define CHUNK_SIZE 4096

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <source_file> <target_file>\n", prog_name);
}

int open_file(const char *path, int flags, mode_t mode) {
    int fd = open(path, flags, mode);
    if (fd == -1) ERROR_EXIT("Failed to open file");
    return fd;
}

void close_file(int fd) {
    if (close(fd) == -1) ERROR_EXIT("Failed to close file");
}

ssize_t read_data(int fd, void *buffer, size_t count) {
    ssize_t bytes_read = read(fd, buffer, count);
    if (bytes_read == -1) ERROR_EXIT("Failed to read data");
    return bytes_read;
}

ssize_t write_data(int fd, const void *buffer, size_t count) {
    ssize_t bytes_written = write(fd, buffer, count);
    if (bytes_written == -1) ERROR_EXIT("Failed to write data");
    return bytes_written;
}

void clone_btrfs(int src_fd, int dest_fd) {
    struct btrfs_ioctl_clone_range_args args = {
        .src_fd = src_fd,
        .src_offset = 0,
        .src_length = 0,
        .dest_offset = 0
    };

    if (ioctl(dest_fd, BTRFS_IOC_CLONE_RANGE, &args) == -1) {
        if (errno == EOPNOTSUPP) {
            fprintf(stderr, "Btrfs clone ioctl not supported; falling back to regular copy.\n");
        } else {
            ERROR_EXIT("Clone ioctl failed");
        }
    }
}

void fallback_copy(int src_fd, int dest_fd) {
    char buffer[CHUNK_SIZE];
    ssize_t bytes;

    while ((bytes = read_data(src_fd, buffer, CHUNK_SIZE)) > 0) {
        write_data(dest_fd, buffer, bytes);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *source_path = argv[1];
    const char *target_path = argv[2];

    int src_fd = open_file(source_path, O_RDONLY, 0);
    int dest_fd = open_file(target_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    clone_btrfs(src_fd, dest_fd);
    fallback_copy(src_fd, dest_fd);

    close_file(src_fd);
    close_file(dest_fd);

    return EXIT_SUCCESS;
}
