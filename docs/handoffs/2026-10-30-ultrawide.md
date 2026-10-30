# Handoff 2026-10-30: PC-port audit + ultrawide (Crackdown 2)

Branch `feat/pc-port-standard` (PR on pacific-city-recompiled/crackdown-2-recomp). Gap list and shared decisions:
`docs/PC-PORT-STANDARD.md` (the Crackdown 1 audit should match its "Shared decisions" table).

## Done

- `--aspect_ratio` (default `16:9`; `16:10`, `21:9`, `32:9`, `4:3` or a number, Hor+): `crackdown2/src/hooks_aspect.cpp`.
  Camera aspect f32 at **0x830A3124** (image value 16:9, but the first frame of a PC run read 4:3; written by the
  camera ctor `sub_8252C1E0` at 0x8252C2EC from 0x820F8D18, toggled 16:9/4:3 by the dev command `sub_8271C950`;
  readers 0x8224FB38, 0x82285AB8, 0x82291BD4, 0x825D367C, 0x825D3828). Written after the ctor and every guest
  frame (`hooks_frametime.cpp`). `ApplyGameDefaults` sets `video_mode_width/height` to 720 x ratio (like CD1).
- Verified on the PC (windowed, isolated user data, 150GS save, logs crackdown2_006/007): 21:9 shows more to the
  sides with the agent unstretched; 16:9 unchanged.

## Next items (one thread each)

1. **Launcher**: show the Aspect ratio row for Crackdown 2 (pacific-city-recompiled-launcher `main.cpp`, the
   `if(game_sel==0)` block around "Aspect ratio"). Same key `aspect_ratio`.
2. **Camera turn speed** (`camera_sensitivity`): CD1's tuning block (+28/+36 0.75, +32/+40 -0.625, +60/+64 3.0)
   has no static match in CD2. A live scan (temporary `--diag_camera_scan`, not committed) found only unrelated
   0.75 blocks at 0xB50887F8.. and 0xB5122410..; no 3.0 top yaw rate. Next: find the player-camera update from the
   right-stick reader instead (CD1: stick rescale sub_82413178, camera updates sub_824FAC28 / sub_824F9518).
3. **120/144 fps**: the measured clock already clamps to 1/240 s; test `--fps_cap=120/144` against the 60 fps
   checklist (stairs, crowd anims, cars, timers), then add the launcher entries.
4. **In-game PC Settings + Quit to Desktop** (CD1 `--pc_menu_mod`, XUI patches): large, Austin to confirm scope.
5. Rebinding UI, invert Y, monitor picker by name (launcher, shared with CD1). README refresh.
