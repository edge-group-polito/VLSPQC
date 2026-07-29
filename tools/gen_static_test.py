#!/usr/bin/env python3
#######################################################################################
##                                                                                   ##
## Auth: Alessandra Dolmeta, Valeria Piscopo                                         ##
## @ EDGE group, at VLSI-LAB, Politecnico di Torino                                  ##
## Date: July 2026                                                                   ##
## Desc: Static (no file I/O) KAT test generator                                     ##
##                                                                                   ##
#######################################################################################

"""
gen_static_test.py

Generates a *static* KAT test (no file I/O at runtime) for one or more
PQC algorithm variants, suitable for compiling onto bare-metal / FPGA /
RISC-V targets where fopen()-ing a .rsp file at runtime is not possible.

For each requested variant it:
  1. (Re)runs the existing `make gen-<variant>` target to produce a fresh
     .rsp KAT file with at least --kat-num entries.
  2. Parses that .rsp file (pure Python, no C build step needed).
  3. Renders a `test_vectors_<suffix>.h` header with the first N KAT
     vectors baked in as C arrays, indexed by KAT number.
  4. Renders a `main.c` that loops `for (i = 0; i < N_KAT; i++)` over the
     vectors, running keygen/sign-or-encaps/verify-or-decaps depending on
     --run-* flags (baked in as literal #define 0/1, not compile flags).
  5. Copies the variant's src/*.c + include/*.h (flattened, no
     subdirectories) into the output directory, so the result is a
     self-contained folder that can be copied straight into a target
     project's test tree (e.g. cva6/tests/<variant>/).

Only the API "shapes" listed in algo_registry.json are supported; see
that file's _unsupported_families_todo for what still needs a shape
renderer added below before it can be generated.
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parent  # pqrv/ (DS/KEM sources live under ROOT/original/)
REGISTRY_PATH = SCRIPT_DIR / "algo_registry.json"
PROFILING_DIR = SCRIPT_DIR / "profiling_commands"

BYTES_PER_LINE = 20


# --------------------------------------------------------------------------
# Performance-counter profiling injection
#
# Every generated main.c always contains `#define PERF_CNT_CYCLES 0/1` plus
# `#if PERF_CNT_CYCLES ... #endif` guards around the platform-specific
# counter reset/read, regardless of whether --profiling was requested --
# they're just empty when it's off. The actual counter code is never
# invented here: it comes verbatim from a tools/profiling_commands/<name>.txt
# file (see that directory's README section below).
# --------------------------------------------------------------------------

def load_profiling(name):
    """Returns None if profiling is off, else {'includes','init','before','after'} (each a str, possibly empty)."""
    if not name or name == "none":
        return None
    path = PROFILING_DIR / f"{name}.txt"
    if not path.exists():
        available = sorted(p.stem for p in PROFILING_DIR.glob("*.txt"))
        raise SystemExit(f"--profiling '{name}': no such file {path}. Available: {', '.join(available)}")

    sections = {"includes": [], "init": [], "before": [], "after": []}
    current = None
    for raw in path.read_text().splitlines():
        line = raw.rstrip()
        m = re.match(r"^#\s*(includes|init|before|after)\s*$", line)
        if m:
            current = m.group(1)
            continue
        if current is None:
            if line.strip():
                raise SystemExit(f"{path}: content before the first '# includes'/'# init'/'# before'/'# after' marker")
            continue
        sections[current].append(line)

    missing = [k for k, v in sections.items() if not any(l.strip() for l in v)]
    if missing:
        raise SystemExit(f"{path}: missing/empty section(s): {', '.join(missing)}")

    return {k: "\n".join(v).strip("\n") for k, v in sections.items()}


_PERF_CALL_RE = re.compile(r"^([ \t]*)(.*;)[ \t]*/\*@PERF:(\w+)@\*/[ \t]*$", re.MULTILINE)


def apply_profiling(main_c: str, profiling) -> str:
    enabled = 1 if profiling is not None else 0

    def wrap_call(m):
        indent, stmt, tag = m.group(1), m.group(2), m.group(3)
        lines = [f"{indent}#if PERF_CNT_CYCLES"]
        if profiling:
            lines.append(f'{indent}printf("-- {tag} --\\n");')
            lines += [f"{indent}{l}" if l else "" for l in profiling["before"].splitlines()]
        lines.append(f"{indent}#endif /* PERF_CNT_CYCLES */")
        lines.append(f"{indent}{stmt}")
        lines.append(f"{indent}#if PERF_CNT_CYCLES")
        if profiling:
            lines += [f"{indent}{l}" if l else "" for l in profiling["after"].splitlines()]
        lines.append(f"{indent}#endif /* PERF_CNT_CYCLES */")
        return "\n".join(lines)

    main_c = _PERF_CALL_RE.sub(wrap_call, main_c)

    top_block = f"#define PERF_CNT_CYCLES {enabled}\n\n#if PERF_CNT_CYCLES\n"
    top_block += (profiling["includes"] + "\n") if profiling else ""
    top_block += "#endif /* PERF_CNT_CYCLES */\n\n"
    main_c, n = re.subn(r"(#define RUN_)", top_block + r"\1", main_c, count=1)
    if n == 0:
        raise ValueError("apply_profiling: couldn't find '#define RUN_' anchor to insert profiling includes/flag")

    init_block = "#if PERF_CNT_CYCLES\n"
    if profiling:
        init_block += "\n".join(f"    {l}" if l else "" for l in profiling["init"].splitlines()) + "\n"
    init_block += "#endif /* PERF_CNT_CYCLES */\n\n"
    main_c, n = re.subn(r"(int main\(void\)\n\{\n)", r"\1" + init_block, main_c, count=1)
    if n == 0:
        raise ValueError("apply_profiling: couldn't find 'int main(void)\\n{' anchor to insert profiling init")

    return main_c


# --------------------------------------------------------------------------
# .rsp parsing (pure Python equivalent of common/kat_parser.c)
# --------------------------------------------------------------------------

def parse_rsp(path: Path):
    entries = []
    current = {}
    with open(path, "r") as f:
        for raw in f:
            line = raw.rstrip("\n").rstrip("\r")
            if not line.strip():
                if current:
                    entries.append(current)
                    current = {}
                continue
            if line.lstrip().startswith("#"):
                continue
            if "=" not in line:
                continue
            name, val = line.split("=", 1)
            name = name.strip()
            val = val.strip()
            if val == "":
                current[name] = b""
            elif val.isdigit() and len(val) <= 18:
                current[name] = int(val)
            else:
                current[name] = bytes.fromhex(val)
    if current:
        entries.append(current)
    return entries


# --------------------------------------------------------------------------
# C code-gen helpers
# --------------------------------------------------------------------------

def c_2d_array(name: str, rows) -> str:
    """rows: list[bytes], all the same length. Emits static const uint8_t NAME[N][LEN] = {...};"""
    n = len(rows)
    length = len(rows[0])
    for r in rows:
        if len(r) != length:
            raise ValueError(f"{name}: inconsistent row length ({len(r)} vs {length}) across KAT entries")

    lines = [f"static const uint8_t {name}[{n}][{length}] = {{"]
    for idx, row in enumerate(rows):
        parts = [f"0x{b:02X}" for b in row]
        chunk_lines = []
        for i in range(0, len(parts), BYTES_PER_LINE):
            chunk = parts[i:i + BYTES_PER_LINE]
            suffix = "," if i + BYTES_PER_LINE < len(parts) else ""
            chunk_lines.append("        " + ", ".join(chunk) + suffix)
        lines.append("    {")
        lines.extend(chunk_lines if chunk_lines else ["        /* empty */"])
        lines.append("    }" + ("," if idx < n - 1 else ""))
    lines.append("};")
    return "\n".join(lines)


def c_1d_array(name: str, data: bytes) -> str:
    """Single (non-KAT-indexed) byte array: static const uint8_t NAME[LEN] = {...};"""
    parts = [f"0x{b:02X}" for b in data]
    chunk_lines = []
    for i in range(0, len(parts), BYTES_PER_LINE):
        chunk = parts[i:i + BYTES_PER_LINE]
        suffix = "," if i + BYTES_PER_LINE < len(parts) else ""
        chunk_lines.append("    " + ", ".join(chunk) + suffix)
    body = "\n".join(chunk_lines) if chunk_lines else "    /* empty */"
    return f"static const uint8_t {name}[{len(data)}] = {{\n{body}\n}};"


def c_var_field(name: str, rows) -> str:
    """rows: list[bytes], possibly differing length across KAT entries (e.g.
    Falcon/CROSS signatures aren't fixed-size). Emits NAME[i] as a flat 2D
    array when every row happens to be the same length, or as per-row named
    arrays + a pointer table when not -- either way NAME[i] can be indexed
    and memcmp'd/memcpy'd directly. Always emits a companion NAME_LEN[N]
    array of per-entry lengths, so callers never need to special-case which
    representation was used."""
    n = len(rows)
    lengths = [len(r) for r in rows]
    uniform = all(l == lengths[0] for l in lengths)

    parts = []
    if uniform:
        parts.append(c_2d_array(name, rows))
    else:
        for idx, row in enumerate(rows):
            parts.append(c_1d_array(f"{name}_{idx}", row))
        ptrs = ", ".join(f"{name}_{idx}" for idx in range(n))
        parts.append(f"static const uint8_t *const {name}[{n}] = {{ {ptrs} }};")

    lens = ", ".join(str(l) for l in lengths)
    parts.append(f"static const size_t {name}_LEN[{n}] = {{ {lens} }};")
    return "\n\n".join(parts)


def guard_name(vec_suffix: str) -> str:
    safe = "".join(c if c.isalnum() else "_" for c in vec_suffix.upper())
    return f"__TEST_VECTORS_{safe}_H__"


def assert_constant(variant: str, entries: list, field: str) -> int:
    value = entries[0][field]
    for idx, e in enumerate(entries):
        if e[field] != value:
            raise ValueError(f"{variant}: '{field}' varies across KAT entries (entry {idx}); "
                              f"this shape assumes it is constant for a whole .rsp batch")
    return value


# --------------------------------------------------------------------------
# Shape renderers
# --------------------------------------------------------------------------

def render_kem_derand(variant: str, info: dict, entries: list, kat_num: int, flags: dict):
    suffix = info["vec_suffix"]
    guard = guard_name(suffix)

    keypair_coins = [e["keypair_coins"] for e in entries]
    enc_coins = [e["enc_coins"] for e in entries]
    pk = [e["pk"] for e in entries]
    sk = [e["sk"] for e in entries]
    ct = [e["ct"] for e in entries]
    ss = [e["ss"] for e in entries]

    header = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Variant: {variant}   KAT vectors: {kat_num}
///////////////////////////////////////////////////////////////////////////////////////

#ifndef {guard}
#define {guard}

#include <stdint.h>

#define N_KAT {kat_num}

{c_2d_array("TVEC_IN_KEM_KEYPAIR", keypair_coins)}

{c_2d_array("TVEC_IN_KEM_ENC", enc_coins)}

{c_2d_array("TVEC_OUT_PK", pk)}

{c_2d_array("TVEC_OUT_SK", sk)}

{c_2d_array("TVEC_OUT_CT", ct)}

{c_2d_array("TVEC_OUT_SS", ss)}

#endif /* {guard} */
"""

    main_c = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Static (no file I/O) KAT test for {variant}
///////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "kem.h"
#include "test_vectors_{suffix}.h"

#define RUN_KEYGEN  {flags['run_keygen']}
#define RUN_ENCAPS  {flags['run_encaps']}
#define RUN_DECAPS  {flags['run_decaps']}

static void printVect(const char *name, const uint8_t *buf, size_t size) {{
    printf("%s = ", name);
    for (size_t i = 0; i < size; i++) printf("%02X", buf[i]);
    printf("\\n");
}}

static uint8_t pk[KYBER_PUBLICKEYBYTES];
static uint8_t sk[KYBER_SECRETKEYBYTES];
static uint8_t ct[KYBER_CIPHERTEXTBYTES];
static uint8_t key_a[KYBER_SSBYTES];
static uint8_t key_b[KYBER_SSBYTES];

int main(void)
{{
    int fails = 0;

    for (int i = 0; i < N_KAT; i++) {{
        printf("=== KAT %d/%d ===\\n", i + 1, N_KAT);

#if RUN_KEYGEN
        crypto_kem_keypair_derand(pk, sk, TVEC_IN_KEM_KEYPAIR[i]); /*@PERF:keygen@*/
        if (memcmp(pk, TVEC_OUT_PK[i], KYBER_PUBLICKEYBYTES)) {{ printf("[%d] ERROR: PK mismatch\\n", i); fails++; }}
        if (memcmp(sk, TVEC_OUT_SK[i], KYBER_SECRETKEYBYTES)) {{ printf("[%d] ERROR: SK mismatch\\n", i); fails++; }}
#else
        memcpy(pk, TVEC_OUT_PK[i], KYBER_PUBLICKEYBYTES);
        memcpy(sk, TVEC_OUT_SK[i], KYBER_SECRETKEYBYTES);
#endif /* RUN_KEYGEN */

#if RUN_ENCAPS
        crypto_kem_enc_derand(ct, key_b, pk, TVEC_IN_KEM_ENC[i]); /*@PERF:encaps@*/
        if (memcmp(ct, TVEC_OUT_CT[i], KYBER_CIPHERTEXTBYTES)) {{ printf("[%d] ERROR: CT mismatch\\n", i); fails++; }}
        if (memcmp(key_b, TVEC_OUT_SS[i], KYBER_SSBYTES)) {{ printf("[%d] ERROR: SS(enc) mismatch\\n", i); fails++; }}
#else
        memcpy(ct, TVEC_OUT_CT[i], KYBER_CIPHERTEXTBYTES);
        memcpy(key_b, TVEC_OUT_SS[i], KYBER_SSBYTES);
#endif /* RUN_ENCAPS */

#if RUN_DECAPS
        crypto_kem_dec(key_a, ct, sk); /*@PERF:decaps@*/
        if (memcmp(key_a, TVEC_OUT_SS[i], KYBER_SSBYTES)) {{ printf("[%d] ERROR: SS(dec) mismatch\\n", i); fails++; }}
#endif /* RUN_DECAPS */
    }}

    if (fails == 0) {{
        printf("Test Successful (%d KAT vector(s))\\n", N_KAT);
    }} else {{
        printf("Test FAILED: %d mismatch(es)\\n", fails);
    }}

    return fails != 0;
}}
"""
    return header, main_c


def render_ds_derand_ctx(variant: str, info: dict, entries: list, kat_num: int, flags: dict):
    suffix = info["vec_suffix"]
    guard = guard_name(suffix)

    mlen_kat = entries[0]["mlen"]
    smlen_kat = entries[0]["smlen"]
    for idx, e in enumerate(entries):
        if e["mlen"] != mlen_kat:
            raise ValueError(f"{variant}: mlen varies across KAT entries (entry {idx}); "
                              f"fixed-length message assumption of ds_derand_ctx shape violated")
        if e["smlen"] != smlen_kat:
            raise ValueError(f"{variant}: smlen varies across KAT entries (entry {idx}); "
                              f"this shape assumes fixed-length signatures -- a ragged-array "
                              f"renderer is needed for variable-length signature schemes")

    keypair_seed = [e["keypair_seed"] for e in entries]
    sign_rnd = [e["sign_rnd"] for e in entries]
    msg = [e["msg"] for e in entries]
    pk = [e["pk"] for e in entries]
    sk = [e["sk"] for e in entries]
    sm = [e["sm"] for e in entries]

    header = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Variant: {variant}   KAT vectors: {kat_num}
///////////////////////////////////////////////////////////////////////////////////////

#ifndef {guard}
#define {guard}

#include <stdint.h>

#define N_KAT {kat_num}
#define MLEN_KAT {mlen_kat}
#define SMLEN_KAT {smlen_kat}

{c_2d_array("TVEC_SEED_IN_KEYPAIR", keypair_seed)}

{c_2d_array("TVEC_SEED_IN_SIGN", sign_rnd)}

{c_2d_array("TVEC_IN_M_SIGN", msg)}

{c_2d_array("TVEC_OUT_PK", pk)}

{c_2d_array("TVEC_OUT_SK", sk)}

{c_2d_array("TVEC_IN_SM_SIGN", sm)}

#endif /* {guard} */
"""

    main_c = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Static (no file I/O) KAT test for {variant}
///////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "api.h"
#include "params.h"
#include "sign.h"
#include "test_vectors_{suffix}.h"

#define RUN_KEYGEN  {flags['run_keygen']}
#define RUN_SIGN    {flags['run_sign']}
#define RUN_VERIFY  {flags['run_verify']}

int main(void)
{{
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];

    uint8_t m[MLEN_KAT];
    uint8_t sm[MLEN_KAT + CRYPTO_BYTES];
    uint8_t m1[MLEN_KAT + CRYPTO_BYTES];

    uint8_t seed_keygen[SEEDBYTES];
    uint8_t seed_sign[RNDBYTES];

    size_t smlen = SMLEN_KAT, mlen1;
    int fails = 0;

    for (int i = 0; i < N_KAT; i++) {{
        printf("=== KAT %d/%d ===\\n", i + 1, N_KAT);
        memcpy(m, TVEC_IN_M_SIGN[i], MLEN_KAT);

#if RUN_KEYGEN
        memcpy(seed_keygen, TVEC_SEED_IN_KEYPAIR[i], SEEDBYTES);
        crypto_sign_keypair(pk, sk, seed_keygen); /*@PERF:keygen@*/
        if (memcmp(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES)) {{ printf("[%d] ERROR: PK mismatch\\n", i); fails++; }}
        if (memcmp(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES)) {{ printf("[%d] ERROR: SK mismatch\\n", i); fails++; }}
#else
        memcpy(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES);
        memcpy(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES);
#endif /* RUN_KEYGEN */

#if RUN_SIGN
        memcpy(seed_sign, TVEC_SEED_IN_SIGN[i], RNDBYTES);
        crypto_sign(sm, &smlen, m, MLEN_KAT, NULL, 0, sk, seed_sign); /*@PERF:sign@*/
        if (smlen != SMLEN_KAT || memcmp(sm, TVEC_IN_SM_SIGN[i], SMLEN_KAT)) {{ printf("[%d] ERROR: SM mismatch\\n", i); fails++; }}
#else
        memcpy(sm, TVEC_IN_SM_SIGN[i], SMLEN_KAT);
        smlen = SMLEN_KAT;
#endif /* RUN_SIGN */

#if RUN_VERIFY
        crypto_sign_open(m1, &mlen1, sm, SMLEN_KAT, NULL, 0, pk); /*@PERF:verify@*/
        if (mlen1 != MLEN_KAT || memcmp(m1, TVEC_IN_M_SIGN[i], MLEN_KAT)) {{ printf("[%d] ERROR: M mismatch\\n", i); fails++; }}
#endif /* RUN_VERIFY */
    }}

    if (fails == 0) {{
        printf("Test Successful (%d KAT vector(s))\\n", N_KAT);
    }} else {{
        printf("Test FAILED: %d mismatch(es)\\n", fails);
    }}

    return fails != 0;
}}
"""
    return header, main_c


def render_ds_derand_rnd(variant: str, info: dict, entries: list, kat_num: int, flags: dict):
    """SLH-DSA (all 4 sub-families): keypair(pk,sk,rnd), sign(sm,&smlen,m,mlen,sk,rnd), open(m,&mlen,sm,smlen,pk)."""
    suffix = info["vec_suffix"]
    guard = guard_name(suffix)
    mlen_kat = assert_constant(variant, entries, "mlen")

    keypair_rnd = [e["keypair_rnd"] for e in entries]
    sig_rnd = [e["sig_rnd"] for e in entries]
    msg = [e["msg"] for e in entries]
    pk = [e["pk"] for e in entries]
    sk = [e["sk"] for e in entries]
    sm = [e["sm"] for e in entries]

    header = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Variant: {variant}   KAT vectors: {kat_num}
///////////////////////////////////////////////////////////////////////////////////////

#ifndef {guard}
#define {guard}

#include <stdint.h>
#include <stddef.h>

#define N_KAT {kat_num}
#define MLEN_KAT {mlen_kat}

{c_2d_array("TVEC_KEYPAIR_RND", keypair_rnd)}

{c_2d_array("TVEC_SIG_RND", sig_rnd)}

{c_2d_array("TVEC_IN_M_SIGN", msg)}

{c_2d_array("TVEC_OUT_PK", pk)}

{c_2d_array("TVEC_OUT_SK", sk)}

{c_var_field("TVEC_IN_SM_SIGN", sm)}

#endif /* {guard} */
"""

    main_c = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Static (no file I/O) KAT test for {variant}
