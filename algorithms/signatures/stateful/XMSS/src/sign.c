#include <string.h>
#include "xmss_variant.h"
#include "xmss_core.h"

int xmss_variant_init(xmss_variant_t *v, const char *name)
{
    if (strncmp(name, "XMSSMT", 6) == 0) {
        if (xmssmt_str_to_oid(&v->oid, name)) return -1;
        if (xmssmt_parse_oid(&v->params, v->oid)) return -1;
    } else {
        if (xmss_str_to_oid(&v->oid, name)) return -1;
        if (xmss_parse_oid(&v->params, v->oid)) return -1;
    }
    return 0;
}

size_t xmss_variant_pk_bytes(const xmss_variant_t *v)
{
    return XMSS_OID_PREFIX_LEN + v->params.pk_bytes;
}

size_t xmss_variant_sk_bytes(const xmss_variant_t *v)
{
    return XMSS_OID_PREFIX_LEN + v->params.sk_bytes;
}

size_t xmss_variant_sig_bytes(const xmss_variant_t *v)
{
    return v->params.sig_bytes;
}

static void put_oid(unsigned char *buf, uint32_t oid)
{
    for (unsigned int i = 0; i < XMSS_OID_PREFIX_LEN; i++) {
        buf[XMSS_OID_PREFIX_LEN - i - 1] = (oid >> (8 * i)) & 0xFF;
    }
}

int xmss_variant_keypair_seed(const xmss_variant_t *v, uint8_t *pk, uint8_t *sk,
                              const uint8_t *seed)
{
    put_oid(pk, v->oid);
    put_oid(sk, v->oid);
    return xmssmt_core_seed_keypair(&v->params, pk + XMSS_OID_PREFIX_LEN,
                                    sk + XMSS_OID_PREFIX_LEN,
                                    (unsigned char *)seed);
}

int xmss_variant_sign(const xmss_variant_t *v, uint8_t *sk,
                      uint8_t *sm, unsigned long long *smlen,
                      const uint8_t *m, unsigned long long mlen)
{
    /* sk here is the full (OID-prefixed) buffer produced by keypair_seed;
     * the core signer only sees (and mutates, advancing its leaf index)
     * the part after the OID prefix. */
    return xmssmt_core_sign(&v->params, sk + XMSS_OID_PREFIX_LEN, sm, smlen, m, mlen);
}

int xmss_variant_open(const xmss_variant_t *v, uint8_t *m, unsigned long long *mlen,
                      const uint8_t *sm, unsigned long long smlen,
                      const uint8_t *pk)
{
    return xmssmt_core_sign_open(&v->params, m, mlen, sm, smlen,
                                 pk + XMSS_OID_PREFIX_LEN);
}

/*
 * "Fast" vs "slow" split: keygen costs O(2^subtree_height) leaf hashes with
 * this non-BDS-cached reference core (BDS only amortizes *later* signs, not
 * the initial root computation -- see DS/XMSS/README.md). For single-tree
 * XMSS, subtree_height == the parameter set's full height; measured
 * near-instant at height 10, minutes-to-over-an-hour at height 16/20.
 *
 * For XMSS^MT, keygen only computes one subtree, of height = full_height/d
 * -- NOT full_height itself. Most XMSS^MT sets keep that per-layer height at
 * <=10 (e.g. 20/4, 40/8, 60/12), but "40/2" and "60/3" both reduce to a
 * per-layer height of *20* (40/2=20, 60/3=20) -- exactly as expensive as a
 * single-tree height-20 XMSS set, not "fast" at all. (An earlier version of
 * this table missed that and classified all 56 XMSS^MT sets as fast, which
 * left `gen-xmss-check` stuck for hours on those 14 "40/2"/"60/3" sets.)
 */
