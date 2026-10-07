#include "common.h"

static void resize(void)
{
    unsigned char *p = wts_map(8 * P);

    wts_fill(p, 3 * P, 0);
    CHECK(munmap(p + 3 * P, 5 * P) == 0);

    CHECK(mremap(p, 3 * P, 5 * P, 0) == p);
    wts_pattern(p, 3 * P, 0);

    for (size_t i = 3 * P; i < 5 * P; ++i) {
        CHECK(p[i] == 0);
    }

    wts_fill(p, 5 * P, 0);
    CHECK(mremap(p, 5 * P, 2 * P, 0) == p);
    wts_pattern(p, 2 * P, 0);
    wts_fault(p + 2 * P, 0, SIGSEGV);

    CHECK(mremap(p, 2 * P, 5 * P, 0) == p);
    wts_pattern(p, 2 * P, 0);

    for (size_t i = 2 * P; i < 5 * P; ++i) {
        CHECK(p[i] == 0);
    }

    CHECK(munmap(p, 5 * P) == 0);
}

static void move_and_grow(void)
{
    for (size_t slice = 0; slice < 4; ++slice) {
        unsigned char *base = wts_map(2 * H);
        size_t at = slice * P;
        unsigned char *p = base + at;

        wts_fill(base, 2 * H, 0);

        /* Keep the next page mapped so growth must move the allocation. */
        CHECK(mprotect(p + 3 * P, P, PROT_READ) == 0);

        unsigned char *q = mremap(p, 3 * P, 5 * P, MREMAP_MAYMOVE);

        CHECK(q != MAP_FAILED && q != p);
        wts_pattern(q, 3 * P, at);

        for (size_t i = 3 * P; i < 5 * P; ++i) {
            CHECK(q[i] == 0);
        }

        wts_fault(p, 0, SIGSEGV);
        wts_pattern(base, at, 0);
        wts_pattern(p + 3 * P, 2 * H - at - 3 * P, at + 3 * P);

        CHECK(munmap(q, 5 * P) == 0);
        CHECK(munmap(base, 2 * H) == 0);
    }
}

static void move_fixed(void)
{
    for (size_t slice = 0; slice < 4; ++slice) {
        unsigned char *source = wts_map(2 * H);
        unsigned char *dest = wts_map(2 * H);
        size_t at = slice * P;
        size_t end = at + 3 * P;

        wts_fill(source, 2 * H, 0);
        wts_fill(dest, 2 * H, 2 * H);

        /* Warthog requires matching offsets within the native 16K page. */
        CHECK(mremap(source + at, 3 * P, 3 * P, MREMAP_MAYMOVE | MREMAP_FIXED,
                     dest + at) == dest + at);

        wts_pattern(dest + at, 3 * P, at);
        wts_fault(source + at, 0, SIGSEGV);

        memset(dest + at, 0x6d, 3 * P);
        wts_pattern(source, at, 0);
        wts_pattern(source + end, 2 * H - end, end);
        wts_pattern(dest, at, 2 * H);
        wts_pattern(dest + end, 2 * H - end, 2 * H + end);

        CHECK(munmap(source, 2 * H) == 0);
        CHECK(munmap(dest, 2 * H) == 0);
    }
}

void wts_run(void)
{
    puts("mremap: grow, shrink and regrow in place");
    resize();

    puts("mremap: move while growing at all four slice offsets");
    move_and_grow();

    puts("mremap: replace a destination while preserving adjacent pages");
    move_fixed();
}
