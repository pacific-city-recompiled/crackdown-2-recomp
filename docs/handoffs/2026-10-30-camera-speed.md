# Handoff 2026-10-30: camera turn speed (`--camera_sensitivity`)

Branch `feat/camera-sensitivity` (off `feat/pc-port-standard`, PR #2): PR #3 https://github.com/pacific-city-recompiled/crackdown-2-recomp/pull/3

## What changed

- `crackdown2/src/hooks_camera.cpp`: `--camera_sensitivity` (default `1.0`, 0.25-4.0, hot reload), the same key and
  range as Crackdown 1. Mouse look feeds the right stick (SDK MnK driver), so it covers the mouse too.
- How: the player-camera updates `sub_8223F520` / `sub_8224FF60` read their turn rates from a tuning object whose
  pointer is at **0x83800AD0** (built by `sub_825587D0`, published at 0x82558834; values parsed from game data and
  unit-converted in `sub_82558A48`, which is why the earlier scan for CD1's values found nothing). Unlike CD1, each
  rate is a triple indexed by camera mode (0-2), read with `lfsx`:
  base yaw +28/+52 (`sub_82395448`, 0x82395598), base pitch +40/+64 (`sub_82227DF0`, 0x82227F40), top yaw +76/+88
  (0x82240164; `sub_8224FF60` 0x822503AC/0x822503B8). The hook on `sub_82395448` (called by both updates before any
  rate is read) scales all 18 floats from saved originals and rescales after a reload. At 1.0 it never touches memory.
- Loaded values (log): yaw 0.75 0.5 0.375, pitch -0.625 -0.5 -0.375, top yaw 4.5 2 0.625.

## Evidence

- Disassembly: Codex trace from the right-stick deadzone call (0x82331250/0x82331288, deadzone 8689) to the camera
  updates (notes in `out/cd2-camscan/answer.md`, local).
- PC playtest (windowed, muted, isolated data in `out/cd2-portstd-test`, scripted `RS_RIGHT:700` in the city): the
  minimap rotated 75° at 1.0 and 151° at 2.0 (2.01x). Bisect: scaling base yaw or pitch alone leaves this turn at
  ~75°; top yaw alone gives 131°, so the held turn is dominated by the top rate. Logs crackdown2_008-014.
- Codex review of the diff (see PR).

## Not covered / next

1. Launcher: show the camera speed slider for Crackdown 2 (shared launcher `main.cpp`, same key).
2. Vehicle-associated camera `sub_827229F0` has its own multipliers (B+360 / B+340, overrides 0x8309AA5C /
   0x8309AA58); scaling them would make driving camera follow the setting too. Unverified whether it needs it.
3. Pitch at 2x was not checked separately on screen (same code path as yaw; RS up/down test would confirm).
