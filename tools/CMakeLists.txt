# SPDX-License-Identifier: GPL-3.0-or-later
#
# Developer tools. Not part of the app bundle.

# zarescope-synth: writes a synthetic FM-band or ADS-B recording (see the source).
add_executable(zarescope-synth synth_recording.cpp)
target_link_libraries(zarescope-synth PRIVATE zarescope_device zarescope_adsb)
zarescope_set_warnings(zarescope-synth)
