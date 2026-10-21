# ASCON — upstream provenance and license

| Item | Value |
|---|---|
| Upstream | https://github.com/ascon/ascon-c |
| Closest upstream state | tag `v1.2.5` (commit `aa21b952d29ceffdd802bc03771cd7a3442a7a6f`); 22 of 25 distinct `.c` / `.h` files byte-identical (`v1.2.4`: 11, `v1.3.0`: 0) — consistent with the `*v12` variant names (Ascon v1.2) |
| License | CC0 1.0 Universal ([`LICENSE`](LICENSE), copied from tag `v1.2.5`) |
| Standard | NIST SP 800-232 (final, August 2025) defines different functions (Ascon-AEAD128, Ascon-Hash256, ...); these v1.2 KATs are not claimed to match it |

## How the pin was found
The repository did not record where this code came from. The pin above is the **closest upstream state found**, not a verified import point: every distinct `.c` / `.h` file of this
folder was hashed and compared with the files of each upstream tag (and the current branch head) and the best match is reported. The code here is a port with local changes
(see the family section of the category README), so a perfect match is not expected. Looked up 2026-10-21.
