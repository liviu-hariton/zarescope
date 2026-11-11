# Dependencies

Every third-party component, with its version, licence and reason. A new dependency gets a row here in the same commit that adds it. Archives fetched by CMake are pinned by SHA-256 in `cmake/Dependencies.cmake`.

## Linked into the app

| Component | Version | Licence | Why | How |
|---|---|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.93.0, docking branch | MIT | The UI; docking for the later panel layout | FetchContent, built as `zs_imgui` (+ `zs_imgui_metal`) |
| [GLFW](https://www.glfw.org) | 3.5.1 | zlib | Window, input and monitors on every platform | FetchContent |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | MIT | Design tokens; later settings, the satellite catalog and presets | FetchContent (header-only) |

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

Cocoa, Foundation, Metal and QuartzCore are part of macOS and are linked from `src/platform/macos` only.

## Planned

These come from the plan and are added (with their rows above) in the milestone that needs them: librtlsdr from the RTL-SDR Blog fork, libusb, ImPlot, KissFFT, miniaudio, an SGP4 implementation, libcurl, SQLite. SatDump is an external program the user installs; it is not linked.
