/*
 * xmss_variant.h -- pqrv adapter over the xmss-reference "core" API.
 *
 * xmss.c's public wrapper (xmss_keypair/xmss_sign/...) hardcodes an internal
 * randombytes() call for keygen, with no derand entry point. xmss_core.h's
 * xmssmt_core_seed_keypair(params, pk, sk, seed) *does* take an explicit
 * 3*n-byte seed and -- since xmss_core_keypair(), xmss_core_sign() and
 * xmss_core_sign_open() are themselves just thin forwarders to the
 * xmssmt_core_* equivalents (xmss_core.c:103-125, xmss_commons.c:120-134) --
 * that single seeded entry point plus xmssmt_core_sign/xmssmt_core_sign_open
 * already cover *both* plain XMSS and XMSS^MT for any parameter set. This
 * header wraps that directly, replicating the OID-prefixing xmss.c does
 * (xmss.c:18-25) so pk/sk still carry the OID the way NIST's reference
 * format expects.
 *
 * XMSS signing is fully deterministic given sk (no RNG at all is called by
 * xmssmt_core_sign) -- so unlike most other DS families here, no sig_rnd
 * field is needed.
 */
#ifndef XMSS_VARIANT_H
#define XMSS_VARIANT_H

#include <stdint.h>
#include "params.h"

#define XMSS_OID_PREFIX_LEN 4

typedef struct {
    xmss_params params;
    uint32_t    oid;
} xmss_variant_t;

/* name: e.g. "XMSS-SHA2_10_256" or "XMSSMT-SHA2_20/2_256". Returns 0 on success. */
int xmss_variant_init(xmss_variant_t *v, const char *name);

/* Full pk/sk sizes, OID prefix included. */
size_t xmss_variant_pk_bytes(const xmss_variant_t *v);
size_t xmss_variant_sk_bytes(const xmss_variant_t *v);
size_t xmss_variant_sig_bytes(const xmss_variant_t *v);

int xmss_variant_keypair_seed(const xmss_variant_t *v, uint8_t *pk, uint8_t *sk,
                              const uint8_t *seed);

int xmss_variant_sign(const xmss_variant_t *v, uint8_t *sk,
                      uint8_t *sm, unsigned long long *smlen,
                      const uint8_t *m, unsigned long long mlen);

int xmss_variant_open(const xmss_variant_t *v, uint8_t *m, unsigned long long *mlen,
                      const uint8_t *sm, unsigned long long smlen,
                      const uint8_t *pk);

/* 49 "fast" parameter sets (7 single-tree height-10 XMSS + 42 XMSS^MT sets
 * whose per-layer height is <=10): used by `make test-xmss-check`. */
extern const char *const xmss_variant_fast_names[];
extern const int xmss_variant_fast_count;

/* 28 "slow" parameter sets (14 single-tree XMSS at height 16/20, plus 14
 * XMSS^MT "40/2"/"60/3" sets whose per-layer height is also 20): keygen
 * costs O(2^height) leaf hashes with this reference core -- minutes to over
 * an hour each. Used by `make test-xmss-check-slow` (never by test-all or
 * the regular -check). */
extern const char *const xmss_variant_slow_names[];
extern const int xmss_variant_slow_count;

#endif
