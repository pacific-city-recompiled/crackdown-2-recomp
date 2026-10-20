# Plan: Crackdown 2 Archipelago

Status: plan, 2026-10-20. Same function and scope as Crackdown 1's Archipelago (Crackdown Recomp 1.0,
`Unowa001/crackdown-recomp`: SETUP.md "Archipelago randomizer", `docs/plan-randomizer.md`, `docs/re-ap-bridge.md`,
`docs/re-m2-adapters.md`, release asset `crackdown.apworld` world_version 0.1.0). Guest addresses below are in the
title-update image (0.0.5.1) unless marked otherwise; nothing here is a hook yet.

## 1. What Crackdown 1 does (the shape to copy)

- **Everything runs inside the game exe.** `crackdown/src/ap/`: `client.cpp` (WinHTTP WebSocket, network thread only
  queues packets), `bridge.cpp` (one coordinator on the per-frame hook; game adapters for orbs, XP, skills, supply
  points, weapon deposits, bosses, goal), `catalog.cpp` (embedded `crackdown/ap/catalog.json`), `session_state.cpp`
  (sidecar `<user_data>/archipelago/session.toml`: seed/slot binding, native-save lineage, received inventory,
  pending checks, goal witness). No separate client window, no memory poking.
- **One catalog, two consumers.** `catalog.json` is generated from discovery data (`tools/re/make_ap_catalog.py`) and
  embedded in both the exe and the apworld, so IDs can't drift. IDs: locations 73400000+, items 73410000+.
- **Checks:** 500 agility orbs, 300 hidden orbs (option), 15 supply points, 19 weapon deposits, 21 bosses.
  **Items:** Progressive Agility/Strength/Firearms/Explosives/Driving (4 each), 19 weapons, filler "Agency
  Commendation". Skill XP from gameplay is dropped in a bound save; skills move only from received items.
  **Goal:** three kingpins (witnessed from a serialized native save), or max skills (dev).
- **Save binding:** a profile's save is bound to the first seed it connects to; other seeds are refused. Offline play
  keeps rules on and queues checks; reconnect resends them. Index-zero inventory replacement and resync per protocol.
- **apworld:** `__init__.py` (one region, permissive logic), `options.py` (include_hidden_orbs, goal), `client.py`
  (Archipelago Launcher entry "Crackdown Client" that starts CrackdownLauncher, `archipelago://` links passed as
  `--connect`), `data/catalog.json`, docs, tests. Host setting `crackdown_options.launcher_path`.
- **Launcher:** ARCHIPELAGO tab per save profile (server, slot, password). Password goes to
  `<profile>/archipelago/connection.toml`, never argv; the launcher passes `--ap_server --ap_slot`. `--connect <uri>`
  fills the tab.
- **Test tools:** `tools/re/ap_server.py` (local AP server), `ap_fake_server.py` (stall/silent), `ap-live.ps1`,
  `ap_replay.py`, discovery/snapshot scripts; host CTest for catalog/state/adapters.

## 2. Crackdown 2 check groups (from the game's own strings, counts to be confirmed in Phase 0)

Class and string evidence from `out/guest-image.bin` (TU image):

| Group | Evidence | First guess | Option |
|---|---|---|---|
| Agility orbs | `eOSE_AgilityOrbPickup` 0x82159378, stats `textTertiaryAgilityOrbs` 0x821662B8, `cSkillPickupProfile` 0x82145DA0 | 500 | always |
| Hidden orbs | `eOSE_HiddenOrbPickup` 0x821592FC, `textTertiaryHiddenOrbs` | 300 | `include_hidden_orbs` |
| Renegade agility orbs | `cRenegadeOrb` 0x820E8708, `cRenegadeOrbSpawnPoint`, "Renegade orb defeated!" 0x82106220 | ? | `include_renegade_orbs` |
| Renegade driving orbs | `textTertiaryDrivingRenegade` 0x82166340 | ? | same |
| Online orbs | `cOnlineOrb` 0x820E9E28 | ? | **off** (Xbox Live co-op only; multiplayer is parked) |
| Rooftop races | `VolkRooftopRace1..` 0x833E6C18 (Volk 5, Green Bay 5, Unity Heights 4, Agency Tower 1) | 15 | `include_races` |
| Road races | `VolkRoadrace1..` (Volk 5, Green Bay 5, Shai-Gen 5) | 15 | `include_races` |
| Wingsuit / vehicle stunt rings | `cStuntRing` 0x820E9D6C, `eOSE_StuntRing_Complete` | ? | `include_stunt_rings` |
| Audio logs | `cAudioLog` 0x820E9EF0, `cAudioLogContainer` | ? | `include_audio_logs` |
| Supply points | `cSupplyEntryPoint` 0x820E8F40 | ? | always |
| Tactical locations / Cell strongholds | `cTacticalObjective` 0x820F4370, `cCellStrongholdMission` 0x820F43D4 | ? | always |
| Absorption units (Freak Lairs) | "Absorption Unit" 0x8215976C, achievements "Get Connected"/"Plugged In" | ? | always |
| Beacons | `cBeaconMission` 0x820F4690 (Hope Springs, Green Bay, Unity Heights, Agency Tower) | ? | always |
| Freak Breaches | achievements "Pest Control"/"The Closer" | ? | `include_breaches` |
| Skill levels reached | only with `skill_mode = vanilla` | 5x5 | off |

