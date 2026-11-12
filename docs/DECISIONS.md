# Decisions

Architecture choices and notable trade-offs, newest last. Each entry gives the date, the decision, the reason and the alternatives that were turned down.

## 2026-11-11 — The development receiver is the RTL-SDR Blog V3; the code assumes no model

- **Decision.** Support any RTL2832U dongle that librtlsdr supports. The tuner is detected at run time, and nothing depends on a specific model. The RTL-SDR Blog V3 (R820T2, 1 ppm TCXO) is the reference for development.
- **Why.** The RTL-SDR Blog V4 is end-of-life: its R828D tuner is no longer made, and the last stock has been used. Its successor, the V4 Lite (R828S), is not reliably available yet and needs a newer driver. The V3 is what RTL-SDR Blog itself recommends in the meantime. Its weaknesses (HF reception, filtering, heat) don't affect FM, airband, ADS-B or Meteor at 137 MHz.
- **Rejected.** Waiting for the V4 Lite. Requiring a V4.

## 2026-11-11 — No CI; every check runs locally

- **Decision.** There is no GitHub Actions workflow. Build, tests and formatting are verified on the development Mac before each commit. `-Werror` is on for every local build of our own code (`ZARESCOPE_WERROR`, default `ON`).
- **Why.** This follows Obdaryn. A single developer on a single platform gets nothing from CI that a local `ctest` doesn't already give. Turning `-Werror` on locally keeps the code as strict as CI would.
- **Rejected.** A macOS arm64 runner on GitHub Actions (the original M0 plan).

## 2026-11-11 — Dependencies are pinned archives with SHA-256 hashes

- **Decision.** `FetchContent` downloads release archives by URL, each with `URL_HASH SHA256=…`. This covers GLFW 3.5.1, Dear ImGui 1.93.0-docking, nlohmann/json 3.12.0 and doctest 2.5.3.
- **Why.** Builds are reproducible, and a tampered or changed archive fails the build. Tags can move; a hash can't.
- **Rejected.**
  - Git tags via `GIT_TAG`: slower, and a tag can be moved.
  - Homebrew packages: versions drift, and Linux and Windows wouldn't follow.
  - Vendoring the sources into the repo: noise in diffs.

## 2026-11-11 — Dear ImGui on Metal through a Renderer interface

- **Decision.**
  - The platform layer exposes `platform::Renderer`. `renderFrame()` takes a callback that runs the whole ImGui frame.
  - The macOS implementation uses a `CAMetalLayer` on the GLFW window's content view, following ImGui's `example_glfw_metal`.
  - Objective-C++ is compiled with ARC, only under `src/platform/macos`.
- **Why.**
  - OpenGL is deprecated on macOS.
  - The callback lets the renderer wrap the frame in an `@autoreleasepool` and skip it when the window is minimised, without the app knowing about Metal.
  - A port adds a Vulkan or OpenGL `Renderer` and changes nothing else.
- **Rejected.**
  - OpenGL 3.2 core: deprecated, and would need a later rewrite.
  - Making the app call `ImGui_ImplMetal_*` directly: this puts Apple code outside `src/platform`.

## 2026-11-11 — Design tokens are loaded at run time and validated

- **Decision.**
  - The app reads `zarescope.tokens.json` from the bundle at start-up, through `DesignTokens`, which has no ImGui dependency.
  - Start-up stops if the file breaks a rule: a missing section, a bad hex value, a token in only one theme, an ImGui mapping to an unknown token, or a LUT that isn't 256 entries.
  - ImGuiCol entries newer than the token file (CheckboxSelectedBg, InputTextCursor, TabDimmedSelectedOverline, DockingPreview, DockingEmptyBg, TextLink, TreeLines, DragDropTargetBg, UnsavedMarker) borrow the token of their closest neighbour, listed in `ImGuiTheme.cpp`.
- **Why.**
  - The token file stays the only place colours are defined, as the design system requires.
  - Validating at start-up turns a typo into a clear message instead of a wrong colour on screen.
  - The fallbacks keep the design package unchanged. A test fails if a future ImGui adds a slot with no colour.
