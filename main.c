// Copyright 2025 Northern.tech AS
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#define _GNU_SOURCE          /* needed for splice() */
#define _FILE_OFFSET_BITS 64 /* needed for working with files >4GiB */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <mtd/ubi-user.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#define BLOCK_SIZE (1024 * 1024ULL) /* 1 MiB */

#define MIN(X, Y) ((X < Y) ? X : Y)

/**
 * Choose the smaller of the sizes X and Y and limit it to SIZE_MAX if needed.
 * @note off_t is the right type for file offsets that can be greater than
 *       size_t, but also, SIZE_MAX may not fit into off_t because both types
 *       can be 64 bits and off_t is signed. I/O syscalls that manipulate data
 *       (read, write,...) use size_t for their arguments.
 */
#define MIN_SIZE(X, Y) ((size_t) MIN(((uintmax_t) SIZE_MAX), (MIN(((uintmax_t) X), ((uintmax_t) Y)))))

static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"write-everything", no_argument, 0, 'w'},
    {"input-size", required_argument, 0, 's'},
    {"fsync-interval", required_argument, 0, 'f'},
    {"input", required_argument, 0, 'i'},
    {"output", required_argument, 0, 'o'},
    {0, 0, 0, 0}};

void PrintHelp() {
    fputs(
          "Usage:\n"
          "  mender-flash [-h|--help] [-w|--write-everything] [-s|--input-size <INPUT_SIZE>] [-f|--fsync-interval <FSYNC_INTERVAL>] -i|--input <INPUT_PATH> -o|--output <OUTPUT_PATH>\n",
          stderr);
}

typedef ssize_t (*io_fn_t)(int, void*, size_t);

struct Stats {
    size_t blocks_written;
    size_t blocks_omitted;
    uint64_t bytes_written;
    uint64_t bytes_omitted;
    uint64_t total_bytes;
};

ssize_t buf_io(io_fn_t io_fn, int fd, unsigned char *buf, size_t len) {
    size_t rem = len;
    ssize_t n_done;
    do {
        n_done = io_fn(fd, buf + (len - rem), rem);
        if (n_done > 0) {
            rem -= n_done;
        }
        else if ((n_done == -1) && (errno == EINTR)) {
            continue;
        }
    } while ((n_done > 0) && (rem > 0) && (len > 0));

    if (n_done < 0) {
        return n_done;
    } else {
        return (len - rem);
    }
}

bool shovel_data(int in_fd, int out_fd, off_t len, bool write_optimized, size_t fsync_interval,
                 struct Stats *stats, int *error) {
    unsigned char buffer[BLOCK_SIZE];
    size_t n_unsynced = 0;
    while (len > 0) {
        ssize_t n_read = buf_io((io_fn_t)read, in_fd, buffer, MIN_SIZE(BLOCK_SIZE, len));
        if (n_read < 0) {
            fprintf(stderr, "Failed to read data: %m\n");
            *error = errno;
            return false;
        }
        if ((n_read == 0) && (len > 0)) {
            fprintf(stderr, "Unexpected end of input!\n");
            return false;
        }
        if (write_optimized) {
            unsigned char out_fd_buffer[BLOCK_SIZE];
            ssize_t out_fd_n_read = buf_io((io_fn_t)read, out_fd, out_fd_buffer, MIN_SIZE(BLOCK_SIZE, len));
            if (out_fd_n_read < 0) {
                fprintf(stderr, "Failed to read data from the target: %m\n");
                *error = errno;
                return false;
            }
            if ((n_read == out_fd_n_read) &&
                (memcmp(buffer, out_fd_buffer, n_read) == 0)) {
                stats->blocks_omitted++;
                stats->total_bytes += n_read;
                len -= n_read;
                continue;
            } else {
                if (lseek(out_fd, -out_fd_n_read, SEEK_CUR) == -1) {
                    fprintf(stderr, "Failed to seek on the target: %m\n");
                    *error = errno;
                    return false;
                }
            }
        }
        ssize_t n_written = buf_io((io_fn_t)write, out_fd, buffer, n_read);
        if (n_written != n_read) {
            fprintf(stderr, "Failed to write data: %m\n");
            *error = errno;
            return false;
        }
        stats->total_bytes += n_read;
        stats->blocks_written++;
        stats->bytes_written += n_written;
        if (fsync_interval != 0) {
            n_unsynced += n_written;
            if (n_unsynced >= fsync_interval) {
                if (fsync(out_fd) == -1) {
                    fprintf(stderr, "warning: Failed to fsync data to target: %m\n");
                }
                n_unsynced = 0;
            }
        }
        len -= n_read;
    }

    if ((fsync_interval != 0) && (n_unsynced >= fsync_interval)) {
        if (fsync(out_fd) == -1) {
            fprintf(stderr, "warning: Failed to fsync data to target: %m\n");
        }
    }
    return true;
}

