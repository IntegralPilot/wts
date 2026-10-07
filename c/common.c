#include "common.h"
#include <sys/utsname.h>

_Noreturn void wts_fail(const char *file, int line, const char *condition)
{
    int error = errno;

    fprintf(stdout, "%s:%d: %s (errno=%d: %s)\n", file, line, condition, error,
            strerror(error));
    fflush(stdout);
    exit(1);
}

unsigned char *wts_map(size_t size)
{
    CHECK(size && size % P == 0);

    unsigned char *raw = mmap(NULL, size + H, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(raw != MAP_FAILED);

    uintptr_t aligned = ((uintptr_t)raw + H - 1) & ~(H - 1);
    size_t prefix = aligned - (uintptr_t)raw;

    if (prefix) {
        CHECK(munmap(raw, prefix) == 0);
    }

    CHECK(munmap((void *)(aligned + size), H - prefix) == 0);

    return (unsigned char *)aligned;
}

unsigned char wts_byte(size_t i)
{
    return (unsigned char)((i / P * 37 + i % 251) % 256);
}

void wts_fill(void *ptr, size_t size, size_t offset)
{
    unsigned char *p = ptr;

    for (size_t i = 0; i < size; ++i) {
        p[i] = wts_byte(offset + i);
    }
}

void wts_pattern(const void *ptr, size_t size, size_t offset)
{
    const unsigned char *p = ptr;

    for (size_t i = 0; i < size; ++i) {
        if (p[i] != wts_byte(offset + i)) {
            printf("byte %zu: got %u expected %u (pattern offset %zu)\n", i, p[i],
                   wts_byte(offset + i), offset);
            CHECK(0);
        }
    }
}

void wts_write(int fd, const void *ptr, size_t size)
{
    const unsigned char *p = ptr;

    while (size) {
        ssize_t n = write(fd, p, size);

        if (n < 0 && errno == EINTR) {
            continue;
        }

        CHECK(n > 0);

        p += n;
        size -= (size_t)n;
    }
}

void wts_read(int fd, void *ptr, size_t size)
{
    unsigned char *p = ptr;

    while (size) {
        ssize_t n = read(fd, p, size);

        if (n < 0 && errno == EINTR) {
            continue;
        }

        CHECK(n > 0);

        p += n;
        size -= (size_t)n;
    }
}

int wts_file(size_t size)
{
    const char *dir = getenv("WTS_TMPDIR");

    CHECK(dir != NULL);

    char *path = NULL;

    CHECK(asprintf(&path, "%s/data-XXXXXX", dir) > 0);

    int fd = mkstemp(path);

    CHECK(fd >= 0);
    CHECK(unlink(path) == 0);
    free(path);

    unsigned char buffer[4096];

    for (size_t at = 0; at < size; at += sizeof(buffer)) {
        size_t n = size - at < sizeof(buffer) ? size - at : sizeof(buffer);

        wts_fill(buffer, n, at);
        wts_write(fd, buffer, n);
    }

    CHECK(lseek(fd, 0, SEEK_SET) == 0);

    return fd;
}

void wts_child(pid_t pid)
{
    int status;

    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

void wts_fault(volatile unsigned char *p, int write_access, int expected_signal)
{
    fflush(NULL);

    pid_t pid = fork();

    CHECK(pid >= 0);

    if (!pid) {
        signal(SIGSEGV, SIG_DFL);
        signal(SIGBUS, SIG_DFL);

        if (write_access) {
            *p = 0x42;
        } else {
            volatile unsigned char value = *p;

            (void)value;
        }

        _exit(99);
    }

    int status;

    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == expected_signal);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    struct rlimit limit = {0, 0};

    CHECK(setrlimit(RLIMIT_CORE, &limit) == 0);

    struct utsname name;

    CHECK(uname(&name) == 0);

    long page = sysconf(_SC_PAGESIZE);
    unsigned long auxv_page = getauxval(AT_PAGESZ);

    printf("test page-size=%ld auxv-page-size=%lu\n", page, auxv_page);

    if (strcmp(name.machine, "aarch64") || page != 4096 || auxv_page != 4096) {
        puts("Requires AArch64 Linux with 4096-byte process pages");
        return 78;
    }

    wts_run();
    puts("all checks passed");

    return 0;
}
