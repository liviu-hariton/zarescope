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

## M2 — Listening: wideband FM, narrowband FM, AM 🟡 (built and tested on synthetic signals; listening to a real station waits for the stick)

- [x] Chain:
  1. frequency shift (NCO);
  2. channel low-pass and decimation in stages;
  3. demodulator (quadrature FM, envelope AM with slow AGC);
  4. de-emphasis for FM broadcast (50 µs default, 75 µs selectable);
  5. audio low-pass;
  6. resampling to the sound card's rate;
  7. miniaudio.
- [x] VFO on the spectrum and waterfall (dashed line, channel edges), with adjustable width (Sound popover) and a squelch with hysteresis that shows the live channel level.
- [x] Tuning moves the VFO. The stick retunes only when the VFO leaves the captured span, landing an eighth of the span off centre. In a recording the VFO moves within what was captured, so a recording can be listened to station by station.
- [x] Presets in `resources/presets.json`, opened from the chevron beside the frequency: FM radio, airband (AM), 2 m amateur, PMR446 (446 MHz), ADS-B, Meteor.
- [x] Language switch (English / Română) and the theme in the settings popover.

**Acceptance.**

| Criterion | Status |
|---|---|
| A synthetic FM signal modulated by a tone demodulates to that tone | ✅ `test_audio.cpp`: 1 kHz at full scale within 10 %; a neighbour 400 kHz away rejected; NFM and AM too |
| An FM station sounds clean | ⏳ waits for the stick. Open it, pick the FM radio preset and choose WFM. |

**Costs.** At 2.4 MS/s on the M4, one core: WFM 2.5 %, NFM and AM 1.3 %, the spectrum 0.6 %.

**Left for later.**

- Stereo FM (the 19 kHz pilot and the L−R subcarrier): mono for now.
- The band edges can't be dragged; the width is set in the Sound popover.
- Sound is not tested through the receiver in CTest, because tests must not play audio or need a sound card. The demodulator is tested directly.

## M3 — ADS-B (1090 MHz) 🟡 (built and tested on published test messages and synthetic IQ; live aircraft wait for the stick)

- [x] The decoder, written from scratch (no readsb). Steps:
  1. magnitude at 2 MS/s;
  2. preamble detection;
  3. 56- and 112-bit Mode S messages;
  4. CRC-24 with single-bit correction;
  5. DF17/DF18 content: identification (callsign), airborne position (CPR global from an even/odd pair, local against the last fix or the station), velocity (ground speed and track, or airspeed and heading; vertical rate), altitude (25-ft and Gillham).
- [x] `AircraftStore`: active aircraft expire after 60 s; short position trails; bad-decode jumps are rejected.
- [x] UI: the Aircraft tab, with a table (ICAO or callsign, altitude, speed, distance, last heard) and a radar centred on the station (range rings, trails, heading arrows).
- [x] The ADS-B preset tunes 1090 MHz at 2 MS/s. Decoding runs whenever the source is set up like that, on the stick or on a recording.
- [x] `zarescope-synth --adsb` writes a demo recording with six aircraft.

**Acceptance.**

| Criterion | Status |
|---|---|
| Tests pass on test vectors (callsign, position, altitude) | ✅ `test_adsb.cpp`, from "The 1090 MHz Riddle": KLM1023; 52.2572° N 3.91937° E at 38 000 ft (global and local); both velocity subtypes |
| On a recorded 1090 MHz IQ file, aircraft appear | ✅ on synthetic recordings: `test_receiver.cpp` end to end, and the radar on screen (six aircraft from `zarescope-synth --adsb`). A real recording comes with the stick. |
| Live, aircraft appear on the radar | ⏳ waits for the stick and an antenna that sees the sky |

**Left for later.**

- **Station location.** It is set in Settings or with `--station`, but not saved yet; persistence comes with Settings (M6).
- **Other downlink formats.** DF0/4/5/16/20/21 (address-overlaid parity) and surface positions are not decoded yet.
- **Demodulation quality.** The demodulator is the classic 2 MS/s pulse comparison. Phase-corrected or 2.4 MS/s demodulation (as in readsb) would catch more weak messages.

