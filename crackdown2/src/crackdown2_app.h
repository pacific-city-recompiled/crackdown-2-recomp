// crackdown2 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>

#include "host_tuning.h"
#include "ap/bridge.h"

class Crackdown2App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Crackdown2App>(new Crackdown2App(ctx, "crackdown2",
        PPCImageConfig));
  }

  // After cvars are parsed and logging is up: pick the GPU plugin.
  void OnPostInitLogging() override { crackdown2::ApplyGpuPluginDefault(); }

  // No-argument starts: portable mode and the default game folder.
  void OnConfigurePaths(rex::PathConfig& paths) override { crackdown2::ApplyStartupDefaults(paths); }

  // After the GPU plugin has loaded, right before the guest starts.
  void OnPreLaunchModule() override {
    crackdown2::ApplyGameDefaults();
    crackdown2::DumpGuestImage(runtime());
    SetGuestFrameStats(&crackdown2::GuestFrameStats);
    crackdown2::ap::Initialize(user_data_root());
  }

  // Close button / Alt+F4: the SDK then hard-exits, so flush the frame log now.
  bool OnWindowCloseRequested() override {
    crackdown2::FlushFrameLog();
    crackdown2::ap::Shutdown();
    return true;
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnPreSetup(rex::RuntimeConfig& config) override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}
};
