/*
 * main_xmss.c
 *
 * KAT test runner for XMSS / XMSS^MT. Same three build modes as
 * PQCgenKAT_sign_xmss.c (see its header comment): single-variant (default),
 * -DXMSS_CHECK_ALL=1 (49 fast variants), -DXMSS_CHECK_ALL=1 -DXMSS_CHECK_SLOW=1
 * (28 slow variants).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "xmss_variant.h"
#include "kat_parser.h"

#ifndef KATNUM
#define KATNUM 10000
#endif
/* runtime override: KATNUM env var takes precedence */
static int katnum = KATNUM;
static void __attribute__((constructor)) katnum_init(void) {
    const char *s = getenv("KATNUM"); if (s) katnum = atoi(s); }

#ifndef RUN_KEYGEN
#define RUN_KEYGEN  1
#endif
#ifndef RUN_SIGN
#define RUN_SIGN    1
#endif
#ifndef RUN_VERIFY
#define RUN_VERIFY  1
#endif
#ifndef DEBUG
#define DEBUG       0
#endif

#if !defined(XMSS_CHECK_ALL) && !defined(XMSS_VARIANT_STR)
#error "Define either -DXMSS_VARIANT_STR=\"...\" or -DXMSS_CHECK_ALL=1"
#endif

#ifdef XMSS_CHECK_ALL
#ifndef RSP_DIR
#error "RSP_DIR must be defined in XMSS_CHECK_ALL mode"
#endif
#else
#ifndef RSP_PATH
#error "RSP_PATH must be defined"
#endif
#endif

static void sanitize(char *dst, const char *src)
{
    for (; *src; src++, dst++) {
        *dst = (*src == '/' || *src == '+') ? '_' : *src;
    }
    *dst = '\0';
}

/* Tests one variant's .rsp file. Returns 0 if every checked stage passed. */
static int test_one(const char *name, const char *rsp_path)
{
    xmss_variant_t v;
    if (xmss_variant_init(&v, name)) {
        fprintf(stderr, "%s: unknown parameter set\n", name);
        return 1;
    }
    size_t n = v.params.n;
    size_t pkb = xmss_variant_pk_bytes(&v);
    size_t skb = xmss_variant_sk_bytes(&v);

    kat_parser_t parser;
    kat_entry_t  entry;

    printf("[%s] Opening .rsp: %s\n", name, rsp_path);
    if (kat_parser_open(&parser, rsp_path) != 0) return 1;

    uint8_t *keypair_seed = malloc(3 * n);
    uint8_t msg[1024];
    long    mlen_rsp = 0;
    uint8_t *pk_rsp = malloc(pkb);
    uint8_t *sk_rsp = malloc(skb);
    long    smlen_rsp = 0;

    uint8_t *pk = malloc(pkb);
    uint8_t *sk = malloc(skb);
    uint8_t *sm = NULL, *m_out = NULL;
    unsigned long long smlen, mlen_out;

    int pass_key = 0, fail_key = 0;
    int pass_sign = 0, fail_sign = 0;
    int pass_ver = 0, fail_ver = 0;
    int count = 0, ret;

    while (count < katnum) {
        ret = kat_parser_next(&parser, &entry);
        if (ret == 1) break;
        if (ret != 0) { fprintf(stderr, "parse error\n"); break; }

        if (kat_get_bytes(&entry, "keypair_seed", keypair_seed, 3 * n) != 0) goto next;
        if (kat_get_long(&entry,  "mlen", &mlen_rsp) != 0) goto next;
        if (kat_get_bytes(&entry, "msg", msg, (size_t)mlen_rsp) != 0) goto next;
        if (kat_get_bytes(&entry, "pk", pk_rsp, pkb) != 0) goto next;
        if (kat_get_bytes(&entry, "sk", sk_rsp, skb) != 0) goto next;
        if (kat_get_long(&entry,  "smlen", &smlen_rsp) != 0) goto next;
        kat_field_t *sm_f = kat_entry_get(&entry, "sm");
        if (!sm_f || !sm_f->data) goto next;

#if RUN_KEYGEN
        ret = xmss_variant_keypair_seed(&v, pk, sk, keypair_seed);
        if (ret != 0 || memcmp(pk, pk_rsp, pkb) != 0 || memcmp(sk, sk_rsp, skb) != 0) {
            printf("[%s][%d] FAIL: keygen\n", name, entry.count);
            fail_key++;
        } else {
            printf("[%s][%d] PASS: keygen\n", name, entry.count);
            pass_key++;
        }
#else
        memcpy(pk, pk_rsp, pkb);
        memcpy(sk, sk_rsp, skb);
#endif

#if RUN_SIGN
        {
            size_t sm_max = (size_t)mlen_rsp + xmss_variant_sig_bytes(&v);
            sm = malloc(sm_max);
            ret = xmss_variant_sign(&v, sk, sm, &smlen, msg, (unsigned long long)mlen_rsp);
            if (ret != 0 || smlen != (unsigned long long)smlen_rsp
                         || memcmp(sm, sm_f->data, (size_t)smlen_rsp) != 0) {
                printf("[%s][%d] FAIL: sign\n", name, entry.count);
                fail_sign++;
            } else {
                printf("[%s][%d] PASS: sign\n", name, entry.count);
                pass_sign++;
            }
        }
#else
        sm = malloc(sm_f->len);
        memcpy(sm, sm_f->data, sm_f->len);
        smlen = (unsigned long long)smlen_rsp;
#endif

#if RUN_VERIFY
        m_out = malloc(smlen + 1);
        ret = xmss_variant_open(&v, m_out, &mlen_out, sm, smlen, pk);
        if (ret != 0 || mlen_out != (unsigned long long)mlen_rsp
                     || memcmp(m_out, msg, (size_t)mlen_rsp) != 0) {
            printf("[%s][%d] FAIL: verify\n", name, entry.count);
            fail_ver++;
        } else {
            printf("[%s][%d] PASS: verify\n", name, entry.count);
            pass_ver++;
        }
#endif

next:
        free(sm); sm = NULL; free(m_out); m_out = NULL;
        kat_entry_free(&entry);
        count++;
    }

    kat_parser_close(&parser);
    printf("=== Results for %s ===\n", name);
    printf("  Keygen : %d pass, %d fail\n", pass_key,  fail_key);
    printf("  Sign   : %d pass, %d fail\n", pass_sign, fail_sign);
    printf("  Verify : %d pass, %d fail\n", pass_ver,  fail_ver);
    int total_fail = fail_key + fail_sign + fail_ver;
    printf("  Total  : %d vectors, %d failures\n\n", count, total_fail);

    free(keypair_seed); free(pk_rsp); free(sk_rsp); free(pk); free(sk);
    return (total_fail > 0) ? 1 : 0;
}

int main(void)
{
#ifdef XMSS_CHECK_ALL
#ifdef XMSS_CHECK_SLOW
    const char *const *names = xmss_variant_slow_names;
    int count = xmss_variant_slow_count;
#else
    const char *const *names = xmss_variant_fast_names;
    int count = xmss_variant_fast_count;
#endif
    int failures = 0;
    for (int i = 0; i < count; i++) {
        char safe[64];
        sanitize(safe, names[i]);
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.rsp", RSP_DIR, safe);
        if (test_one(names[i], path) != 0) failures++;
    }
    printf("=== XMSS check summary: %d/%d variants fully passed ===\n",
           count - failures, count);
    return failures ? 1 : 0;
#else
    return test_one(XMSS_VARIANT_STR, RSP_PATH);
#endif
}