- **Rejected.**
  - Generating C++ from the JSON at build time: one more tool, and the data is tiny.
  - Hard-coding a C++ palette: two sources of truth.
  - Editing the design package's token file to add the new slots: it belongs to the design package and is versioned with it.

## 2026-11-11 — Fonts: dynamic glyphs, JetBrains Mono NL, thin spaces from Inter

- **Decision.**
  - Load each TTF once, with no fixed size and no glyph ranges; Dear ImGui ≥ 1.92 rasterizes glyphs on demand at the drawn size and density.
  - Use the NL (no ligatures) cut of JetBrains Mono.
  - Merge U+2009 (thin space) and U+202F (narrow no-break space) from Inter of the same weight into each mono font.
- **Why.**
  - Glyphs:
    - DESIGN.md's "load at size × scale and set FontGlobalScale = 1/scale" applies to ImGui before 1.92; 1.92 handles Retina density itself.
    - Romanian Ș ș Ț ț are picked up the first time they appear.
  - Ligatures add nothing for numbers and could join characters like `->`.
  - JetBrains Mono has no thin spaces, and the frequency format `137.900 000 MHz` needs one. The glyph test caught this.
- **Rejected.**
  - Fixed-size atlases per role: blurry on Retina and more memory.
  - Drawing the frequency gap as a manual offset: every number format would need its own workaround.

## 2026-11-11 — The logo is drawn with vector primitives

- **Decision.** `LogoMark` draws the Polar Sky symbol with `ImDrawList` circles, Bézier curves and dots. The geometry is copied from `design/brand/logo/symbol-*.svg`, and the drawing switches from the full to the small version below 40 px, as the brand rules say. The knockout gap is drawn as a disc in the surface colour.
- **Why.** The logo stays sharp at any size and density without an SVG rasterizer. Two shapes don't justify a new dependency.
- **Rejected.**
  - nanosvg: worth adding when the 12 line icons arrive (M1); revisit then.
  - PNG textures: blurry when scaled.

## 2026-11-11 — Local time without the C++20 time-zone database

- **Decision.** UTC is formatted with `std::format` on `sys_time`. Local time uses `std::localtime`, called only from the UI thread.
- **Why.** Apple's libc++ does not ship `std::chrono::zoned_time` / `current_zone()`. `std::localtime` exists on every platform.
- **Rejected.**
  - Platform code for one clock.
  - Howard Hinnant's `date` library: a dependency for one line.

## 2026-11-11 — Apple Silicon only, macOS 14 and later

- **Decision.** `CMAKE_OSX_ARCHITECTURES=arm64`, `CMAKE_OSX_DEPLOYMENT_TARGET=14.0`.
- **Why.** The v1 target is Apple Silicon. macOS 14 covers every Apple Silicon Mac that is still updated and allows `std::format`.
- **Rejected.** A universal binary: no Intel Mac to test on.

## 2026-11-11 — The development stick is an RTL-SDR Blog V4

- **Decision.** The stick bought for development is an RTL-SDR Blog V4 (R828D). It hasn't arrived yet; its authenticity is checked on arrival. The code still assumes no model, and the V3 stays the recommendation for users, because the V4 is out of production.
- **Why.** A V4 turned up for sale after the end-of-life notice. The V4 needs the RTL-SDR Blog fork of librtlsdr, which the brief already requires.
- **Rejected.** Changing the reference stick in the code: the earlier "assume no model" decision covers both.

## 2026-11-11 — libusb is built from source, not taken from Homebrew

- **Decision.** Build libusb 1.0.30 statically through the libusb project's own CMake build (libusb-cmake v1.0.30-0), pinned by hash.
- **Why.** Homebrew's static `libusb-1.0.a` targets the newest macOS only (the linker warned "built for macOS 26, linking for 14"). A bundle carrying it could fail on older Macs, which would break M7's acceptance. Building it ourselves also removes a Homebrew dependency.
- **Rejected.**
  - Homebrew's dylib: the same minimum-version problem, plus bundling.
  - Raising the deployment target: excludes users for no reason.