///////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "api.h"
#include "params.h"
#include "test_vectors_{suffix}.h"

#define RUN_KEYGEN  {flags['run_keygen']}
#define RUN_SIGN    {flags['run_sign']}
#define RUN_VERIFY  {flags['run_verify']}

int main(void)
{{
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t m[MLEN_KAT];
    uint8_t sm[MLEN_KAT + CRYPTO_BYTES];
    uint8_t m1[MLEN_KAT + CRYPTO_BYTES];
    unsigned long long smlen, mlen1;
    int fails = 0;

    for (int i = 0; i < N_KAT; i++) {{
        printf("=== KAT %d/%d ===\\n", i + 1, N_KAT);
        memcpy(m, TVEC_IN_M_SIGN[i], MLEN_KAT);

#if RUN_KEYGEN
        crypto_sign_keypair(pk, sk, TVEC_KEYPAIR_RND[i]); /*@PERF:keygen@*/
        if (memcmp(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES)) {{ printf("[%d] ERROR: PK mismatch\\n", i); fails++; }}
        if (memcmp(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES)) {{ printf("[%d] ERROR: SK mismatch\\n", i); fails++; }}
#else
        memcpy(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES);
        memcpy(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES);
#endif /* RUN_KEYGEN */

#if RUN_SIGN
        crypto_sign(sm, &smlen, m, MLEN_KAT, sk, TVEC_SIG_RND[i]); /*@PERF:sign@*/
        if (smlen != TVEC_IN_SM_SIGN_LEN[i] || memcmp(sm, TVEC_IN_SM_SIGN[i], TVEC_IN_SM_SIGN_LEN[i])) {{
            printf("[%d] ERROR: SM mismatch\\n", i); fails++;
        }}
#else
        memcpy(sm, TVEC_IN_SM_SIGN[i], TVEC_IN_SM_SIGN_LEN[i]);
        smlen = TVEC_IN_SM_SIGN_LEN[i];
#endif /* RUN_SIGN */

#if RUN_VERIFY
        crypto_sign_open(m1, &mlen1, sm, smlen, pk); /*@PERF:verify@*/
        if (mlen1 != MLEN_KAT || memcmp(m1, TVEC_IN_M_SIGN[i], MLEN_KAT)) {{ printf("[%d] ERROR: M mismatch\\n", i); fails++; }}
#endif /* RUN_VERIFY */
    }}

    if (fails == 0) {{
        printf("Test Successful (%d KAT vector(s))\\n", N_KAT);
    }} else {{
        printf("Test FAILED: %d mismatch(es)\\n", fails);
    }}

    return fails != 0;
}}
"""
    return header, main_c


def render_ds_derand_seed(variant: str, info: dict, entries: list, kat_num: int, flags: dict):
    """XMSS, LMS, HSS: keypair(pk,sk,seed), sign(sm,&smlen,m,mlen,sk), open(m,&mlen,sm,smlen,pk).
    Unlike every other ds_derand_* shape, signing takes no randomness at all
    (these three are deterministic given sk) -- so there's no *_rnd field,
    and crypto_sign() takes one fewer argument than render_ds_derand_rnd's."""
    suffix = info["vec_suffix"]
    guard = guard_name(suffix)
    mlen_kat = assert_constant(variant, entries, "mlen")

    keypair_seed = [e["keypair_seed"] for e in entries]
    msg = [e["msg"] for e in entries]
    pk = [e["pk"] for e in entries]
    sk = [e["sk"] for e in entries]
    sm = [e["sm"] for e in entries]

    header = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Variant: {variant}   KAT vectors: {kat_num}
