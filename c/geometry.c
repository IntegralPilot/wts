#include "common.h"

void wts_run(void)
{
    CHECK(getpagesize() == 4096);

    unsigned char *p = wts_map(2 * H);

    wts_fill(p, 2 * H, 0);

    for (size_t slice = 0; slice < 8; ++slice) {
        unsigned char *q = p + slice * P;

        CHECK(mprotect(q, P, PROT_NONE) == 0);
        wts_fault(q, 0, SIGSEGV);

        if (slice) {
            wts_pattern(q - P, P, (slice - 1) * P);
        }

        if (slice < 7) {
            wts_pattern(q + P, P, (slice + 1) * P);
        }

        CHECK(mprotect(q, P, PROT_READ) == 0);
        wts_pattern(q, P, slice * P);
        wts_fault(q + P - 1, 1, SIGSEGV);
        CHECK(mprotect(q, P, PROT_READ | PROT_WRITE) == 0);
    }

    CHECK(munmap(p + P, P) == 0);
    wts_fault(p + P, 0, SIGSEGV);

    CHECK(mmap(p + P, P, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == p + P);

    for (size_t i = P; i < 2 * P; ++i) {
        CHECK(p[i] == 0);
    }

    wts_pattern(p, P, 0);
    wts_pattern(p + 2 * P, 6 * P, 2 * P);

    CHECK(munmap(p, 2 * H) == 0);

    puts("4K auxv/sysconf, eight protection boundaries, unmap and exact hole reuse");
}
