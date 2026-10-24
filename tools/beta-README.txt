Crackdown 2 Recomp beta (Windows x64)

Requirements: x64 CPU with AVX2 and FMA3, a D3D12 GPU, Windows 10/11, the
Microsoft Visual C++ x64 Redistributable, and Python 3.8+ (for the one-time setup).
Supply your own Crackdown 2 files:
  - the World multi-language disc (ISO, or a folder with the extracted disc).
    Supported default.xex SHA256:
    8959c449e382661d29b8ea479c685f506cd5bd3eca40b3d73a51cc10f281069f
  - the title update package (TU_16L625S..., brings the game to 0.0.5.1).
    Required: the recomp is built from the updated executable.
  - optional DLC packages (Toy Box, Deluge, Pre-Order Armor), as the original
    package files from the console's Content\...\4D5308BC\00000002 folder.

Setup (once). Extract this entire ZIP into a user-writable folder, then open a
command prompt in that folder and run:

  python -I setup_game.py "<disc .iso or folder>" "<title update package>" ["<DLC package>" ...]

It checks every disc file against game-manifest.txt and every update/DLC block,
then creates game\ and tu\ next to crackdown2.exe. DLC goes into your save folder
(Documents\crackdown2). To add DLC later:
  python -I install_dlc.py "%USERPROFILE%\Documents\crackdown2" "<DLC package>" ...
With Deluge and Keys to the City installed, their main-menu entries are enabled.

Play: run crackdown2.exe. It finds game\ and tu\ by itself.
60 fps is the default. --fps_cap=30 (or --vsync=true for the stock path) gives 30 fps.
Options go on the command line or in crackdown2.toml next to the exe.
Saves and the shader cache go to Documents\crackdown2. An empty portable.txt next
to the exe keeps them in userdata\ instead.

Known issues:
  - Multiplayer (co-op, Deluge online, Host/Join) does not work yet. It is off
    unless you pass --net_mode; single player does not touch the network.
  - The log may show "Resolve region ... outside the surface pitch" and a failed
    draw around menu transitions (under investigation).
  - Effects use a faster GPU path (readback_memexport off). If particles or
    effects look wrong, try --readback_memexport=true and report it.

Bug reports: include the latest logs\crackdown2_NNN.log (next to crackdown2.exe),
VERSION.txt, what happened and how to reproduce it.

Third-party notices are in LICENSES. The exact revision and SDK patch hashes are
in VERSION.txt. Source and releases: https://gitlab.com/pacific-city-recompiled/crackdown-2-recomp
Unofficial fan project, not affiliated with or endorsed by Microsoft. It contains
no game data; you must own the game and supply your own files.
