/* bench shape ds_derand_rnd (SLH-DSA): keypair(pk,sk,3n random bytes), sign(...,sk,n random bytes), open(...,pk). */
#include "api.h"
#include "params.h"

#define BENCH_SEED(tag)   (bs_state = 0x9e3779b97f4a7c15ull + (tag))
#define BENCH_SIG_BYTES   CRYPTO_BYTES

static int bench_keygen(void)
{
    uint8_t rnd[3 * SPX_N];
    bs_fill(rnd, sizeof rnd);
    return crypto_sign_keypair(g_pk, g_sk, rnd);
}
static int bench_sign(BENCH_LEN_T *n)
{
    uint8_t rnd[SPX_N];
    bs_fill(rnd, sizeof rnd);
    return crypto_sign(g_sm, n, g_msg, MSG_LEN, g_sk, rnd);
}
static int bench_verify(void)
{
    BENCH_LEN_T mlen2 = 0;
    int r = crypto_sign_open(g_m2, &mlen2, g_sm, g_smlen, g_pk);
    if (r == 0 && (mlen2 != MSG_LEN || memcmp(g_m2, g_msg, MSG_LEN))) r = -2;
    return r;
}
