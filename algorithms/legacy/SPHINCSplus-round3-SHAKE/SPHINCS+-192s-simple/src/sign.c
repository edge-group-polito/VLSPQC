/*
 * sign.c -- adapter exposing pqrv's crypto_sign_keypair/crypto_sign/crypto_sign_open
 * convention on top of Saarinen's runtime-parametrized slh_keygen/slh_sign/slh_verify
 * API, pinned to the slh_dsa_shake_192s parameter set (an extern slh_param_t declared in
 * slh_dsa.h). Self-contained -- no external -D flag needed, so this variant compiles
 * like every other family here (matters for the static-test generator in tools/,
 * which just does `gcc *.c` with no per-variant flags).
 *
 * Randomness plumbing: slh_keygen() calls rbg(sk, 3*n) exactly once (SK.seed ||
 * SK.prf || PK.seed), and slh_sign() calls rbg(opt_rand, n) exactly once -- so a
 * callback that just serves bytes from the caller-supplied keypair_rnd/signature_rnd
 * buffer reproduces pqrv's explicit-derand convention exactly.
 */
#include <string.h>
#include "api.h"
#include "slh_dsa.h"

#define SLH_PARAM slh_dsa_shake_192s

static const uint8_t *g_rnd_buf;
static size_t g_rnd_pos;

static int shim_rbg(uint8_t *x, size_t xlen)
{
    memcpy(x, g_rnd_buf + g_rnd_pos, xlen);
    g_rnd_pos += xlen;
    return 0;
}

int crypto_sign_keypair(unsigned char *pk, unsigned char *sk, uint8_t *keypair_rnd)
{
    g_rnd_buf = keypair_rnd;
    g_rnd_pos = 0;
    return slh_keygen(pk, sk, shim_rbg, &SLH_PARAM) == 0 ? 0 : -1;
}

int crypto_sign(unsigned char *sm, unsigned long long *smlen,
                const unsigned char *m, unsigned long long mlen,
                const unsigned char *sk, uint8_t *signature_rnd)
{
    g_rnd_buf = signature_rnd;
    g_rnd_pos = 0;
    size_t siglen = slh_sign(sm, m, (size_t)mlen, sk, shim_rbg, &SLH_PARAM);
    if (siglen == 0) return -1;
    memmove(sm + siglen, m, (size_t)mlen);
    *smlen = (unsigned long long)siglen + mlen;
    return 0;
}

int crypto_sign_open(unsigned char *m, unsigned long long *mlen,
                     const unsigned char *sm, unsigned long long smlen,
                     const unsigned char *pk)
{
    size_t siglen = slh_sig_sz(&SLH_PARAM);
    if (smlen < siglen) return -1;
    unsigned long long msglen = smlen - siglen;
    if (!slh_verify(sm + siglen, (size_t)msglen, sm, pk, &SLH_PARAM)) return -1;
    memmove(m, sm + siglen, (size_t)msglen);
    *mlen = msglen;
    return 0;
}
