// crackdown2 - host-side defaults the SDK leaves off
//
// vsync=false: with the SDK default (true) the guest's loading is ~3x slower. The command processor's
// PM4 WAIT_REG_MEM handler sleeps `wait/0x100` ms per poll when vsync is on (sdk
// src/graphics/command_processor.cpp, the `if (!REXCVAR_GET(vsync))` branch), and the game issues those waits
// all through a level load. PC run 2026-10-18, same scripted input (--auto_input START + 14 A):
// New Game to first gameplay HUD files 164 s -> 51 s after launch (about 25 s of that is the script's own
// start delay); with vsync on, the guest's main loader thread spent ~100 s spinning in sub_829196A8/sub_82918A10
// waiting for ring space (cdb stack samples, 30 samples: 17 in those two functions). Same default Crackdown 1
// uses, there for the 60 fps work as well.
// clear_memory_page_state=false: Crackdown 1's measured fix (2026-10-17: re-uploaded ~20 MB/frame otherwise).

#include "host_tuning.h"

#include <string_view>

#include <rex/cvar.h>
#include <rex/logging/macros.h>

namespace crackdown2 {

void ApplyGameDefaults() {
  struct Default {
    const char* name;
    const char* value;
  };
  static constexpr Default kDefaults[] = {
      {"vsync", "false"},
      {"clear_memory_page_state", "false"},
  };
  for (const Default& d : kDefaults) {
    // Only where nothing else set it. GetFlagSource also reports kDefault for an unregistered flag.
    if (rex::cvar::GetFlagSource(d.name) != rex::cvar::Source::kDefault) continue;
    if (rex::cvar::SetFlagByName(d.name, d.value)) {
      REXLOG_INFO("crackdown2 default: --{}={}", d.name, d.value);
    } else {
      REXLOG_WARN("crackdown2 default: could not set --{}={}", d.name, d.value);
    }
  }
}

}  // namespace crackdown2
