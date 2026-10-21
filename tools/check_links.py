#!/usr/bin/env python3
"""
check_links.py -- verify that every relative link in the repository's Markdown files resolves.

For each `[text](target)` / `![alt](target)` outside fenced code blocks:
  * http(s):, mailto: and other URL schemes are not checked (no network access);
  * `path`, `path#anchor`, `#anchor`: the file or directory must exist and, for a Markdown target with an anchor, a heading
    with that GitHub-style slug must exist.

Skipped: build outputs (`build/`, `vectors/`, `bench/`), `.git/`, and third-party Markdown inside `algorithms/` that is not one
of the repository's own READMEs (anything not named README.md / UPSTREAM.md below `algorithms/`).

Usage (from the repo root):  python3 -I tools/check_links.py [files...]      exit status 1 if a link is broken
"""
import re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKIP_TOP = {"build", "vectors", "bench", ".git"}
LINK = re.compile(r"!?\[(?:[^\]\\]|\\.)*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")


def md_files():
    for p in sorted(ROOT.rglob("*.md")):
        rel = p.relative_to(ROOT)
        if rel.parts[0] in SKIP_TOP:
            continue
        if rel.parts[0] == "algorithms" and p.name not in ("README.md", "UPSTREAM.md"):
            continue
        yield p


def slug(h):
    h = re.sub(r"`", "", h.strip().lower())
    h = re.sub(r"[^\w\- ]", "", h, flags=re.UNICODE)
    return h.replace(" ", "-")


_anchor_cache = {}


def anchors(p):
    if p not in _anchor_cache:
        seen, out, fence = {}, set(), False
        for line in p.read_text(errors="replace").splitlines():
            if line.lstrip().startswith("```"):
                fence = not fence
            if fence:
                continue
            m = re.match(r"#{1,6}\s+(.*?)\s*#*\s*$", line)
            if m:
                s = slug(m.group(1))
                n = seen.get(s, 0)
                seen[s] = n + 1
                out.add(s if n == 0 else "%s-%d" % (s, n))
        _anchor_cache[p] = out
    return _anchor_cache[p]


def check(p):
    bad, fence = [], False
    for no, line in enumerate(p.read_text(errors="replace").splitlines(), 1):
        if line.lstrip().startswith("```"):
            fence = not fence
            continue
        if fence:
            continue
        for m in LINK.finditer(line):
            t = m.group(1)
            if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", t) or t.startswith("<"):
                continue
            path, _, frag = t.partition("#")
            target = p if path == "" else (p.parent / path).resolve()
            if not target.exists():
                bad.append((no, t, "missing file"))
            elif frag and target.suffix == ".md" and target.is_file() and frag.lower() not in anchors(target):
                bad.append((no, t, "missing anchor"))
    return bad


def main():
    files = [Path(a).resolve() for a in sys.argv[1:]] or list(md_files())
    total = nbad = 0
    for p in files:
        for no, t, why in check(p):
            nbad += 1
            print("%s:%d: %s -> %s" % (p.relative_to(ROOT), no, why, t))
        total += 1
    print("%d Markdown files checked, %d broken links" % (total, nbad))
    sys.exit(1 if nbad else 0)


if __name__ == "__main__":
    main()
