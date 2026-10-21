/* pqrv: merged config.h. First block = XKCP sha3/config.h (verbatim); second block =
 * sw/common/portable/config.h of the FAEST software tree, which upstream forces via
 * -DHAVE_CONFIG_H when building the pure C code (no AES-NI/SSE2/AVX2 auto-detection). */
#define XKCP_has_Sponge_Keccak
#define XKCP_has_FIPS202
#define XKCP_has_KeccakP1600
#define XKCP_has_KeccakP1600times4

#define HAVE_ALIGNED_ALLOC
#define HAVE_EXPLICIT_BZERO
#define HAVE_MEMPCPY
