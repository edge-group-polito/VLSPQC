/* bench shape kem_derand (ML-KEM): keygen = keypair_derand, "sign" = encaps, "verify" = decaps.
 * The ciphertext is kept in g_sm; the reported "signature size" is the ciphertext size. */
#include "kem.h"
#include "api.h"
#include "params.h"

#define BENCH_SEED(tag)   (bs_state = 0x9e3779b97f4a7c15ull + (tag))
#define BENCH_SIG_BYTES   CRYPTO_CIPHERTEXTBYTES

static uint8_t g_ss[KYBER_SSBYTES], g_ss2[KYBER_SSBYTES];

static int bench_keygen(void)
{
    uint8_t coins[2 * KYBER_SYMBYTES];
    bs_fill(coins, sizeof coins);
    return crypto_kem_keypair_derand(g_pk, g_sk, coins);
}
static int bench_sign(BENCH_LEN_T *n)
{
    uint8_t coins[KYBER_SYMBYTES];
    bs_fill(coins, sizeof coins);
    *n = MSG_LEN + CRYPTO_CIPHERTEXTBYTES;
    return crypto_kem_enc_derand(g_sm, g_ss, g_pk, coins);
}
static int bench_verify(void)
{
    int r = crypto_kem_dec(g_ss2, g_sm, g_sk);
    if (r == 0 && memcmp(g_ss, g_ss2, KYBER_SSBYTES)) r = -2;
    return r;
}
