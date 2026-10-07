#include "library.h"

#include <unistd.h>

static int initialized;
static int *unloaded;
static volatile unsigned char bss[8193];
static const unsigned char rodata[8193] = {
    [0] = 0x17,
    [4096] = 0x83,
    [8192] = 0x59,
};
static _Thread_local unsigned long tls = 17;

__attribute__((constructor)) static void initialize(void)
{
    initialized = 42;
}

__attribute__((destructor)) static void finalize(void)
{
    if (unloaded) {
        *unloaded = 1;
    }
}

static int page_size(void)
{
    return (int)sysconf(_SC_PAGESIZE);
}

static unsigned long exchange_tls(unsigned long value)
{
    unsigned long previous = tls;

    tls = value;

    return previous;
}

static void on_unload(int *result)
{
    unloaded = result;
}

const struct library_api wts_library = {
    .page_size = page_size,
    .exchange_tls = exchange_tls,
    .on_unload = on_unload,
    .initialized = &initialized,
    .bss = bss,
    .rodata = rodata,
};
