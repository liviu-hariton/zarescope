# Zarescope

**Listen to the sky.** A desktop app for RTL-SDR reception: live spectrum, aircraft (ADS-B) and Meteor-M weather satellites in one window, with automatic pass recording and a journal of everything received. Free and open source (GPL-3.0-or-later). Site: [zarescope.eu](https://zarescope.eu).

> **Status: M7, packaging.** `tools/package.sh` makes a self-contained, ad-hoc signed `Zarescope.app` (zip and dmg) for other Apple Silicon Macs.
>
> **M6, journal, Discovery and settings.**
> - New here? The Discover tab walks you through it: set your location, connect a receiver, then pick FM radio, air traffic, aircraft on radar, amateurs, walkie-talkies or Meteor images, each with the antenna that helps.
> - The Journal tab keeps every pass, image, aircraft (one row per day) and recording, with search and CSV/JSON export.
> - Settings (location, theme, language, receiver defaults, data folder, automatic recording) are saved between runs.
>
> **M5, automatic Meteor recording.**
> - Turn on "Record and decode passes automatically" in the Satellites tab. At each Meteor-M pass the app tunes the stick, records until LOS, gives the receiver back, and has [SatDump](https://www.satdump.org) turn the recording into images (in `~/Library/Application Support/Zarescope/products/`).
> - Passes are predicted for 48 hours from CelesTrak's orbits (M4), with a countdown, a polar plot and a notification before each pass.
> - ADS-B (M3) is decoded at 1090 MHz and shown in a table and on a radar.
>
> **M2, listening.** The app opens an RTL-SDR stick, or plays a recording, and shows the live spectrum and waterfall.
> - Listen to wideband FM, narrowband FM or AM, with squelch and presets.
> - Tune by typing, scrolling or clicking.
> - Record raw IQ.
>
> It has been built and tested on synthetic signals and recordings; the live checks wait for the development stick. See [docs/MILESTONES.md](docs/MILESTONES.md).

Zarescope only receives. It never transmits and never decodes encrypted or private communications.

## Hardware

Any RTL2832U dongle supported by librtlsdr: the RTL-SDR Blog V4 (development stick), V4 Lite and V3, the Nooelec NESDR SMArt v5, and generic R820T/R860 sticks. The V4 is out of production, so the V3 is the one to buy today. For Meteor-M LRPT at 137 MHz, pick a dongle with a TCXO; cheap ones without it drift as they warm up.

## Building (macOS, Apple Silicon)

You need the Xcode Command Line Tools, CMake ≥ 3.25 and Ninja. Every library (GLFW, Dear ImGui, ImPlot, KissFFT, librtlsdr, libusb, nlohmann/json, doctest) is downloaded by CMake at a pinned version and checked against its SHA-256, and linked statically: the app depends only on macOS itself.

```bash
xcode-select --install
brew install cmake ninja

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure

open build/Zarescope.app
# start in the light theme:
open build/Zarescope.app --args --theme light
# no stick? write a synthetic FM-band recording and play it:
build/tools/zarescope-synth /tmp/fm.cu8 20
open build/Zarescope.app --args --open /tmp/fm.cu8
# or six aircraft around a station, on the radar:
build/tools/zarescope-synth /tmp/adsb.cu8 30 --adsb --station 44.4268,26.1025
open build/Zarescope.app --args --open /tmp/adsb.cu8 --station 44.4268,26.1025
# the next Meteor passes over a station:
open build/Zarescope.app --args --station 44.4268,26.1025 --tab satellites
```

For satellite images, install SatDump separately (`brew install --cask satdump`); Zarescope finds it in `/Applications` and runs it after each pass. Without it, everything else works and the Satellites tab says how to install it. To try the whole chain without waiting for a pass: `--open` a 137.9 MHz recording and add `--simulate-pass 60`.

Recordings are saved to `~/Library/Application Support/Zarescope/recordings/` as `.cu8` plus a JSON sidecar ([docs/FORMATS.md](docs/FORMATS.md)).

**A copy for another Mac:** `tools/package.sh` builds, signs (ad hoc) and checks a Release `Zarescope.app` and packs it as a `.zip` and a `.dmg` in `build-release/dist/`; [docs/RELEASE.md](docs/RELEASE.md) explains how to open it there the first time.

Run from a terminal to see start-up errors: `build/Zarescope.app/Contents/MacOS/Zarescope`.

The tests need no receiver, window or GPU. With a stick plugged in, `build/tests/zarescope_hardware_tests` checks it: what it claims to be, whether its tuner agrees, and whether it streams at the requested rate. Warnings are errors in Zarescope's own code; pass `-DZARESCOPE_WERROR=OFF` to relax that while experimenting.

## Code style

`.clang-format` is the formatter (it ships with the Command Line Tools); run it before every commit:

```bash
find src tests -name '*.cpp' -o -name '*.h' -o -name '*.mm' | xargs xcrun clang-format -i
```

`.clang-tidy` holds the static-analysis checks and the naming rules; clang-tidy comes with Homebrew's `llvm`.

## Project layout

```
src/app/            main() and the frame loop
src/device/         sample sources (stick, file), the receiver pipeline, IQ recorder
src/dsp/            FFT, spectrum analyser, audio chain, lock-free queues
src/decoders/adsb/  Mode S / ADS-B: CRC, CPR, demodulator, aircraft store
src/audio/          sound output (miniaudio)
src/sat/            satellite catalog, TLEs, SGP4, pass prediction
src/ui/             the Dear ImGui screens, fonts, strings (EN + RO)
src/ui/theme/       design tokens → Dear ImGui style
src/platform/       small interfaces; macos/ holds Metal and the macOS paths
resources/fonts/    Inter and JetBrains Mono (SIL OFL 1.1, licences alongside)
design/             the design system: spec, tokens, waterfall LUT, logo, icons
tests/              doctest unit tests; tests/hardware needs a stick
tools/              developer tools (zarescope-synth)
docs/               architecture, decisions, dependencies, file formats, milestones
```

Start with [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), then [docs/MILESTONES.md](docs/MILESTONES.md) and [docs/DECISIONS.md](docs/DECISIONS.md).

## Licence

GPL-3.0-or-later, see [LICENSE](LICENSE). Third-party components and their licences are listed in [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md).
