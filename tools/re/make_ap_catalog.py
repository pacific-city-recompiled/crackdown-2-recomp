"""Build crackdown2/ap/catalog.json from an --ap_probe_dump registry export.

The catalog is the single source of IDs for the in-game bridge (embedded at build time) and the apworld
(archipelago/crackdown2/data/catalog.json is a copy). Location ID = LOCATION_BASE + registry ordinal; the runtime
re-checks each ordinal's type and position against the loaded world before trusting it.
Usage: make_ap_catalog.py <probe.json> crackdown2/ap/catalog.json [archipelago/crackdown2/data/catalog.json]
"""
import hashlib
import json
import sys

VERSION = "crackdown2-ap-2"
LOCATION_BASE = 75_400_000  # Crackdown 1 uses 734xxxxx; distinct so both games can share a multiworld
ITEM_BASE = 75_410_000
SKILLS = ("Agility", "Driving", "Explosives", "Strength", "Firearms")  # native cSkillSet order
# TU 0.0.5.1 (tu/default.xexp) applied to the disc XEX; the only supported image.
XEXP_SHA256 = "a5e49fc1ff7f6f65e6f95a7b5c4b9392bb9e38674d1237ac0e2f0149d05929d7"
KINDS = {0: "agility", 1: "agility", 2: "agility", 3: "agility", 4: "hidden", 9: "audio_log", 10: "online"}
NAMES = {"agility": "Agility Orb", "hidden": "Hidden Orb", "audio_log": "Audio Log", "online": "Online Orb"}
# Logic v1 (docs/plan-archipelago.md "Logic"): native Agility level a pickup needs. The game's own agility orb tier
# (type 0-3, which rises with height: medians 48/56/73/117 m) plus absolute height for the tallest spots.
# Playtest corrections go in AGILITY_OVERRIDES (registry ordinal -> level).
AGILITY_OVERRIDES = {}


def agility_needed(p):
    level = p["type"] + 1 if p["type"] <= 3 else 1
    y = p["pos"][1]
    if y >= 300:
        level = max(level, 5)
    elif y >= 150:
        level = max(level, 4)
    return AGILITY_OVERRIDES.get(p["n"], level)


EXPECTED = {"agility": 500, "hidden": 300, "audio_log": 52, "online": 80}


def main(src, dsts):
    raw = open(src, "rb").read()
    pickups = json.loads(raw)
    assert [p["n"] for p in pickups] == list(range(len(pickups))) and all(p["reg"] == p["n"] for p in pickups)
    counters, entries = {}, []
    for p in pickups:
        kind = KINDS[p["type"]]
        counters[kind] = counters.get(kind, 0) + 1
        entries.append({"id": LOCATION_BASE + p["n"], "ordinal": p["n"], "kind": kind, "type": p["type"],
                        "district": p["district"], "agility": agility_needed(p), "name": f"{NAMES[kind]} {counters[kind]:03d}",
                        "position": [round(v, 3) for v in p["pos"]]})
    assert counters == EXPECTED, counters
    items = {f"Progressive {name}": ITEM_BASE + i for i, name in enumerate(SKILLS)}
    items["Agency Commendation"] = ITEM_BASE + 1000
    catalog = {
        "version": VERSION, "game": "Crackdown 2", "supported_xexp_sha256": XEXP_SHA256,
        "source": {"file": "ap_probe_dump (crackdown2_003)", "sha256": hashlib.sha256(raw).hexdigest()},
        "location_base": LOCATION_BASE, "item_base": ITEM_BASE, "items": items,
        "skills": [{"index": i, "name": name, "item": ITEM_BASE + i, "starting": 1, "maximum": 5}
                   for i, name in enumerate(SKILLS)],
        "filler_item": ITEM_BASE + 1000,
        "pickups": entries,
    }
    for dst in dsts:
        with open(dst, "w", encoding="ascii", newline="\n") as f:
            json.dump(catalog, f, indent=1)
            f.write("\n")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