## M4 — Satellites: TLE, positions, passes ✅ (2026-11-11; checked against real CelesTrak data)

- [x] `SatCatalog` from `resources/sat_catalog.json`: Meteor-M N2-3 (57166) and N2-4 (59051), LRPT at 137.9 MHz. Frequencies come only from the catalog.
- [x] TLEs from CelesTrak's GP API by NORAD id:
  - at most once every 12 h per satellite;
  - User-Agent `Zarescope/<version> (+https://zarescope.eu)`;
  - cached in `tle_cache/`, used offline, with its age shown.
- [x] SGP4 (dnwrnr/sgp4 3.0) and the pass predictor: 48 h, AOS / TCA / LOS, maximum elevation, azimuths at AOS and LOS, direction, minimum-elevation filter (default 20°, set in Settings).
- [x] Satellites tab:
  - a next-pass card with a countdown;
  - the pass list;
  - a polar plot of the selected pass.
- [x] A macOS notification five minutes before each AOS.

**Acceptance.**

| Criterion | Status |
|---|---|
| SGP4 positions match known reference values for a fixed TLE (tolerance documented) | ✅ Vallado's verification satellite 00005 at 0 and 360 min, within 1 m and 1 mm/s (`test_sat.cpp`) |
| Passes predicted for a fixed location and TLE are stable (golden test) | ✅ `tests/data/golden_passes.txt`, a sun-synchronous TLE over Bucharest |

**Checked live.** On 2026-11-11 the app fetched both Meteor TLEs from CelesTrak. It predicted 13 passes above 20° over Bucharest in the next 48 h; the next was N2-4 at 03:30 local the following night, 87° and southbound.

**Left for later.**

- The station location and minimum elevation aren't saved between runs yet (Settings, M6). The notification permission prompt appears the first time a pass is five minutes away.
- The catalog's SatDump pipeline and recording rate are placeholders, checked in M5 against the installed SatDump. (Done in M5.)

## M5 — Automatic recording and Meteor decoding 🟡 (built and tested on a simulated pass with the real SatDump; a real pass waits for the stick)

SatDump 1.2.2 was installed with `brew install --cask satdump`, and its command line, pipelines and output were checked on the machine before any code was written (`docs/DECISIONS.md`).

