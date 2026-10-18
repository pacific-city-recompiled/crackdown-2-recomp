// crackdown2 - host-side defaults the SDK leaves off
//
// vsync=false + fps_cap=30 (2026-10-18): the game steps its simulation a fixed 1/30 s per frame (dt = [0x832B60F8] *
// 1/30 at 0x82114994, sub_82369AA0), so it must run at exactly 30 frames a second. With vsync on, the guest's vblank
// waits hold that, but the SDK also sleeps on PM4 WAIT_REG_MEM polls (patch 0027 removes that) and New Game loaded
// ~3x slower. With vsync off the game has no limiter of its own and ran too fast (Austin, PC). So vsync is off
// and the host pacer in hooks_frametime.cpp holds 30 fps. City, 150GS save, last 30 s (PC runs crackdown2_055/056):
// vsync on 33.33 ms avg / 35.7 ms 1% low; vsync off + fps_cap=30 33.33 / 34.1; uncapped 16.75 / 30.7 (59.7 fps).
// An explicit --vsync=true keeps the stock path (and no cap); an explicit --fps_cap wins.
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
      {"vsync", "false"},
      {"fps_cap", "30"},
      {"clear_memory_page_state", "false"},
      // SDK patch 0027: yield instead of sleeping on PM4 WAIT_REG_MEM polls, so loads run at the vsync-off
      // speed while vsync keeps the frame rate.
      {"wait_reg_mem_sleep", "false"},
  };
  const bool stock_vsync = rex::cvar::GetFlagSource("vsync") != rex::cvar::Source::kDefault &&
                           rex::cvar::Query<bool>("vsync");
  for (const Default& d : kDefaults) {
    const std::string_view name = d.name;
    if (stock_vsync && (name == "vsync" || name == "fps_cap")) continue;
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
