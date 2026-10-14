/*
 * memtrack.c
 *
 * Heap-usage tracker for tools/bench/bench.c.
 *
 * Linked with  -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free,
 *              --wrap=posix_memalign,--wrap=aligned_alloc,--wrap=memalign
 * so that every allocation made by the algorithm sources (and by the
 * harness) goes through the __wrap_* functions below. Each block carries a
 * 32-byte header {raw pointer, requested size}, which keeps the user pointer
 * 32-byte aligned regardless of the original request and lets free()/realloc()
 * recover the size. Only the requested bytes are accounted (no allocator
 * overhead), which is the number an embedded port would have to reserve.
 *
 * Tracking is switched on/off by the harness (mt_enable), so buffers that
 * belong to the harness itself (keys, messages, signatures) are not charged
 * to the algorithm.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

void *__real_malloc(size_t);
void  __real_free(void *);

#define MT_HDR 32u

typedef struct { void *raw; size_t size; } mt_hdr_t;

static int    mt_on;
static size_t mt_cur, mt_peak, mt_calls;

void   mt_enable(int on)      { mt_on = on; }
void   mt_reset(void)         { mt_cur = 0; mt_peak = 0; mt_calls = 0; }
size_t mt_peak_bytes(void)    { return mt_peak; }
size_t mt_alloc_calls(void)   { return mt_calls; }

static void *mt_alloc(size_t size, size_t align)
{
    if (align < MT_HDR) align = MT_HDR;
    char *raw = __real_malloc(size + align + MT_HDR);
    if (!raw) return NULL;
    uintptr_t u = ((uintptr_t)raw + MT_HDR + align - 1) & ~(uintptr_t)(align - 1);
    mt_hdr_t *h = (mt_hdr_t *)(u - sizeof(mt_hdr_t));
    h->raw = raw;
    h->size = size;
    if (mt_on) {
        mt_cur += size; mt_calls++;
        if (mt_cur > mt_peak) mt_peak = mt_cur;
    }
    return (void *)u;
}

static void mt_release(void *p)
{
    mt_hdr_t *h = (mt_hdr_t *)((uintptr_t)p - sizeof(mt_hdr_t));
    if (mt_on) mt_cur = (mt_cur >= h->size) ? mt_cur - h->size : 0;
    __real_free(h->raw);
}

void *__wrap_malloc(size_t n) { return mt_alloc(n, MT_HDR); }

void *__wrap_calloc(size_t a, size_t b)
{
    size_t n = a * b;
    if (a && n / a != b) return NULL;
    void *p = mt_alloc(n, MT_HDR);
    if (p) memset(p, 0, n);
    return p;
}

void __wrap_free(void *p) { if (p) mt_release(p); }

void *__wrap_realloc(void *p, size_t n)
{
    if (!p) return mt_alloc(n, MT_HDR);
    if (n == 0) { mt_release(p); return NULL; }
    mt_hdr_t *h = (mt_hdr_t *)((uintptr_t)p - sizeof(mt_hdr_t));
    void *q = mt_alloc(n, MT_HDR);
    if (!q) return NULL;
    memcpy(q, p, h->size < n ? h->size : n);
    mt_release(p);
    return q;
}

int __wrap_posix_memalign(void **out, size_t align, size_t n)
{
    void *p = mt_alloc(n, align);
    if (!p) return ENOMEM;
    *out = p;
    return 0;
}

void *__wrap_aligned_alloc(size_t align, size_t n) { return mt_alloc(n, align); }
void *__wrap_memalign(size_t align, size_t n)      { return mt_alloc(n, align); }
