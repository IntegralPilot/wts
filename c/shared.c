#include "common.h"

static void check_shared(const unsigned char *p, size_t offset)
{
    for (size_t i = 0; i < 4 * P; ++i) {
        unsigned char expected = wts_byte(offset + i);

        if (i >= P && i < 2 * P) {
            expected = 0x6d;
        } else if (i >= 2 * P && i < 3 * P) {
            expected = 0xb7;
        }

        CHECK(p[i] == expected);
    }
}

static void shared_mapping(int file)
{
    int fd = file ? wts_file(8 * P) : -1;
    size_t offset = file ? P : 0;
    unsigned char *a = mmap(NULL, 4 * P, PROT_READ | PROT_WRITE,
                            MAP_SHARED | (file ? 0 : MAP_ANONYMOUS), fd, (off_t)offset);

    CHECK(a != MAP_FAILED);

    unsigned char *b = a;

    if (file) {
        b = mmap(NULL, 4 * P, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)offset);

        CHECK(b != MAP_FAILED);
    } else {
        wts_fill(a, 4 * P, 0);
    }

    int ready[2], done[2];

    CHECK(pipe(ready) == 0);
    CHECK(pipe(done) == 0);

    pid_t child = fork();

    CHECK(child >= 0);

    if (!child) {
        close(ready[0]);
        close(done[1]);

        memset(a + P, 0x6d, P);
        wts_write(ready[1], "x", 1);

        char token;

        wts_read(done[0], &token, 1);
        check_shared(a, offset);
        _exit(0);
    }

    close(ready[1]);
    close(done[0]);

    char token;

    wts_read(ready[0], &token, 1);
    memset(b + 2 * P, 0xb7, P);
    wts_write(done[1], "x", 1);
    wts_child(child);

    close(ready[0]);
    close(done[1]);
    check_shared(a, offset);
    check_shared(b, offset);

    if (file) {
        CHECK(msync(b, 4 * P, MS_SYNC) == 0);
    }

    CHECK(madvise(a + P, P, MADV_DONTNEED) == 0);
    check_shared(a, offset);
    check_shared(b, offset);

    if (file) {
        unsigned char data[8 * P];

        wts_read(fd, data, sizeof(data));
        wts_pattern(data, P, 0);
        check_shared(data + P, P);
        wts_pattern(data + 5 * P, 3 * P, 5 * P);

        CHECK(munmap(b, 4 * P) == 0);
        CHECK(close(fd) == 0);
    }

    CHECK(munmap(a, 4 * P) == 0);
}

void wts_run(void)
{
    puts("MAP_SHARED: anonymous parent/child visibility and refault");
    shared_mapping(0);

    puts("MAP_SHARED: file aliases, parent/child visibility, msync and refault");
    shared_mapping(1);
}
