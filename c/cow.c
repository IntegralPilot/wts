#include "common.h"

void wts_run(void)
{
    int fd = wts_file(8 * H);

    for (int file = 0; file < 2; ++file) {
        for (size_t slice = 0; slice < 4; ++slice) {
            size_t offset = file ? slice * P : 0;
            unsigned char *p = mmap(NULL, 4 * H, PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | (file ? 0 : MAP_ANONYMOUS),
                                    file ? fd : -1, (off_t)offset);
            CHECK(p != MAP_FAILED);

            if (!file) {
                wts_fill(p, 4 * H, 0);
            }

            int ready[2], done[2];

            CHECK(pipe(ready) == 0);
            CHECK(pipe(done) == 0);

            pid_t child = fork();

            CHECK(child >= 0);

            if (!child) {
                close(ready[0]);
                close(done[1]);

                for (size_t i = P; i < 2 * P; ++i) {
                    p[i] ^= 0x5a;
                }

                wts_write(ready[1], "x", 1);

                char token;

                wts_read(done[0], &token, 1);

                for (size_t i = 0; i < 4 * H; ++i) {
                    CHECK(p[i] == (unsigned char)(wts_byte(offset + i) ^
                                                  ((i >= P && i < 2 * P) ? 0x5a : 0)));
                }

                _exit(0);
            }

            close(ready[1]);
            close(done[0]);

            char token;

            wts_read(ready[0], &token, 1);

            wts_pattern(p, 4 * H, offset);
            memset(p + 3 * P, 0xb7, P);

            wts_write(done[1], "x", 1);
            wts_child(child);

            for (size_t i = 0; i < 4 * H; ++i) {
                CHECK(p[i] == (i >= 3 * P && i < 4 * P ? 0xb7 : wts_byte(offset + i)));
            }

            close(ready[0]);
            close(done[1]);
            CHECK(munmap(p, 4 * H) == 0);
        }
    }

    unsigned char data[4096];

    for (size_t at = 0; at < 8 * H; at += P) {
        CHECK(pread(fd, data, P, (off_t)at) == (ssize_t)P);
        wts_pattern(data, P, at);
    }

    close(fd);
    puts("synchronized parent/child writes preserve adjacent anonymous and file-backed "
         "slices");
}
