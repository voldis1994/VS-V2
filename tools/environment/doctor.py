#!/usr/bin/env python3
from __future__ import annotations
import shutil, sys

def check(name: str, ok: bool, detail: str = "") -> None:
    status = "OK" if ok else "MISSING"
    print(f"[{status}] {name} {detail}")

def main() -> int:
    check("cmake", shutil.which("cmake") is not None)
    check("ninja", shutil.which("ninja") is not None)
    check("node", shutil.which("node") is not None)
    check("npm", shutil.which("npm") is not None)
    check("python3", shutil.which("python3") is not None)
    check("psql", shutil.which("psql") is not None, "(optional)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
