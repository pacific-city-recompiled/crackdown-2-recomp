# Plan: one launcher for Crackdown 1 and Crackdown 2

Decision (2026-10-18, project default): extend the existing Crackdown 1 launcher (`crackdown/launcher` in
Unowa001/crackdown-recomp, SDL3 + Dear ImGui) with a game selector, instead of writing a second launcher.

## What is Crackdown-1-specific today (audit of the 1.0 tree)

- **Hashes and disc:** `core.h:14` `supported_xex` (default argument of `verify_folder`, hard-coded again in `install_image` core.cpp:269 and main.cpp:44). The XDVDFS reader rejects XGD3 (core.cpp:93-102); Crackdown 2's World disc is XGD2 (partition 0xFD90000), so it reads fine.
- **Files and folders:** `game-manifest.txt`, `verified-files.txt`, `crackdown.toml`, `home/game`, `game-install-*`, `.crackdown-import`, the `crackdown-verified 1` stamp header (core.cpp:139-303, main.cpp:58-271). All assume one game per package folder.
- **Paths:** `crackdown.exe` (platform.cpp:135-166), `Documents\crackdown` / XDG `crackdown` (platform.cpp:239-242), log prefix `crackdown_*` (platform.cpp:251), single-instance mutex name (platform.cpp:88).
- **UI and features:** window title, tab list with `tab_ap=6` / `tab_about=7` hard-coded (main.cpp:212-213), co-op and Archipelago sections, gameplay text, 10-language list, disc hint, About text.
- **Art:** `XUIFrontEnd/Scene.xzp` textures and crop UVs (visuals.cpp:73-109). Crackdown 2 has no Scene.xzp; its art lives in Scaleform `.pack` files (`UI/*.pack`), so CD2 starts with the text fallback.
- **Steam shortcut:** appid from `exe + "Crackdown"`, AppName/tag "Crackdown" (steam.cpp:71-74).
- **Updater:** repo `Unowa001/crackdown-recomp` (v2.h:7), required files and the ZIP allowlist name `crackdown.exe`, `game-manifest.txt`, the xui patches (update.cpp:106-261), package `CrackdownRecomp-*.zip`.

## Design

```cpp
struct GameProfile {
  const char* id;                         // "cd1", "cd2": subfolder and launcher.toml keys
  const char* title;                      // window, About, Steam AppName
  const char* exe;                        // crackdown.exe / crackdown2.exe
  const char* config;                     // crackdown.toml / crackdown2.toml
  const char* user_folder;                // Documents\crackdown / Documents\crackdown2
  const char* log_prefix;                 // crackdown_ / crackdown2_
  const char* manifest;                   // game-manifest.txt / cd2-game-manifest.txt
  std::vector<std::string> xex_hashes;    // CD2: base default.xex (the TU is a separate step)
  bool needs_title_update;                // CD2: yes (STFS package -> default.xexp + game\TU + update root)
  ArtSource art;                          // CD1: Scene.xzp entries; CD2: none yet (text logo)
  bool coop, archipelago;                 // feature flags hide tabs/sections
};
extern const GameProfile kGames[];
```

- **Folder layout:** Crackdown 1 stays at the package root (no migration for existing installs). Crackdown 2 lives under `cd2\` with the same structure (`cd2\game`, `cd2\crackdown2.toml`, `cd2\verified-files.txt`). `launcher.toml` gains `selected_game` and per-game `profile.<id>`.
- **Selector:** two toggle buttons in the header row next to `RECOMP %s` (main.cpp:385), disabled while a job runs. Switching calls a `load_game(profile)` lambda built from the current start-up block (main.cpp:122-161, 209): it clears the art, reloads the config and runs the quick check. Tabs come from a list filtered by the feature flags; `tab_ap`/`tab_about` become lookups by name.
- **Crackdown 2 install step:** ISO import as today, plus "Install title update" (the launcher already has STFS reading in `dlc.cpp` on the 1.1 branch): extract `default.xexp` next to `default.xex`, copy the update tree to `game\TU`, and pass `--update_data_root`. DLC uses the same installer as Crackdown 1's (Xenia layout under the user folder, `--dlc_root`, SDK patch 0025).
- **Launch:** same as Crackdown 1 (`--game_data_root --user_data_root --cache_root`), plus `--update_data_root=<cd2>\tu` for Crackdown 2. Steam: one shortcut per game with `--game=<id>` as launch options and the appid salted by the game title.
- **Updater / package:** one combined package (`CrackdownRecomp-<ver>.zip` gains `crackdown2.exe`, its DLLs and `cd2-game-manifest.txt`), so one launcher updates both. The Crackdown 2 exe is built from Unowa001/Crackdown-2-Recomp; the package script pulls the built exe in. Alternative: separate packages per repo (more code in the updater).

## Order of work

1. GameProfile + selector + per-game paths (no CD2 install yet): Crackdown 1 must behave exactly as 1.0 (its tests: `launcher_tests`, `launcher_v2_tests`).
2. CD2 verify/import + title-update install + launch.
3. Package script: combined ZIP. 4. CD2 art (from `UI/initial.pack` or `nxeart`), DLC installer for CD2.

## Open decision (Austin)

Where the launcher code changes land: a branch of Unowa001/crackdown-recomp (recommended, the launcher lives there), and whether the
combined package is the release format.
