# Zarescope architecture

This describes what exists today (end of M1). Each milestone adds its part here when it lands ([MILESTONES.md](MILESTONES.md)).

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
| `zarescope_dsp` | `SpscSlots`, `TripleBuffer`, `blackmanHarris`, `FftEngine` (KissFFT), `SpectrumAnalyzer`, `SignalGenerator`, cu8 conversion | KissFFT |
| `zarescope_device` | `SampleSource`, `RtlSdrSource`, `FileSource`, `IqMetadata`, `IqRecorder`, `DeviceIdentity`, `Receiver` | `zarescope_dsp`, librtlsdr (static), libusb (static), nlohmann/json |
| `zarescope_theme` | `DesignTokens` | nlohmann/json |
| `zarescope_ui` | the screens, the ImGui and ImPlot themes, fonts, strings, formats | `zarescope_theme`, `zarescope_device`, Dear ImGui, ImPlot |
| `zarescope_platform` | Metal renderer, macOS paths, window appearance, file dialog | Dear ImGui's Metal backend, Cocoa, UniformTypeIdentifiers |
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

## The UI (`src/ui`)

- **UiKit** is the design system at the point of use. It covers:
  - colours, sizes and type by token name;
  - strings by `StringId` (EN and RO);
  - the shared widgets: buttons, status chips, segmented control, banners and a few line icons.
- **MainWindow** is the fixed layout of DESIGN.md §6.
  - **Top bar:** logo, frequency display, gain, receiver chip, LIVE/REPLAY chip, record button and theme switch.
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

## Start-up and frame loop

`App`'s constructor runs these steps in order:

1. GLFW and the window;
2. the ImGui and ImPlot contexts;
3. the paths, the tokens and the fonts;
4. the themes;
5. the `Receiver`;
6. the `MainWindow`, given the receiver, the recordings folder (`~/Library/Application Support/Zarescope/recordings`) and the file dialog;
7. the backends.

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

`ctest --test-dir build --output-on-failure` runs 50 test cases with no stick, window or GPU. ImGui and ImPlot run headless.

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

`build/tests/zarescope_hardware_tests` needs a stick and is run by hand. It covers:

- **identity:** what the stick claims and whether its tuner agrees;
- **streaming:** that it streams at the requested rate without drops;
- **spectrum:** a spectrum from the live receiver.
