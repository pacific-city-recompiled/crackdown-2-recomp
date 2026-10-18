// crackdown2 - host-side defaults the SDK leaves off
//
// vsync is left on (2026-10-18): with vsync=false the game runs too fast in play (Austin, PC), so the frame
// limiter must be found first (60 fps work). Kept for reference: with vsync on, loading is ~3x slower. The command processor's
// PM4 WAIT_REG_MEM handler sleeps `wait/0x100` ms per poll when vsync is on (sdk
// src/graphics/command_processor.cpp, the `if (!REXCVAR_GET(vsync))` branch), and the game issues those waits
// all through a level load. PC run 2026-10-18, same scripted input (--auto_input START + 14 A):
// New Game to first gameplay HUD files 164 s -> 51 s after launch (about 25 s of that is the script's own
// start delay); with vsync on, the guest's main loader thread spent ~100 s spinning in sub_829196A8/sub_82918A10
// waiting for ring space (cdb stack samples, 30 samples: 17 in those two functions). Same default Crackdown 1
// uses, there for the 60 fps work as well.
// clear_memory_page_state=false: Crackdown 1's measured fix (2026-10-17: re-uploaded ~20 MB/frame otherwise).

#include "host_tuning.h"

#include <fstream>
#include <string>
#include <string_view>

#include <rex/cvar.h>
#include <rex/logging/macros.h>
#include <rex/memory/utils.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/system/user_module.h>
#include <rex/system/xex_module.h>
#include <rex/system/xmemory.h>

namespace crackdown2 {

void ApplyGameDefaults() {
  struct Default {
    const char* name;
    const char* value;
  };
  static constexpr Default kDefaults[] = {
      {"clear_memory_page_state", "false"},
      // SDK patch 0027: yield instead of sleeping on PM4 WAIT_REG_MEM polls, so loads run at the vsync-off
      // speed while vsync keeps the frame rate.
      {"wait_reg_mem_sleep", "false"},
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

// --dump_image=<file>: write the loaded (title-update-patched) guest image for offline analysis (string and
// data references). Same as Crackdown 1's helper.
REXCVAR_DEFINE_STRING(dump_image, "", "Crackdown2", "Write the loaded guest image to this file at startup");

namespace crackdown2 {

void DumpGuestImage(rex::Runtime* runtime) {
  const std::string& path = REXCVAR_GET(dump_image);
  if (path.empty() || !runtime || !runtime->kernel_state()) return;
  auto module = runtime->kernel_state()->GetExecutableModule();
  if (!module || !module->xex_module()) {
    REXLOG_WARN("dump_image: no executable module");
    return;
  }
  const uint32_t base = module->xex_module()->base_address();
  const uint32_t size = module->xex_module()->image_size();
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(runtime->memory()->TranslateVirtual<uint8_t*>(base)), size);
  REXLOG_INFO("dump_image: {:08X}..{:08X} ({} bytes) -> {}", base, base + size, size, path);
}

}  // namespace crackdown2
