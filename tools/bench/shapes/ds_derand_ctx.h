/* bench shape ds_derand_ctx (ML-DSA): keypair(pk,sk,seed), sign(...,ctx,ctxlen,sk,rnd), open(...,ctx,ctxlen,pk),
 * pure ML-DSA with an empty context string. */
#include "api.h"
#include "params.h"
#include "sign.h"

#define BENCH_SEED(tag)   (bs_state = 0x9e3779b97f4a7c15ull + (tag))
#define BENCH_SIG_BYTES   CRYPTO_BYTES

static int bench_keygen(void)
{
    uint8_t seed[SEEDBYTES];
    bs_fill(seed, sizeof seed);
    return crypto_sign_keypair(g_pk, g_sk, seed);
}
static int bench_sign(BENCH_LEN_T *n)
{
    uint8_t rnd[RNDBYTES];
    bs_fill(rnd, sizeof rnd);
    return crypto_sign(g_sm, n, g_msg, MSG_LEN, NULL, 0, g_sk, rnd);
}
static int bench_verify(void)
{
    BENCH_LEN_T mlen2 = 0;
    int r = crypto_sign_open(g_m2, &mlen2, g_sm, g_smlen, NULL, 0, g_pk);
    if (r == 0 && (mlen2 != MSG_LEN || memcmp(g_m2, g_msg, MSG_LEN))) r = -2;
    return r;
}