///////////////////////////////////////////////////////////////////////////////////////

#ifndef {guard}
#define {guard}

#include <stdint.h>
#include <stddef.h>

#define N_KAT {kat_num}
#define MLEN_KAT {mlen_kat}

{c_2d_array("TVEC_KEYPAIR_SEED", keypair_seed)}

{c_2d_array("TVEC_IN_M_SIGN", msg)}

{c_2d_array("TVEC_OUT_PK", pk)}

{c_2d_array("TVEC_OUT_SK", sk)}

{c_var_field("TVEC_IN_SM_SIGN", sm)}

#endif /* {guard} */
"""

    main_c = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Static (no file I/O) KAT test for {variant}
///////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "api.h"
#include "test_vectors_{suffix}.h"

#define RUN_KEYGEN  {flags['run_keygen']}
#define RUN_SIGN    {flags['run_sign']}
#define RUN_VERIFY  {flags['run_verify']}

int main(void)
{{
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t m[MLEN_KAT];
    uint8_t sm[MLEN_KAT + CRYPTO_BYTES];
    uint8_t m1[MLEN_KAT + CRYPTO_BYTES];
    unsigned long long smlen, mlen1;
    int fails = 0;

    for (int i = 0; i < N_KAT; i++) {{
        printf("=== KAT %d/%d ===\\n", i + 1, N_KAT);
        memcpy(m, TVEC_IN_M_SIGN[i], MLEN_KAT);

#if RUN_KEYGEN
        crypto_sign_keypair(pk, sk, TVEC_KEYPAIR_SEED[i]); /*@PERF:keygen@*/
        if (memcmp(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES)) {{ printf("[%d] ERROR: PK mismatch\\n", i); fails++; }}
        if (memcmp(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES)) {{ printf("[%d] ERROR: SK mismatch\\n", i); fails++; }}
#else
        memcpy(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES);
        memcpy(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES);
#endif /* RUN_KEYGEN */

#if RUN_SIGN
        crypto_sign(sm, &smlen, m, MLEN_KAT, sk); /*@PERF:sign@*/
        if (smlen != TVEC_IN_SM_SIGN_LEN[i] || memcmp(sm, TVEC_IN_SM_SIGN[i], TVEC_IN_SM_SIGN_LEN[i])) {{
            printf("[%d] ERROR: SM mismatch\\n", i); fails++;
        }}
#else
        memcpy(sm, TVEC_IN_SM_SIGN[i], TVEC_IN_SM_SIGN_LEN[i]);
        smlen = TVEC_IN_SM_SIGN_LEN[i];
#endif /* RUN_SIGN */

#if RUN_VERIFY
        crypto_sign_open(m1, &mlen1, sm, smlen, pk); /*@PERF:verify@*/
        if (mlen1 != MLEN_KAT || memcmp(m1, TVEC_IN_M_SIGN[i], MLEN_KAT)) {{ printf("[%d] ERROR: M mismatch\\n", i); fails++; }}
#endif /* RUN_VERIFY */
    }}

    if (fails == 0) {{
        printf("Test Successful (%d KAT vector(s))\\n", N_KAT);
    }} else {{
        printf("Test FAILED: %d mismatch(es)\\n", fails);
    }}

    return fails != 0;
}}
"""
    return header, main_c


