# Milestones

This file records each milestone: what it delivers, its acceptance criteria, and where it stands.

## M0 — Skeleton ✅ (2026-11-11)

- [x] CMake (≥ 3.25, Ninja) builds `build/Zarescope.app`, a macOS bundle for arm64 on macOS 14 and later, with an icon built from `design/brand/app-icon` and a complete `Info.plist`.
- [x] Dear ImGui (docking branch) on Metal through `platform::Renderer`, in a GLFW window.
- [x] Dependencies pinned by version and SHA-256 (`cmake/Dependencies.cmake`).
- [x] The design system as the ImGui theme:
  - tokens are loaded and validated at start-up;
  - dark is the default, light is one click away;
  - every `ImGuiCol` and style value comes from the tokens.
- [x] Inter and JetBrains Mono, with the type scale from the tokens. Romanian comma-below letters and thin spaces are present in every font.
- [x] The shell:
  - top bar with the logo, a receiver chip and the theme switch;
  - an empty state that says receiver support isn't there yet;
  - status bar with the version, UTC and local time.
- [x] Every UI string in a central EN + RO table.
- [x] doctest via CTest: 18 tests, no hardware, window or GPU needed.
- [x] `.clang-format`, `.clang-tidy`, `.gitignore`, `README.md`.
- [x] The native title bar follows the app's theme (`platform::setWindowAppearance`).
- [x] No CI, by decision (see [DECISIONS.md](DECISIONS.md)). Build, tests and formatting are checked locally.

**Acceptance.**

- `cmake --build build` produces the app with no warnings under `-Werror`.
- The app opens in both themes, checked on screen.
- `ctest` passes 18/18.

**Left for later.**

- The language follows a setting that arrives in M6; English is shown until then.
- clang-tidy hasn't been run yet; it needs `brew install llvm`.

## M1 — Device, spectrum and waterfall 🟡 (built and tested on recordings; live checks wait for the stick)

- [x] Device: list sticks, open/close, frequency, sample rate (default 2.4 MS/s), gain (manual on the tuner's steps, or automatic), ppm. Bias-tee off by default, forced off on every open and close, and turned on only after an inline warning.
- [x] Stick identity: the EEPROM's model claim against the tuner chip. A "Blog V4" that isn't an R828D is flagged as a likely counterfeit.
- [x] Source thread → SPSC block queue → DSP thread → FFT with a Blackman-Harris window and averaging (configurable FFT size and trace smoothing).
- [x] Spectrum (ImPlot) and waterfall (GPU texture updated one row at a time). The replay of a 2.4 MS/s recording draws at the display's rate at about 6 % CPU.
- [x] Tuning:
  - type the frequency;
  - scroll or use the arrow keys over a digit;
  - scroll over the spectrum or waterfall;
  - click the spectrum or waterfall to tune there.
- [x] `IqRecorder`: raw `cu8` plus a JSON sidecar (frequency, sample rate, UTC start, gain, ppm, device, sample count), written on its own thread.
- [x] `FileSource`: plays a recording through exactly the same chain; the app opens one with the system dialog or `--open`.
- [x] `zarescope-synth` writes a synthetic FM-band recording for working without a stick.

**Acceptance.**

| Criterion | Status |
|---|---|
| A synthetic tone at a known offset peaks in the right FFT bin | ✅ `test_dsp.cpp` (four offsets, level within 0.05 dB) |
| A recorded file replays identically | ✅ `test_iq_files.cpp` (byte for byte, through the recorder and back) |
| With the stick connected, the FM band appears correctly | ⏳ waits for the RTL-SDR Blog V4. Then run `build/tests/zarescope_hardware_tests` and look at 88–108 MHz. |

**When the stick arrives.**

1. Run `build/tests/zarescope_hardware_tests`. It checks:
   - the identity verdict, i.e. whether the V4 is genuine;
   - the streaming rate and dropped blocks;
   - a live spectrum.
2. Open the stick in the app at 100 MHz and check that the FM stations show up and line up with known frequencies.
3. Check the ppm correction against a known carrier.

**Left for later.**

- Errors that come from the device layer (librtlsdr failures) are English text inside the translated banner; they get StringIds with Settings (M6).
- The line icons are drawn with primitives. The 24-grid SVG set in `design/icons` needs an SVG rasterizer (nanosvg), planned for when more icons are needed.
- The frequency display shows the integer MHz part at its natural width (98 vs 100), so the highest digits appear only when the value needs them.
