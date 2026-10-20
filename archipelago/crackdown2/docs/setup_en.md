# Crackdown 2 Multiworld Setup Guide

## Required software

- [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases) 0.6.8 or newer
- Crackdown 2 Recomp with the Archipelago-enabled game, installed from your own disc dump and title update
- `crackdown2.apworld` from the same release

## Installing the world

Double-click `crackdown2.apworld`, or copy it into the `custom_worlds` folder of your Archipelago install, then
restart the Archipelago Launcher.

## Creating your options file

Open the Archipelago Launcher, choose **Options Creator**, select **Crackdown 2**, set your options and export the
YAML.

## Connecting

1. Open the Crackdown Recomp launcher on the Crackdown 2 page (or **Crackdown 2 Client** in the Archipelago
   Launcher; the first time it asks where `CrackdownLauncher` is).
2. Pick a save profile and fill in the **ARCHIPELAGO** tab: server address, slot name and password.
3. Press Play and start a new game. The game connects to the server by itself.

Without the launcher tab, start the game with `--ap_server=<host:port> --ap_slot=<name>`; the password goes in
`<user data>/archipelago/connection.toml` as `password = "..."`.

Use a fresh save profile for each multiworld seed. A profile's save is bound to the first seed it connects to;
with any other seed the game refuses the connection and stays offline.
