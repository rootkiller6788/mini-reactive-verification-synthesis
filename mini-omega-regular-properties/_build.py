#!/usr/bin/env python3
"""Generate all source files for mini-omega-regular-properties."""
import os
BASE = r"F:\nano-everything\mini-theory-of-computation\19. mini-reactive-verification-synthesis\mini-omega-regular-properties"
def w(path, content):
    full = os.path.join(BASE, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "w", encoding="utf-8") as f:
        f.write(content)
    lines = content.count("\n")
    print(f"  WROTE {path}: {lines} lines")
