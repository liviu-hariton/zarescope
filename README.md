# Zarescope

**Listen to the sky.** A desktop app for RTL-SDR reception: live spectrum, aircraft (ADS-B) and Meteor-M weather satellites in one window, with automatic pass recording and a journal of everything received. Free and open source (GPL-3.0-or-later). Site: [zarescope.eu](https://zarescope.eu).

## What it does

- **Listen.** Live spectrum and waterfall; wideband FM, narrowband FM and AM, with squelch and presets. Tune by typing, scrolling or clicking; record raw IQ, or play a recording back.
- **Aircraft.** ADS-B at 1090 MHz, decoded by Zarescope itself, in a table and on a radar around your station.
- **Weather satellites.** Meteor-M passes predicted 48 hours ahead from CelesTrak's orbits, with a countdown, a polar plot and a notification. Turn on automatic recording and, at each pass, the app tunes the stick, records until the satellite sets and has [SatDump](https://www.satdump.org) turn the recording into images.
- **Journal.** Every pass, image, aircraft (one row per day) and recording, with search and CSV/JSON export.
- **Guidance.** The Discover tab says what to listen to and which antenna helps; the user guide is built in (Help, ⌘?). English and Romanian, dark and light.

## Status

**Version 0.11.0**: every v1 milestone (M0–M7) is built and tested on synthetic signals, recordings and a simulated satellite pass with the real SatDump. The checks with a live receiver are under way; [docs/MILESTONES.md](docs/MILESTONES.md) has the criterion-by-criterion state.

## Download

macOS 14 (Sonoma) or later, Apple Silicon: the `.dmg` from the [latest release](https://github.com/liviu-hariton/zarescope/releases/latest), or from [zarescope.eu/download](https://zarescope.eu/download/). The app isn't notarized yet, so macOS asks for confirmation the first time; [docs/RELEASE.md](docs/RELEASE.md#installing-on-another-mac) has the steps. For satellite images, also install SatDump: `brew install --cask satdump`.

Zarescope only receives. It never transmits and never decodes encrypted or private communications.

## Hardware

Any RTL2832U dongle supported by librtlsdr: the RTL-SDR Blog V4 (development stick), V4 Lite and V3, the Nooelec NESDR SMArt v5, and generic R820T/R860 sticks. The V4 is out of production, so the V3 is the one to buy today. For Meteor-M LRPT at 137 MHz, pick a dongle with a TCXO; cheap ones without it drift as they warm up.

## Building (macOS, Apple Silicon)

You need the Xcode Command Line Tools, CMake ≥ 3.25 and Ninja. Every library (GLFW, Dear ImGui, ImPlot, KissFFT, librtlsdr, libusb, miniaudio, sgp4, nlohmann/json, doctest) is downloaded by CMake at a pinned version and checked against its SHA-256, and linked statically: the app depends only on macOS itself.

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

**Website:** https://zarescope.eu, built from `site/` (Astro). See [docs/RELEASE.md](docs/RELEASE.md#the-website) for previewing, screenshots and deploying.

**A copy for another Mac:** `tools/package.sh` builds, signs (ad hoc) and checks a Release `Zarescope.app` and packs it as a `.zip` and a `.dmg` in `build-release/dist/`; [docs/RELEASE.md](docs/RELEASE.md) explains how to open it there the first time.

The user guide is in the app (Help tab, ⌘?) and in [resources/help](resources/help/guide.en.md). Every command is in the macOS menu bar with its shortcut; `--print-menus` prints the structure.

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
src/app/            main(), the frame loop, settings, pass automation, update check
src/device/         sample sources (stick, file), the receiver pipeline, IQ recorder, device arbiter
src/dsp/            FFT, spectrum analyser, audio chain, lock-free queues
src/decoders/adsb/  Mode S / ADS-B: CRC, CPR, demodulator, aircraft store
src/audio/          sound output (miniaudio)
src/net/            small HTTPS downloads (orbits, the update check)
src/sat/            satellite catalog, TLEs, SGP4, pass prediction, pass scheduler
src/integrations/   SatDump, run as a separate process
src/store/          the journal (SQLite)
src/ui/             the Dear ImGui screens, menu model, user guide, fonts, strings (EN + RO)
src/ui/theme/       design tokens → Dear ImGui style
src/platform/       small interfaces; macos/ holds Metal, the menu bar, notifications, location, paths
resources/          satellite catalog, presets, user guide (EN + RO), fonts (SIL OFL 1.1)
design/             the design system: spec, tokens, waterfall LUT, logo, icons
tests/              doctest unit tests; tests/hardware needs a stick
tools/              zarescope-synth, packaging, release, website capture and deploy scripts
site/               the zarescope.eu website (Astro)
docs/               architecture, decisions, dependencies, file formats, milestones, releasing
```

Start with [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), then [docs/MILESTONES.md](docs/MILESTONES.md) and [docs/DECISIONS.md](docs/DECISIONS.md).

## Licence

GPL-3.0-or-later, see [LICENSE](LICENSE). Third-party components and their licences are listed in [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md).