def render_ds_derand_rnd_nonce(variant: str, info: dict, entries: list, kat_num: int, flags: dict):
    """Falcon: keypair(pk,sk,rnd), sign(sm,&smlen,m,mlen,sk,nonce,rnd), open(m,&mlen,sm,smlen,pk)."""
    suffix = info["vec_suffix"]
    guard = guard_name(suffix)
    mlen_kat = assert_constant(variant, entries, "mlen")

    keypair_rnd = [e["keypair_rnd"] for e in entries]
    nonce = [e["nonce"] for e in entries]
    sign_rnd = [e["sign_rnd"] for e in entries]
    msg = [e["msg"] for e in entries]
    pk = [e["pk"] for e in entries]
    sk = [e["sk"] for e in entries]
    sm = [e["sm"] for e in entries]

    header = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Variant: {variant}   KAT vectors: {kat_num}
///////////////////////////////////////////////////////////////////////////////////////

#ifndef {guard}
#define {guard}

#include <stdint.h>
#include <stddef.h>

#define N_KAT {kat_num}
#define MLEN_KAT {mlen_kat}

{c_2d_array("TVEC_KEYPAIR_RND", keypair_rnd)}

{c_2d_array("TVEC_NONCE", nonce)}

{c_2d_array("TVEC_SIGN_RND", sign_rnd)}

{c_2d_array("TVEC_IN_M_SIGN", msg)}

{c_2d_array("TVEC_OUT_PK", pk)}

{c_2d_array("TVEC_OUT_SK", sk)}

{c_var_field("TVEC_IN_SM_SIGN", sm)}

#endif /* {guard} */
"""

    main_c = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Static (no file I/O) KAT test for {variant}
///////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "api.h"
#include "test_vectors_{suffix}.h"

#define RUN_KEYGEN  {flags['run_keygen']}
#define RUN_SIGN    {flags['run_sign']}
#define RUN_VERIFY  {flags['run_verify']}

int main(void)
{{
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t m[MLEN_KAT];
    uint8_t sm[MLEN_KAT + CRYPTO_BYTES];
    uint8_t m1[MLEN_KAT + CRYPTO_BYTES];
    unsigned long long smlen, mlen1;
    int fails = 0;

    for (int i = 0; i < N_KAT; i++) {{
        printf("=== KAT %d/%d ===\\n", i + 1, N_KAT);
        memcpy(m, TVEC_IN_M_SIGN[i], MLEN_KAT);

#if RUN_KEYGEN
        crypto_sign_keypair(pk, sk, TVEC_KEYPAIR_RND[i]); /*@PERF:keygen@*/
        if (memcmp(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES)) {{ printf("[%d] ERROR: PK mismatch\\n", i); fails++; }}
        if (memcmp(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES)) {{ printf("[%d] ERROR: SK mismatch\\n", i); fails++; }}
#else
        memcpy(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES);
        memcpy(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES);
#endif /* RUN_KEYGEN */

#if RUN_SIGN
        crypto_sign(sm, &smlen, m, MLEN_KAT, sk, TVEC_NONCE[i], TVEC_SIGN_RND[i]); /*@PERF:sign@*/
        if (smlen != TVEC_IN_SM_SIGN_LEN[i] || memcmp(sm, TVEC_IN_SM_SIGN[i], TVEC_IN_SM_SIGN_LEN[i])) {{
            printf("[%d] ERROR: SM mismatch\\n", i); fails++;
        }}
#else
        memcpy(sm, TVEC_IN_SM_SIGN[i], TVEC_IN_SM_SIGN_LEN[i]);
        smlen = TVEC_IN_SM_SIGN_LEN[i];
#endif /* RUN_SIGN */

#if RUN_VERIFY
        crypto_sign_open(m1, &mlen1, sm, smlen, pk); /*@PERF:verify@*/
        if (mlen1 != MLEN_KAT || memcmp(m1, TVEC_IN_M_SIGN[i], MLEN_KAT)) {{ printf("[%d] ERROR: M mismatch\\n", i); fails++; }}
#endif /* RUN_VERIFY */
    }}

    if (fails == 0) {{
        printf("Test Successful (%d KAT vector(s))\\n", N_KAT);
    }} else {{
        printf("Test FAILED: %d mismatch(es)\\n", fails);
    }}

    return fails != 0;
}}
"""
    return header, main_c


def _render_ds_drbg_common(variant, info, entries, kat_num, flags, *, guard, suffix,
                            mlen_kat, seed, msg, pk, sk, sm, extra_includes,
                            reseed_code):
    header = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Variant: {variant}   KAT vectors: {kat_num}
///////////////////////////////////////////////////////////////////////////////////////

#ifndef {guard}
#define {guard}

#include <stdint.h>
#include <stddef.h>

#define N_KAT {kat_num}
#define MLEN_KAT {mlen_kat}

{c_2d_array("TVEC_SEED", seed)}

{c_2d_array("TVEC_IN_M_SIGN", msg)}

{c_2d_array("TVEC_OUT_PK", pk)}

{c_2d_array("TVEC_OUT_SK", sk)}

{c_var_field("TVEC_IN_SM_SIGN", sm)}

#endif /* {guard} */
"""

    includes = "\n".join(f'#include "{h}"' for h in extra_includes)
    # crypto_sign_keypair() must run every iteration regardless of RUN_KEYGEN,
    # because it's what advances the shared CSPRNG/DRBG stream position that
    # crypto_sign() depends on -- these families have no explicit-randomness
    # ("derand") entry point, so skipping keygen would desync sign() even
    # when only checking keygen's own output is disabled. This also covers
    # keygens with a variable-length internal retry/rejection-sampling loop
    # (HAWK, FAEST): only calling the real function reproduces its exact
    # randomness consumption, which a hand-replicated "throwaway read of N
    # bytes" can't do safely.
    keygen_section = """        /* always called: advances the shared CSPRNG/DRBG stream so sign() lines up */
        crypto_sign_keypair(pk, sk); /*@PERF:keygen@*/
