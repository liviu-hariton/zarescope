# Zarescope

**Listen to the sky.** A desktop app for RTL-SDR reception: live spectrum, aircraft (ADS-B) and Meteor-M weather satellites in one window, with automatic pass recording and a journal of everything received. Free and open source (GPL-3.0-or-later). Site: [zarescope.eu](https://zarescope.eu).

> **Status: M0, the skeleton.** The app opens a Metal window with the Zarescope design system (dark and light themes, Inter and JetBrains Mono). It does not talk to a receiver yet; that arrives in M1. See [docs/MILESTONES.md](docs/MILESTONES.md).

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
```

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
src/ui/             the Dear ImGui screens, fonts, strings (EN + RO)
src/ui/theme/       design tokens → Dear ImGui style
src/platform/       small interfaces; macos/ holds Metal and the macOS paths
resources/fonts/    Inter and JetBrains Mono (SIL OFL 1.1, licences alongside)
design/             the design system: spec, tokens, waterfall LUT, logo, icons
tests/              doctest unit tests
docs/               architecture, decisions, dependencies, file formats, milestones
```

Start with [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), then [docs/MILESTONES.md](docs/MILESTONES.md) and [docs/DECISIONS.md](docs/DECISIONS.md).

## Licence

GPL-3.0-or-later, see [LICENSE](LICENSE). Third-party components and their licences are listed in [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md).
