// crackdown2 - host-side defaults the SDK leaves off
#pragma once

#include <cstdint>

#include <rex/ui/overlay/debug_overlay.h>

namespace rex { class Runtime; struct PathConfig; }

namespace crackdown2 {
// Sets SDK flags the game wants unless the user set them (command line, config, environment).
// Call from OnPreLaunchModule: GPU plugin flags only exist once the plugin has loaded.
void ApplyGameDefaults();
// gpu_plugin=xenos unless set; portable mode and game folder discovery (host_tuning.cpp).
void ApplyGpuPluginDefault();
void ApplyStartupDefaults(rex::PathConfig& paths);
// --input_all_pads_user0: every controller drives user 0, the only signed-in user.
void ApplyInputDefaults(rex::Runtime* runtime);
// --dump_image: writes the loaded guest image to a file.
void DumpGuestImage(rex::Runtime* runtime);
// Guest frame rate for the F3 overlay, and --frame_log flushing (hooks_frametime.cpp).
rex::ui::FrameStats GuestFrameStats();
void FlushFrameLog();
// --sim_game_clock: measures this frame (hooks_framerate.cpp); called from the per-frame hook.
void UpdateSimClock();
// --aspect_ratio as a number (16:9 when unset or not understood), and whether it is 16:9 (hooks_aspect.cpp).
double AspectRatio();
bool AspectRatioIsDefault();
// Writes the camera aspect ratio when --aspect_ratio isn't 16:9; per guest frame (hooks_frametime.cpp).
void ApplyAspectRatio(uint8_t* base);
}  // namespace crackdown2
