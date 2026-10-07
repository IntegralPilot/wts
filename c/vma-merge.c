#include "common.h"

static void extend_backwards(int shared, int prefault, size_t file_slice,
                             size_t address_slice)
{
    const size_t length = 8 * H;
    const size_t start = 15 * P;
    size_t offset = file_slice * P;

    printf("backwards merge: shared=%d prefault=%d file-slice=%zu address-slice=%zu\n",
           shared, prefault, file_slice, address_slice);

    int fd = memfd_create("wts-vma-merge", MFD_CLOEXEC);

    CHECK(fd >= 0);

    unsigned char buffer[4096];

    for (size_t at = 0; at < length + H; at += P) {
        wts_fill(buffer, P, at);
        wts_write(fd, buffer, P);
    }

    unsigned char *base = wts_map(length + H);
    unsigned char *p = base + address_slice * P;

    CHECK(mmap(p, length, PROT_NONE,
               MAP_FIXED | MAP_NORESERVE | (shared ? MAP_SHARED : MAP_PRIVATE),
               fd, (off_t)offset) == p);

    CHECK(mprotect(p + 16 * P, 2 * P, PROT_READ | PROT_WRITE) == 0);

    if (prefault) {
        p[16 * P] = wts_byte(offset + 16 * P);
    }

    /* Leave PROT_NONE below this range so only the upper VMA can merge. */
    CHECK(mprotect(p + start, P, PROT_READ | PROT_WRITE) == 0);
    wts_pattern(p + start, 3 * P, offset + start);

    unsigned char original = wts_byte(offset + start);
    unsigned char changed = original ^ 0xff;
    unsigned char expected = shared ? changed : original;
    unsigned char stored;

    p[start] = changed;

    CHECK(pread(fd, &stored, 1, (off_t)(offset + start)) == 1);
    CHECK(stored == expected);

    CHECK(madvise(p + start, 3 * P, MADV_DONTNEED) == 0);
    CHECK(p[start] == expected);
    wts_pattern(p + start + 1, 3 * P - 1, offset + start + 1);

    CHECK(munmap(base, length + H) == 0);
    CHECK(close(fd) == 0);
}

void wts_run(void)
{
    for (int shared = 0; shared < 2; ++shared) {
        for (int prefault = 0; prefault < 2; ++prefault) {
            for (size_t file_slice = 0; file_slice < 4; ++file_slice) {
                for (size_t address_slice = 0; address_slice < 4; ++address_slice) {
                    extend_backwards(shared, prefault, file_slice, address_slice);
                }
            }
        }
    }
}