## 2026-11-11 — The receive chain: blocks, queues and threads

- **Decision.**
  - **Blocks and queues:**
    - Blocks are 64 KiB (32 768 complex samples, 13.7 ms at 2.4 MS/s), a multiple of librtlsdr's 512-byte USB granularity.
    - Source → DSP uses a 128-slot SPSC queue (about 1.7 s of slack).
    - DSP → UI uses a 64-slot queue of rows, plus a triple buffer for the trace.
  - **Threads:**
    - A control thread runs every librtlsdr control call.
    - The recorder has its own writer thread.
  - **Tuning coalesces:** only the newest requested frequency is sent.
- **Why.**
  - **Hand-offs:** SPSC queues and a triple buffer need neither locks nor allocation, as the sample path requires (ARCHITECTURE.md).
  - **Slack:** 1.7 s absorbs a busy DSP thread or a slow frame without dropping samples.
  - **USB calls:** control transfers take milliseconds to tens of milliseconds, so they must not run on the UI thread.
  - **Disk:** a stalled disk must not stall the DSP chain.
  - **Retunes:** scrolling over the frequency would otherwise queue dozens of USB retunes.
- **Rejected.**
  - A mutex-protected ring: contention on the hot path.
  - Writing recordings on the DSP thread: one slow write would drop samples.

## 2026-11-11 — A retune ends a recording

- **Decision.** If a block arrives with a different frequency or sample rate than the recording's, the recording ends there, and the UI says so.
- **Why.** A `.cu8` file has one sidecar, so one frequency and one rate. Splitting into several files silently would surprise; mixing frequencies in one file would corrupt it for SatDump (M5).
- **Rejected.**
  - Refusing to tune while recording: it blocks the user for a reason they can't see.
  - Splitting into numbered files.

## 2026-11-11 — The waterfall uses Dear ImGui's user textures

- **Decision.**
  - **Texture:** the waterfall is a 2048 × 1024 RGBA ring texture, an `ImTextureData` registered with `ImGui::RegisterUserTexture`.
  - **Uploads:** each new row is uploaded alone with `ImTextureDataQueueUpload`, and the renderer backend (Metal today) updates the GPU copy.
  - **Resampling:** FFT bins are max-resampled to 2048 columns.
  - **Storage:** levels are stored at that width so a range change can repaint.
- **Why.**
  - The waterfall needs no graphics-API code, so a port only needs a `Renderer`.
  - One-row uploads keep the GPU traffic at 8 KB per row.
  - Max-resampling keeps narrow carriers visible at any FFT size.
- **Risk.** `RegisterUserTexture` is marked experimental in ImGui 1.93. If it changes, the fallback is a small `TextureUploader` in `src/platform`.
- **Rejected.**
  - A Metal texture managed by hand: platform code in the UI.
  - Re-uploading the whole texture each frame: 8 MB per frame.

## 2026-11-11 — The spectrum uses ImPlot for the plot, with hand-drawn axes

- **Decision.**
  - ImPlot draws the trace (shaded fill and line), the grid and the plot area. Its tick labels are turned off.
  - The dBFS labels, the frequency axis under the waterfall and the time labels are drawn by `SpectrumView`.
  - ImPlot's colours come from tokens (`PlotTheme.cpp`).
- **Why.** The spectrum and waterfall must share exact pixel columns, and the design puts the frequency axis below the waterfall. ImPlot's own axis gutter would shift the plot against the waterfall.
- **Rejected.** Drawing the spectrum entirely by hand: ImPlot is the stack's choice and handles lines and fills well.

## 2026-11-11 — The level range fits the signal at first

- **Decision.** The first trace of each new source sets the waterfall's dBFS range:
  - the floor 10 dB under the noise (its 20th percentile);
  - the ceiling 5 dB over the strongest signal;
  - both on 5-dB steps, at least 30 dB apart.

  "Fit to signal" redoes it on demand. Retuning keeps the user's range.
