"""Archipelago Launcher entry for Crackdown 2.

The game connects to the server itself; this component only starts the Crackdown Recomp launcher (which hosts
both games) on its Crackdown 2 page. An archipelago:// link from a room page is passed on as `--connect <uri>`.
"""
import logging
import os
import subprocess

logger = logging.getLogger("Crackdown 2")


def launch_client(*args: str) -> None:
    from settings import get_settings
    from Utils import env_cleared_lib_path
    try:
        # Prompts for the path when it is missing; cancelling the picker raises.
        launcher = str(get_settings().crackdown2_options.launcher_path)
    except (FileNotFoundError, OSError) as error:
        logger.error("Crackdown launcher not set: %s", error)
        return
    if not os.path.isfile(launcher):
        logger.error("Crackdown launcher not found at %s; set crackdown2_options.launcher_path in host.yaml",
                     launcher)
        return
    uri = next((arg for arg in args if isinstance(arg, str) and arg.startswith("archipelago://")), None)
    command = [launcher, "--game", "crackdown2", *(["--connect", uri] if uri else [])]
    try:
        # env: drop the AP AppImage's bundled LD_LIBRARY_PATH on Linux.
        subprocess.Popen(command, cwd=os.path.dirname(launcher) or None, env=env_cleared_lib_path())
    except OSError as error:  # never raise into the AP Launcher GUI
        logger.error("Could not start the Crackdown launcher %s: %s", launcher, error)
