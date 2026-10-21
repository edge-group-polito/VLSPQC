/* bench shape kem_prng_reseed (HQC): keygen = crypto_kem_keypair, "sign" = encaps, "verify" = decaps.
 * Randomness comes from HQC's own SHAKE-256 PRNG, seeded once per process by prng_init(). */
#include "api.h"
#include "symmetric.h"

#define BENCH_SEED(tag)   do { uint8_t s_[48]; for (int i_ = 0; i_ < 48; i_++) s_[i_] = (uint8_t)(i_ + (tag)); prng_init(s_, NULL, 48, 0); } while (0)
#define BENCH_SIG_BYTES   CRYPTO_CIPHERTEXTBYTES

static uint8_t g_ss[CRYPTO_BYTES], g_ss2[CRYPTO_BYTES];

static int bench_keygen(void) { return crypto_kem_keypair(g_pk, g_sk); }
static int bench_sign(BENCH_LEN_T *n)
{
    *n = MSG_LEN + CRYPTO_CIPHERTEXTBYTES;
    return crypto_kem_enc(g_sm, g_ss, g_pk);
}
static int bench_verify(void)
{
    int r = crypto_kem_dec(g_ss2, g_sm, g_sk);
    if (r == 0 && memcmp(g_ss, g_ss2, CRYPTO_BYTES)) r = -2;
    return r;
}
