"""Zip archipelago/crackdown2 into crackdown2.apworld (default out/crackdown2.apworld).

Refuses to build when the world's data/catalog.json differs from crackdown2/ap/catalog.json (the file the game
embeds), so the two can't drift. Usage: python tools/build_apworld.py [output path]
"""
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORLD = ROOT / "archipelago" / "crackdown2"


def main(out: Path) -> None:
    if (WORLD / "data" / "catalog.json").read_bytes() != (ROOT / "crackdown2" / "ap" / "catalog.json").read_bytes():
        sys.exit("archipelago/crackdown2/data/catalog.json differs from crackdown2/ap/catalog.json; "
                 "re-run tools/re/make_ap_catalog.py with both outputs")
    out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for path in sorted(WORLD.rglob("*")):
            if path.is_file() and "__pycache__" not in path.parts:
                z.write(path, Path("crackdown2") / path.relative_to(WORLD))
    print(f"wrote {out}")


if __name__ == "__main__":
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "out" / "crackdown2.apworld")
