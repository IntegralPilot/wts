#include "common.h"
#include <pthread.h>

static volatile unsigned char bss[65539];
static const unsigned char rodata[8193] = {[0] = 0x17, [4096] = 0x83, [8192] = 0x59};
static _Thread_local volatile uint64_t tls = UINT64_C(0x123456789abcdef0);
static volatile int constructed;

__attribute__((constructor)) static void init(void)
{
    constructed = 42;
}

static void *worker(void *arg)
{
    uintptr_t id = (uintptr_t)arg;

    CHECK(tls == UINT64_C(0x123456789abcdef0));
    tls ^= id;

    unsigned char *p = wts_map(4 * H);

    for (int round = 0; round < 32; ++round) {
        wts_fill(p, 4 * H, id + (size_t)round);

        for (size_t slice = 0; slice < 4; ++slice) {
            CHECK(mprotect(p + slice * P, P, PROT_READ) == 0);
            wts_pattern(p, 4 * H, id + (size_t)round);
            CHECK(mprotect(p + slice * P, P, PROT_READ | PROT_WRITE) == 0);
        }

        CHECK(tls == (UINT64_C(0x123456789abcdef0) ^ id));
    }

    munmap(p, 4 * H);

    return NULL;
}

void wts_run(void)
{
    CHECK(constructed == 42);

    for (size_t i = 0; i < sizeof(bss); ++i) {
        CHECK(bss[i] == 0);
    }

    const volatile unsigned char *ro = rodata;

    CHECK(ro[0] == 0x17 && ro[4096] == 0x83 && ro[8192] == 0x59);
    wts_fault((volatile unsigned char *)(uintptr_t)rodata, 1, SIGSEGV);

    pthread_t threads[4];

    for (uintptr_t i = 0; i < 4; ++i) {
        CHECK(pthread_create(&threads[i], NULL, worker, (void *)(i + 1)) == 0);
    }

    for (size_t i = 0; i < 4; ++i) {
        CHECK(pthread_join(threads[i], NULL) == 0);
    }

    CHECK(tls == UINT64_C(0x123456789abcdef0));

    puts("ELF constructors, BSS tail, read-only segment, TLS and concurrent 4K "
         "protection changes");
}
