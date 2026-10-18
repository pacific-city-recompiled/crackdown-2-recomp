// crackdown2 - the mid-asm hooks in config/midasm_hooks.toml
//
// The generated code declares each [[midasm_hook]] itself (extern, by name); this list is for the C++ side.

#pragma once

#include <rex/ppc/context.h>

// hooks_framerate.cpp: replaces the game's fixed 33 ms per frame with the measured frame ms when
// --sim_game_clock is on. One hook per site (bit index, guest address of the `li rX,33`), so
// --sim_game_clock_sites can bisect them.
#define CD2_FRAME_MS_SITES(X)                                              \
  X(0, 823AB418) X(1, 823AB474) X(2, 823914B0) X(3, 8252D34C)              \
  X(4, 8276226C) X(5, 827EAA24) X(6, 82381634) X(7, 82240290)              \
  X(8, 8233869C) X(9, 8220C0DC) X(10, 8278FC88) X(11, 827837FC)            \
  X(12, 823397F0) X(13, 82250438) X(14, 827AEAE0) X(15, 827671A4)

#define CD2_DECLARE_FRAME_MS(bit, addr) void Cd2FrameMs_##addr(PPCRegister& r);
CD2_FRAME_MS_SITES(CD2_DECLARE_FRAME_MS)
#undef CD2_DECLARE_FRAME_MS
