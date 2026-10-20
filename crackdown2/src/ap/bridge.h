// In-process Archipelago bridge (bridge.cpp) and its Phase 0 probe (ap_probe.cpp).
#pragma once
#include <cstdint>
#include <filesystem>

struct PPCContext;

namespace crackdown2::ap {
// Opens this profile's AP sidecar and starts the client when --ap_server is set.
void Initialize(const std::filesystem::path& user_data_root);
void Shutdown();
// Game thread, after each game update (hooks_framerate.cpp, sub_82915C70).
void OnFrame(PPCContext& ctx, uint8_t* base);
void ProbeFrame(uint8_t* base);
void ProbeSetActive(PPCContext& ctx, uint8_t* base);
void ProbeAddXP(PPCContext& ctx, uint8_t* base);  // runs the native AddXP
void ProbeUpdate(PPCContext& ctx, uint8_t* base);  // test sweep, after each game update
}  // namespace crackdown2::ap
