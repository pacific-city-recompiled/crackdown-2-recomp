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

// hooks_framerate.cpp, --sim_frame_fixes: frame-locked steps outside the 16 ms sites (config/midasm_hooks.toml).
#define CD2_SCALE_STEP_SITES(X)   X(1, 825E4CBC) X(1, 825E4CEC) X(1, 825E4F80) X(1, 825E4FB0) X(2, 82733908) X(4, 826C9618) X(5, 82549968)   X(6, 8256DFC0)
#define CD2_DECLARE_SCALE_STEP(bit, addr) void Cd2ScaleStep_##addr(PPCRegister& f);
CD2_SCALE_STEP_SITES(CD2_DECLARE_SCALE_STEP)
#undef CD2_DECLARE_SCALE_STEP
void Cd2ScaleDecay_82334B2C(PPCRegister& f);
void Cd2FrameCounterTick(PPCRegister& r);
