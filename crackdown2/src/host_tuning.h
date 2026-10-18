// crackdown2 - host-side defaults the SDK leaves off
#pragma once

#include <rex/ui/overlay/debug_overlay.h>

namespace rex { class Runtime; }

namespace crackdown2 {
// Sets SDK flags the game wants unless the user set them (command line, config, environment).
// Call from OnPreLaunchModule: GPU plugin flags only exist once the plugin has loaded.
void ApplyGameDefaults();
// --dump_image: writes the loaded guest image to a file.
void DumpGuestImage(rex::Runtime* runtime);
// Guest frame rate for the F3 overlay, and --frame_log flushing (hooks_frametime.cpp).
rex::ui::FrameStats GuestFrameStats();
void FlushFrameLog();
// --sim_game_clock: measures this frame (hooks_framerate.cpp); called from the per-frame hook.
void UpdateSimClock();
}  // namespace crackdown2
