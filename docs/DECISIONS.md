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
