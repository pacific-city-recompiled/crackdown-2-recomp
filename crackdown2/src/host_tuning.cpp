// crackdown2 - host-side defaults the SDK leaves off
//
// vsync=false + fps_cap=30 (2026-10-18): the game steps its simulation a fixed 1/30 s per frame (dt = [0x832B60F8] *
// 1/30 at 0x82114994, sub_82369AA0), so it must run at exactly 30 frames a second. With vsync on, the guest's vblank
// waits hold that, but the SDK also sleeps on PM4 WAIT_REG_MEM polls (patch 0027 removes that) and New Game loaded
// ~3x slower. With vsync off the game has no limiter of its own and ran too fast (Austin, PC). So vsync is off
// and the host pacer in hooks_frametime.cpp holds 30 fps. City, 150GS save, last 30 s (PC runs crackdown2_055/056):
// vsync on 33.33 ms avg / 35.7 ms 1% low; vsync off + fps_cap=30 33.33 / 34.1; uncapped 16.75 / 30.7 (59.7 fps).
// An explicit --vsync=true keeps the stock path (and no cap); an explicit --fps_cap wins.
// 60 fps default (2026-10-20): fps_cap=60 + sim_game_clock (hooks_framerate.cpp: measured step, the 16 ms sites and
// --sim_frame_fixes). Austin's launcher playtest at 60 fps: speech, crowd anims, stairs, driving all fine.
// --vsync=true still gives the stock 30 fps path; --fps_cap=30 keeps the clock at a 1/30 step.
// clear_memory_page_state=false: Crackdown 1's measured fix (2026-10-17: re-uploaded ~20 MB/frame otherwise).

#include "host_tuning.h"

#include <cmath>
#include <fstream>
#include <string>
#include <string_view>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/input/device_assignment.h>
#include <rex/input/input_system.h>
#include <rex/logging/macros.h>
#include <rex/rex_app.h>
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
      {"fps_cap", "60"},
      {"sim_game_clock", "true"},
      {"clear_memory_page_state", "false"},
      // SDK patch 0027: yield instead of sleeping on PM4 WAIT_REG_MEM polls, so loads run at the vsync-off
      // speed while vsync keeps the frame rate.
      {"wait_reg_mem_sleep", "false"},
      // The SDK reads every memexport draw's output back to the CPU and waits for the GPU each time (Xenia's
      // d3d12_readback_memexport defaults to false). City spawn, uncapped, PC 2026-10-19: the GPU command
      // thread sat in CheckSubmissionFence under IssueDraw_MemexportReadbackFullPath in 13 of 30 cdb samples;
      // off: 69.0 -> 146.5 fps avg, 1% low 17.0 -> 8.7 ms (runs 068/071). Same image at 30 fps (072/073).
      {"readback_memexport", "false"},
  };
  const bool stock_vsync = rex::cvar::GetFlagSource("vsync") != rex::cvar::Source::kDefault &&
                           rex::cvar::Query<bool>("vsync");
  for (const Default& d : kDefaults) {
    const std::string_view name = d.name;
    if (stock_vsync && (name == "vsync" || name == "fps_cap" || name == "sim_game_clock")) continue;
    // Only where nothing else set it. GetFlagSource also reports kDefault for an unregistered flag.
    if (rex::cvar::GetFlagSource(d.name) != rex::cvar::Source::kDefault) continue;
    if (rex::cvar::SetFlagByName(d.name, d.value)) {
      REXLOG_INFO("crackdown2 default: --{}={}", d.name, d.value);
    } else {
      REXLOG_WARN("crackdown2 default: could not set --{}={}", d.name, d.value);
    }
  }
  // --aspect_ratio: give the guest frame buffer the same shape as the camera (hooks_aspect.cpp), width a multiple
  // of 8. Height: the window's when one is set, else 720. The SDK takes the video mode height from window_height
  // whenever video_mode_height holds its default (720), so a 720-line width in a 1080-high window would become a
  // 14:9 mode that the presenter letterboxes (Crackdown 1 GitHub issue #13).
  const auto user_set = [](const char* name) { return rex::cvar::GetFlagSource(name) != rex::cvar::Source::kDefault; };
  if (!AspectRatioIsDefault() && !user_set("video_mode_width") && !user_set("video_mode_height")) {
    const int32_t window_height = rex::cvar::Query<int32_t>("window_height");
    const int height = user_set("window_height") && window_height > 0 ? window_height : 720;
    const int width = int(std::lround(height * AspectRatio() / 8.0)) * 8;
    if (rex::cvar::SetFlagByName("video_mode_width", std::to_string(width)) &&
        rex::cvar::SetFlagByName("video_mode_height", std::to_string(height))) {
      REXLOG_INFO("crackdown2 default: --video_mode_width={} --video_mode_height={} (aspect_ratio)", width, height);
    } else {
      REXLOG_WARN("crackdown2 default: could not set the video mode for aspect_ratio");
    }
  }
}

}  // namespace crackdown2

