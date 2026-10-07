#ifndef WTS_COMMON_H
#define WTS_COMMON_H

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define P ((size_t)4096)
#define H (4 * P)

#define CHECK(c)                              \
    do {                                      \
        if (!(c))                             \
            wts_fail(__FILE__, __LINE__, #c); \
    } while (0)

_Noreturn void wts_fail(const char *, int, const char *);
unsigned char *wts_map(size_t);
unsigned char wts_byte(size_t);
void wts_fill(void *, size_t, size_t);
void wts_pattern(const void *, size_t, size_t);
void wts_write(int, const void *, size_t);
void wts_read(int, void *, size_t);
int wts_file(size_t);
void wts_child(pid_t);
void wts_fault(volatile unsigned char *, int, int);
void wts_run(void);

#endif
