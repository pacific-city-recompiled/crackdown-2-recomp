# PC-port standard: Crackdown 2 audit (2026-10-30)

What the recomp/decomp community treats as a port that "feels like a PC port", checked against Crackdown 2, with
the decisions both Crackdown recomps should share. **The Crackdown 1 audit should match the names, defaults, keys
and paths in "Shared decisions" below**, and record where it differs and why.

Sources: Austin's checklist (the linkssy2 / TGP482 standard: Darkwatch Darker Edition, Urban Chaos Burner
Edition, SCDA-XBERecomp, BLACK-XBERecomp), the r/decomps thread "Shoutout to all the projects that actually work to
FEEL like a pc port" (Oct 2026), and the ports it names (Unleashed Recompiled, Edge of Time Recompiled, Reblue,
Ship of Harkinian, Zelda64Recomp).

## What the community asks for

From the thread, in the order people brought it up:

- **Settings inside the game**, not only in a launcher or config file ("I love when the settings are available in
  game"; Unleashed, Edge of Time and Reblue named as examples). One commenter's caveat: a quirky 3D menu can't take
  new rows cleanly, and then a good ImGui panel is the honest fallback.
- **Mouse control in menus** (cited as the moment people knew a linkssy2/TGP482 port was "quality").
- **An in-game quit button**, though Alt+F4 being instant was also praised.
- **A checklist**: aspect ratios, keyboard+mouse and controller, quit button, fps limit or unlocked. F1 is becoming a
  common key for options.
- **Portable packages** that run out of the box with no emulator setup, so people can archive them.
- **Performance first** (60 fps at native resolution on weak hardware).

## Gap list, ranked by impact

Impact = how many players notice it times how bad it is when missing. "CD1" = what the Crackdown 1 recomp (1.3.x)
already has, which usually makes the Crackdown 2 work a port of a known hook.

| # | Gap | Crackdown 2 today | CD1 | Plan |
|---|---|---|---|---|
| 1 | **Ultrawide / other aspect ratios** | 16:9 only; launcher hides the row for CD2 | `--aspect_ratio` (hooks_aspect.cpp: camera aspect float after the camera ctor + matching video mode) | Find CD2's camera ctor and aspect float, port the hook, enable the launcher row. Small. |
| 2 | **Settings and Quit to Desktop in the game's own menu** | None; settings live in the launcher and the SDK's unthemed F4 overlay | `--pc_menu_mod`: native XUI "PC Settings" page (frame rate, resolution, display) + Quit to Desktop, via hash-gated XUI patches | Map CD2's menu/scene code (screen registry, XuiElementGetChildById, Options screen), then the same patch approach. Large; ask Austin before starting. |
| 3 | **Camera turn speed** (stick and mouse) | Game's own only | `--camera_sensitivity` (hooks_camera.cpp: scales the turn-rate tuning block) | Find CD2's player-camera tuning block (same engine), port, enable the launcher slider. Small to medium. |
| 4 | **Frame rates above 60** | 60 (default) or 30 | 120 / 144 (experimental) | CD2's measured clock (`--sim_game_clock`) already scales dt; test 120/144 with the 60 fps checklist (stairs, crowd anims, cars, timers). Medium, mostly testing. |
| 5 | **Rebinding UI** | `keybind_*` cvars + the F4 overlay's keybind page (unthemed); the launcher only shows a help sentence | Same | Launcher "Controls" page: list the binds, click-to-rebind for keyboard; pad remap later. Shared launcher, so CD1 gets it too. Medium. |
| 6 | **Invert Y** (stick and mouse) | Game option for the stick? (check the Options menu); no mouse invert | No | `mnk_invert_y` in the SDK patch stack. Small. |
| 7 | **Mouse cursor in menus** | Menus are pad-only (keys map to the pad) | No | Needs XUI hit-testing; after #2, since it uses the same menu map. Large. |
| 8 | **Button prompts that match the device** | Xbox 360 prompts only | No | Swap the prompt glyphs for PlayStation/Switch/keyboard by connected device, art from the user's disc or license-clean glyph sets. Large. |
| 9 | **Monitor picker by name, resolution list from the monitor** | Launcher: monitor is a 0-16 slider; resolution is 720p/1440p/4K presets | Same | Launcher: list monitors by name (SDL), list render scales by the monitor's resolution. Small, shared. |
| 10 | **FPS counter, borderless, Alt+Enter** | F3 SDK overlay; fullscreen is borderless (F11) | Same | Check that Alt+Enter also toggles; otherwise fine. |
| 11 | **Polish**: README screenshots, credits, release notes listing fixes | README still says "boots to the title screen" (2026-10-18) | Done in 1.x | Refresh the README status, add screenshots and the settings table. Small. |

Already at the standard: portable mode (`portable.txt`), 60 fps by default with the game logic stepped correctly,
intro logo skip, controller hot-plug (every pad drives player 1), keyboard+mouse mode with sensitivity, launcher
with ISO import + verify + title update + DLC install, render scale up to 4K, FXAA, anisotropic filtering, FSR/CAS
upscale, no stretching at 16:9, DLC, Archipelago client in the exe.

Not planned: HDR (renderer has no HDR output), MSAA beyond what the game draws (the Xenos EDRAM path decides it),
FOV slider (no camera FOV found yet; revisit with #1, since the aspect hook sits next to the projection code).

## Shared decisions (both Crackdown recomps)

Settings are SDK/game cvars, saved in the game's toml next to its exe (`crackdown.toml`, `crackdown2.toml`). The
launcher, the in-game page and the F4 overlay all write the same file. A setting at its default is **removed** from
the file, never written out.

| Setting | Key | Default | Values |
|---|---|---|---|
| Fullscreen | `fullscreen` | `true` (borderless) | F11 toggles in game |
| Monitor | `monitor` | `0` (system default) | 1-based monitor index |
| Window size | `window_width`, `window_height` | automatic | 1280x720, 1920x1080, 2560x1440 |
| Internal resolution | `resolution_scale` | `1` (720p) | 2 = 1440p, 3 = 4K |
| Aspect ratio | `aspect_ratio` | `"16:9"` | `"16:10"`, `"21:9"`, `"32:9"`, `"4:3"`, or a number (Hor+) |
| Frame rate | `fps_cap` | `60` (CD1: unset = 60 via its defaults) | `30` (stock), `120`, `144` (experimental); `vsync = true` gives the console's 30 fps path |
| Anti-aliasing | `swap_post_effect` | `"none"` | `"fxaa"`, `"fxaa_extreme"` |
| Texture filtering | `anisotropic_override` | `3` (4x) | `-1` game's own, `2` 2x, `4` 8x, `5` 16x |
| Upscale filter | `present_effect` | `"bilinear"` | `"cas"`, `"fsr"` |
| Skip intro logos | `skip_intro_videos` | `true` | |
| Language | `user_language` | `1` (English) | Xbox language ids 1-10 |
| Keyboard + mouse | `mnk_mode` | `false` | |
| Mouse sensitivity | `mnk_sensitivity` | `1.0` | 0.1-5.0 |
| Camera turn speed | `camera_sensitivity` | `1.0` | 0.25-4.0 (stick and mouse) |
| Invert Y (new) | `invert_y` | `false` | stick and mouse |
| In-game PC menu | `pc_menu_mod` | `true` | `false` hides PC Settings / Quit to Desktop |

**Keys.** Keyboard binds are the SDK's `keybind_*` cvars. Defaults (both games): WASD move, mouse aims, left click
fires (RT), right click locks on (LT), Space jumps (A), Backspace B, L X, P Y, 1 / 3 bumpers, F left-stick press,
K right-stick press, Enter or X Start, Tab or Z Back, Shift+arrows D-pad. Host keys: **F3** fps / debug overlay,
**F4** settings overlay (advanced panel only), **F11** fullscreen, **Alt+F4** quit. F1 stays free for a future
in-game PC Settings shortcut (the community convention).

**Saves and data.** Default user data is `Documents\crackdown` / `Documents\crackdown2` (the SDK's per-exe folder).
With `portable.txt` beside the exe, saves go to `userdata\` and the shader cache to `userdata\cache\` beside it; the
launcher creates the marker. Test runs always get their own `--user_data_root` / `--cache_root`.

**Launcher.** One shared launcher (pacific-city-recompiled-launcher), game selector in the header, pages HOME /
DISPLAY / GAME / SAVES. Import ISO or folder, verify every file against `game-manifest.txt` and the XEX hash,
install title update and DLC from their packages, then Play. "Changes save automatically and apply the next time
you press Play." A row that a game doesn't support yet is hidden for that game, not greyed out.

## Order of work

1, 3 and 4 are ports of known Crackdown 1 hooks and come first (each one PR, with evidence: guest addresses, the
disassembly, and a screenshot/log comparison). 2 (in-game menu) is the biggest single gap and is next, after Austin
agrees to the scope. 5 and 9 are launcher work, shared with Crackdown 1.
