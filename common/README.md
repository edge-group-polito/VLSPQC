# `common/` — shared `.rsp` parser

`kat_parser.h` / `kat_parser.c` implement a small, dependency-free parser for
NIST PQC KAT `.rsp` files, used by every `kat/gen/main_*.c` test runner (see
[`../kat/README.md`](../kat/README.md) for how it fits into the generate/test
flow).

- Reads one blank-line-separated block at a time into a `kat_entry_t`
  (up to `KAT_MAX_FIELDS` fields, each either a hex-decoded byte string or an
  integer).
- `kat_entry_get()` / `kat_get_bytes()` / `kat_get_long()` look up a field by
  name (e.g. `"pk"`, `"sign_rnd"`, `"enc_msg"`) regardless of which algorithm
  wrote it — this is what lets one parser serve every DS and KEM family
  without per-algorithm parsing code.
- `KAT_DATA_MAX` (256 KB) sizes the largest field seen across all variants
  (LESS public keys, ~197 KB).
- `kat_fprint_comment/int/hex()` are the matching write-side helpers used by
  the `PQCgenKAT_*.c` generators to produce `.rsp` files in the same format.
