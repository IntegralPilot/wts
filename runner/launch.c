#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <sys/personality.h>
#include <unistd.h>

#ifndef ADDR_PAGE_SIZE_4KB
#define ADDR_PAGE_SIZE_4KB 0x40000000UL
#endif

int main(int argc, char **argv)
{
    if (argc != 2 && (argc != 3 || strcmp(argv[2], "--4k"))) {
        fputs("Usage: launch test\n", stderr);
        return 2;
    }

    long page = sysconf(_SC_PAGESIZE);

    if (page < 0) {
        perror("WTS: read process page size");
        return 2;
    }

    if (page != 4096) {
        if (argc == 3) {
            fprintf(stderr, "WTS: kernel did not enable 4K process pages (got %ld)\n",
                    page);
            return 2;
        }

        int previous = personality(0xffffffffUL);

        if (previous == -1 ||
            personality((unsigned int)previous | ADDR_PAGE_SIZE_4KB) == -1) {
            perror("WTS: request 4K process pages");
            return 2;
        }

        char *args[] = {argv[0], argv[1], "--4k", NULL};

        execv("/proc/self/exe", args);
        perror("WTS: restart launcher in 4K mode");
        return 2;
    }

    char *args[] = {argv[1], NULL};

    execv(argv[1], args);
    perror("WTS: execute test");
    return 2;
}
