// crackdown2 - host-side defaults the SDK leaves off
#pragma once

namespace crackdown2 {
// Sets SDK flags the game wants unless the user set them (command line, config, environment).
// Call from OnPreLaunchModule: GPU plugin flags only exist once the plugin has loaded.
void ApplyGameDefaults();
}  // namespace crackdown2