- **Why.** The design's default of −120…−20 dBFS suits no real stick. With an 8-bit RTL-SDR the noise per bin sits around −80 to −90 dBFS, so the default showed bright blue noise and a heavy fill.
- **Rejected.**
  - A fixed range tuned to one stick.
  - Continuous automatic ranging: the picture would breathe.

## 2026-11-11 — Hardware tests are a separate executable

- **Decision.** Tests that need a stick are named `[hardware] …` and built as `zarescope_hardware_tests`, which CTest doesn't run.
- **Why.** `ctest` must pass on any machine without hardware, and a separate binary can't be run by accident.
- **Rejected.** Excluding test cases by doctest filter in CTest: easy to get wrong silently.

## 2026-11-11 — Listening: the VFO moves, the stick retunes only when it must

- **Decision.**
  - The frequency display, clicks and the scroll wheel move a VFO inside the captured span.
  - The stick retunes only when the VFO leaves the span (minus a margin), and then lands an eighth of the span off centre.
  - In a recording the VFO is clamped to the captured span.
- **Why.**
  - Retuning is a USB transaction and resets the waterfall; moving within the span is free.
  - The RTL-SDR has a DC spike at the centre, so a station there would sit on it.
  - Moving the VFO makes recordings listenable station by station.
- **Rejected.** Always retuning so the VFO is at the centre (the SDR# "centre tuning" mode): the DC spike, and a waterfall that resets on every step.

## 2026-11-11 — The audio chain decimates in stages and resamples last

- **Decision.**
  - **Channel rate.** Decimate by integer factors of at most 8 down to a channel rate (WFM ≥ 200 kHz, NFM and AM ≥ 24 kHz), with one windowed-sinc low-pass per stage.
  - **Audio.** After demodulation, a low-pass at 15 kHz (WFM) or 4 kHz (voice), then Catmull-Rom resampling to the sound card's own rate.
- **Why.**
  - Short filters per stage cost about 2.5 % of a core for WFM at 2.4 MS/s, against roughly 6× more for one long filter.
  - Resampling last adapts to any sound card rate (44.1, 48, 96 kHz) and any capture rate (2.048 MS/s isn't a multiple of 48 kHz).
- **Rejected.**
  - A polyphase rational resampler: more code for no audible gain at these bandwidths.
  - Asking the sound card for 48 kHz: macOS would resample anyway.

## 2026-11-11 — Sound through miniaudio, with latency bounded in the ring

- **Decision.**
  - Sound goes through miniaudio's default playback device (mono, float, native rate), fed by a lock-free ring.
  - When more than 150 ms are waiting, the audio callback skips the oldest samples. When the ring runs dry, it plays silence.
  - The device opens the first time a mode is chosen, on the control thread.
- **Why.**
  - The stick's clock and the sound card's never agree, so the fill drifts; skipping keeps the delay between waterfall and sound short.
  - Opening lazily means the app never holds the sound device unless asked to.
- **Rejected.** Adaptive resampling to track the clock drift: better, but more complex. It is worth revisiting if the skips become audible.

## 2026-11-11 — ADS-B decoding: our own decoder, at 2 MS/s

- **Decision.**
  - The Mode S demodulator and decoder are Zarescope's own (M3), using the classic 2 MS/s method: two samples per bit, comparing the halves of each microsecond.
  - Decoding runs on the DSP thread whenever the source is at 2 MS/s with 1090 MHz within 250 kHz of the centre.
  - The ADS-B preset sets that up.
- **Why.**
  - At 2 MS/s each 0.5 µs pulse is one sample, so the method needs no interpolation.
  - It is well documented and cheap.
  - Tying decoding to the source's setup means recordings decode too.
- **Rejected.**
  - Embedding readsb: its licence is compatible, but the brief asks for our own decoder.
  - 2.4 MS/s with phase tracking: better weak-signal performance, worth adding later.

## 2026-11-11 — Aircraft age by arrival time

- **Decision.** The UI hands messages to the `AircraftStore` stamped with the time they arrived, not the samples' timestamp.
- **Why.** A replayed recording carries its original times, so expiry would remove every aircraft on arrival. Live, the two times are the same.
- **Rejected.** A replay clock in the store: more state for no visible gain.

## 2026-11-11 — Orbits with dnwrnr/sgp4, TLEs from CelesTrak

- **Decision.**
  - **Propagation.** dnwrnr/sgp4 3.0 (Apache-2.0), pinned.
  - **Downloads.** TLEs are fetched one satellite at a time from `gp.php?CATNR=<id>&FORMAT=TLE`, with the system's libcurl:
    - at most every 12 h per satellite;
    - HTTPS only;
    - answers capped at 16 KiB;
    - an identifiable User-Agent;
    - cached as JSON with the fetch time.
  - **Validation.** A TLE is accepted only when its checksums pass and SGP4 can initialise on it.
- **Why.**
  - The brief names that library, and it implements Vallado's 2006 SGP4. Our test reproduces Vallado's verification values to the metre.
  - CelesTrak's policy asks for low request rates and an identifiable client.
  - TLEs from the network are untrusted, so they are validated before the predictor uses them.
- **Rejected.**
  - Fetching the whole "weather" group: more data than two satellites need.
  - Space-Track: it requires an account.

## 2026-11-11 — Pass prediction: scan, bisect, golden-section

- **Decision.**
  - Scan elevation in 30-s steps and bisect each horizon crossing to 1 s.
  - Find the culmination by golden-section search, and drop passes under the minimum elevation.
  - Stop a day past the window at most.
- **Why.**
  - A LEO pass lasts minutes, so 30-s steps can't skip one, and bisection makes AOS and LOS exact.
  - Elevation has one maximum per pass.
  - The hard stop protects against orbits that never set.
- **Rejected.** Analytic pass prediction: fragile with real TLEs, and this costs a few milliseconds per satellite per 48 h on the worker.

## 2026-11-11 — The tabs show without a receiver

- **Decision.** Spectrum, Aircraft and Satellites are always reachable. Without a source, Spectrum and Aircraft show the empty state; Satellites works fully.
- **Why.** Pass prediction needs only a station and TLEs. Planning a Meteor pass shouldn't require plugging in the stick first.

## 2026-11-12 — SatDump 1.2.2's command line, checked on the machine

- **Decision.** Decoding runs `satdump meteor_m2-x_lrpt baseband <file.cu8> <output dir> --samplerate <S/s> --baseband_format cu8 --dc_block`. The pipeline and `--dc_block` come from the catalog.
- **Checked.**
  - `satdump --help` prints the usage `satdump [pipeline_id] [input_level] [input_file] [output_file_or_directory]` and the options `--samplerate`, `--baseband_format [cf32/cs16/cs8/cu8]`, `--dc_block` and `--iq_swap`.
  - `Resources/pipelines/Meteor-M.json` in the app bundle defines `meteor_m2-x_lrpt` ("METEOR M2-x LRPT 72k", 137.9 MHz primary, 137.1 MHz backup, default 1 MS/s) and `meteor_m2-x_lrpt_80k`.
  - A run on noise: exit code 0 even with nothing decoded. The output folder holds `meteor_m2-x_lrpt.cadu`, `MSU-MR/product.cbor`, `dataset.json` and `telemetry.json`; images appear as PNGs under `MSU-MR/`.
  - The Homebrew link `/opt/homebrew/bin/satdump` fails ("Couldn't load config file"). SatDump finds its resources through the path it was started by, so the real binary inside `SatDump.app` must be run.
- **Consequences.**
  - Success means "at least one PNG came out", not the exit code.
  - Links are resolved (`std::filesystem::canonical`) before running SatDump.
  - Recordings are given to SatDump as they are (`cu8`); there is no conversion.
- **Rejected.** SatDump's live mode, which would take the stick from us. In v1 SatDump runs on our file after LOS.

## 2026-11-12 — Pass recordings: 1.024 MS/s, on the downlink frequency, AGC when we open the stick

- **Decision.** Recordings run at 1.024 MS/s (`recordSampleRateSps`), with the stick tuned exactly to the catalog frequency.
- **Why.**
  - **Sample rate.** SatDump's pipeline defaults to 1 MS/s. The RTL2832U makes 1.024 MS/s exactly (28.8 MHz / 28.125), while 1 MS/s is approximated. Either rate is far above the 72 k symbols/s OQPSK signal's ~120 kHz.
  - **Tuning.** SatDump expects the signal centred. `--dc_block` removes the RTL-SDR's DC spike under it.
  - **Gain.** The user's gain is kept if a stick was already open. Otherwise the tuner's AGC is used (no gain setting exists before M6).
- **Size.** 2 MB/s, about 1.8 GB for a 15-min pass. The free space is checked before recording, with a 512 MiB margin.
- **Rejected.** Tuning off-centre to dodge the DC spike: SatDump would then need a frequency shift we can't pass on its command line.

## 2026-11-12 — After a decode: images kept, baseband deleted

- **Decision.** The PNGs, `satdump.log` and `pass.json` go to `products/<YYYY-MM-DD_HHMMZ>_<satellite>/`. SatDump's scratch folder is deleted. The baseband is deleted when images came out and kept when none did.
- **Why.** Unattended, a day of passes would fill about 10 GB with recordings nobody needs once decoded. A failed pass is worth keeping for another try.
- **Rejected.** Keeping everything. That will be a Settings option (`AutomationConfig::keepBaseband`, M6).

## 2026-11-12 — The pass automation runs on the UI thread's frame and only posts commands

- **Decision.** `PassAutomation::update(now, passes)` is called every frame. It reads statuses and posts commands; SatDump runs on the runner's own thread.
- **Why.**
  - The receiver's commands already return at once, so nothing new has to be synchronised.
  - The time comes in as a parameter, so tests drive a whole pass with a fake clock.
  - The idle loop still wakes four times a second, and App Nap is held off while automatic recording is on, so passes start within a fraction of a second.
- **Rejected.** A scheduler thread of its own, which would need locking around the receiver and the arbiter for no gain.

## 2026-11-12 — One events table for the journal, versioned with user_version

- **Decision.** The journal is one SQLite table of events with a `kind`. Rows with a `key` are updated in place: one aircraft per UTC day. The schema version lives in `PRAGMA user_version`. Migrations are an append-only list, each applied in a transaction, and a file from a newer Zarescope is refused untouched.
- **Why.** Every event the brief lists has a time, a title, a detail line and maybe a file; anything else fits a JSON `data` column. One table keeps filtering and export simple.
- **Rejected.**
  - A table per kind: more joins and migrations for the same queries.
  - The `aircraft_days` aggregate as its own table: the keyed row does the same job.

## 2026-11-12 — Settings are forgiving and written off the UI thread

- **Decision.**
  - `settings.json` is read once at start-up. A missing, invalid or out-of-range value keeps its default, with a warning on stderr; the app never refuses to start over it.
  - Changes are saved from the frame loop when the settings differ from the last saved copy. A worker thread writes the latest copy to a temporary file and renames it over the old one.
  - Command-line choices (`--theme`, `--station`) are remembered like any other change.
- **Why.** A crash or power cut mid-write leaves either the old file or the new one, never half of each. The UI never waits on the disk.

## 2026-11-12 — The data folder changes after a restart

- **Decision.** Recordings, products, the TLE cache and the journal live in the data folder: the default (Application Support) or the one chosen in Settings. A change takes effect on the next start. `settings.json` itself always stays in Application Support.
- **Why.** Moving an open journal, a recording in progress and the TLE cache under running threads is risky for little benefit.
- **Rejected.** Moving the existing files automatically: the user decides what to carry over.

## 2026-11-12 — Discovery is a tab, and the first run opens on it

- **Decision.** Discover is the first tab. A new user lands on it (`discoverySeen` in the settings); later runs open on Spectrum.
- **Why.**
  - It stays reachable after the first run, which a one-off dialog wouldn't.
  - It ticks its steps off from the app's real state (location set, receiver running), so it doubles as a checklist.
  - Its texts live in the string table, not in `presets.json`, so they are translated with everything else.
