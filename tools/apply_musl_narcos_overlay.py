#!/usr/bin/env python3
import shutil
import sys
from pathlib import Path


def copy_tree(src: Path, dst: Path) -> None:
    for path in src.rglob("*"):
        if path.is_dir():
            continue
        rel = path.relative_to(src)
        out = dst / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, out)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: apply_musl_narcos_overlay.py /path/to/musl", file=sys.stderr)
        return 2
    repo_root = Path(__file__).resolve().parents[1]
    overlay = repo_root / "user" / "ports" / "musl" / "overlay"
    musl = Path(sys.argv[1]).resolve()
    if not overlay.is_dir():
        print(f"overlay not found: {overlay}", file=sys.stderr)
        return 1
    if not (musl / "src").is_dir() or not (musl / "arch").is_dir():
        print(f"not a musl source tree: {musl}", file=sys.stderr)
        return 1
    copy_tree(overlay, musl)
    print(f"applied NarcOs musl overlay to {musl}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