const char *const xmss_variant_fast_names[] = {
    /* 7 single-tree (height 10) XMSS parameter sets */
    "XMSS-SHA2_10_192", "XMSS-SHA2_10_256", "XMSS-SHA2_10_512",
    "XMSS-SHAKE256_10_192", "XMSS-SHAKE256_10_256",
    "XMSS-SHAKE_10_256", "XMSS-SHAKE_10_512",
    /* 42 XMSS^MT parameter sets with per-layer (full_height/d) height <=10 */
    "XMSSMT-SHA2_20/2_192", "XMSSMT-SHA2_20/2_256", "XMSSMT-SHA2_20/2_512",
    "XMSSMT-SHA2_20/4_192", "XMSSMT-SHA2_20/4_256", "XMSSMT-SHA2_20/4_512",
    "XMSSMT-SHA2_40/4_192", "XMSSMT-SHA2_40/4_256", "XMSSMT-SHA2_40/4_512",
    "XMSSMT-SHA2_40/8_192", "XMSSMT-SHA2_40/8_256", "XMSSMT-SHA2_40/8_512",
    "XMSSMT-SHA2_60/12_192", "XMSSMT-SHA2_60/12_256", "XMSSMT-SHA2_60/12_512",
    "XMSSMT-SHA2_60/6_192", "XMSSMT-SHA2_60/6_256", "XMSSMT-SHA2_60/6_512",
    "XMSSMT-SHAKE256_20/2_192", "XMSSMT-SHAKE256_20/2_256",
    "XMSSMT-SHAKE256_20/4_192", "XMSSMT-SHAKE256_20/4_256",
    "XMSSMT-SHAKE256_40/4_192", "XMSSMT-SHAKE256_40/4_256",
    "XMSSMT-SHAKE256_40/8_192", "XMSSMT-SHAKE256_40/8_256",
    "XMSSMT-SHAKE256_60/12_192", "XMSSMT-SHAKE256_60/12_256",
    "XMSSMT-SHAKE256_60/6_192", "XMSSMT-SHAKE256_60/6_256",
    "XMSSMT-SHAKE_20/2_256", "XMSSMT-SHAKE_20/2_512",
    "XMSSMT-SHAKE_20/4_256", "XMSSMT-SHAKE_20/4_512",
    "XMSSMT-SHAKE_40/4_256", "XMSSMT-SHAKE_40/4_512",
    "XMSSMT-SHAKE_40/8_256", "XMSSMT-SHAKE_40/8_512",
    "XMSSMT-SHAKE_60/12_256", "XMSSMT-SHAKE_60/12_512",
    "XMSSMT-SHAKE_60/6_256", "XMSSMT-SHAKE_60/6_512",
};

const int xmss_variant_fast_count =
    (int)(sizeof(xmss_variant_fast_names) / sizeof(xmss_variant_fast_names[0]));

/*
 * 28 "slow" parameter sets: 14 single-tree (height 16/20) XMSS sets, plus 14
 * XMSS^MT sets whose per-layer height is also 20 ("40/2" and "60/3").
 */
const char *const xmss_variant_slow_names[] = {
    "XMSS-SHA2_16_192", "XMSS-SHA2_16_256", "XMSS-SHA2_16_512",
    "XMSS-SHA2_20_192", "XMSS-SHA2_20_256", "XMSS-SHA2_20_512",
    "XMSS-SHAKE256_16_192", "XMSS-SHAKE256_16_256",
    "XMSS-SHAKE256_20_192", "XMSS-SHAKE256_20_256",
    "XMSS-SHAKE_16_256", "XMSS-SHAKE_16_512",
    "XMSS-SHAKE_20_256", "XMSS-SHAKE_20_512",
    "XMSSMT-SHA2_40/2_192", "XMSSMT-SHA2_40/2_256", "XMSSMT-SHA2_40/2_512",
    "XMSSMT-SHA2_60/3_192", "XMSSMT-SHA2_60/3_256", "XMSSMT-SHA2_60/3_512",
    "XMSSMT-SHAKE256_40/2_192", "XMSSMT-SHAKE256_40/2_256",
    "XMSSMT-SHAKE256_60/3_192", "XMSSMT-SHAKE256_60/3_256",
    "XMSSMT-SHAKE_40/2_256", "XMSSMT-SHAKE_40/2_512",
    "XMSSMT-SHAKE_60/3_256", "XMSSMT-SHAKE_60/3_512",
};

const int xmss_variant_slow_count =
    (int)(sizeof(xmss_variant_slow_names) / sizeof(xmss_variant_slow_names[0]));
