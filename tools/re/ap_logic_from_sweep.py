"""Estimate each pickup's climb height from --ap_dev_sweep CSVs (crackdown2/src/ap/ap_probe.cpp).

surface = where the agent landed after being put on the pickup (the roof/ledge it sits on; the orb's own height
when it floats over nothing). street = lowest surface among sweep landings within RADIUS metres (pickup rows and
any ring-drop rows). climb = surface - street. Prints a summary per catalog tier and writes
ordinal,kind,type,orb_y,surface,street,climb rows to the output CSV.
Usage: ap_logic_from_sweep.py crackdown2/ap/catalog.json out.csv sweep1.csv [sweep2.csv ...]
"""
import csv
import json
import math
import sys
from collections import defaultdict

RADIUS = 75.0


def main(catalog_path, out_path, sweeps):
    cat = {p["ordinal"]: p for p in json.load(open(catalog_path))["pickups"]}
    surface, points = {}, []
    for path in sweeps:
        for r in csv.DictReader(open(path)):
            x, y, z = float(r["x"]), float(r["y"]), float(r["z"])
            points.append((x, y, z))
            if r["what"] == "pickup":
                o = int(r["ordinal"])
                ty = float(r["ty"])
                surface[o] = y if ty - y < 4.0 else ty  # fell far: the orb floats over nothing
    rows = []
    for o, s in sorted(surface.items()):
        p = cat[o]
        px, pz = p["position"][0], p["position"][2]
        near = [y for x, y, z in points if math.hypot(x - px, z - pz) <= RADIUS]
        street = min(near) if near else s
        rows.append((o, p["kind"], p["type"], p["position"][1], s, street, s - street))
    with open(out_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["ordinal", "kind", "type", "orb_y", "surface", "street", "climb"])
        for r in rows:
            w.writerow([r[0], r[1], r[2], f"{r[3]:.1f}", f"{r[4]:.1f}", f"{r[5]:.1f}", f"{r[6]:.1f}"])
    by = defaultdict(list)
    for r in rows:
        by[(r[1], r[2])].append(r[6])
    for key, v in sorted(by.items()):
        v.sort()
        q = lambda f: v[min(len(v) - 1, int(f * len(v)))]
        print(key, len(v), "climb p10 %.0f p50 %.0f p90 %.0f max %.0f" % (q(.1), q(.5), q(.9), v[-1]))
    print("pickups covered:", len(rows), "of", len(cat))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3:])
