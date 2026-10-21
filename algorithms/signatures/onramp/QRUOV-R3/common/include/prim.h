#ifndef PRIM_H
#define PRIM_H

#include <stddef.h>
#include <stdint.h>
#include "fips202.h"
#include "qruov_aes.h"

// Portable, OpenSSL-free replacement of the upstream prim.h: SHAKE128/SHAKE256
// are backed by the self-contained fips202.c (incremental absorb/squeeze), and
// AES-128-CTR by the constant-time portable aes_c.c. The function contract is
// unchanged w.r.t. the upstream (OpenSSL EVP based) prim.h.

typedef struct shake256 {
    keccak_state st;
    int finalized;
} shake256;

typedef struct shake128 {
    keccak_state st;
} shake128;

typedef struct aes128 {
    aes128ctx ctx;
} aes128;

// Requires:
//   - message := update* -> digestfinal | update* -> squeeze*
//   - call order: init -> message -> (reset -> message)* -> free
void shake256_init(shake256 *ctx);
void shake256_reset(shake256 *ctx);
void shake256_update(shake256 *ctx, const uint8_t *data, size_t size);
void shake256_digestfinal(shake256 *ctx, uint8_t *dst, size_t size);
void shake256_squeeze(shake256 *ctx, uint8_t *dst, size_t size);
void shake256_free(shake256 *ctx);

// Requires: call order is init -> update* -> digestfinal -> free
void shake128_init(shake128 *ctx);
void shake128_update(shake128 *ctx, const uint8_t *data, size_t size);
void shake128_digestfinal(shake128 *ctx, uint8_t *dst, size_t size);
void shake128_free(shake128 *ctx);

// Requires: call order is init -> stream* -> free
void aes128ctr_init(aes128 *ctx, const uint8_t key[16]);
void aes128ctr_stream(aes128 *ctx, const uint8_t iv[8], uint8_t *dst, size_t size);
void aes128ctr_free(aes128 *ctx);

#endif // PRIM_H
