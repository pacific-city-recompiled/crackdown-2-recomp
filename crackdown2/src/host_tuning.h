// crackdown2 - host-side defaults the SDK leaves off
#pragma once

namespace rex { class Runtime; }

namespace crackdown2 {
// Sets SDK flags the game wants unless the user set them (command line, config, environment).
// Call from OnPreLaunchModule: GPU plugin flags only exist once the plugin has loaded.
void ApplyGameDefaults();
// --dump_image: writes the loaded guest image to a file.
void DumpGuestImage(rex::Runtime* runtime);
}  // namespace crackdown2
