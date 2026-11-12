# Zarescope architecture

This describes what exists today: version 0.10.0, with all of v1 (M0–M7). Each milestone adds its part here when it lands ([MILESTONES.md](MILESTONES.md)).

## Threads and data flow

```
                         control thread (Receiver)
                         opens/closes sources, every librtlsdr control call
                                     │
 ┌──────────── source thread ────────▼──────┐
 │ RtlSdrSource: rtlsdr_read_async callback │
 │ FileSource:   reads a .cu8, real-time    │
 └───────────────┬──────────────────────────┘
                 │ SampleBlock (64 KiB cu8 + frequency, rate, UTC time)
                 ▼ SpscSlots, 128 slots ≈ 1.7 s, lock-free, preallocated
 ┌──────────── DSP thread (Receiver) ───────┐
 │ IqRecorder::offer ──▶ recorder queue ──▶ writer thread ──▶ .cu8 + .json
 │ SpectrumAnalyzer: window, FFT, average   │
 │ AudioDemodulator ──▶ AudioRing ──▶ sound card callback (miniaudio thread)
 │ ModeSDemodulator (2 MS/s, 1090 MHz) ──▶ SpscSlots of messages ──▶ UI: AircraftStore
 └───────┬──────────────────────┬───────────┘
         │ every row            │ smoothed trace (latest only)
         ▼ SpscSlots (64)       ▼ TripleBuffer
 ┌──────────── UI thread ───────────────────┐
 │ MainWindow → SpectrumView (waterfall texture, spectrum plot) │
 │ posts commands to the Receiver; never waits on it            │
 └──────────────────────────────────────────┘
```

These rules hold:

- **The UI thread never blocks.** It only posts commands, which return at once, and reads results through lock-free queues. A short status copy is guarded by a mutex that no thread holds across I/O.
- **The DSP path doesn't allocate.** After setup it never allocates; only a change of FFT size rebuilds the analyser.
- **Errors travel as data.** They are reported as status (`ReceiverStatus::error`, `RecorderStatus::error`), never as exceptions crossing threads.

## Libraries

| Target | Holds | Depends on |
|---|---|---|
| `zarescope_dsp` | `SpscSlots`, `TripleBuffer`, `blackmanHarris`, `FftEngine` (KissFFT), `SpectrumAnalyzer`, the filters, `AudioDemodulator`, `SignalGenerator`, cu8 conversion | KissFFT |
| `zarescope_adsb` | `ModeS`, `Cpr`, `ModeSDemodulator`, `AircraftStore`, `Encoder` | `zarescope_dsp` |
| `zarescope_sat` | `SatCatalog`, `Orbit` (TLE, propagator, passes), `SatelliteService`, `PassScheduler` | sgp4, libcurl (system), nlohmann/json, `zarescope_device` (timestamps) |
| `zarescope_audio` | `AudioOutput`, `AudioRing` | miniaudio |
| `zarescope_device` | `SampleSource`, `RtlSdrSource`, `FileSource`, `IqMetadata`, `IqRecorder`, `DeviceIdentity`, `Presets`, `Receiver` | `zarescope_dsp`, `zarescope_audio`, librtlsdr (static), libusb (static), nlohmann/json |
| `zarescope_os` | `runProcess` (POSIX), where external tools live (macOS) | — |
| `zarescope_integrations` | `SatDumpRunner` | `zarescope_os`, `zarescope_device`, nlohmann/json |
| `zarescope_store` | `Journal`, `JournalService` | SQLite (system), nlohmann/json, `zarescope_device` (timestamps) |
| `zarescope_appcore` | `PassAutomation`, `Settings` (`src/app`) | `zarescope_device`, `zarescope_sat`, `zarescope_integrations` |
| `zarescope_theme` | `DesignTokens` | nlohmann/json |
| `zarescope_ui` | the screens, the ImGui and ImPlot themes, fonts, strings, formats | `zarescope_theme`, `zarescope_device`, Dear ImGui, ImPlot |
| `zarescope_platform` | Metal renderer, macOS paths, window appearance, file dialog, notifications, Finder and keep-awake | Dear ImGui's Metal backend, Cocoa, UniformTypeIdentifiers |
| `Zarescope` | the `.app` bundle | all of the above |
| `zarescope_tests` | doctest tests, run by CTest | `zarescope_ui` |
| `zarescope_hardware_tests` | tests that need a stick, run by hand | `zarescope_device` |
| `zarescope-synth` | writes a synthetic FM-band recording | `zarescope_device` |

