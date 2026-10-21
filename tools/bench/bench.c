/*
 * bench.c
 *
 * Single-operation benchmark harness for the NIST-style signature API
 *   crypto_sign_keypair / crypto_sign / crypto_sign_open
 * Built against a flattened static-test folder (vectors/static/<variant>/, see
 * tools/gen_static_test.py) with that folder's main.c left out; driven by
 * tools/bench/run_bench.py. One process measures ONE operation
 * (keygen | sign | verify) so the memory figures are not polluted by the
 * other operations.
 *
 * Reported per operation (one JSON object on stdout):
 *   - wall time per call (ns): min / median / mean / stddev over N calls
 *   - TSC cycles per call (median; x86 only, 0 elsewhere)
 *   - heap_peak : peak bytes live inside malloc/calloc/realloc (tools/bench/memtrack.c)
 *   - stack_peak: deepest stack use, from the pages touched in a dedicated
 *                 mmap'ed thread stack (mincore, page granularity, minus the
 *                 thread-descriptor pages of an empty thread)
 *   - vmhwm_kb  : peak resident set of the whole process (VmHWM) during the op
 *   - pk/sk/sig sizes (CRYPTO_*BYTES and the measured signature length)
 *
 * Compile-time macros (set by run_bench.py):
 *   BENCH_RNG_HDR   header declaring randombytes_init() (e.g. "rng.h");
 *                   not needed when BENCH_SHAPE_HDR is used
 *   BENCH_SHAPE_HDR optional header (tools/bench/shapes/*.h) that replaces the
 *                   plain NIST signature API calls (keygen / sign / verify) by
 *                   the API of another registry shape, e.g. KEMs (sign = encaps,
 *                   verify = decaps) or the explicit-randomness signature APIs
 *   BENCH_LEN_T     type of smlen/mlen in this variant's API
 *                   (unsigned long long or size_t)
 *   BENCH_NAME      variant name for the report
 * Environment:
 *   BENCH_TIME_S    wall-clock budget per op in seconds (default 2.0)
 *   BENCH_MIN_ITERS (default 5)   BENCH_MAX_ITERS (default 1000)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sched.h>
#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#define BENCH_CYCLES() ((uint64_t)__rdtsc())
#else
#define BENCH_CYCLES() ((uint64_t)0)
#endif

#ifdef BENCH_RNG_HDR
#include BENCH_RNG_HDR
#endif
#include "api.h"

#ifndef BENCH_NAME
#define BENCH_NAME "unknown"
#endif
#ifndef BENCH_LEN_T
#define BENCH_LEN_T unsigned long long
#endif

#define MSG_LEN        1000u                 /* 'hello world ' repeated, as in the reference paper */
#define STACK_BYTES    ((size_t)1 << 30)     /* 1 GiB virtual, only touched pages become resident */

void   mt_enable(int on);
void   mt_reset(void);
size_t mt_peak_bytes(void);
size_t mt_alloc_calls(void);

typedef enum { OP_KEYGEN, OP_SIGN, OP_VERIFY } op_t;

static op_t    g_op;
static uint8_t g_msg[MSG_LEN];
static uint8_t *g_pk, *g_sk, *g_sm, *g_m2;   /* g_m2: verify output buffer, allocated untracked in prep */
static BENCH_LEN_T g_smlen;
static double  g_budget = 2.0;
static int     g_min_iters = 5, g_max_iters = 1000;

/* results of the measured thread */
static double   *g_ns;
static uint64_t *g_cyc;
static int       g_n;
static int       g_fail;

static uint64_t now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}

#ifndef BENCH_SHAPE_HDR
static void seed_drbg(uint8_t tag)
{
    uint8_t seed[48];
    for (int i = 0; i < 48; i++) seed[i] = (uint8_t)(i + tag);
    randombytes_init(seed, NULL, 256);
}
#define BENCH_SEED(tag)   seed_drbg(tag)
#define BENCH_SIG_BYTES   CRYPTO_BYTES
static int bench_keygen(void) { return crypto_sign_keypair(g_pk, g_sk); }
static int bench_sign(BENCH_LEN_T *n) { return crypto_sign(g_sm, n, g_msg, MSG_LEN, g_sk); }
static int bench_verify(void)
{
    BENCH_LEN_T mlen2 = 0;
    int r = crypto_sign_open(g_m2, &mlen2, g_sm, g_smlen, g_pk);
    if (r == 0 && (mlen2 != MSG_LEN || memcmp(g_m2, g_msg, MSG_LEN))) r = -2;
    return r;
}
#else
/* Cheap xorshift filler for the explicit randomness inputs of the derand APIs
 * (new bytes for every call; the exact distribution does not matter for timing). */