// Every physical pad drives guest user 0 (2026-10-26). The SDK gives each pad its own user index in connection order,
// but only user 0 has a signed-in profile (xam_user.cpp). Austin's PC lists a PS4 pad first and his Xbox One S pad
// second (cd2 log crackdown2_001), so his Start arrived as user 1 and the Press Start screen never moved on.
// Crackdown 2 has no split screen (co-op is one player per PC), so nothing needs users 1-3.
// --input_all_pads_user0=false gives the SDK's one-user-per-pad routing back.
REXCVAR_DEFINE_BOOL(input_all_pads_user0, true, "Crackdown2", "Route every controller to player 1 (user 0)");

namespace crackdown2 {

void ApplyInputDefaults(rex::Runtime* runtime) {
  if (!REXCVAR_GET(input_all_pads_user0) || !runtime || !runtime->input_system()) return;
  // InputSystem is the only IInputSystem implementation (sdk input_system.h).
  static_cast<rex::input::InputSystem*>(runtime->input_system())
      ->SetDeviceAssignment(std::make_unique<rex::input::SharedAssignment>());
  REXLOG_INFO("crackdown2 default: every controller drives user 0 (--input_all_pads_user0)");
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

REXCVAR_DECLARE(std::string, gpu_plugin);  // SDK rex_app.cpp

namespace crackdown2 {

// Runs from OnPostInitLogging: after the config file has loaded (a gpu_plugin set there wins) and before the
// plugin is chosen. Without it a start with no --gpu_plugin (the launcher's) has no GPU and stays black
// (Austin, launcher test package 2026-10-19: "VdInitializeRingBuffer: no GPU emulation loaded"). Same as
// Crackdown 1.
void ApplyGpuPluginDefault() {
  if (rex::cvar::GetFlagSource("gpu_plugin") == rex::cvar::Source::kDefault && REXCVAR_GET(gpu_plugin).empty()) {
    rex::cvar::SetFlagByName("gpu_plugin", "xenos");
    REXLOG_INFO("crackdown2 default: --gpu_plugin=xenos");
  }
}

// Portable mode: a portable.txt next to the exe keeps saves and the shader cache in userdata/ beside it (the
// launcher creates the marker and also passes the roots). A start without --game_data_root finds game/ next to
// the exe or in a parent folder (the dev build under crackdown2/out/build/<preset>/).
void ApplyStartupDefaults(rex::PathConfig& paths) {
  const std::filesystem::path exe_dir = rex::filesystem::GetExecutableFolder();
  std::error_code ec;
  if (std::filesystem::is_regular_file(exe_dir / "portable.txt", ec)) {
    if (rex::cvar::Query<std::string>("user_data_root").empty()) paths.user_data_root = exe_dir / "userdata";
    if (rex::cvar::Query<std::string>("cache_root").empty()) paths.cache_root = exe_dir / "userdata" / "cache";
  }
  if (paths.game_data_root.empty()) {
    std::filesystem::path dir = exe_dir;
    for (int depth = 0; depth < 6 && !dir.empty(); ++depth, dir = dir.parent_path()) {
      if (std::filesystem::is_regular_file(dir / "game" / "default.xex", ec)) {
        paths.game_data_root = dir / "game";
        REXLOG_INFO("crackdown2 default: --game_data_root={}", paths.game_data_root.string());
        break;
      }
    }
  }
  // Title update tree (update:\Version.txt etc.): tu/ beside game/ (tools/setup_game.py writes both), else the
  // game/TU copy the game also reads as D:\TU. Lets the release package run with no arguments.
  if (paths.game_data_root.empty() || !paths.update_data_root.empty() ||
      !rex::cvar::Query<std::string>("update_data_root").empty()) {
    return;
  }
  for (const std::filesystem::path& tu :
       {paths.game_data_root.parent_path() / "tu", paths.game_data_root / "TU"}) {
    if (std::filesystem::is_regular_file(tu / "Version.txt", ec)) {
      paths.update_data_root = tu;
      REXLOG_INFO("crackdown2 default: --update_data_root={}", tu.string());
      return;
    }
  }
}

}  // namespace crackdown2
