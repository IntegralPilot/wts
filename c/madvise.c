#include "common.h"

static void discard_anonymous(void)
{
    unsigned char *p = wts_map(2 * H);

    for (size_t page = 0; page < 8; ++page) {
        wts_fill(p, 2 * H, 0);
        CHECK(madvise(p + page * P, P, MADV_DONTNEED) == 0);

        for (size_t i = 0; i < 2 * H; ++i) {
            CHECK(p[i] == (i / P == page ? 0 : wts_byte(i)));
        }
    }

    CHECK(munmap(p, 2 * H) == 0);
}

static void discard_private_file(void)
{
    int fd = wts_file(8 * P);
    unsigned char *p =
        mmap(NULL, 4 * P, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, (off_t)P);

    CHECK(p != MAP_FAILED);

    for (size_t page = 0; page < 4; ++page) {
        memset(p, 0xa5, 4 * P);
        CHECK(madvise(p + page * P, P, MADV_DONTNEED) == 0);

        for (size_t i = 0; i < 4 * P; ++i) {
            CHECK(p[i] == (i / P == page ? wts_byte(P + i) : 0xa5));
        }
    }

    unsigned char data[8 * P];

    wts_read(fd, data, sizeof(data));
    wts_pattern(data, sizeof(data), 0);

    CHECK(munmap(p, 4 * P) == 0);
    CHECK(close(fd) == 0);
}

void wts_run(void)
{
    puts("MADV_DONTNEED: zero one anonymous 4K page, preserve its neighbours");
    discard_anonymous();

    puts("MADV_DONTNEED: restore one private file page, preserve adjacent writes");
    discard_private_file();
}
