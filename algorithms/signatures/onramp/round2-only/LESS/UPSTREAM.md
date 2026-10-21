# LESS — upstream provenance and license

| Item | Value |
|---|---|
| Upstream | https://github.com/less-sig/LESS |
| Closest upstream state | tag `v2.0.0` (commit `c57593900d4fd68f643d053b24f229e9dea43aea`, 2025-02-28); 26 of 47 distinct `.c` / `.h` files byte-identical (`v2.1.0`: 18, `v2.1.1`: 5) |
| License | source headers: "This code is hereby placed in the public domain." Upstream has carried a `LICENSE` file (CC0 1.0 Universal, [`LICENSE`](LICENSE) here) only since 2026-07-13 (commit `ce332f6`, shipped in `v2.1.1`); **the matched tag `v2.0.0` has no license file**, so the public-domain statement in the headers is the license evidence for this code, and the CC0 file is the upstream's current license |
| NIST status | round 2 candidate only, see [`../../README.md`](../../README.md) |

## How the pin was found
The repository did not record where this code came from. The pin above is the **closest upstream state found**, not a verified import point: every distinct `.c` / `.h` file of this
folder was hashed and compared with the files of each upstream tag (and the current branch head) and the best match is reported. The code here is a port with local changes
(see the family section of the category README), so a perfect match is not expected. Looked up 2026-10-21.
