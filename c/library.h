#ifndef WTS_LIBRARY_H
#define WTS_LIBRARY_H

struct library_api {
    int (*page_size)(void);
    unsigned long (*exchange_tls)(unsigned long);
    void (*on_unload)(int *);
    const int *initialized;
    volatile unsigned char *bss;
    const unsigned char *rodata;
};

extern const struct library_api wts_library;

#endif