#if RUN_KEYGEN
        if (memcmp(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES)) { printf("[%d] ERROR: PK mismatch\\n", i); fails++; }
        if (memcmp(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES)) { printf("[%d] ERROR: SK mismatch\\n", i); fails++; }
#else
        memcpy(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES);
        memcpy(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES);
#endif /* RUN_KEYGEN */"""

    main_c = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Static (no file I/O) KAT test for {variant}
//
// NOTE: this family has no explicit-randomness ("derand") entry point --
// determinism comes from reseeding its internal CSPRNG/DRBG with the
// captured `seed` before each keygen+sign pair, exactly like
// kat/gen/main_{{cross,less,hawk,faest}}.c does for the dynamic KAT test.
// crypto_sign_keypair() is always called (even with RUN_KEYGEN=0) purely to
// advance that stream to the position sign() expects.
///////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

{includes}
#include "test_vectors_{suffix}.h"

#define RUN_KEYGEN  {flags['run_keygen']}
#define RUN_SIGN    {flags['run_sign']}
#define RUN_VERIFY  {flags['run_verify']}

int main(void)
{{
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t m[MLEN_KAT];
    uint8_t sm[MLEN_KAT + CRYPTO_BYTES];
    uint8_t m1[MLEN_KAT + CRYPTO_BYTES];
    unsigned long long smlen, mlen1;
    int fails = 0;

    for (int i = 0; i < N_KAT; i++) {{
        printf("=== KAT %d/%d ===\\n", i + 1, N_KAT);
        memcpy(m, TVEC_IN_M_SIGN[i], MLEN_KAT);

{reseed_code}

{keygen_section}

#if RUN_SIGN
        crypto_sign(sm, &smlen, m, MLEN_KAT, sk); /*@PERF:sign@*/
        if (smlen != TVEC_IN_SM_SIGN_LEN[i] || memcmp(sm, TVEC_IN_SM_SIGN[i], TVEC_IN_SM_SIGN_LEN[i])) {{
            printf("[%d] ERROR: SM mismatch\\n", i); fails++;
        }}
#else
        memcpy(sm, TVEC_IN_SM_SIGN[i], TVEC_IN_SM_SIGN_LEN[i]);
        smlen = TVEC_IN_SM_SIGN_LEN[i];
#endif /* RUN_SIGN */

#if RUN_VERIFY
        crypto_sign_open(m1, &mlen1, sm, smlen, pk); /*@PERF:verify@*/
        if (mlen1 != MLEN_KAT || memcmp(m1, TVEC_IN_M_SIGN[i], MLEN_KAT)) {{ printf("[%d] ERROR: M mismatch\\n", i); fails++; }}
#endif /* RUN_VERIFY */
    }}

    if (fails == 0) {{
        printf("Test Successful (%d KAT vector(s))\\n", N_KAT);
    }} else {{
        printf("Test FAILED: %d mismatch(es)\\n", fails);
    }}

    return fails != 0;
}}
"""
    return header, main_c


def render_ds_drbg_cross(variant: str, info: dict, entries: list, kat_num: int, flags: dict):
    suffix = info["vec_suffix"]
    mlen_kat = assert_constant(variant, entries, "mlen")
    seed = [e["seed"] for e in entries]
    msg = [e["msg"] for e in entries]
    pk = [e["pk"] for e in entries]
    sk = [e["sk"] for e in entries]
    sm = [e["sm"] for e in entries]

    reseed_code = ("        csprng_initialize(&platform_csprng_state, TVEC_SEED[i], 48, "
                   "CSPRNG_DOMAIN_SEP_CONST);")

    return _render_ds_drbg_common(
        variant, info, entries, kat_num, flags,
        guard=guard_name(suffix), suffix=suffix, mlen_kat=mlen_kat,
        seed=seed, msg=msg, pk=pk, sk=sk, sm=sm,
        extra_includes=["api.h", "csprng_hash.h", "parameters.h"],
        reseed_code=reseed_code,
    )


def render_ds_drbg_less(variant: str, info: dict, entries: list, kat_num: int, flags: dict):
    suffix = info["vec_suffix"]
    mlen_kat = assert_constant(variant, entries, "mlen")
    seed = [e["seed"] for e in entries]
    msg = [e["msg"] for e in entries]
    pk = [e["pk"] for e in entries]
    sk = [e["sk"] for e in entries]
    sm = [e["sm"] for e in entries]

    reseed_code = (
        "        /* LESS doesn't expose these in a public header (see kat/gen/main_less.c) */\n"
        "        extern SHAKE_STATE_STRUCT platform_csprng_state;\n"
        "        extern void initialize_csprng(SHAKE_STATE_STRUCT *shake_state, const unsigned char *seed, "
        "const uint32_t seed_len_bytes);\n"
        "        initialize_csprng(&platform_csprng_state, TVEC_SEED[i], 48);"
    )

    return _render_ds_drbg_common(
        variant, info, entries, kat_num, flags,
        guard=guard_name(suffix), suffix=suffix, mlen_kat=mlen_kat,
        seed=seed, msg=msg, pk=pk, sk=sk, sm=sm,
        extra_includes=["api.h", "parameters.h", "sha3.h"],
        reseed_code=reseed_code,
    )


def render_ds_drbg_hawk(variant: str, info: dict, entries: list, kat_num: int, flags: dict):
    suffix = info["vec_suffix"]
    mlen_kat = assert_constant(variant, entries, "mlen")
    seed = [e["seed"] for e in entries]
    msg = [e["msg"] for e in entries]
    pk = [e["pk"] for e in entries]
    sk = [e["sk"] for e in entries]
    sm = [e["sm"] for e in entries]

    reseed_code = (
        "        randombytes_init(TVEC_SEED[i], NULL, 256);\n"
        "        { uint8_t _skip[MLEN_KAT]; randombytes(_skip, MLEN_KAT); } "
        "/* advance past the msg draw the generator made before keygen */"
    )

    return _render_ds_drbg_common(
        variant, info, entries, kat_num, flags,
        guard=guard_name(suffix), suffix=suffix, mlen_kat=mlen_kat,
        seed=seed, msg=msg, pk=pk, sk=sk, sm=sm,
        extra_includes=["rng.h", "api.h"],
        reseed_code=reseed_code,
    )


def render_ds_drbg_faest(variant: str, info: dict, entries: list, kat_num: int, flags: dict):
    """FAEST (all 12 variants): keygen has no derand entry point (data-dependent
    rejection-sampling loop over rand_bytes(), no seed parameter) -- so with
    RUN_KEYGEN=1 it's still driven by reseeding the DRBG with `seed` exactly
    like ds_drbg_hawk. Sign, however, DOES have one
    (faest_<variant>_sign_with_randomness(), wired up via crypto_sign_with_
    randomness()), so it takes an explicit fixed-size `rho` instead of
    touching the DRBG -- meaning RUN_KEYGEN=0 can skip keygen entirely
    (load TVEC_OUT_PK/SK directly), same shape as ds_derand_rnd."""
    suffix = info["vec_suffix"]
    guard = guard_name(suffix)
    mlen_kat = assert_constant(variant, entries, "mlen")

    seed = [e["seed"] for e in entries]
    rho = [e["rho"] for e in entries]
    msg = [e["msg"] for e in entries]
    pk = [e["pk"] for e in entries]
    sk = [e["sk"] for e in entries]
    sm = [e["sm"] for e in entries]

    header = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Variant: {variant}   KAT vectors: {kat_num}
///////////////////////////////////////////////////////////////////////////////////////

#ifndef {guard}
#define {guard}

#include <stdint.h>
#include <stddef.h>

#define N_KAT {kat_num}
#define MLEN_KAT {mlen_kat}

{c_2d_array("TVEC_SEED", seed)}

{c_2d_array("TVEC_SIGN_RHO", rho)}

{c_2d_array("TVEC_IN_M_SIGN", msg)}

{c_2d_array("TVEC_OUT_PK", pk)}

{c_2d_array("TVEC_OUT_SK", sk)}

{c_var_field("TVEC_IN_SM_SIGN", sm)}

#endif /* {guard} */
"""

    main_c = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Static (no file I/O) KAT test for {variant}
//
// NOTE: keygen has no derand entry point -- determinism comes from
// reseeding the DRBG with the captured `seed` before keygen, exactly like
// kat/gen/main_faest.c does for the dynamic KAT test, and it only runs at
// all when RUN_KEYGEN=1. Sign, however, uses the explicit `rho` captured
// at generation time via crypto_sign_with_randomness(), so with
// RUN_KEYGEN=0 keygen is skipped entirely (not just discarded) -- pk/sk are
// loaded straight from TVEC_OUT_PK/SK.
///////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rng.h"
#include "api.h"
#include "test_vectors_{suffix}.h"