- [x] `PassScheduler` (`src/sat`): which pass is due, from the catalog (active, LRPT downlink, pipeline, sample rate). It joins late while 2 min remain, gives overlapping passes to the higher one, and never repeats a pass. Tested with a fake clock.
- [x] `DeviceArbiter` (`src/device`): Idle → Live → HandoffPending → External → Reclaiming. A pass outranks the user's live view; the user's tuning, gain, device and recording controls are locked during it. The handoff states are tested and wait for rtl_433 (v2).
- [x] `SatDumpRunner` (`src/integrations`): runs `satdump <pipeline> baseband <file.cu8> <dir> --samplerate N --baseband_format cu8 [catalog options]` in its own process group, with a 30-min timeout. It saves the log without colours, copies the PNG images to `products/<date>_<satellite>/`, writes `pass.json`, and deletes the baseband after a successful decode.
- [x] It detects a missing SatDump and shows `brew install --cask satdump` with a "Check again" button. The rest of the app is unaffected.
- [x] `PassAutomation`: claim the stick, tune (or open it), record AOS → LOS with a disk-space check, give the receiver back as it was, decode. Each pass ends in an outcome banner and a notification.
- [x] UI:
  - the "Record and decode passes automatically" switch and SatDump's state in the Satellites tab;
  - the next-pass card shows "Recorded automatically";
  - banners for tuning, recording (with progress and Stop), saving, decoding (with SatDump's progress) and the result (Show in Finder / Show log).
- [x] The app keeps App Nap off while automatic recording is on, and idle sleep off during a pass.
- [x] `--simulate-pass SECONDS` runs the whole chain on a made-up pass.

**Acceptance.**

| Criterion | Status |
|---|---|
| A simulated pass (fake clock + `FileSource` on a recording) triggers the recording and the decoding | ✅ `test_automation.cpp` runs it with a fake SatDump. In the app (`--open` on a 137.9 MHz recording, `--simulate-pass 60`), the real SatDump 1.2.2 ran on the 117 MB recording and reported no image, since it was noise, as expected. |
| Live, a high pass produces an image with no intervention | ⏳ waits for the stick (and an antenna) |

**Left for later.**

- Automatic recording is off at every start, until Settings are saved (M6). The products aren't in a journal yet (M6); `pass.json` holds what the journal will need. (Both done in M6.)
- No real Meteor recording was available, so SatDump has decoded only noise through the app. The first real pass is the remaining check.
- A Mac that sleeps misses passes. The app doesn't schedule wake-ups, which would need administrator rights.

## M6 — Journal, Discovery, settings ✅ in code (2026-11-12; the end-to-end acceptance with a live stick waits for it)

- [x] `Journal` (SQLite, `src/store`):
  - events: passes and their outcome, images (the products folder), one row per aircraft per UTC day (updated when the callsign arrives), and the user's recordings;
  - filters by kind, text and time;
  - CSV and JSON export;
  - the schema is versioned with `user_version`, migrations run in transactions, and a newer file is refused without being touched.
  - `JournalService` runs it on a worker thread.
- [x] Journal tab: kind filter, search, Export CSV / Export JSON through the save dialog, Show in Finder.
- [x] Discover tab:
  - a "Start here" checklist: location, receiver, pick something; each ticks itself off;
  - the presets by category (broadcast, aviation, amateurs and walkie-talkies, weather satellites), each with two or three sentences on what you'll hear, the antenna that helps, and one action;
  - it opens on the first run.
- [x] Tooltips on the top-bar controls, the tabs, the FFT button, export, Show and the auto-record switch.
- [x] Settings saved to `settings.json`: theme, language, station, minimum elevation, automatic recording at start, keep pass recordings, default sample rate, ppm, data folder (applies after a restart). It is written atomically on its own thread; anything missing or invalid falls back to its default.
- [x] All UI text is in the string table, in English and Romanian.

**Acceptance.**

| Criterion | Status |
|---|---|
| A new user can, without external documentation, set the location, listen to FM, see aircraft and schedule a Meteor pass | 🟡 Every step is reachable from Discover: Set location opens Settings; Connect opens the receiver list; FM radio and ADS-B tune their presets; Plan passes opens Satellites with the automatic-recording switch. Checked on screen without a stick (recordings); the live run waits for the V4. |

**Left for later.**

- The journal has no date-range control in the UI yet (the filter supports it).
- Tooltips cover the main controls, not every field of every popover.

## M7 — Packaging 🟡 (built and checked on this Mac; a second Mac hasn't been tried)

- [x] Self-contained `.app`:
  - every third-party library is static, and libcurl and SQLite come from macOS, so there are no dylibs to embed and no RPATH to fix;
  - it has the icon;
  - the `Info.plist` is complete. macOS has no Info.plist key for the notification permission (docs/RELEASE.md).
- [x] `tools/package.sh`:
  - Release build and tests;
  - strip, then ad-hoc signature with the hardened runtime over the whole bundle;
  - checks: system libraries only, no build-machine paths, arm64, resources present;
  - `.zip` (ditto) and `.dmg` with an Applications link.
- [x] `docs/RELEASE.md`: building, installing on another Mac (Gatekeeper's first-open steps), what the app writes, and the Developer ID and notarization steps, on hold by decision.

**Acceptance.**

| Criterion | Status |
|---|---|
| The app copied to another Apple Silicon Mac, with only SatDump installed separately, starts and works | 🟡 The `.zip` unpacked to another folder on this Mac starts with the hardened runtime. It ran a whole simulated pass and launched SatDump 1.2.2. `codesign --verify --strict` passes on the copy. A second Mac hasn't been tried. |

**Left for later.**

- Developer ID signing and notarization (on hold, 2026-11-12).
- Opening `.cu8` files from Finder (a document type) isn't there yet.
