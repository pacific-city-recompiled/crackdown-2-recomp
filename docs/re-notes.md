# Crackdown 2 recomp notes

Evidence log. Every address is in the title-update-patched image (0.0.5.1), `.text` 0x821D0000-0x8304E2F4.

## Setup findings (2026-10-18)

- **ISO:** XDVDFS partition at 0xFD90000 (XGD2). `tools/xiso_extract.py` reads it directly; 99 files, 3.36 GiB.
- **Title update:** one STFS package (`TU_16L625S...`), `Version.txt` = "15.10.30.57636". Holds `default.xexp`, `Local/*/TUText/TitleUpdateStrings.str`, `UI/Arenas.gfx`, `UI/FeMenuArrows.gfx`, `Wwise/streaming.Init.pack(.toc)`. The game opens its files through `D:\TU\...` and `update:\...` (log: `D:\TU\Wwise\streaming.Init.pack`, `update:\Version.txt`), so the update tree goes in both `game\TU\` and `--update_data_root`.
- **DLC:** `Pre-Order Armor` is a 55-byte `PreOrder.txt` (title 4D5308BC, type 2). Toy Box (51 files) and Deluge (53 files) carry resblocks, packs and per-language strings. Toy Box extraction reports 1 block failing SHA-1 (`stfs_extract.py`); not investigated yet (likely a hash-table edge case in the extractor, files not yet used by the game).
- **Codegen** (`out/codegen1.log`, SDK 0.10.0 + patches 0001-0024, 0026): 51 unresolved branches, 32 targets. 31 are the Crackdown 1 pattern (C++ this-adjustor thunks `addi r3,r3,-N; b X`, plus `lwz r3,100(r3); b 0x82cf7bd8`), listed in `crackdown2/config/functions.toml` with their source addresses. One is different: 0x82CFA85C was a gap-fill function whose `beq` pointed 8 bytes before its start; sub_82CFA7D8 ends in a tail call, and the real start is 0x82CFA828.
- **Gap scan** (`tools/gapscan.sh`): 6 rounds took `config/gapfill.toml` to 1439 entries (238 rejected), 0 unresolved. Not converged, same as Crackdown 1. Run-driven additions on top: 0x82F1E9B0 (8-byte stub `b 0x822c1010` between sub_82F1E908 and sub_82F1E9B8; first runtime FATAL "invalid or unregistered function"). **Converged 2026-10-20** (round 5 run, `out/gapscan5.log`): 6 more rounds took 1774 -> 1787 entries; the last rounds only walked back a chain of 3-instruction entry stubs at 0x822C239C-0x822C2408 and 0x822C28C4-0x822C28F4 (each falls into a shared tail, so each one registered exposes the one before it) plus 0x826A2000/0x826A203C. The 232 candidates left are all previously rejected (mid-function tails). ptrscan on a city image dump (`out/ptrscan5.txt`): 18 `gap` refs are xstart (0x823BF8C8, not named `sub_`, so the funcs list misses it), the known chunk 0x8235F908, the save/restore-register helpers 0x82F1E3D0/0x82F1E420 and import thunks at 0x8304xxxx; the 603 `inside` refs are mostly r31-frame code right after a `b` (C++ EH unwind funclets, only run if an exception is thrown). 4 min city run on the new build (crackdown2_076): no FATAL.
- **Jump table clipped at 512 entries:** sub_82516AF8 (`cmplwi cr6,r3,517; ble` before the `lwzx`/`bctr` at 0x82516C20, table at 0x82516C24) has 518 entries. The SDK default `max_jump_table_entries=512` emitted `__builtin_trap` for r3 = 512..517, and the game hit it right after boot (Illegal instruction, found with `cdb -g -c "sxi av;g;.exr -1;k"`). Fixed with an explicit `[[switch_tables]]` in `functions.toml`, checked to give byte-identical output to `--max_jump_table_entries=4096`. Only this one file changed with the higher limit, so no other table is clipped.
- **Result:** with those, the game reaches the title screen ("PRESS START", 2560x1440 window, RTX 5080, D3D12). It idles there; the log is only failed opens of directory paths (`UI\`, `Video\`, `HUD\`).

## Game-specific facts

- Title ID 4D5308BC, media ID 18B6CD67.
- The engine matches Crackdown 1's: same `ReXGlue` pipeline, same guest-code idioms. Hook addresses from Crackdown 1 do **not** carry over (different binary). Expect the same runtime gaps as `docs/playbook.md` lists (inline event reset, XMountUtilityDrive HDD cache, spin barrier in XAudio) and re-find them by pattern.

## Randomizer notes

(none yet; record pickup/orb/spawn tables here with guest addresses.)

## Press Start to gameplay (2026-10-18)

- **Input for unattended runs:** `crackdown2/src/hooks_autoinput.cpp`, `--auto_input=START,A,A,...` presses one button per 3 s (300 ms each) after 25 s, through the game's XInputGetState wrappers. Guest 0x829551D0 is a raw `XamInputGetState(user r3, flags r4, state r5)` thunk to import 0x83047EAC; 0x829551D8 is `XInputGetState(user r3, state r4)` (sets r4=1, r5=state); 0x829552B0 is XInputGetKeystroke (import 0x83047ECC). Same shape as Crackdown 1's 0x82743798/0x827437C0. Windows would not give the test run keyboard focus (foreground lock), hence the hook.
- **Bug found on the way:** the first version of the hook used r4 as the state pointer in 0x829551D0 and wrote to guest 0x5; found with cdb by logging every access violation (`sxe -c ".exr -1; k 8; g" av`) and grepping for `Parameter[1]: 0000000100000005`. The many other first-chance AVs (sub_8291B900+0xc3b, sub_82F1E480+0xdcd, sub_82917780) are the SDK's lazy guest-memory commits and are harmless.
- **One more run-driven gap:** 0x82580A58 (`li r11,0; stb r11,32(r3); sth r11,28(r3) ...`) inside a rejected gap-scan range.
- **Reached with no Crackdown 1 hooks at all:** Press Start, main menu (Main Game, Deluge, Keys to the City, Multiplayer, Options: the DLC/title-update entries show), Agent Setup (head select, 3D agent), and the first gameplay area (the training room: HUD, radar, tutorial prompts, agent model). `--auto_input` with 14 A presses reaches it about 5 min after launch (includes load). Screenshot: `out/ingame.png` (local).
- **Not yet checked:** the event-reset and HDD-cache hangs from Crackdown 1 did not appear in this path (no hang in 5 min, but New Game into the open city is not tested). Render log spam: `Resolve region 16376 <= x < 16384 is outside the surface pitch 1040` and `PM4_DRAW_INDX_2 ... Failed in backend (edram_mode=6)` appear around the menu-to-agent-setup transition; the image looks correct, so low priority.

## Load time (2026-10-18)

The first New Game load took about 100 s. cdb stack samples (30, one per ~2 s during the load) put 17 in the main loader thread's spin in sub_829196A8 / sub_82918A10 (guest D3D ring-buffer "wait for space": `db16cyc` delay loop, reads the CP's read-pointer writeback, yields via sub_82928910 after 5000 ticks), while the GPU command thread was mostly sleeping in `rex::thread::Sleep` or waiting on the D3D12 upload buffer pool. The SDK sleeps `wait/0x100` ms per PM4 WAIT_REG_MEM poll when `vsync` is on (sdk src/graphics/command_processor.cpp, the `REXCVAR_GET(vsync)` branch). With `--vsync=false` the first gameplay HUD files open 51 s after launch instead of 164 s (same scripted input, about 25 s of it the script's own start delay). `crackdown2/src/host_tuning.cpp` now defaults `vsync=false` and `clear_memory_page_state=false` (Crackdown 1's measured upload fix).
SDK ideas still open: the CP writes the read-pointer writeback only after a whole `ExecutePrimaryBuffer` batch (command_processor.cpp:272), so a guest waiting for ring space sees a stale pointer until the batch ends; writing it every N packets inside the loop would let the guest refill earlier.
Not measured yet: frame pacing with vsync off (the game's own limiter should hold ~30 fps, as in Crackdown 1), so game speed in the city is still unchecked.

## Boot logos (2026-10-18)

- Order: MGS.usm (Microsoft Game Studios, plays during state 0), then the boot state machine sub_828AB9F0 (state u32 at this+324): 0 save notice (SaveWarning.gfx), 1 Ruffian.usm, 2 legal text for 4500 ms (start time u32 at this+328, ms from sub_82954E48), 3 TitleFadeUp.usm, then Press Start. Strings at 0x82163980 ("MGS.usm"), 0x82163988 ("Ruffian.usm"), 0x821639B4 ("TitleFadeUp.usm"). Ruffian.usm is in UI\splash.pack.
- sub_828B60C8 = "any of the four pads has A/B/Start down" (bits 12/13/4 of the button word). States 1 and 3 advance when it returns nonzero; state 2 treats a press as "wait for release".
- `crackdown2/src/hooks_intro.cpp` (--skip_intro_videos, default on): forces "pressed" in states 1 and 3 after 15 frames, and moves state 2's start time back 4500 ms. Forcing state 0 or state 2 left the game stuck on the save notice. PC runs crackdown2_049/050: Press Start shows about 11 s after launch (was ~30 s). MGS.usm is not skipped yet.

## Frame timing (2026-10-18)

- Guest 0x82385DA8 is the end-of-frame/present function (only caller of the VdSwap import). `crackdown2/src/hooks_frametime.cpp` times its calls: `--frame_log=<csv>` (frame_time_us rows, Crackdown 1 format) and the F3 overlay frame rate.
- 150GS save, standing in the city at night (PC run crackdown2_055, vsync on): last 40 s = 1200 frames, avg 33.33 ms (30.0 fps), 1% low 35.7 ms, max 36.3 ms. Stock 30 fps holds with headroom unknown (next: uncapped test once the game-speed clock is understood; vsync=false alone runs the game too fast).
- 2026-10-18 later: host pacer `--fps_cap` (hooks_frametime.cpp, before the present). Defaults are now `vsync=false fps_cap=30`: city last 30 s 33.33 ms avg, 1% low 34.1-34.3 ms (runs 056, 058), steadier than vsync on (35.7 ms). Uncapped: 59.7 fps avg, 1% low 30.7 ms (run 057).
- Game clock for 60 fps (same design as Crackdown 1): fixed step dt = [0x832B60F8] * 1/30 (pooled literal 0x82114994, read in sub_82369AA0 and ~30 other functions); clock object at [0x82800B0C] with the "locked" byte at +13, variable-clock flag byte 0x82800FB9, variable ms at [0x8277ADCC]; 16 inlined `ms = locked ? 33 : (flag ? var : 16)` sites (Crackdown 1 had 13), e.g. 0x823AB4xx in crackdown2_recomp.7.cpp.
- 60 fps leftovers (2026-10-19, `--sim_frame_fixes`, hooks_framerate.cpp; addresses checked against out/guest-image.bin):
  - Global frame counter 0x8383A96C, `+= 1` per frame in sub_82338648 (0x82338688, same function as ms site 0x8233869C),
    38 reader functions using it as a 30 Hz clock: `counter > t + 30` (sub_82720FD0), `(counter - t) / 30 >= 10`
    (sub_82835698), `counter % 1800` (sub_827758C8), deadline `t + durationMs / 1000 * 30` (sub_82721EC0, line/event
    expiry; inferred to be the early speech cut-off, not yet heard). Now advanced by whole 30 Hz ticks.
  - Pooled 1/30 0x82114994 readers (22): crowd anim clock sub_825E4AA0 (anim time [+88] += 1/30, wrapped at clip
    length; Crackdown 1 sub_82330888), sub_827338E8 ([+3876] += 1/30), sub_82334738 (x *= 1 - 1/30, decay),
    dt arguments in sub_82549940 / sub_8256DCD0; 0.033 at 0x820F98D4 passed as dt by sub_826C9278. Others look like
    unit conversions or thresholds, left alone. 0x832D5088 and 0x837C312C hold copies of the (already scaled) step.
  - Stairs ground snap: sub_82208998 (called by the character controller sub_822498F8), out.y = -drop*gain/dt at
    0x8220911C..0x82209124, same as Crackdown 1 sub_822B4D40.
  - Run crackdown2_002 (dev, `--fps_cap=60 --sim_game_clock --diag_sim_sites`, city at night): counter and decay sites
    run exactly once per frame (300/5 s); the crowd, timer and dt sites never ran there, ground snap not exercised
    (standing). Audible speech and stairs need a playtest.
  - No CD1-style wave safety timeout (`samples/48000*30+70`) exists in CD2; its audio stack differs.

## 60 fps: measured game clock (2026-10-18)

- `crackdown2/src/hooks_framerate.cpp` + `config/midasm_hooks.toml` (--sim_game_clock): scales the update dt (sub_82915C70, only caller sub_82369AA0) and the step query sub_827671D0 by measured step / (1/30), and gives the 16 `li rX,33` timer sites the measured whole ms. The pooled 1/30 literal is left alone (Crackdown 1 lesson: Havok and others read it).
- Frame rate, city standing, last 20 s (runs 059/060/061): --fps_cap=30 33.33 ms avg; --fps_cap=60 16.67 ms avg, 1% low 17.7 ms (clock on) / 17.3 ms (off).
- Speed A/B (runs 062-064): same save, scripted 1.2 s forward run, screenshot 80 s after launch. 30 fps and 60 fps + clock stop at the same spot; 60 fps without the clock runs about twice as far (out/speed_ab.png, local). Not yet checked at 60: vehicles, stairs (Crackdown 1 needed a ground-snap fix), animation clocks, audio/VO timeouts, physics; defaults stay at 30 until then.

## City frame rate: memexport readback (2026-10-19)

- cdb samples (30, city spawn, uncapped, RelWithDebInfo SDK DLLs for symbols) put the guest render thread in the ring-space wait (sub_82385DA8 -> sub_829196A8 -> sub_82918A10) and the GPU command thread in `CheckSubmissionFence` under `IssueDraw_MemexportReadbackFullPath` in 13 of 30 samples: the SDK's `readback_memexport` (default true) copies every memexport draw's output to the CPU and waits for the GPU. Xenia's `d3d12_readback_memexport` defaults to false.
- `--readback_memexport=false`: 69.0 -> 146.5 fps avg, 1% low 17.0 -> 8.7 ms (runs 068/071; default build run 074: 144.2 / 9.8). Screenshots at 30 fps with it on and off are the same image (072/073). Now a default in `host_tuning.cpp`.
- Ruled out first: writing the CP read pointer back every 1024 dwords inside a batch (tried as SDK patch, dropped): 66.8 fps vs 69.0.
- Note: uncapped, the simulation runs faster than real time (fixed 1/30 s step per frame), so time of day moves on and screenshots taken at the same wall time differ. Compare visuals at 30 fps.
