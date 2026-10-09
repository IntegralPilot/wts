#include "common.h"
#include <linux/io_uring.h>
#include <sys/syscall.h>

void wts_run(void)
{
    struct io_uring_params params = {0};
    int fd = syscall(SYS_io_uring_setup, 1024, &params);

    CHECK(fd >= 0);
    CHECK(params.features & IORING_FEAT_SINGLE_MMAP);

    size_t size = params.cq_off.cqes +
                  params.cq_entries * sizeof(struct io_uring_cqe);
    size_t array_end = params.sq_off.array +
                       params.sq_entries * sizeof(unsigned);

    if (array_end > size) {
        size = array_end;
    }

    size = (size + H - 1) & ~(H - 1);

    /* Each native page must map all four slices, not just one 4K PTE. */
    unsigned char *ring = mmap(NULL, size, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, IORING_OFF_SQ_RING);

    CHECK(ring != MAP_FAILED);
    CHECK(*(unsigned *)(ring + params.sq_off.ring_entries) == params.sq_entries);
    CHECK(*(unsigned *)(ring + params.cq_off.ring_entries) == params.cq_entries);

    volatile struct io_uring_cqe *cqes = (void *)(ring + params.cq_off.cqes);

    for (unsigned i = 0; i < params.cq_entries; ++i) {
        CHECK(cqes[i].user_data == 0);
        CHECK(cqes[i].res == 0);
        CHECK(cqes[i].flags == 0);
    }

    unsigned *array = (void *)(ring + params.sq_off.array);

    for (unsigned i = 0; i < params.sq_entries; ++i) {
        CHECK(array[i] == 0);
        array[i] = params.sq_entries - 1 - i;
    }

    unsigned char *alias = mmap(NULL, size, PROT_READ, MAP_SHARED, fd,
                                IORING_OFF_SQ_RING);

    CHECK(alias != MAP_FAILED);
    CHECK(munmap(ring, size) == 0);

    volatile unsigned *shared_array = (void *)(alias + params.sq_off.array);

    for (unsigned i = 0; i < params.sq_entries; ++i) {
        CHECK(shared_array[i] == params.sq_entries - 1 - i);
    }

    CHECK(munmap(alias, size) == 0);
    CHECK(close(fd) == 0);
}
