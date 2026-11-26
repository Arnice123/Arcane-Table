# Technical spikes: throwaway prototypes that exist to answer one question and
# produce an ADR (docs/adr/). Nothing here ships and nothing in core/, net/ or
# server/ may depend on it.
#
# Enabled with -DARCANE_BUILD_SPIKES=ON (preset: spikes-release). Added from the
# root CMakeLists.txt only when the Qt client is enabled.
#
# Spike code is allowed to be rough, so these targets deliberately do NOT link
# arcane_warnings (no -Werror from CI presets). Add a spike by creating a
# folder with its own CMakeLists.txt and listing it below.

add_subdirectory(s2_hud)   # ARCH-05: Qt overlay widgets vs GL-drawn HUD
add_subdirectory(s10_deps)