Dependencies point one way. Nothing outside `src/platform/macos` includes an Apple header or uses `#ifdef __APPLE__`. Every third-party library is linked statically, so the app needs only macOS itself.

## The receive chain (`src/device`, `src/dsp`)

- **SampleSource.** It produces `SampleBlock`s on its own thread.
  - **`RtlSdrSource`** wraps librtlsdr from the RTL-SDR Blog fork.
    - Its reader thread runs `rtlsdr_read_async`; the callback copies each 64 KiB USB transfer into the block queue.
    - Controls cover tuning, sample rate, gain (automatic, or the nearest tuner step), ppm and bias-tee.
    - The bias-tee is forced off on every open and close.
  - **`FileSource`** plays a recording.
    - **Normal playback:** at the recorded rate, looping, with timestamps continuing from the recording's start.
    - **In tests:** once, as fast as the consumer allows.
- **Receiver** ties it together.
  - **Control thread.** Runs the command queue. Opening a stick, every USB control call and stopping all happen there.
  - **Tuning coalesces.** Only the newest requested frequency is applied, so scrolling over the frequency never queues dozens of USB retunes.
  - **DSP thread.**
    - Hands each block to the recorder.
    - Feeds the analyser.
    - Queues every finished row for the waterfall.
    - Keeps an exponentially smoothed trace for the spectrum line.
  - **Retunes.** A change of frequency or rate in the blocks resets the analyser and the trace.
- **SpectrumAnalyzer.** The pipeline is:
  1. convert cu8 to complex float;
  2. apply a 4-term Blackman-Harris window (periodic);
  3. run the FFT;
  4. take |X|² and average over the frames of one row (`framesPerRow` follows the sample rate and a 30 rows/s target);
  5. convert to dBFS (a full-scale tone reads 0 dBFS) and FFT-shift, so −fs/2 is on the left.
- **IqRecorder.**
  - The DSP thread `offer()`s blocks without waiting.
  - The recorder's own writer thread creates the folder and files, writes, and finally rewrites the sidecar with the sample count.
  - A block with another frequency or rate ends the recording.
  - A seq_cst handshake (`m_offersInFlight` against `m_recording`) guarantees the final drain sees every block.
- **DeviceIdentity.** Compares the model claimed in the EEPROM's USB strings (`RTLSDRBlog` / `Blog V4`, `Blog V4L`, `Blog V3`, matched exactly as librtlsdr does) with the tuner read over I²C. A V4 must report an R828D; anything else is flagged as a mismatch, the usual sign of a counterfeit.

## Listening (`src/dsp/AudioDemodulator`, `src/audio`)

The `AudioDemodulator` chain runs on the DSP thread:

1. **cu8 → complex.**
2. **Shift.** `FrequencyShifter` moves the VFO's offset to 0 Hz.
3. **Channel filter.** `DecimatingFir` stages (factors ≤ 8) bring the signal down to the channel rate.
4. **Squelch.** It acts on the channel's mean power, with 3 dB of hysteresis.
5. **Demodulate.** FM uses the phase difference between consecutive samples; AM uses the envelope, then a DC blocker and a slow AGC.
6. **De-emphasis.** 50 µs or 75 µs, wideband FM only.
7. **Audio low-pass.** 15 kHz for WFM, 4 kHz for voice.
8. **Resample.** `Resampler` (Catmull-Rom) to the sound card's rate.
9. **Volume.** Applied through a squelch gate ramped over 5 ms.

`configure()` allocates and `process()` never does.

Settings travel from the UI to the DSP through a `TripleBuffer`. Offset, volume and squelch apply between blocks; a mode, bandwidth or rate change reconfigures the chain.

