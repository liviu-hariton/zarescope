# Dependencies

Every third-party component, with its version, licence and reason. A new dependency gets a row here in the same commit that adds it. Archives fetched by CMake are pinned by SHA-256 in `cmake/Dependencies.cmake`.

## Linked into the app

| Component | Version | Licence | Why | How |
|---|---|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.93.0, docking branch | MIT | The UI; docking for the later panel layout | FetchContent, built as `zs_imgui` (+ `zs_imgui_metal`) |
| [GLFW](https://www.glfw.org) | 3.5.1 | zlib | Window, input and monitors on every platform | FetchContent |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | MIT | Design tokens, IQ sidecar files; later settings, the satellite catalog and presets | FetchContent (header-only) |
| [ImPlot](https://github.com/epezent/implot) | 1.1 | MIT | The spectrum plot and its frequency axis | FetchContent, built as `zs_implot` |
| [KissFFT](https://github.com/mborgerding/kissfft) | 131.1.0 | BSD-3-Clause | FFT behind `dsp::FftEngine` (vDSP may follow in v2) | FetchContent, `kiss_fft.c` only, as `zs_kissfft` |
| [librtlsdr, RTL-SDR Blog fork](https://github.com/rtlsdrblog/rtl-sdr-blog) | V1.4.0 (2026-08) | GPL-2.0-or-later | Access to the RTL-SDR stick; this release recognises the Blog V4 (R828D) and V4 Lite (R828S) | FetchContent, the library and tuner sources built as static `zs_rtlsdr` (no pkg-config needed) |
| [miniaudio](https://miniaud.io) | 0.11.26 | Public domain (Unlicense) or MIT-0 | Sound output on every platform (CoreAudio on macOS) | FetchContent, `miniaudio.c` built as `zs_miniaudio`, playback only, system frameworks linked directly |
| [sgp4](https://github.com/dnwrnr/sgp4) | 3.0 (2026-09) | Apache-2.0 | SGP4/SDP4 orbit propagation from TLEs (Vallado 2006) | FetchContent, `libsgp4/*.cpp` built as `zs_sgp4` |
| [libcurl](https://curl.se) | the system's (macOS SDK) | curl (MIT-style) | TLE downloads from CelesTrak | `find_package(CURL)`; on Linux and Windows a system or vcpkg package |
| [libusb](https://libusb.info) | 1.0.30 (via [libusb-cmake](https://github.com/libusb/libusb-cmake) v1.0.30-0) | LGPL-2.1-or-later | USB access for librtlsdr | FetchContent, built static for our deployment target (Homebrew's static library targets the newest macOS only) |

## Tests only

| Component | Version | Licence | Why | How |
|---|---|---|---|---|
| [doctest](https://github.com/doctest/doctest) | 2.5.3 | MIT | Unit tests | FetchContent (header-only) |

## Bundled files

| Component | Version | Licence | Where |
|---|---|---|---|
| [Inter](https://rsms.me/inter/) | 4.1 (static TTF: Regular, Medium, SemiBold) | SIL OFL 1.1 | `resources/fonts/`, licence in `Inter-OFL.txt`, copied into the app bundle |
| [JetBrains Mono](https://www.jetbrains.com/lp/mono/) | 2.304 (NL static TTF: Regular, Medium) | SIL OFL 1.1 | `resources/fonts/`, licence in `JetBrainsMono-OFL.txt`, copied into the app bundle |

## System frameworks (macOS)

Cocoa, Foundation, Metal, QuartzCore, UniformTypeIdentifiers and UserNotifications are part of macOS and are linked from `src/platform/macos`. IOKit, CoreFoundation and Security come in through libusb, CoreAudio and AudioToolbox through miniaudio.

## External programs (not linked)

| Program | Version checked | Licence | Why | How |
|---|---|---|---|---|
| [SatDump](https://www.satdump.org) | 1.2.2 | GPL-3.0 | Decodes Meteor-M LRPT recordings into images (delegated to it in v1) | The user installs it (`brew install --cask satdump`). Run as a separate process; found in `/Applications/SatDump.app`, `~/Applications`, Homebrew's bin or `PATH`. The app works without it and says how to install it. |

## Planned

These come from the plan and are added (with their rows above) in the milestone that needs them: SQLite (the journal, M6).
