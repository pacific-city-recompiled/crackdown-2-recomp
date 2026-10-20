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
    src/ap/ap_probe.cpp
    src/ap/bridge.cpp
    src/ap/catalog.cpp
    src/ap/client.cpp
    src/ap/session_state.cpp
    common/toml_edit.cpp
)

if(WIN32)
    add_executable(crackdown2 WIN32 ${CRACKDOWN2_SOURCES})
else()
    add_executable(crackdown2 ${CRACKDOWN2_SOURCES})
endif()

rexglue_setup_target(crackdown2 GPU_PLUGINS xenos)

# Archipelago (src/ap): SHA-256 and TOML from the SDK's third-party tree, nlohmann/json from inja.
set(CRACKDOWN2_THIRDPARTY "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/thirdparty")
target_sources(crackdown2 PRIVATE "${CRACKDOWN2_THIRDPARTY}/crypto/sha256.cpp")
target_include_directories(crackdown2 PRIVATE "${CRACKDOWN2_THIRDPARTY}/crypto"
    "${CRACKDOWN2_THIRDPARTY}/tomlplusplus/include" "${CRACKDOWN2_THIRDPARTY}/inja/third_party/include")

# AP catalog: one versioned JSON for the C++ bridge and the apworld, embedded as bytes (string literals this
# size exceed compiler limits). Same scheme as Crackdown 1.
set(AP_CATALOG "${CMAKE_CURRENT_SOURCE_DIR}/ap/catalog.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${AP_CATALOG}")
file(READ "${AP_CATALOG}" _ap_hex HEX)
string(LENGTH "${_ap_hex}" _ap_len)
math(EXPR _ap_bytes "${_ap_len} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _ap_array "${_ap_hex}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ap_generated/ap_catalog_data.h.tmp"
    "// Generated from crackdown2/ap/catalog.json; do not edit.\n#pragma once\n"
    "inline constexpr unsigned char kCatalogJson[] = {${_ap_array}};\n"
    "inline constexpr unsigned long kCatalogJsonSize = ${_ap_bytes};\n")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/ap_generated/ap_catalog_data.h.tmp"
    "${CMAKE_CURRENT_BINARY_DIR}/ap_generated/ap_catalog_data.h" COPYONLY)
target_include_directories(crackdown2 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/ap_generated")

# Public-symbol PDB on Windows: link-only (no -g), so debugger frames show sub_XXXXXXXX (guest address).
# /DEBUG turns off lld's default /OPT:REF,ICF, so restore them explicitly.
if(WIN32)
    target_link_options(crackdown2 PRIVATE -Wl,/DEBUG:FULL -Wl,/OPT:REF,/OPT:ICF)
endif()
