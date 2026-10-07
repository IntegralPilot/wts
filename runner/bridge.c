#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <string.h>
#include <unistd.h>

struct payload {
    const unsigned char *start;
    const unsigned char *end;
};

extern const struct payload wts_payloads[];
extern const unsigned int wts_payload_count;

long wts_page_size(void)
{
    return sysconf(_SC_PAGESIZE);
}

int wts_arch_ok(void)
{
    struct utsname u;

    return uname(&u) == 0 && !strcmp(u.sysname, "Linux") &&
           !strcmp(u.machine, "aarch64");
}

int wts_extract(unsigned int index, const char *path)
{
    if (index >= wts_payload_count) {
        errno = EINVAL;
        return -1;
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0700);

    if (fd < 0) {
        return -1;
    }

    const unsigned char *p = wts_payloads[index].start;
    size_t left = (size_t)(wts_payloads[index].end - p);

    while (left) {
        ssize_t n = write(fd, p, left);

        if (n < 0 && errno == EINTR) {
            continue;
        }

        if (n <= 0) {
            int error = errno;

            close(fd);
            unlink(path);
            errno = error;
            return -1;
        }

        p += n;
        left -= (size_t)n;
    }

    return close(fd);
}
