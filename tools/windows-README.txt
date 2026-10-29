Crackdown 2 Recomp (Windows x64)

Requirements: x64 CPU with AVX2 and FMA3, a D3D12 GPU, Windows 10/11 and the
Microsoft Visual C++ x64 Redistributable.

Run Crackdown2Recomp.exe (app launchers such as Quiver start it for you).
The first time, choose INSTALL GAME and select your own Crackdown 2 files:
  - the World multi-language disc, as an ISO or a folder with the extracted disc.
    Supported default.xex SHA256:
    8959c449e382661d29b8ea479c685f506cd5bd3eca40b3d73a51cc10f281069f
  - then INSTALL TITLE UPDATE: the title update package (TU_16L625S...,
    version 0.0.5.1). It is required: the recomp is built from the updated game.
  - optional: INSTALL DLC for each DLC package (Toy Box, Deluge, Pre-Order Armor),
    as the original package files from the console's Content\...\4D5308BC\00000002.
Every disc file and package block is checked. The game files go to cd2\game and
cd2\tu; DLC goes to your save folder (Documents\crackdown2).

Settings, save profiles and Archipelago are in the launcher. 60 fps is the default.
Saves and the shader cache go to Documents\crackdown2 (an empty cd2\portable.txt
keeps them in cd2\userdata instead).

Bug reports: include the latest cd2\logs\crackdown2_NNN.log, cd2\VERSION.txt, what
happened and how to reproduce it.

Third-party notices are in LICENSES and cd2\LICENSES. Source and releases:
https://gitlab.com/pacific-city-recompiled/crackdown-2-recomp
Launcher source: https://gitlab.com/pacific-city-recompiled/launcher
Unofficial fan project, not affiliated with or endorsed by Microsoft. It contains
no game data; you must own the game and supply your own files.