#define RUN_KEYGEN  {flags['run_keygen']}
#define RUN_SIGN    {flags['run_sign']}
#define RUN_VERIFY  {flags['run_verify']}

int main(void)
{{
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t m[MLEN_KAT];
    uint8_t sm[MLEN_KAT + CRYPTO_BYTES];
    uint8_t m1[MLEN_KAT + CRYPTO_BYTES];
    unsigned long long smlen, mlen1;
    int fails = 0;

    for (int i = 0; i < N_KAT; i++) {{
        printf("=== KAT %d/%d ===\\n", i + 1, N_KAT);
        memcpy(m, TVEC_IN_M_SIGN[i], MLEN_KAT);

#if RUN_KEYGEN
        randombytes_init(TVEC_SEED[i], NULL, 256);
        {{ uint8_t _skip[MLEN_KAT]; randombytes(_skip, MLEN_KAT); }} /* advance past the msg draw the generator made before keygen */
        crypto_sign_keypair(pk, sk); /*@PERF:keygen@*/
        if (memcmp(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES)) {{ printf("[%d] ERROR: PK mismatch\\n", i); fails++; }}
        if (memcmp(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES)) {{ printf("[%d] ERROR: SK mismatch\\n", i); fails++; }}
#else
        memcpy(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES);
        memcpy(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES);
#endif /* RUN_KEYGEN */

#if RUN_SIGN
        crypto_sign_with_randomness(sm, &smlen, m, MLEN_KAT, sk, TVEC_SIGN_RHO[i]); /*@PERF:sign@*/
        if (smlen != TVEC_IN_SM_SIGN_LEN[i] || memcmp(sm, TVEC_IN_SM_SIGN[i], TVEC_IN_SM_SIGN_LEN[i])) {{
            printf("[%d] ERROR: SM mismatch\\n", i); fails++;
        }}
#else
        memcpy(sm, TVEC_IN_SM_SIGN[i], TVEC_IN_SM_SIGN_LEN[i]);
        smlen = TVEC_IN_SM_SIGN_LEN[i];
#endif /* RUN_SIGN */

#if RUN_VERIFY
        crypto_sign_open(m1, &mlen1, sm, smlen, pk); /*@PERF:verify@*/
        if (mlen1 != MLEN_KAT || memcmp(m1, TVEC_IN_M_SIGN[i], MLEN_KAT)) {{ printf("[%d] ERROR: M mismatch\\n", i); fails++; }}
#endif /* RUN_VERIFY */
    }}

    if (fails == 0) {{
        printf("Test Successful (%d KAT vector(s))\\n", N_KAT);
    }} else {{
        printf("Test FAILED: %d mismatch(es)\\n", fails);
    }}

    return fails != 0;
}}
"""
    return header, main_c


def render_ds_drbg_hawk_rho(variant: str, info: dict, entries: list, kat_num: int, flags: dict):
    """HAWK: keygen has no derand entry point (Gaussian/NTRU sampling draws a
    data-dependent, variable amount of randomness via an rng callback, no
    seed parameter) -- so with RUN_KEYGEN=1 it's still driven by reseeding
    the DRBG with `seed` exactly like ds_drbg_hawk, and it only runs at all
    when RUN_KEYGEN=1. Sign, however, is driven entirely by whatever bytes
    its rng callback hands back, so crypto_sign_with_randomness() takes an
    explicit (variable-length) `rho` trace instead of touching the DRBG --
    meaning RUN_KEYGEN=0 can skip keygen entirely (load TVEC_OUT_PK/SK
    directly), same shape as ds_derand_rnd."""
    suffix = info["vec_suffix"]
    guard = guard_name(suffix)
    mlen_kat = assert_constant(variant, entries, "mlen")

    seed = [e["seed"] for e in entries]
    rho = [e["rho"] for e in entries]
    msg = [e["msg"] for e in entries]
    pk = [e["pk"] for e in entries]
    sk = [e["sk"] for e in entries]
    sm = [e["sm"] for e in entries]

    header = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Variant: {variant}   KAT vectors: {kat_num}
///////////////////////////////////////////////////////////////////////////////////////

#ifndef {guard}
#define {guard}

#include <stdint.h>
#include <stddef.h>

#define N_KAT {kat_num}
#define MLEN_KAT {mlen_kat}

{c_2d_array("TVEC_SEED", seed)}

{c_var_field("TVEC_SIGN_RHO", rho)}

{c_2d_array("TVEC_IN_M_SIGN", msg)}

{c_2d_array("TVEC_OUT_PK", pk)}

{c_2d_array("TVEC_OUT_SK", sk)}

{c_var_field("TVEC_IN_SM_SIGN", sm)}

#endif /* {guard} */
"""

    main_c = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Static (no file I/O) KAT test for {variant}
//
// NOTE: keygen has no derand entry point -- determinism comes from
// reseeding the DRBG with the captured `seed` before keygen, exactly like
// kat/gen/main_hawk.c does for the dynamic KAT test, and it only runs at
// all when RUN_KEYGEN=1. Sign, however, uses the explicit `rho` trace
// captured at generation time via crypto_sign_with_randomness(), so with
// RUN_KEYGEN=0 keygen is skipped entirely (not just discarded) -- pk/sk are
// loaded straight from TVEC_OUT_PK/SK.
///////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rng.h"
#include "api.h"
#include "test_vectors_{suffix}.h"

#define RUN_KEYGEN  {flags['run_keygen']}
#define RUN_SIGN    {flags['run_sign']}
#define RUN_VERIFY  {flags['run_verify']}

int main(void)
{{
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t m[MLEN_KAT];
    uint8_t sm[MLEN_KAT + CRYPTO_BYTES];
    uint8_t m1[MLEN_KAT + CRYPTO_BYTES];
    unsigned long long smlen, mlen1;
    int fails = 0;

    for (int i = 0; i < N_KAT; i++) {{
        printf("=== KAT %d/%d ===\\n", i + 1, N_KAT);
        memcpy(m, TVEC_IN_M_SIGN[i], MLEN_KAT);

#if RUN_KEYGEN
        randombytes_init(TVEC_SEED[i], NULL, 256);
        {{ uint8_t _skip[MLEN_KAT]; randombytes(_skip, MLEN_KAT); }} /* advance past the msg draw the generator made before keygen */
        crypto_sign_keypair(pk, sk); /*@PERF:keygen@*/
        if (memcmp(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES)) {{ printf("[%d] ERROR: PK mismatch\\n", i); fails++; }}
        if (memcmp(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES)) {{ printf("[%d] ERROR: SK mismatch\\n", i); fails++; }}
#else
        memcpy(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES);
        memcpy(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES);
#endif /* RUN_KEYGEN */

#if RUN_SIGN
        crypto_sign_with_randomness(sm, &smlen, m, MLEN_KAT, sk, TVEC_SIGN_RHO[i], TVEC_SIGN_RHO_LEN[i]); /*@PERF:sign@*/
        if (smlen != TVEC_IN_SM_SIGN_LEN[i] || memcmp(sm, TVEC_IN_SM_SIGN[i], TVEC_IN_SM_SIGN_LEN[i])) {{
            printf("[%d] ERROR: SM mismatch\\n", i); fails++;
        }}
#else
        memcpy(sm, TVEC_IN_SM_SIGN[i], TVEC_IN_SM_SIGN_LEN[i]);
        smlen = TVEC_IN_SM_SIGN_LEN[i];
#endif /* RUN_SIGN */

#if RUN_VERIFY
        crypto_sign_open(m1, &mlen1, sm, smlen, pk); /*@PERF:verify@*/
        if (mlen1 != MLEN_KAT || memcmp(m1, TVEC_IN_M_SIGN[i], MLEN_KAT)) {{ printf("[%d] ERROR: M mismatch\\n", i); fails++; }}
#endif /* RUN_VERIFY */
    }}

    if (fails == 0) {{
        printf("Test Successful (%d KAT vector(s))\\n", N_KAT);
    }} else {{
        printf("Test FAILED: %d mismatch(es)\\n", fails);
    }}

    return fails != 0;
}}
"""
    return header, main_c


def render_kem_prng_reseed(variant: str, info: dict, entries: list, kat_num: int, flags: dict):
    """HQC-2025: reseed prng_init(seed,...), keypair(pk,sk), enc(ct,ss,pk), dec(ss,ct,sk)."""
    suffix = info["vec_suffix"]
    guard = guard_name(suffix)

    seed = [e["seed"] for e in entries]
    pk = [e["pk"] for e in entries]
    sk = [e["sk"] for e in entries]
    ct = [e["ct"] for e in entries]
    ss = [e["ss"] for e in entries]

    header = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Variant: {variant}   KAT vectors: {kat_num}
///////////////////////////////////////////////////////////////////////////////////////

#ifndef {guard}
#define {guard}

#include <stdint.h>

#define N_KAT {kat_num}

{c_2d_array("TVEC_SEED", seed)}

{c_2d_array("TVEC_OUT_PK", pk)}

{c_2d_array("TVEC_OUT_SK", sk)}

{c_2d_array("TVEC_OUT_CT", ct)}

{c_2d_array("TVEC_OUT_SS", ss)}

#endif /* {guard} */
"""

    main_c = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Static (no file I/O) KAT test for {variant}
