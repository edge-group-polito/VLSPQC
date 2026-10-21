/*
 * main_hss.c
 *
 * KAT test runner for HSS. Same build modes as PQCgenKAT_sign_hss.c (see its
 * header comment): default (hss_variant_default), -DHSS_CHECK_ALL=1 (16 fast
 * variants), -DHSS_CHECK_ALL=1 -DHSS_CHECK_SLOW=1 (24 slow variants).
 *
 * sm = signature || message (siglen fixed per parameter set, recovered via
 * hss_variant_sig_bytes()); smlen in the .rsp is siglen+mlen, matching every
 * other family's convention here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "hss_variant.h"
#include "kat_parser.h"

#ifndef KATNUM
#define KATNUM 10000
#endif
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

#ifdef HSS_CHECK_ALL
#ifndef RSP_DIR
#error "RSP_DIR must be defined in HSS_CHECK_ALL mode"
#endif
#else
#ifndef RSP_PATH
#error "RSP_PATH must be defined"
#endif
#endif

static void sanitize(char *dst, const char *src)
{
    for (; *src; src++, dst++) *dst = *src;
    *dst = '\0';
}

static int test_one(const hss_named_variant_t *nv, const char *rsp_path)
{
    hss_variant_t v;
    if (hss_variant_init(&v, nv->levels, nv->lm_type, nv->lm_ots_type)) {
        fprintf(stderr, "%s: bad parameter set\n", nv->name);
        return 1;
    }
    size_t pkb = hss_variant_pk_bytes(&v);
    size_t skb = hss_variant_sk_bytes(&v);
    size_t sigb = hss_variant_sig_bytes(&v);

    kat_parser_t parser;
    kat_entry_t  entry;

    printf("[%s] Opening .rsp: %s\n", nv->name, rsp_path);
    if (kat_parser_open(&parser, rsp_path) != 0) return 1;

    uint8_t keypair_seed[48];
    uint8_t msg[1024];
    long    mlen_rsp = 0;
    uint8_t *pk_rsp = malloc(pkb);
    uint8_t *sk_rsp = malloc(skb);
    long    smlen_rsp = 0;

    uint8_t *pk = malloc(pkb);
    uint8_t *sk = malloc(skb);
    uint8_t *sig = malloc(sigb);

    int pass_key = 0, fail_key = 0;
    int pass_sign = 0, fail_sign = 0;
    int pass_ver = 0, fail_ver = 0;
    int count = 0, ret;

    while (count < katnum) {
        ret = kat_parser_next(&parser, &entry);
        if (ret == 1) break;
        if (ret != 0) { fprintf(stderr, "parse error\n"); break; }

        if (kat_get_bytes(&entry, "keypair_seed", keypair_seed, 48) != 0) goto next;
        if (kat_get_long(&entry,  "mlen", &mlen_rsp) != 0) goto next;
        if (kat_get_bytes(&entry, "msg", msg, (size_t)mlen_rsp) != 0) goto next;
        if (kat_get_bytes(&entry, "pk", pk_rsp, pkb) != 0) goto next;
        if (kat_get_bytes(&entry, "sk", sk_rsp, skb) != 0) goto next;
        if (kat_get_long(&entry,  "smlen", &smlen_rsp) != 0) goto next;
        kat_field_t *sm_f = kat_entry_get(&entry, "sm");
        if (!sm_f || !sm_f->data) goto next;

#if RUN_KEYGEN
        ret = hss_variant_keypair_seed(&v, pk, sk, keypair_seed);
        if (ret != 0 || memcmp(pk, pk_rsp, pkb) != 0 || memcmp(sk, sk_rsp, skb) != 0) {
            printf("[%s][%d] FAIL: keygen\n", nv->name, entry.count);
            fail_key++;
        } else {
            printf("[%s][%d] PASS: keygen\n", nv->name, entry.count);
            pass_key++;
        }
#else
        memcpy(pk, pk_rsp, pkb);
        memcpy(sk, sk_rsp, skb);
#endif

#if RUN_SIGN
        {
            size_t siglen;
            ret = hss_variant_sign(&v, sk, sig, &siglen, msg, (size_t)mlen_rsp);
            if (ret != 0 || siglen != (size_t)smlen_rsp - (size_t)mlen_rsp
                         || memcmp(sig, sm_f->data, siglen) != 0) {
                printf("[%s][%d] FAIL: sign\n", nv->name, entry.count);
                fail_sign++;
            } else {
                printf("[%s][%d] PASS: sign\n", nv->name, entry.count);
                pass_sign++;
            }
        }
#else
        memcpy(sig, sm_f->data, sigb);
#endif

#if RUN_VERIFY
        ret = hss_variant_verify(msg, (size_t)mlen_rsp, sig, sigb, pk);
        if (ret != 0) {
            printf("[%s][%d] FAIL: verify\n", nv->name, entry.count);
            fail_ver++;
        } else {
            printf("[%s][%d] PASS: verify\n", nv->name, entry.count);
            pass_ver++;
        }
#endif

next:
        kat_entry_free(&entry);
        count++;
    }

    kat_parser_close(&parser);
    printf("=== Results for %s ===\n", nv->name);
    printf("  Keygen : %d pass, %d fail\n", pass_key,  fail_key);
    printf("  Sign   : %d pass, %d fail\n", pass_sign, fail_sign);
    printf("  Verify : %d pass, %d fail\n", pass_ver,  fail_ver);
    int total_fail = fail_key + fail_sign + fail_ver;
    printf("  Total  : %d vectors, %d failures\n\n", count, total_fail);

    free(pk_rsp); free(sk_rsp); free(pk); free(sk); free(sig);
    return (total_fail > 0) ? 1 : 0;
}

int main(void)
{
#ifdef HSS_CHECK_ALL
#ifdef HSS_CHECK_SLOW
    const hss_named_variant_t *tbl = hss_variant_slow;
    int count = hss_variant_slow_count;
#else
    const hss_named_variant_t *tbl = hss_variant_fast;
    int count = hss_variant_fast_count;
#endif
    int failures = 0;
    for (int i = 0; i < count; i++) {
        char safe[64];
        sanitize(safe, tbl[i].name);
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.rsp", RSP_DIR, safe);
        if (test_one(&tbl[i], path) != 0) failures++;
    }
    printf("=== HSS check summary: %d/%d variants fully passed ===\n",
           count - failures, count);
    return failures ? 1 : 0;
#else
    return test_one(&hss_variant_default, RSP_PATH);
#endif
}