`AudioOutput` (miniaudio) pulls from an `AudioRing` on its own real-time thread and keeps latency under 150 ms (see DECISIONS).

## ADS-B (`src/decoders/adsb`)

- **ModeS.** Implements:
  - CRC-24 (0xFFF409), with single-bit correction by syndrome for DF17/18;
  - extended-squitter fields: identification, airborne position (25-ft/Gillham altitude, GNSS height, CPR fields), velocity.

  Layouts follow "The 1090 MHz Riddle" and DO-260B.
- **Cpr.** NL zones, global and local decoding, distance and bearing.
- **ModeSDemodulator.**
  1. Magnitude at 2 MS/s.
  2. The four-pulse preamble, with quiet between and after the pulses.
  3. Pulse-position bits.
  4. CRC check.

  The tail of each block is kept for messages that straddle blocks.
- **AircraftStore.** Owned by the UI thread and fed from the receiver's ADS-B queue. Aircraft age by arrival time.
  - Positions decode globally from an even/odd pair within 10 s, then locally against the last fix (or the station).
  - Jumps of more than 600 km are rejected.
- **Encoder.** Builds DF17 messages and modulates them into IQ, for tests and demo recordings only. Nothing is ever transmitted.

## Satellites (`src/sat`)

- **SatCatalog.** The satellites, their downlinks, their SatDump pipeline and options, and the recording rate, from `resources/sat_catalog.json`.
- **PassScheduler.** Pure logic with the time passed in: which pass is due, which is next, and which are done.
- **Orbit.**
  - `parseTle` checks the lines' length, their start digits and checksums, and that SGP4 accepts them.
  - `Propagator` wraps libsgp4: TEME state, and look angles from the station.
  - `predictPasses` scans in 30-s steps. It refines AOS and LOS by bisection to 1 s, finds the culmination by golden-section search, and never scans more than a day past its window.
  - `passTrack` gives the polar plot's points.
- **SatelliteService.** It has its own worker thread, and the UI reads a mutex-guarded snapshot.
  - **Start-up.** It loads the cache.
  - **Downloads.** It fetches stale TLEs (older than 12 h) through libcurl, HTTPS only, with answers capped at 16 KiB.
  - **Predictions.** It predicts 48 h of passes when its inputs change, and every 10 minutes.
- **Notifications.** `MainWindow` announces each pass five minutes before AOS through `platform::notify`.

## Automatic pass recording (M5)

```
UI thread, every frame                       runner thread         SatDump process
PassAutomation::update(now, passes)
  PassScheduler.due ─▶ DeviceArbiter.claimForPass
  Receiver: tune / open ─▶ startRecording (disk check)
  … LOS ─▶ stopRecording ─▶ restore ─▶ releasePass
  SatDumpRunner.submit ─────────────────────▶ runProcess ──────────▶ satdump … baseband …
                                              copy PNGs, pass.json ◀─ exit
  collect result (mutex slot) ◀──────────────  callback (exactly once)
```

- **`device::DeviceArbiter`.** Who owns the stick. The pass outranks the user; `MainWindow` locks tuning, gain, device and recording while the scheduler owns it, and mutes the demodulator.
- **`integrations::SatDumpRunner`.** One job at a time on its own thread.
  - `platform::runProcess` (POSIX `posix_spawn`, its own process group, a timeout, a cancel flag) runs SatDump.
  - The search for SatDump (`platform::satDumpCandidates`, links resolved) also runs on that thread.
- **`app::PassAutomation`.**
  - **Phases.** Waiting → Preparing → Recording → Finishing → Decoding.
  - **Inputs.** The receiver's status (read each frame), the arbiter, the scheduler, the runner.
  - **Output.** `AutomationStatus` for the UI. The last outcome carries a typed `PassFailure`, worded in both languages by the UI.
  - **Library.** It lives in `zarescope_appcore` so the tests can drive it.
- **`platform::KeepAwake`.** NSProcessInfo activities: no App Nap while waiting, no idle sleep during a pass.

