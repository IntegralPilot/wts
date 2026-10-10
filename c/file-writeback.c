#include "common.h"
#include <linux/fs.h>
#include <linux/magic.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>

static void check_disk(int fd, unsigned char *data,
                       const unsigned char *expected, size_t size)
{
    CHECK(pread(fd, data, size, 0) == (ssize_t)size);

    for (size_t i = 0; i < size; ++i) {
        if (data[i] != expected[i]) {
            printf("disk byte %zu: got %u expected %u after msync\n",
                   i, data[i], expected[i]);
            CHECK(0);
        }
    }
}

static void writeback(size_t file_slice, size_t address_slice, size_t slice)
{
    const size_t size = 2 * H;
    size_t offset = file_slice * P;
    int fd = wts_file(0);
    struct statfs fs;

    CHECK(fstatfs(fd, &fs) == 0);

    if (fs.f_type == TMPFS_MAGIC || fs.f_type == RAMFS_MAGIC) {
        puts("file-writeback requires a disk-backed scratch directory with O_DIRECT");
        CHECK(0);
    }

    /* Compressed Btrfs reads can fall back to the page cache. */
    if (fs.f_type == BTRFS_SUPER_MAGIC) {
        unsigned flags;

        CHECK(ioctl(fd, FS_IOC_GETFLAGS, &flags) == 0);

        if (!(flags & FS_NOCOW_FL)) {
            flags = (flags & ~FS_COMPR_FL) | FS_NOCOMP_FL;
            CHECK(ioctl(fd, FS_IOC_SETFLAGS, &flags) == 0);
        }
    }

    unsigned char *data = wts_map(size);
    unsigned char *expected = wts_map(size);
    unsigned char *base = wts_map(size);
    unsigned char *p = base + address_slice * P;

    wts_fill(expected, size, 0);

    /* Small writes avoid large folios. */
    for (size_t at = 0; at < size; at += P) {
        wts_write(fd, expected + at, P);
    }

    CHECK(fsync(fd) == 0);

    int flags = fcntl(fd, F_GETFL);

    CHECK(flags >= 0);
    CHECK(fcntl(fd, F_SETFL, flags | O_DIRECT) == 0);

    check_disk(fd, data, expected, size);
    CHECK(mmap(p, H, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED,
               fd, (off_t)offset) == p);

    printf("writeback: file-slice=%zu address-slice=%zu rewrite-slice=%zu\n",
           file_slice, address_slice, slice);

    memset(p, 0x42, H);
    memset(expected + offset, 0x42, H);
    CHECK(msync(p, H, MS_SYNC) == 0);
    check_disk(fd, data, expected, size);

    memset(p + slice * P, 0x43, P);
    memset(expected + offset + slice * P, 0x43, P);
    CHECK(msync(p, H, MS_SYNC) == 0);
    check_disk(fd, data, expected, size);

    CHECK(munmap(base, size) == 0);
    CHECK(munmap(expected, size) == 0);
    CHECK(munmap(data, size) == 0);
    CHECK(close(fd) == 0);
}

void wts_run(void)
{
    for (size_t file_slice = 0; file_slice < 4; ++file_slice) {
        for (size_t address_slice = 0; address_slice < 4; ++address_slice) {
            for (size_t slice = 0; slice < 4; ++slice) {
                writeback(file_slice, address_slice, slice);
            }
        }
    }
}
