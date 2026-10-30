# Crackdown 2 recomp

Static recompilation of Crackdown 2 (Xbox 360, 2010, title 4D5308BC) to native PC, built on the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) (`sdk/`, pinned to v0.10.0). It reuses the pipeline from the Crackdown 1 recomp (both games run on the same engine).

**No game data and no generated code is in this repository.** You supply your own disc dump, title update and DLC.

**Getting it:** releases are on [GitHub](https://github.com/pacific-city-recompiled/crackdown-2-recomp/releases).
The newest [Crackdown 1 recomp launcher](https://github.com/pacific-city-recompiled/crackdown-recomp/releases)
installs and updates Crackdown 2 for you: pick CRACKDOWN 2 in its header and choose **Download runtime**.

## Status (2026-10-18)

- Codegen is clean (0 unresolved calls). The project builds and links on Windows.
- The game boots to the **title screen** (Xenos GPU, D3D12). Menus, gameplay, audio, input and the port-quality work are not tested yet.
- Not done: hooks (events/storage/sleep), 60 fps, PC options, launcher, installer. See `docs/plan-next.md`.

## Inputs (what the code was checked against)

| Item | Detail |
|---|---|
| Disc | World multi-language ISO (XGD2), 99 files. `default.xex` sha256 `8959c449e382661d29b8ea479c685f506cd5bd3eca40b3d73a51cc10f281069f` |
| Title update | `default.xexp` (STFS package, media 18B6CD67), sha256 `a5e49fc1ff7f6f65e6f95a7b5c4b9392bb9e38674d1237ac0e2f0149d05929d7`. The SDK applies it to the XEX: 0.0.0.1 -> 0.0.5.1, filetime 2010-10-30 |
| DLC | Toy Box Pack (DLC1), Deluge Pack (DLC2), Pre-Order Armor (title 4D5308BC, type 2) |

Everything addresses the **title-update-patched** binary (the update is a full rebuild, so disc-only addresses differ). The recomp always runs with the update applied.

## Prepare the game folder

```
python -I tools/xiso_extract.py "<disc>.iso" game          # XDVDFS extractor, no extra tools
python -I tools/stfs_extract.py "<title update package>" tu  # checks every block SHA-1
copy tu\default.xexp game\                                   # SDK picks the sibling .xexp
xcopy /E tu game\TU\                                         # the game also reads D:\TU\...
```

DLC: `python -I tools/install_dlc.py "%USERPROFILE%\Documents\crackdown2" "<DLC package>" ...` unpacks each package
where the game finds it (the user data root, or any folder you pass as `--dlc_root`). With the packs in, the main menu's
Deluge and Keys to the City entries are enabled.

## Build and run (Windows)

1. VS 2022 (x64 tools), LLVM/Clang 20, CMake, Ninja. Developer Mode on (SDK dependencies use symlinks).
2. Build and install the SDK with `patches/sdk/*.patch` applied (`tools/apply-sdk-patches.sh`, same procedure as Crackdown 1, see `docs/playbook.md`).
3. `build-cd2.bat [sdk-install-dir]` runs codegen and builds `crackdown2/out/build/win-amd64-release/crackdown2.exe`.
4. Run from that folder: `crackdown2.exe --game_data_root=<game> --update_data_root=<tu> --gpu_plugin=xenos`.

## Docs

- `docs/re-notes.md`: what was needed to get this game through codegen and to the title screen.
- `docs/playbook.md`: the generic recomp playbook inherited from Crackdown 1.
- `docs/plan-next.md`: next steps, including the launcher game selector.