## Settings and the journal (M6)

- **`app::Settings`.**
  - `parseSettings` and `settingsToJson` are pure.
  - `SettingsStore` writes the latest copy on its own thread, atomically.
  - `App` reads the file once at start-up and hands it to `MainWindow`.
  - `MainWindow` compares its copy with the last saved one at the end of every frame and saves when they differ.
- **`store::Journal`.** SQLite: migrations, add (keyed rows update in place), query, CSV and JSON export. `JournalService` puts it on a worker thread, and the UI reads a snapshot of the latest query.
- **Where events come from.** `MainWindow::updateJournal`, every frame:
  - each new pass outcome (and its images);
  - each recording the user finished;
  - each aircraft once per UTC day, again when its callsign arrives.
- **Views.** `JournalView` and `DiscoveryView` draw and return what the user asked for; `MainWindow` acts on it.

## The UI (`src/ui`)

- **UiKit** is the design system at the point of use. It covers:
  - colours, sizes and type by token name;
  - strings by `StringId` (EN and RO);
  - the shared widgets: buttons, status chips, segmented control, banners and a few line icons.
- **MainWindow** is the fixed layout of DESIGN.md §6.
  - **Top bar:** logo, frequency display (the VFO) with presets, gain, mode, sound, receiver chip, LIVE/REPLAY chip, record button and settings (theme, language).
  - **Tabs.** Spectrum, Aircraft (`AircraftView`: table and radar) and Satellites (`SatelliteView`: next-pass card, pass list, polar plot). They show even without a receiver.
  - **VFO.** The window keeps the VFO, an absolute frequency, and publishes its offset from the centre to the receiver every frame. Leaving the span retunes the stick off centre.
  - **Banner:** recording, or a notice or error.
  - **Body:** the spectrum area, or the empty state.
  - **Status bar.**
  - **Popovers:** receiver, gain and FFT settings.
- **FrequencyDisplay** tunes digit by digit:
  - scroll over a digit;
  - click a digit, then use ↑ ↓ ← →;
  - type digits.

  The arrow keys are claimed with `SetKeyOwner` while editing, so ImGui's keyboard navigation doesn't take them.
- **SpectrumView** draws the spectrum and the waterfall.
  - **Spectrum.** ImPlot draws the trace and the grid. The axis labels are drawn by hand so the spectrum and the waterfall share exact pixel columns.
  - **Waterfall texture.** A 2048 × 1024 RGBA ring, registered with `ImGui::RegisterUserTexture`. Each new row is written above the previous one and uploaded alone with `ImTextureDataQueueUpload`, and the renderer backend updates the GPU texture. No graphics-API code is involved.
  - **Row resampling.** FFT bins are reduced to the texture width by taking the strongest bin per column, so narrow carriers survive.
  - **Level range.** It fits the first trace of each source (`suggestRange`). Changing it repaints the texture from the stored levels.
- **Themes.** `applyTheme` (ImGui) and `applyPlotTheme` (ImPlot) map every colour slot to a token, and the native title bar follows through `platform::setWindowAppearance`.
- **Text size** (100, 110 or 120 %). `applyInterfaceScale` tells Dear ImGui the window is that much smaller and its pixels that much denser, so every size, gap and plot grows together and the fonts are rasterized sharp at the new density. Mouse positions already queued for the frame are converted too, which is why input trickling is off.

## macOS integration (`src/platform/macos`)