/**
 * @return 1 if true, 0 if false, -1 in case of an error
 */
int is_ubi_device(char *path) {
    struct stat sb;
    if ((stat(path, &sb) == -1) && (errno != ENOENT)) {
        fprintf(stderr, "Failed to stat() output '%s': %m\n", path);
        return -1;
    }
    if (!S_ISCHR(sb.st_mode)) {
        return 0;
    }

    char *sysfs_ubi_dir;
    if (asprintf(&sysfs_ubi_dir, "/sys/class/ubi/%s", basename(path)) == -1) {
        fprintf(stderr, "Cannot check if output '%s' is a UBI device: %m\n", path);
        return -1;
    }

    struct stat sysfs_dir_stat;
    int ret;
    if (stat(sysfs_ubi_dir, &sysfs_dir_stat) == 0) {
        ret = 1;
    } else if (errno == ENOENT) {
        ret = 0;
    } else {
        fprintf(stderr, "Failed to check if output '%s' is a UBI device: %m\n", path);
        ret = -1;
    }
    free(sysfs_ubi_dir);
    return ret;
}

#ifdef __linux__
/* Same signature as sendfile() so that we can treat the same (see comment about
 * splice() and sendfile() below). */
ssize_t splice_sendfile(int out_fd, int in_fd, __attribute__(( unused )) off_t *offset, size_t count) {
    return splice(in_fd, 0, out_fd, 0, count, 0);
}
#endif  /* __linux__ */