Not checks: achievements as such (most are stunts), Keys to the City (sandbox cheat mode; blocked in a bound save),
Deluge (multiplayer mode). DLC: **Toy Box** adds skill level 6 ("Beyond the Infinite"), thrusters, mag grenades and the
squad chopper, so it changes the item pool: option `toy_box` adds a fifth Progressive level per skill and the Toy Box
gear as items. Pre-order armor is cosmetic.

## 3. Items

| Item | Count | Class |
|---|---|---|
| Progressive Agility / Driving / Explosives / Firearms / Strength | 4 each (5 with `toy_box`) | progression |
| Weapons / gear (supply-point unlocks; CD2 unlocks weapons by skill level and deposits; Phase 0 decides which applies) | ? | useful (progression if logic needs it) |
| Wingsuit (unlocked by an Agility level in vanilla; level to confirm) | 1 | progression (gates wingsuit rings) |
| Supply point / tactical location unlocks (spawn points) | option | useful |
| Agency Commendation | rest | filler |

Same rule as Crackdown 1: gameplay XP is dropped for the primary agent in a bound save; received items set skill
levels absolutely (re-applied after any older native load).

## 4. Goal and logic

- `goal = sunburst` (default): complete the final phase of Project Sunburst at Agency Tower ("Tower Power"),
  witnessed from a serialized native save like CD1's kingpin witness. `goal = beacons` (all beacons) and
  `max_skills` (dev) as alternatives.
- Logic v0 is permissive like CD1 (one region, everything reachable). Real rules after playtests: Beacons need the
  Absorption Units of their district; rooftop races and high orbs need Agility bands; road races and driving
  renegade orbs need Driving; wingsuit rings need the Wingsuit; Agency Tower final phase needs every other beacon.
  The story's mission order is the main thing to verify, because CD2 is more linear than CD1.

## 5. Runtime (crackdown2/src/ap/, same files as CD1)

Port CD1's game-agnostic parts as they are (they are this project's own code): `client.cpp`, `session_state.cpp`,
`catalog.cpp`, the CTest host tests, `toml_edit`. Rewrite the adapters for CD2's addresses:

1. per-frame coordinator (present hook 0x82385DA8 already exists in `hooks_frametime.cpp`),
2. orb pickup (agility/hidden/renegade) -> check, no XP,
3. XP award and skill setter (drop XP, set absolute level),
4. supply point / tactical location / absorption unit / beacon / race / stunt ring / audio log completion -> check,
5. weapon unlock path (deposit or skill-level unlock) -> filtered by received items,
6. native save/load serializers for binding and the goal witness,
7. block Keys to the City in a bound save.

Cvars as CD1: `--ap_server`, `--ap_slot`, `--ap_password` (manual), password from
`<user_data>/archipelago/connection.toml`. User data is `Documents\crackdown2` (title 4D5308BC).

## 6. apworld (in this repo, `archipelago/crackdown2/`)

Crackdown 1's world source lives outside its repo (an uncommitted APWorldMaker folder); only the built .apworld is
released. For CD2 the world source is committed here: `archipelago/crackdown2/` (game "Crackdown 2",
`crackdown2_options.launcher_path`, component "Crackdown 2 Client"), with `tools/build_apworld.py` zipping it to
`out/crackdown2.apworld`. Location IDs 75400000+, items 75410000+ (CD1 uses 734xxxxx; a different base keeps both
games in one multiworld without clashes).

## 7. Launcher (owned by the "Combined Crackdown launcher" thread)

The CD2 game page needs the same ARCHIPELAGO tab as CD1 (per profile; `connection.toml`; `--ap_server --ap_slot`;
`--connect <uri>` routes to the CD2 profile when the URI's game is Crackdown 2). Nothing here edits launcher code.

## 8. Phase 0 RE (blocking; results go in `docs/re-notes.md` "Randomizer notes")

Order: orbs and skills first (vertical slice), then the rest. Method as CD1: `--ap_trace` style logging hooks,
`--auto_input` scripted runs from the test save, image dumps, generated C++ for disassembly.

1. Pickup manager and orb table (count, type, position, collected flags), pickup function, XP award, skill setter.
2. Save/load serializers and the save file layout (enough for lineage).
3. Supply points, tactical locations/strongholds, absorption units, beacons, breaches: tables and completion flags.
4. Races, stunt rings, audio logs, renegade orbs.
5. Weapon unlock rules; Toy Box gating (skill level 6).
6. Final mission completion flag for the goal.

## 9. Milestones

- **M0** this plan + Phase 0 orbs/skills + catalog generator.
- **M1** vertical slice: connect, agility orbs as checks, Progressive Agility as items, binding, offline queue.
- **M2** full pool: every group in section 2, goal witness, Keys to the City block.
- **M3** apworld options/logic from playtests, launcher tab (other thread), setup docs.
- **M4** release with the apworld and YAML as release assets, like CD1.