static uint64_t bs_state = 0x9e3779b97f4a7c15ull;
static void bs_fill(uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        bs_state ^= bs_state << 13; bs_state ^= bs_state >> 7; bs_state ^= bs_state << 17;
        p[i] = (uint8_t)(bs_state >> 24);
    }
}
#include BENCH_SHAPE_HDR
#endif

static void do_op(op_t op)
{
    BENCH_LEN_T n = 0;
    int r = 0;
    switch (op) {
    case OP_KEYGEN: r = bench_keygen(); break;
    case OP_SIGN:   r = bench_sign(&n); break;
    case OP_VERIFY: r = bench_verify(); break;
    }
    if (r != 0) g_fail = r;
}

static int cmp_d(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}
static int cmp_u(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static void *measure_thread(void *arg)
{
    (void)arg;
    g_ns  = malloc(sizeof(double)   * (size_t)g_max_iters);   /* not tracked: mt off */
    g_cyc = malloc(sizeof(uint64_t) * (size_t)g_max_iters);

    mt_reset();
    mt_enable(1);

    /* first call doubles as warm-up and as the estimate for the iteration count */
    uint64_t t0 = now_ns();
    do_op(g_op);
    double first = (double)(now_ns() - t0) * 1e-9;
    int n = (int)(g_budget / (first > 1e-9 ? first : 1e-9));
    if (n < g_min_iters) n = g_min_iters;
    if (n > g_max_iters) n = g_max_iters;

    for (int i = 0; i < n; i++) {
        uint64_t c0 = BENCH_CYCLES();
        uint64_t a  = now_ns();
        do_op(g_op);
        uint64_t b  = now_ns();
        uint64_t c1 = BENCH_CYCLES();
        g_ns[i]  = (double)(b - a);
        g_cyc[i] = c1 - c0;
    }
    g_n = n;
    mt_enable(0);
    return NULL;
}

/* Run fn on a fresh mmap'ed stack; returns the stack bytes actually touched
 * (page granularity) via *peak. */
static void run_on_stack(void *(*fn)(void *), size_t *peak)
{
    char *stk = mmap(NULL, STACK_BYTES, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_STACK, -1, 0);
    if (stk == MAP_FAILED) { perror("mmap"); exit(2); }
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstack(&at, stk, STACK_BYTES);
    pthread_t th;
    if (pthread_create(&th, &at, fn, NULL)) { perror("pthread_create"); exit(2); }
    pthread_join(th, NULL);

    size_t pg = (size_t)sysconf(_SC_PAGESIZE), npg = STACK_BYTES / pg;
    unsigned char *vec = malloc(npg);
    size_t lowest = npg;
    if (mincore(stk, STACK_BYTES, vec) == 0) {
        for (size_t i = 0; i < npg; i++)
            if (vec[i] & 1) { lowest = i; break; }
    }
    free(vec);
    *peak = (lowest == npg) ? 0 : (npg - lowest) * pg;
    munmap(stk, STACK_BYTES);
}

static void *nop_thread(void *arg) { (void)arg; return NULL; }

static void *prep_thread(void *arg)
{
    (void)arg;
    g_pk = malloc(CRYPTO_PUBLICKEYBYTES);
    g_sk = malloc(CRYPTO_SECRETKEYBYTES);
    g_sm = malloc(MSG_LEN + BENCH_SIG_BYTES + 64);
    g_m2 = malloc(MSG_LEN + BENCH_SIG_BYTES + 64);
    BENCH_SEED(0);
    if (bench_keygen()) { g_fail = -10; return NULL; }
    if (g_op != OP_KEYGEN) {
        BENCH_LEN_T n = 0;
        if (bench_sign(&n)) { g_fail = -11; return NULL; }
        g_smlen = n;
    }
    return NULL;
}

static long read_vmhwm_kb(void)
{
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    long v = -1;
    if (!f) return -1;
    while (fgets(line, sizeof line, f))
        if (!strncmp(line, "VmHWM:", 6)) { v = atol(line + 6); break; }
    fclose(f);
    return v;
}

static void reset_vmhwm(void)
{
    FILE *f = fopen("/proc/self/clear_refs", "w");
    if (f) { fputs("5", f); fclose(f); }
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s keygen|sign|verify\n", argv[0]); return 2; }
    if      (!strcmp(argv[1], "keygen")) g_op = OP_KEYGEN;
    else if (!strcmp(argv[1], "sign"))   g_op = OP_SIGN;
    else if (!strcmp(argv[1], "verify")) g_op = OP_VERIFY;
    else { fprintf(stderr, "bad op\n"); return 2; }

    const char *s;
    if ((s = getenv("BENCH_TIME_S")))    g_budget = atof(s);
    if ((s = getenv("BENCH_MIN_ITERS"))) g_min_iters = atoi(s);
    if ((s = getenv("BENCH_MAX_ITERS"))) g_max_iters = atoi(s);

    for (unsigned i = 0; i < MSG_LEN; i++) g_msg[i] = "hello world "[i % 12];

    /* Prep (keypair + signature) on its own throw-away stack, so its stack/RSS
     * footprint does not leak into the measured operation. */
    size_t dummy;
    mt_enable(0);
    run_on_stack(prep_thread, &dummy);
    if (g_fail) { fprintf(stderr, "prep failed (%d)\n", g_fail); return 1; }

    /* Fresh DRBG state for the measured loop (sign/keygen consume randomness). */
    BENCH_SEED(1);
    reset_vmhwm();
    long rss_before = read_vmhwm_kb();

    size_t stack_peak = 0, stack_base = 0;
    run_on_stack(nop_thread, &stack_base);       /* thread descriptor/TLS pages, not algorithm stack */
    run_on_stack(measure_thread, &stack_peak);
    stack_peak = stack_peak > stack_base ? stack_peak - stack_base : 0;
    long vmhwm = read_vmhwm_kb();
    if (g_fail) { fprintf(stderr, "operation failed (%d)\n", g_fail); return 1; }

    qsort(g_ns, (size_t)g_n, sizeof(double), cmp_d);
    qsort(g_cyc, (size_t)g_n, sizeof(uint64_t), cmp_u);
    double sum = 0, sq = 0;
    for (int i = 0; i < g_n; i++) { sum += g_ns[i]; sq += g_ns[i] * g_ns[i]; }
    double mean = sum / g_n, var = sq / g_n - mean * mean;
    if (var < 0) var = 0;

    printf("{\"name\":\"%s\",\"op\":\"%s\",\"iters\":%d,"
           "\"ns_min\":%.0f,\"ns_median\":%.0f,\"ns_mean\":%.0f,\"ns_std\":%.0f,"
           "\"cycles_median\":%llu,"
           "\"heap_peak\":%zu,\"heap_allocs\":%zu,\"stack_peak\":%zu,"
           "\"vmhwm_kb\":%ld,\"vmhwm_base_kb\":%ld,"
           "\"pk_bytes\":%ld,\"sk_bytes\":%ld,\"sig_bytes_max\":%ld,\"sig_bytes\":%lld}\n",
           BENCH_NAME, argv[1], g_n,
           g_ns[0], g_ns[g_n / 2], mean, __builtin_sqrt(var),
           (unsigned long long)g_cyc[g_n / 2],
           mt_peak_bytes(), mt_alloc_calls(), stack_peak,
           vmhwm, rss_before,
           (long)CRYPTO_PUBLICKEYBYTES, (long)CRYPTO_SECRETKEYBYTES, (long)BENCH_SIG_BYTES,
           g_op == OP_KEYGEN ? -1LL : (long long)g_smlen - (long long)MSG_LEN);
    return 0;
}
