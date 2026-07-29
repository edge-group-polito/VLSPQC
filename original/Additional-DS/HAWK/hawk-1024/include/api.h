#ifndef api_h
#define api_h

#include <stddef.h>

/* Private key size (bytes) */
#define CRYPTO_SECRETKEYBYTES   360

/* Public key size (bytes) */
#define CRYPTO_PUBLICKEYBYTES   2440

/* Signature size (bytes) */
#define CRYPTO_BYTES            1221

/* Algorithm name */
#define CRYPTO_ALGNAME          "Hawk-1024"

/* Degree logarithm (matches src/api.c's private LOGN); exposed here so
 * external callers driving hawk_sign_finish() directly (e.g. the KAT
 * generator's recording-rng path) don't need to guess it. */
#define HAWK_LOGN               10

/*
 * Generate a new key pair. The public and private key have sizes
 * CRYPTO_PUBLICKEYBYTES and CRYPTO_SECRETKEYBYTES, respectively,
 * and are written in the provided pk and sk buffers, respectively.
 *
 * Returned value is 0 on success, -1 on error.
 */
int crypto_sign_keypair(unsigned char *pk, unsigned char *sk);

/*
 * Sign a message. Source message is found in buffer m[], of size
 * mlen bytes. A wrapped object that includes both the source message
 * and the signature is written into sm[], and its length is written
 * in *smlen. The output size (*smlen) is at most mlen + CRYPTO_BYTES.
 * The private key is provided as pointer sk.
 *
 * Buffers m[] and sm[] may overlap freely.
 *
 * Returned value is 0 on success, -1 on error.
 */
int crypto_sign(unsigned char *sm, unsigned long long *smlen,
	const unsigned char *m, unsigned long long mlen,
	const unsigned char *sk);

/*
 * Verify a signed message. A wrapped object (as produced by crypto_sign())
 * is expected in sm[], with length smlen bytes. If the signature is valid,
 * then the message itself is extracted and written into m[], with its size
 * written into *mlen (necessarily less than smlen). The public key is
 * obtained from pk[].
 *
 * Buffers m[] and sm[] may overlap freely.
 *
 * Returned value is 0 on success, -1 on error.
 */
int crypto_sign_open(unsigned char *m, unsigned long long *mlen,
	const unsigned char *sm, unsigned long long smlen,
	const unsigned char *pk);

/*
 * Sign a message using explicit randomness instead of the global DRBG.
 * Same contract as crypto_sign(), but the random bytes HAWK's signing
 * algorithm would otherwise draw internally are instead served from
 * rho[] (rho_len bytes) in order -- this lets sign be replayed
 * deterministically from a captured (sk, rho) pair alone, without
 * needing crypto_sign_keypair() to run first.
 *
 * Returned value is 0 on success, -1 on error.
 */
int crypto_sign_with_randomness(unsigned char *sm, unsigned long long *smlen,
	const unsigned char *m, unsigned long long mlen,
	const unsigned char *sk, const unsigned char *rho, size_t rho_len);

#endif /* api_h */
