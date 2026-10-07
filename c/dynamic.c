#include "common.h"
#include "library.h"

#include <dlfcn.h>
#include <pthread.h>

static const struct library_api *library;
static pthread_mutex_t ready = PTHREAD_MUTEX_INITIALIZER;

static void *worker(void *arg)
{
    unsigned long value = (uintptr_t)arg;

    CHECK(pthread_mutex_lock(&ready) == 0);
    CHECK(pthread_mutex_unlock(&ready) == 0);

    CHECK(library->page_size() == 4096);
    CHECK(library->exchange_tls(value) == 17);

    for (int i = 0; i < 1000; ++i) {
        CHECK(library->exchange_tls(value + 1) == value);
        ++value;
    }

    return NULL;
}

static void run_library(int mode)
{
    pthread_t threads[4];

    CHECK(pthread_mutex_lock(&ready) == 0);

    /* Existing threads must acquire TLS for a library loaded later. */
    for (uintptr_t i = 0; i < 4; ++i) {
        CHECK(pthread_create(&threads[i], NULL, worker, (void *)(i + 100)) == 0);
    }

#ifdef WTS_DLOPEN
    void *handle = dlopen("libwts-test.so", mode | RTLD_LOCAL);

    if (!handle) {
        puts(dlerror());
    }

    CHECK(handle != NULL);

    dlerror();
    library = dlsym(handle, "wts_library");

    const char *error = dlerror();

    if (error) {
        puts(error);
    }

    CHECK(error == NULL && library != NULL);
#else
    (void)mode;
    library = &wts_library;
#endif

    CHECK(*library->initialized == 42);

    for (size_t i = 0; i < 8193; ++i) {
        CHECK(library->bss[i] == 0);
        library->bss[i] = wts_byte(i);
    }

    const volatile unsigned char *ro = library->rodata;

    CHECK(ro[0] == 0x17 && ro[P] == 0x83 && ro[2 * P] == 0x59);
    wts_fault((volatile unsigned char *)(uintptr_t)ro, 1, SIGSEGV);

    CHECK(library->exchange_tls(99) == 17);

    CHECK(pthread_mutex_unlock(&ready) == 0);

    for (size_t i = 0; i < 4; ++i) {
        CHECK(pthread_join(threads[i], NULL) == 0);
    }

    CHECK(library->exchange_tls(17) == 99);

#ifdef WTS_DLOPEN
    int unloaded = 0;

    library->on_unload(&unloaded);
    CHECK(dlclose(handle) == 0);
    library = NULL;

    CHECK(unloaded == 1);
#endif
}

void wts_run(void)
{
    run_library(RTLD_LAZY);

#ifdef WTS_DLOPEN
    run_library(RTLD_NOW);
    puts(
        "dlopen: lazy/eager binding, relocations, constructors, TLS and unload/reload");
#else
    puts("startup linking: relocations, constructors, BSS, read-only data and TLS");
#endif
}