//
// NOTE: HQC-2025's keypair()/enc() have no explicit-randomness entry point --
// determinism comes from reseeding its internal SHAKE-256 PRNG (prng_init)
// with the captured `seed` before each keygen+encaps pair, exactly like
// kat/gen/main_hqc2025.c. With RUN_KEYGEN=0 the PRNG position used by
// encaps won't match generation time (same caveat as ds_drbg_hawk) --
// see tools/README.md.
///////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "api.h"
#include "symmetric.h"
#include "test_vectors_{suffix}.h"

#define RUN_KEYGEN  {flags['run_keygen']}
#define RUN_ENCAPS  {flags['run_encaps']}
#define RUN_DECAPS  {flags['run_decaps']}

int main(void)
{{
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
    uint8_t ss_enc[CRYPTO_BYTES];
    uint8_t ss_dec[CRYPTO_BYTES];
    int fails = 0;

    for (int i = 0; i < N_KAT; i++) {{
        printf("=== KAT %d/%d ===\\n", i + 1, N_KAT);
        prng_init(TVEC_SEED[i], NULL, 48, 0);

#if RUN_KEYGEN
        crypto_kem_keypair(pk, sk); /*@PERF:keygen@*/
        if (memcmp(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES)) {{ printf("[%d] ERROR: PK mismatch\\n", i); fails++; }}
        if (memcmp(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES)) {{ printf("[%d] ERROR: SK mismatch\\n", i); fails++; }}
#else
        memcpy(pk, TVEC_OUT_PK[i], CRYPTO_PUBLICKEYBYTES);
        memcpy(sk, TVEC_OUT_SK[i], CRYPTO_SECRETKEYBYTES);
#endif /* RUN_KEYGEN */

#if RUN_ENCAPS
        crypto_kem_enc(ct, ss_enc, pk); /*@PERF:encaps@*/
        if (memcmp(ct, TVEC_OUT_CT[i], CRYPTO_CIPHERTEXTBYTES)) {{ printf("[%d] ERROR: CT mismatch\\n", i); fails++; }}
        if (memcmp(ss_enc, TVEC_OUT_SS[i], CRYPTO_BYTES)) {{ printf("[%d] ERROR: SS(enc) mismatch\\n", i); fails++; }}
#else
        memcpy(ct, TVEC_OUT_CT[i], CRYPTO_CIPHERTEXTBYTES);
        memcpy(ss_enc, TVEC_OUT_SS[i], CRYPTO_BYTES);
#endif /* RUN_ENCAPS */

#if RUN_DECAPS
        crypto_kem_dec(ss_dec, ct, sk); /*@PERF:decaps@*/
        if (memcmp(ss_dec, TVEC_OUT_SS[i], CRYPTO_BYTES)) {{ printf("[%d] ERROR: SS(dec) mismatch\\n", i); fails++; }}
#endif /* RUN_DECAPS */
    }}

    if (fails == 0) {{
        printf("Test Successful (%d KAT vector(s))\\n", N_KAT);
    }} else {{
        printf("Test FAILED: %d mismatch(es)\\n", fails);
    }}

    return fails != 0;
}}
"""
    return header, main_c


def render_aead_direct(variant: str, info: dict, entries: list, kat_num: int, flags: dict):
    """ASCON AEAD variants (ascon128v12, ascon128av12, ascon80pqv12): no
    keygen, no derand/coins -- crypto_aead_encrypt/decrypt take
    key/nonce/ad/plaintext directly and are deterministic by construction.
    RUN_ENCAPS/RUN_DECAPS gate encrypt/decrypt (there's no keygen stage)."""
    suffix = info["vec_suffix"]
    guard = guard_name(suffix)

    key = [e["key"] for e in entries]
    nonce = [e["nonce"] for e in entries]
    ad = [e["ad"] for e in entries]
    pt = [e["pt"] for e in entries]
    ct = [e["ct"] for e in entries]

    header = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Variant: {variant}   KAT vectors: {kat_num}
///////////////////////////////////////////////////////////////////////////////////////

#ifndef {guard}
#define {guard}

#include <stdint.h>

#define N_KAT {kat_num}

{c_2d_array("TVEC_IN_KEY", key)}

{c_2d_array("TVEC_IN_NONCE", nonce)}

{c_2d_array("TVEC_IN_AD", ad)}

{c_2d_array("TVEC_IN_PT", pt)}

{c_2d_array("TVEC_OUT_CT", ct)}

#endif /* {guard} */
"""

    ad_len = len(ad[0])
    pt_len = len(pt[0])
    ct_len = len(ct[0])

    main_c = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Static (no file I/O) KAT test for {variant}
///////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "api.h"
#include "crypto_aead.h"
#include "test_vectors_{suffix}.h"

#define AD_LEN {ad_len}
#define PT_LEN {pt_len}
#define CT_LEN {ct_len}

#define RUN_ENCAPS  {flags['run_encaps']}
#define RUN_DECAPS  {flags['run_decaps']}

int main(void)
{{
    uint8_t ct[CT_LEN];
    uint8_t pt[PT_LEN];
    unsigned long long clen, mlen;
    int fails = 0;

    for (int i = 0; i < N_KAT; i++) {{
        printf("=== KAT %d/%d ===\\n", i + 1, N_KAT);

#if RUN_ENCAPS
        crypto_aead_encrypt(ct, &clen, TVEC_IN_PT[i], PT_LEN, TVEC_IN_AD[i], AD_LEN, NULL, TVEC_IN_NONCE[i], TVEC_IN_KEY[i]); /*@PERF:encrypt@*/
        if (clen != CT_LEN || memcmp(ct, TVEC_OUT_CT[i], CT_LEN)) {{ printf("[%d] ERROR: CT mismatch\\n", i); fails++; }}
#else
        memcpy(ct, TVEC_OUT_CT[i], CT_LEN);
        clen = CT_LEN;
#endif /* RUN_ENCAPS */

#if RUN_DECAPS
        crypto_aead_decrypt(pt, &mlen, NULL, ct, clen, TVEC_IN_AD[i], AD_LEN, TVEC_IN_NONCE[i], TVEC_IN_KEY[i]); /*@PERF:decrypt@*/
        if (mlen != PT_LEN || memcmp(pt, TVEC_IN_PT[i], PT_LEN)) {{ printf("[%d] ERROR: PT mismatch\\n", i); fails++; }}
#endif /* RUN_DECAPS */
    }}

    if (fails == 0) {{
        printf("Test Successful (%d KAT vector(s))\\n", N_KAT);
    }} else {{
        printf("Test FAILED: %d mismatch(es)\\n", fails);
    }}

    return fails != 0;
}}
"""
    return header, main_c


def render_hash_direct(variant: str, info: dict, entries: list, kat_num: int, flags: dict):
    """ASCON hash/XOF variants (asconhashv12, asconhashav12, asconxofv12,
    asconxofav12): single crypto_hash(out,in,inlen) call, no key/nonce/AD.
    There's only one stage, so RUN_HASH is always 1 -- it exists purely as
    the '#define RUN_' anchor apply_profiling() looks for, not a real toggle
    (no --run-hash CLI flag)."""
    suffix = info["vec_suffix"]
    guard = guard_name(suffix)

    msg = [e["msg"] for e in entries]
    md = [e["md"] for e in entries]

    header = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Variant: {variant}   KAT vectors: {kat_num}
///////////////////////////////////////////////////////////////////////////////////////

#ifndef {guard}
#define {guard}

#include <stdint.h>

#define N_KAT {kat_num}

{c_2d_array("TVEC_IN_MSG", msg)}

{c_2d_array("TVEC_OUT_MD", md)}

#endif /* {guard} */
"""

    msg_len = len(msg[0])
    md_len = len(md[0])

    main_c = f"""\
///////////////////////////////////////////////////////////////////////////////////////
// Auto-generated by tools/gen_static_test.py -- DO NOT EDIT BY HAND
// Static (no file I/O) KAT test for {variant}
///////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "api.h"
#include "crypto_hash.h"
#include "test_vectors_{suffix}.h"

#define MSG_LEN {msg_len}
#define MD_LEN  {md_len}

#define RUN_HASH  1

int main(void)
{{
    uint8_t md[MD_LEN];
    int fails = 0;

    for (int i = 0; i < N_KAT; i++) {{
        printf("=== KAT %d/%d ===\\n", i + 1, N_KAT);

        crypto_hash(md, TVEC_IN_MSG[i], MSG_LEN); /*@PERF:hash@*/
        if (memcmp(md, TVEC_OUT_MD[i], MD_LEN)) {{ printf("[%d] ERROR: MD mismatch\\n", i); fails++; }}
    }}

    if (fails == 0) {{
        printf("Test Successful (%d KAT vector(s))\\n", N_KAT);
    }} else {{
        printf("Test FAILED: %d mismatch(es)\\n", fails);
    }}

    return fails != 0;
}}
"""
    return header, main_c


SHAPE_RENDERERS = {
    "kem_derand": render_kem_derand,
    "ds_derand_ctx": render_ds_derand_ctx,
    "ds_derand_rnd": render_ds_derand_rnd,
    "ds_derand_seed": render_ds_derand_seed,
    "ds_derand_rnd_nonce": render_ds_derand_rnd_nonce,
    "ds_drbg_cross": render_ds_drbg_cross,
    "ds_drbg_less": render_ds_drbg_less,
    "ds_drbg_hawk": render_ds_drbg_hawk,
    "ds_drbg_faest": render_ds_drbg_faest,
    "ds_drbg_hawk_rho": render_ds_drbg_hawk_rho,
    "kem_prng_reseed": render_kem_prng_reseed,
    "aead_direct": render_aead_direct,
    "hash_direct": render_hash_direct,
}


# --------------------------------------------------------------------------
# Orchestration
# --------------------------------------------------------------------------

def load_registry():
    with open(REGISTRY_PATH) as f:
        return json.load(f)


def resolve_targets(registry, requested):
    variants = []
    for name in requested:
        if name in registry["families"]:
            variants.extend(registry["families"][name]["variants"])
        elif name in registry["variants"]:
            variants.append(name)
        else:
            supported_families = ", ".join(sorted(registry["families"]))
            supported_variants = ", ".join(sorted(registry["variants"]))
            todo = ", ".join(registry.get("_unsupported_families_todo", []))
            raise SystemExit(
                f"Unknown target '{name}'.\n"
                f"  Supported families: {supported_families}\n"
                f"  Supported variants: {supported_variants}\n"
                f"  Not yet implemented (need a new shape renderer): {todo}"
            )
    # de-duplicate, preserve order
    seen = set()
    out = []
    for v in variants:
        if v not in seen:
            seen.add(v)
            out.append(v)
    return out


def process_variant(registry, variant: str, kat_num: int, flags: dict, outdir: Path, profiling=None):
    info = registry["variants"][variant]
    shape = info["shape"]
    renderer = SHAPE_RENDERERS.get(shape)
    if renderer is None:
        raise SystemExit(f"{variant}: shape '{shape}' has no renderer implemented yet")

    rsp_path = ROOT / info["rsp_path"]
    if rsp_path.exists():
        rsp_path.unlink()  # force a clean regen; make's mtime-based rule won't rebuild otherwise

    # The gen_<variant> binary bakes KATNUM in at compile time via -DKATNUM=...,
    # but make's rule only depends on source file mtimes, not on KATNUM's value.
    # If a previous run already built it for a different --kat-num, make would
    # skip recompiling and silently reuse the old count. Force it too.
    # Derived from gen_target (not `variant`) since they can differ -- e.g.
    # XMSS/LMS/HSS variants share one gen_target ("gen-xmss") across many
    # registry keys ("XMSS-SHA2_10_256", ...), same as every other family
    # where they happen to already be equal.
    gen_name = info["gen_target"].removeprefix("gen-")
    gen_binary = ROOT / "build" / f"gen_{gen_name.replace('-', '_')}"
    if gen_binary.exists():
        gen_binary.unlink()

    print(f"[{variant}] generating {kat_num} KAT vector(s) via `make {info['gen_target']}`...")
    subprocess.run(
        ["make", "-C", str(ROOT), info["gen_target"], f"KATNUM={kat_num}"],
        check=True,
    )

    if not rsp_path.exists():
        if info.get("requires_riscv"):
            # Matches the Makefile's own arch-gated SKIP stub for this family
            # (see the ARCH guard around SLH-DSA-Haraka-AES): don't fail a
            # multi-variant run just because one variant needs RISC-V hardware
            # to even generate KAT data.
            print(f"[{variant}] SKIP: requires RISC-V hardware (gen_ binary is arch-gated on this host)")
            return
        raise SystemExit(f"{variant}: `make {info['gen_target']}` did not produce {rsp_path}.")

    entries = parse_rsp(rsp_path)
    if len(entries) < kat_num:
        raise SystemExit(f"{variant}: .rsp only contains {len(entries)} entries, need {kat_num}")
    entries = entries[:kat_num]

    header, main_c = renderer(variant, info, entries, kat_num, flags)
    main_c = apply_profiling(main_c, profiling)

    out_variant = outdir / variant
    out_variant.mkdir(parents=True, exist_ok=True)

    exclude_files = set(info.get("exclude_files", []))
    # extra_dirs: for families that split a shared-across-variants "common/"
    # dir out of src_dir/include_dir (currently just FAEST, to avoid
    # duplicating ~30 identical files x12 variants) -- flattened in here
    # alongside src_dir/include_dir so the static-test output stays fully
    # self-contained, same as every other family.
    for d in (info["src_dir"], info["include_dir"], *info.get("extra_dirs", [])):
        src_dir = ROOT / d
        for f in sorted(src_dir.iterdir()):
            if f.is_file() and f.name not in exclude_files:
                shutil.copy2(f, out_variant / f.name)

    (out_variant / f"test_vectors_{info['vec_suffix']}.h").write_text(header)
    (out_variant / "main.c").write_text(main_c)

    print(f"[{variant}] wrote static test to {out_variant}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--target", nargs="+", required=True,
                     help="Family name(s) (e.g. mlkem, mldsa) and/or specific variant name(s) (e.g. ml-kem-512)")
    ap.add_argument("--kat-num", type=int, default=1, help="Number of KAT vectors to bake into the static test (default: 1)")
    ap.add_argument("--run-keygen", type=int, choices=(0, 1), default=1)
    ap.add_argument("--run-sign", type=int, choices=(0, 1), default=1, help="DS families only")
    ap.add_argument("--run-verify", type=int, choices=(0, 1), default=1, help="DS families only")
    ap.add_argument("--run-encaps", type=int, choices=(0, 1), default=1, help="KEM families only")
    ap.add_argument("--run-decaps", type=int, choices=(0, 1), default=1, help="KEM families only")
    ap.add_argument("--outdir", type=Path, default=ROOT / "kat" / "static",
                     help="Output root directory (default: kat/static/ under this repo)")
    ap.add_argument("--profiling", default="none",
                     help="Name of a tools/profiling_commands/<name>.txt file to bake in as PERF_CNT_CYCLES=1 "
                          "cycle-counter hooks around keygen/sign-or-encaps/verify-or-decaps. "
                          "Default 'none': PERF_CNT_CYCLES=0, hooks present but empty.")
    args = ap.parse_args()

    if args.kat_num < 1:
        raise SystemExit("--kat-num must be >= 1")

    registry = load_registry()
    variants = resolve_targets(registry, args.target)
    profiling = load_profiling(args.profiling)

    flags = {
        "run_keygen": args.run_keygen,
        "run_sign": args.run_sign,
        "run_verify": args.run_verify,
        "run_encaps": args.run_encaps,
        "run_decaps": args.run_decaps,
    }

    for variant in variants:
        process_variant(registry, variant, args.kat_num, flags, args.outdir, profiling)


if __name__ == "__main__":
    main()
