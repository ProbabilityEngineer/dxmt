#!/usr/bin/env python3
"""Check that every winemetal unixcall code lines up with the dispatch table.

DXMT reaches Metal through a numeric unixcall code: the thunk in
winemetal_thunks.c calls UNIX_CALL(n, ...) and the handler sits at index n of
__wine_unix_call_funcs[] / __wine_unix_call_wow64_funcs[] in
winemetal/unix/winemetal_unix.c. Nothing in the build cross-checks the two, and
getting it wrong dispatches the wrong handler with the wrong argument struct --
a silent memory-corruption bug rather than a link error.

Adding a Metal 4 entry point means appending to three places that must agree, so
this runs as part of reviewing that change. It is not wired into CI (the DXMT
workflow is a build with no test job).

    python3 tests/check_winemetal_unixtable.py
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
THUNKS_C = ROOT / "src" / "winemetal" / "winemetal_thunks.c"
UNIX_C = ROOT / "src" / "winemetal" / "unix" / "winemetal_unix.c"

TABLES = (
    ("__wine_unix_call_funcs[]", "arm64"),
    ("__wine_unix_call_wow64_funcs[]", "wow64"),
)

# Pre-existing name differences that are not bugs. Both sides are stripped of a
# leading "&_" before comparison. WMTCopyAllDevices is a thunk whose handler is
# named MTLCopyAllDevices; the wow64 table additionally routes the SM50 thunks
# through thunk32_ wrappers.
ALIASES = {"WMTCopyAllDevices": {"MTLCopyAllDevices"}}
WOW64_PASSTHROUGH = re.compile(r"^thunk32_")


def read_table(lines, marker):
    start = next(i for i, l in enumerate(lines) if l.startswith("const void *" + marker))
    end = next(j for j in range(start, len(lines)) if lines[j].startswith("};"))
    entries = []
    for line in lines[start + 1 : end]:
        stripped = line.strip()
        if not stripped or stripped.startswith("//"):
            continue
        entries.append(stripped.rstrip(","))
    return entries


def main():
    thunk_src = THUNKS_C.read_text()
    unix_lines = UNIX_C.read_text().split("\n")

    # WINEMETAL_API <ret> Name(...) { ... UNIX_CALL(n, &params); ... }
    codes = {}
    for m in re.finditer(r"WINEMETAL_API[^\n]*\n(\w+)\(([^;{]*)\)\s*\{(.*?)\n\}", thunk_src, re.S):
        call = re.search(r"UNIX_CALL\((\d+),", m.group(3))
        if call:
            codes[m.group(1)] = int(call.group(1))

    tables = {name: read_table(unix_lines, marker) for marker, name in TABLES}

    errors = []

    for name, code in sorted(codes.items(), key=lambda kv: kv[1]):
        expected = "&_" + name
        for tname, entries in tables.items():
            if code >= len(entries):
                errors.append(f"{name}: code {code} is past the end of {tname} ({len(entries)} entries)")
                continue
            got = entries[code]
            if got == expected:
                continue
            got_name = got.lstrip("&_")
            if got_name in ALIASES.get(name, set()):
                continue
            if tname == "wow64" and WOW64_PASSTHROUGH.match(got_name):
                continue
            errors.append(f"{tname}[{code}] is {got}, but the thunk for {name} uses that code")

    # Every Metal 4 slot must have an implementation.
    metal4 = sorted(n for n in codes if "MTL4" in n or "Metal4" in n)
    implemented = set(re.findall(r"\n(_[A-Za-z0-9_]+)\(void \*obj\)", UNIX_C.read_text()))
    for name in metal4:
        if "_" + name not in implemented:
            errors.append(f"{name}: registered but has no _-prefixed handler in winemetal_unix.c")

    for marker, name in TABLES:
        print(f"{name}: {len(tables[name])} entries (codes 0..{len(tables[name]) - 1})")
    print(f"thunks carrying a code: {len(codes)}")
    print(f"Metal 4 entry points: {len(metal4)} -> {', '.join(metal4)}")

    if errors:
        print("\nMISMATCHES:")
        for e in errors:
            print("  " + e)
        return 1
    print("\nOK: every unixcall code maps to the correct slot in both tables, and every Metal 4 entry point is implemented")
    return 0


if __name__ == "__main__":
    sys.exit(main())