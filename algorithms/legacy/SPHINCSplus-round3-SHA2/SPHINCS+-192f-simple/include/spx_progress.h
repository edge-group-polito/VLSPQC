#ifndef SPX_PROGRESS_H
#define SPX_PROGRESS_H

#include <stdint.h>

#ifdef SPX_PROGRESS

#ifdef SPX_SPIKE
extern int putchar(int ch);

static inline void spx_progress_putc(char c)
{
    putchar((unsigned char)c);
}
#else
#include "uart.h"

static inline void spx_progress_putc(char c)
{
    char s[2];

    s[0] = c;
    s[1] = '\0';
    print_uart(s);
}
#endif

static inline void spx_progress_print(const char *s)
{
    while (*s != '\0') {
        spx_progress_putc(*s);
        s++;
    }
}

static inline void spx_progress_u32(uint32_t v)
{
    if (v >= 10U) {
        spx_progress_u32(v / 10U);
    }

    spx_progress_putc((char)('0' + (v % 10U)));
}

static inline void spx_progress_hex_u64(uint64_t v)
{
    static const char hex[] = "0123456789ABCDEF";

    spx_progress_print("0x");
    for (int i = 15; i >= 0; i--) {
        spx_progress_putc(hex[(v >> (4U * (uint32_t)i)) & 0xFU]);
    }
}

#define SPX_PROGRESS_MSG(msg) \
    do { \
        spx_progress_print("progress: " msg "\n"); \
    } while (0)

#define SPX_PROGRESS_U32(prefix, value, suffix) \
    do { \
        spx_progress_print("progress: " prefix); \
        spx_progress_u32((uint32_t)(value)); \
        spx_progress_print(suffix "\n"); \
    } while (0)

#define SPX_PROGRESS_U32_PAIR(prefix, value0, middle, value1, suffix) \
    do { \
        spx_progress_print("progress: " prefix); \
        spx_progress_u32((uint32_t)(value0)); \
        spx_progress_print(middle); \
        spx_progress_u32((uint32_t)(value1)); \
        spx_progress_print(suffix "\n"); \
    } while (0)

#else

#define SPX_PROGRESS_MSG(msg) do { } while (0)
#define SPX_PROGRESS_U32(prefix, value, suffix) do { } while (0)
#define SPX_PROGRESS_U32_PAIR(prefix, value0, middle, value1, suffix) do { } while (0)

#endif

#endif
