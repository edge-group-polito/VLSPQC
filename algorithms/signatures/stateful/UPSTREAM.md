# Stateful hash-based signatures (XMSS, LMS, HSS) — upstream provenance and license

| Item | Value |
|---|---|
| XMSS upstream | https://github.com/XMSS/xmss-reference — closest state: branch `master` head, commit `171ccbd26f098542a67eb5d2b128281c80bd71a6` (2021-03-16); 17 of 28 distinct `.c` / `.h` files byte-identical |
| XMSS license | CC0 1.0 Universal ([`XMSS/LICENSE`](XMSS/LICENSE), copied from upstream) |
| LMS / HSS upstream | https://github.com/cisco/hash-sigs — closest state: branch `master` head, commit `cea4dda4f937e56ab42e1a74665f6397473a2838` (2026-10-09, the head when looked up); 33 of 49 distinct `.c` / `.h` files byte-identical (same library for LMS and HSS) |
| LMS / HSS license | Cisco Systems 3-clause BSD-style license, "Copyright (c) 2017 Cisco Systems, Inc." ([`LMS/LICENSE`](LMS/LICENSE), [`HSS/LICENSE`](HSS/LICENSE), copied from upstream `license.txt`) |
| Standard | NIST SP 800-208, final (October 2020) |

## How the pin was found
The repository did not record where this code came from. The pin above is the **closest upstream state found**, not a verified import point: every distinct `.c` / `.h` file of this
folder was hashed and compared with the files of each upstream tag (and the current branch head) and the best match is reported. The code here is a port with local changes
(see the family section of the category README), so a perfect match is not expected. Looked up 2026-10-21.
