"""Build crackdown2/ap/catalog.json from an --ap_probe_dump registry export.

The catalog is the single source of IDs for the in-game bridge (embedded at build time) and the apworld
(archipelago/crackdown2/data/catalog.json is a copy). Location ID = LOCATION_BASE + registry ordinal; the runtime
re-checks each ordinal's type and position against the loaded world before trusting it.
Races come from the probe's <dump>.races.json (race manager [0x83801614], 30 races in save order); a race's
location ID = LOCATION_BASE + 1000 + its ordinal.
Usage: make_ap_catalog.py <probe.json> [--climb crackdown2/ap/climb.csv] crackdown2/ap/catalog.json
       [archipelago/crackdown2/data/catalog.json]
"""
import csv
import hashlib
import json
import sys

VERSION = "crackdown2-ap-4"
LOCATION_BASE = 75_400_000  # Crackdown 1 uses 734xxxxx; distinct so both games can share a multiworld
ITEM_BASE = 75_410_000
SKILLS = ("Agility", "Driving", "Explosives", "Strength", "Firearms")  # native cSkillSet order
# TU 0.0.5.1 (tu/default.xexp) applied to the disc XEX; the only supported image.
XEXP_SHA256 = "a5e49fc1ff7f6f65e6f95a7b5c4b9392bb9e38674d1237ac0e2f0149d05929d7"
KINDS = {0: "agility", 1: "agility", 2: "agility", 3: "agility", 4: "hidden", 9: "audio_log", 10: "online"}
NAMES = {"agility": "Agility Orb", "hidden": "Hidden Orb", "audio_log": "Audio Log", "online": "Online Orb"}
# Logic v1 (docs/plan-archipelago.md "Logic"): native Agility level a pickup needs. The game's own agility orb tier
# (type 0-3, which rises with height: medians 48/56/73/117 m) plus absolute height for the tallest spots.
# Logic v2 adds the measured climb (crackdown2/ap/climb.csv from tools/re/ap_logic_from_sweep.py: the surface the
# teleport sweep landed on, minus the lowest sweep landing within 75 m), banded by CLIMB_BANDS. The level is the
# higher of the two. Playtest corrections go in AGILITY_OVERRIDES (registry ordinal -> level).
AGILITY_OVERRIDES = {}
CLIMB_BANDS = ((15, 1), (30, 2), (50, 3), (80, 4))  # climb below N metres -> level; anything higher -> 5


def agility_needed(p, climb):
    level = p["type"] + 1 if p["type"] <= 3 else 1
    y = p["pos"][1]
    if y >= 300:
        level = max(level, 5)
    elif y >= 150:
        level = max(level, 4)
    if p["n"] in climb:
        level = max(level, next((lv for limit, lv in CLIMB_BANDS if climb[p["n"]] < limit), 5))
    return AGILITY_OVERRIDES.get(p["n"], level)


# Race logic v1 (to tune in playtests): a road race needs Driving 2 (one Progressive Driving), a rooftop race
# Agility 3 (the races run across rooftops).
RACE_NEEDS = {"road": {"driving": 2, "agility": 1}, "rooftop": {"driving": 1, "agility": 3}}


def races_from(path):
    races = json.load(open(path))
    assert [r["n"] for r in races] == list(range(30)) and sorted(r["kind"] for r in races) == ["road"] * 15 + ["rooftop"] * 15
    counters, out = {}, []
    for r in races:
        counters[r["kind"]] = counters.get(r["kind"], 0) + 1
        out.append({"id": LOCATION_BASE + 1000 + r["n"], "ordinal": r["n"], "kind": r["kind"], "guid": r["guid"],
                    "name": f"{'Road' if r['kind'] == 'road' else 'Rooftop'} Race {counters[r['kind']]:02d}",
                    "position": [round(v, 3) for v in r["pos"]], **RACE_NEEDS[r["kind"]]})
    return out


EXPECTED = {"agility": 500, "hidden": 300, "audio_log": 52, "online": 80}


def main(src, dsts, climb_path=None):
    climb = {int(r["ordinal"]): float(r["climb"]) for r in csv.DictReader(open(climb_path))} if climb_path else {}
    raw = open(src, "rb").read()
    pickups = json.loads(raw)
    assert [p["n"] for p in pickups] == list(range(len(pickups))) and all(p["reg"] == p["n"] for p in pickups)
    counters, entries = {}, []
    for p in pickups:
        kind = KINDS[p["type"]]
        counters[kind] = counters.get(kind, 0) + 1
        entries.append({"id": LOCATION_BASE + p["n"], "ordinal": p["n"], "kind": kind, "type": p["type"],
                        "district": p["district"], "agility": agility_needed(p, climb), "name": f"{NAMES[kind]} {counters[kind]:03d}",
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
        "races": races_from(src + ".races.json"),
    }
    for dst in dsts:
        with open(dst, "w", encoding="ascii", newline="\n") as f:
            json.dump(catalog, f, indent=1)
            f.write("\n")


if __name__ == "__main__":
    args = sys.argv[2:]
    climb_file = None
    if args[:1] == ["--climb"]:
        climb_file, args = args[1], args[2:]
    main(sys.argv[1], args, climb_file)
