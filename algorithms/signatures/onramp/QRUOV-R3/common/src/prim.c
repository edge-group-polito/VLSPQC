#include <stdlib.h>
#include <string.h>
#include "prim.h"

// Portable OpenSSL-free implementation of the QR-UOV Round-3 primitives
// (upstream prim.c uses the OpenSSL EVP API).

void shake256_init(shake256 *ctx)
{
    shake256_reset(ctx);
}

void shake256_reset(shake256 *ctx)
{
    f202_shake256_init(&ctx->st);
    ctx->finalized = 0;
}

void shake256_update(shake256 *ctx, const uint8_t *data, size_t size)
{
    if (ctx->finalized) abort();
    f202_shake256_absorb(&ctx->st, data, size);
}

void shake256_digestfinal(shake256 *ctx, uint8_t *dst, size_t size)
{
    if (!ctx->finalized) {
        f202_shake256_finalize(&ctx->st);
        ctx->finalized = 1;
    }
    f202_shake256_squeeze(dst, size, &ctx->st);
}

void shake256_squeeze(shake256 *ctx, uint8_t *dst, size_t size)
{
    // The first squeeze finalizes the absorb phase; later calls continue the stream.
    shake256_digestfinal(ctx, dst, size);
}

void shake256_free(shake256 *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

void shake128_init(shake128 *ctx)
{
    f202_shake128_init(&ctx->st);
}

void shake128_update(shake128 *ctx, const uint8_t *data, size_t size)
{
    f202_shake128_absorb(&ctx->st, data, size);
}

void shake128_digestfinal(shake128 *ctx, uint8_t *dst, size_t size)
{
    f202_shake128_finalize(&ctx->st);
    f202_shake128_squeeze(dst, size, &ctx->st);
}

void shake128_free(shake128 *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

void aes128ctr_init(aes128 *ctx, const uint8_t key[16])
{
    aes128_ctr_keyexp(&ctx->ctx, key);
}

void aes128ctr_stream(aes128 *ctx, const uint8_t iv[8], uint8_t *dst, size_t size)
{
    // Upstream (OpenSSL) IV layout: 8-byte nonce || 64-bit big-endian block counter
    // starting at 0. The portable aes128_ctr() takes a 12-byte IV followed by a
    // 32-bit big-endian counter starting at 0, which yields the identical keystream
    // as long as fewer than 2^32 blocks are requested per call.
    uint8_t iv12[12] = {0};

    if (size == 0) return;
    memcpy(iv12, iv, 8);
    aes128_ctr(dst, size, iv12, &ctx->ctx);
}

void aes128ctr_free(aes128 *ctx)
{
    aes128_ctx_release(&ctx->ctx);
}
