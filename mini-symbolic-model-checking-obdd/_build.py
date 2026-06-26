import os, sys
BASE = r"F:\nano-everything\mini-theory-of-computation\19. mini-reactive-verification-synthesis\mini-symbolic-model-checking-obdd"
def w(path, content):
    p = os.path.join(BASE, path)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"  {path} ({len(content)} bytes, {content.count(chr(10))+1} lines)", flush=True)