- **Menu bar.** `ui::buildMenus` (`AppMenuModel`) describes the menus as data: titles, shortcuts, command ids, in English and Romanian. `platform::AppMenu` builds a native `NSMenu` from it; GLFW's default menu is turned off. A chosen item is queued and read by the window on its next frame; `validateMenuItem` reads the enabled and checked state the window publishes every frame. Edit commands (copy, paste, select all) reach ImGui's text fields as ⌘-key presses. `--print-menus` prints the structure without opening a window.
- **Help and About.** The user guide is Markdown (`resources/help/guide.en.md`, `guide.ro.md`), read once at start-up and parsed by `ui::Guide` (a small subset that still reads well on GitHub: headings, paragraphs, bullets, numbered steps and two-column tables; `**bold**` and `` `code` `` marks are dropped). `HelpView` draws it with an "On this page" rail. About is a modal with the version, the system and the copyright years. A test keeps the two guides' sections in step.
- **Location.** `platform::LocationFinder` asks Core Location once for "Use my current location" and calls back exactly once: a position, or why there isn't one (permission denied, Location Services off, or no fix). The hardened runtime needs the `personal-information.location` entitlement for it, and `Info.plist` carries the usage text.
- **Notifications, file dialogs, opening links, keeping awake during a pass**: small functions behind `platform/` headers, each with its macOS implementation here.

## Start-up and frame loop

`App`'s constructor runs these steps in order:

1. GLFW (with its own menu bar off) and the window;
2. the ImGui and ImPlot contexts;
3. the paths (`ZARESCOPE_HOME` replaces Application Support, for screenshots and tests), the tokens and the fonts;
4. the settings, read once, and the store that writes them;
5. the data folder and the journal;
6. the `Receiver`;
7. the satellite catalog, `SatelliteService` and `PassAutomation`;
8. the location finder, the menu bar and the two user guides;
9. the `MainWindow`, given all of these as services;
10. the backends.

Any failure throws, and `main()` prints the reason.

Teardown order matters and lives in `App::State`'s destructor:

1. receiver threads;
2. renderer (releases GPU textures);
3. window (unregisters the waterfall texture, which still needs the ImGui context);
4. ImPlot;
5. GLFW backend;
6. ImGui;
7. the window, then GLFW.

How often the loop draws:

- **Idle:** the loop sleeps until input, waking four times a second for the clock.
- **After input:** it draws at the display's rate for half a second.
- **While live:** it draws continuously whenever data is live or a recording runs.

## Tests

`ctest --test-dir build --output-on-failure` runs 82 test cases with no stick, window, GPU or sound card. ImGui and ImPlot run headless.

| File | Checks |
|---|---|
| `test_smoke.cpp` | the runner; the version format |
| `test_design_tokens.cpp` | token parsing and validation; LUT against the CSV; WCAG AA for the pairs drawn |
| `test_imgui_theme.cpp` | every ImGui and ImPlot colour maps in both themes; values land where the design says |
| `test_typography.cpp` | fonts load; every glyph the design needs (incl. Ș ș Ț ț, thin spaces) is in every font |
| `test_strings_clock.cpp` | both languages filled in; comma-below letters; matching `{}` placeholders; clock format |
| `test_dsp.cpp` | SPSC queue across threads; window; FFT; cu8 conversion; **a tone peaks in the right bin at the right level** (M1 acceptance); split input gives identical rows |
| `test_iq_files.cpp` | sidecar format; byte-exact playback; real-time pacing; **a recording replays identically** (M1 acceptance); a retune ends a recording |
| `test_receiver.cpp` | the whole chain on recordings: rows, trace, FFT size changes, recording through the receiver; the stick identity rules |
| `test_ui_format.cpp` | number formats; axis ticks; waterfall resampling, colouring and range fitting |
| `test_audio.cpp` | FIR response; decimation; shifter; resampler; de-emphasis; **an FM tone demodulates back** (M2 acceptance) with neighbour rejection; NFM; AM; squelch |
| `test_presets.cpp` | the shipped presets file; malformed files refused |
| `test_sat.cpp` | the catalog; TLE parsing; **SGP4 against Vallado's verification values**; pass physics; **golden passes** (M4 acceptance); a never-setting orbit; the TLE cache |
| `test_adsb.cpp` | **the Riddle's worked examples** (M3 acceptance); CRC and correction; Gillham altitude; encode → decode; the demodulator on noisy IQ across block edges; the store |

`build/tests/zarescope_hardware_tests` needs a stick and is run by hand. It covers:

- **identity:** what the stick claims and whether its tuner agrees;
- **streaming:** that it streams at the requested rate without drops;
- **spectrum:** a spectrum from the live receiver.
