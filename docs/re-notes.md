# Crackdown 2 recomp notes

Evidence log. Every address is in the title-update-patched image (0.0.5.1), `.text` 0x821D0000-0x8304E2F4.

## Setup findings (2026-10-18)

- **ISO:** XDVDFS partition at 0xFD90000 (XGD2). `tools/xiso_extract.py` reads it directly; 99 files, 3.36 GiB.
- **Title update:** one STFS package (`TU_16L625S...`), `Version.txt` = "15.10.30.57636". Holds `default.xexp`, `Local/*/TUText/TitleUpdateStrings.str`, `UI/Arenas.gfx`, `UI/FeMenuArrows.gfx`, `Wwise/streaming.Init.pack(.toc)`. The game opens its files through `D:\TU\...` and `update:\...` (log: `D:\TU\Wwise\streaming.Init.pack`, `update:\Version.txt`), so the update tree goes in both `game\TU\` and `--update_data_root`.
- **DLC:** `Pre-Order Armor` is a 55-byte `PreOrder.txt` (title 4D5308BC, type 2). Toy Box (51 files) and Deluge (53 files) carry resblocks, packs and per-language strings. Toy Box extraction reports 1 block failing SHA-1 (`stfs_extract.py`); not investigated yet (likely a hash-table edge case in the extractor, files not yet used by the game).
- **Codegen** (`out/codegen1.log`, SDK 0.10.0 + patches 0001-0024, 0026): 51 unresolved branches, 32 targets. 31 are the Crackdown 1 pattern (C++ this-adjustor thunks `addi r3,r3,-N; b X`, plus `lwz r3,100(r3); b 0x82cf7bd8`), listed in `crackdown2/config/functions.toml` with their source addresses. One is different: 0x82CFA85C was a gap-fill function whose `beq` pointed 8 bytes before its start; sub_82CFA7D8 ends in a tail call, and the real start is 0x82CFA828.
- **Gap scan** (`tools/gapscan.sh`): 6 rounds took `config/gapfill.toml` to 1439 entries (238 rejected), 0 unresolved. Not converged, same as Crackdown 1. Run-driven additions on top: 0x82F1E9B0 (8-byte stub `b 0x822c1010` between sub_82F1E908 and sub_82F1E9B8; first runtime FATAL "invalid or unregistered function").
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
