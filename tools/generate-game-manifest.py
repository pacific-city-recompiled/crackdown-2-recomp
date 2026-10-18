"""Write the launcher's disc manifest (path, size, SHA256; metadata only) from an extracted Crackdown 2 disc.
The title update's files (default.xexp, TU/) are not disc files and are left out.
Usage: py -I tools/generate-game-manifest.py <game folder> [--output game-manifest.txt]"""
import argparse
import hashlib
from pathlib import Path

SUPPORTED = "8959c449e382661d29b8ea479c685f506cd5bd3eca40b3d73a51cc10f281069f"  # World disc default.xex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--output", type=Path, default=Path("game-manifest.txt"))
    args = parser.parse_args()
    def rel(p):
        return p.relative_to(args.folder).as_posix()
    files = sorted((p for p in args.folder.rglob("*") if p.is_file() and p.name != ".verified"
                    and rel(p).lower() != "default.xexp" and not rel(p).lower().startswith("tu/")),
                   key=lambda p: rel(p).lower())
    xex = [p for p in files if rel(p).lower() == "default.xex"]
    if len(xex) != 1:
        parser.error("folder has no default.xex")
    with xex[0].open("rb") as stream:
        if hashlib.file_digest(stream, "sha256").hexdigest() != SUPPORTED:
            parser.error("default.xex is not the supported World disc version")
    with args.output.open("w", newline="\n") as out:
        out.write("# Crackdown 2 World disc; path<TAB>bytes<TAB>SHA256 (metadata only, no game data)\n")
        for p in files:
            with p.open("rb") as stream:
                digest = hashlib.file_digest(stream, "sha256").hexdigest()
            out.write(f"{rel(p)}\t{p.stat().st_size}\t{digest}\n")
    print(f"{len(files)} files -> {args.output}")


if __name__ == "__main__":
    main()
