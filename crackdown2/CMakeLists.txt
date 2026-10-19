# crackdown2 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(crackdown2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(CRACKDOWN2_SOURCES
    src/main.cpp
    src/hooks_autoinput.cpp
    src/host_tuning.cpp
    src/host_tuning.cpp
    src/hooks_intro.cpp
    src/hooks_frametime.cpp
    src/hooks_framerate.cpp
)

if(WIN32)
    add_executable(crackdown2 WIN32 ${CRACKDOWN2_SOURCES})
else()
    add_executable(crackdown2 ${CRACKDOWN2_SOURCES})
endif()

rexglue_setup_target(crackdown2 GPU_PLUGINS xenos)

# Public-symbol PDB on Windows: link-only (no -g), so debugger frames show sub_XXXXXXXX (guest address).
# /DEBUG turns off lld's default /OPT:REF,ICF, so restore them explicitly.
if(WIN32)
    target_link_options(crackdown2 PRIVATE -Wl,/DEBUG:FULL -Wl,/OPT:REF,/OPT:ICF)
endif()