int main(int argc, char *argv[]) {
    char *input_path = NULL;
    char *output_path = NULL;
    uint64_t volume_size = 0;
    bool write_optimized = true;
    off_t fsync_interval = BLOCK_SIZE;

    int option_index = 0;
    int c = getopt_long(argc, argv, "hws:f:i:o:", long_options, &option_index);
    while (c != -1) {
        switch (c) {
        case 'h':
            PrintHelp();
            return 0;

        case 'i':
            input_path = optarg;
            break;

        case 'o':
            output_path = optarg;
            break;

        case 's': {
            char *end = optarg;
            long long ret = strtoll(optarg, &end, 10);
            if ((ret == 0) || (*end != '\0')) {
                fprintf(stderr, "Invalid input size given: %s\n", optarg);
                return EXIT_FAILURE;
            } else {
                volume_size = ret;
            }
            break;
        }

        case 'f': {
            char *end = optarg;
            unsigned long long ret = strtoull(optarg, &end, 10);
            if (((ret == 0) && (strcmp(optarg, "0") != 0)) || (*end != '\0')) {
                fprintf(stderr, "Invalid fsync interval given: %s\n", optarg);
                return EXIT_FAILURE;
            } else {
                fsync_interval = ret;
            }
            break;
        }

        case 'w':
            write_optimized = false;
            break;

        default:
            PrintHelp();
            return EXIT_FAILURE;
        }
        c = getopt_long(argc, argv, "hws:i:o:", long_options, &option_index);
    }

    if ((input_path == NULL) || (output_path == NULL)) {
        fprintf(stderr, "Wrong input parameters!\n");
        PrintHelp();
        return EXIT_FAILURE;
    }

    int in_fd;
    if (strcmp(input_path, "-") == 0) {
        in_fd = STDIN_FILENO;
    } else {
        in_fd = open(input_path, O_RDONLY);
        if (in_fd == -1) {
            fprintf(stderr, "Failed to open '%s' for reading: %m\n", input_path);
            return EXIT_FAILURE;
        }
    }

    struct stat in_fd_stat;
    if (fstat(in_fd, &in_fd_stat) == -1) {
        close(in_fd);
        fprintf(stderr, "Failed to stat() input '%s': %m\n", input_path);
        return EXIT_FAILURE;
    }

    int is_ubi = is_ubi_device(output_path);
    if (is_ubi == -1) {
        /* error already logged */
        close(in_fd);
        return EXIT_FAILURE;
    } else if (is_ubi == 1) {
        write_optimized = false;
        fsync_interval = 0;
    }

    int out_fd;
    if (write_optimized) {
        out_fd = open(output_path, O_CREAT | O_RDWR, 0600);
    } else {
        out_fd = open(output_path, O_CREAT | O_WRONLY, 0600);
    }
    if (out_fd == -1) {
        fprintf(stderr, "Failed to open '%s' for writing: %m\n", output_path);
        close(in_fd);
        return EXIT_FAILURE;
    }

    if ((is_ubi == 1) && (ioctl(out_fd, UBI_IOCVOLUP, &volume_size) == -1)) {
        fprintf(stderr, "Failed to setup UBI volume '%s': %m\n", output_path);
        close(in_fd);
        close(out_fd);
        return EXIT_FAILURE;
    }

    size_t len;
    if (volume_size != 0) {
        len = volume_size;
    } else {
        if (in_fd_stat.st_size == 0) {
            fprintf(stderr, "Input size not specified and cannot be determined from stat()\n");
            close(in_fd);
            close(out_fd);
            return EXIT_FAILURE;
        } else {
            len = in_fd_stat.st_size;
        }
    }

    struct Stats stats = {0};
    bool success = false;
    int error = 0;

#ifndef __linux__
    /* Nothing better available for non-Linux platforms (for now). */
    success = shovel_data(in_fd, out_fd, len, write_optimized, fsync_interval, &stats, &error);
#else  /* __linux__ */
    /* The fancy syscalls below don't support write-optimized approach or
       UBI devices so we cannot use them for that. */
    if ((is_ubi == 1) || write_optimized) {
        success = shovel_data(in_fd, out_fd, len, write_optimized, fsync_interval, &stats, &error);
    } else {
        /***
            On Linux the splice() and sendfile() syscalls can be useful for us (see
            their descriptions taken from the respective man pages below), on other
            operating systems there might be functions with the same names, but
            potentially doing something completely different.

            splice() moves  data  between two file descriptors without copying be‐
            tween kernel address space and user address space.  It transfers up  to
            len bytes of data from the file descriptor fd_in to the file descriptor
            fd_out, where one of the file descriptors must refer to a pipe.

            sendfile()  copies  data  between one file descriptor and another.  Be‐
            cause this copying is done within the kernel, sendfile() is more  effi‐
            cient than the combination of read(2) and write(2), which would require
            transferring data to and from user space.
            The   in_fd   argument   must  correspond  to  a  file  which  supports
            mmap(2)-like operations (i.e., it cannot be a socket or a pipe).
        ***/
        ssize_t (*sendfile_fn)(int out_fd, int in_fd, off_t *offset, size_t count);
        if (S_ISFIFO(in_fd_stat.st_mode)) {
            sendfile_fn = splice_sendfile;
        } else {
            sendfile_fn = sendfile;
        }

        ssize_t ret;
        off_t n_unsynced = 0;
        size_t chunk_size;
        if (fsync_interval != 0) {
            chunk_size = MIN_SIZE(len, fsync_interval);
        } else {
            chunk_size = len;
        }
        do {
            ret = sendfile_fn(out_fd, in_fd, 0, chunk_size);
            if (ret > 0) {
                len -= ret;
                stats.total_bytes += ret;
                n_unsynced += ret;
                if ((fsync_interval != 0) && (n_unsynced >= fsync_interval)) {
                    fsync(out_fd);
                    n_unsynced = 0;
                }
            }
        } while ((ret > 0) && (len > 0));
        success = ((ret == 0) || ((ret > 0) && (len == 0)));
        error = errno;
    }
#endif  /* __linux__ */

    close(in_fd);
    /* close() doesn't guarantee a sync, but we should allow using
       --fsync-interval=0 to disable fsync completely. */
    if (fsync_interval != 0) {
        fsync(out_fd);
    }
    close(out_fd);

    if (!success) {
        if (error != 0) {
            fprintf(stderr, "Failed to copy data: %s\n", strerror(error));
            printf("Total bytes written: %ju\n", (intmax_t) stats.total_bytes);
        } else {
            fprintf(stderr, "Failed to copy data\n");
            printf("Total bytes written: %ju\n", (intmax_t) stats.total_bytes);
        }
        return EXIT_FAILURE;
    } else {
        if (write_optimized) {
            puts("================ STATISTICS ================");
            printf("Blocks written: %10zu\n", stats.blocks_written);
            printf("Blocks omitted: %10zu\n", stats.blocks_omitted);
            printf("Bytes written: %11ju\n", (intmax_t) stats.bytes_written);
            printf("Total bytes: %13ju\n", (intmax_t) stats.total_bytes);
            puts("============================================");
        } else {
            printf("Total bytes written: %ju\n", (intmax_t) stats.total_bytes);
        }
    }

    return 0;
}
