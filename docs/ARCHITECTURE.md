# Zarescope architecture

This describes what exists today (M0). Each milestone adds its part here when it lands ([MILESTONES.md](MILESTONES.md)).

```
┌─────────────────────────── src/app ───────────────────────────┐
│  main.cpp   command line (--theme)                            │
│  App        start-up, frame loop, shutdown                    │
└───────┬───────────────────────────┬───────────────────────────┘
        │                           │
┌───────▼──────── src/ui ───────┐ ┌─▼────────── src/platform ─────────┐
│ MainWindow  top bar, body,    │ │ Renderer  (interface)             │
│             status bar        │ │ AppPaths  (interface)             │
│ Typography  fonts, type scale │ │ macos/MetalRenderer.mm            │
│ Strings     EN + RO table     │ │ macos/MacAppPaths.mm              │
│ LogoMark    vector logo       │ └───────────────────────────────────┘
│ Clock       UTC / local text  │
│ theme/ImGuiTheme  tokens →    │
│                   ImGuiStyle  │
└───────┬───────────────────────┘
┌───────▼──── src/ui/theme (no ImGui) ───┐
│ DesignTokens  loads and validates      │
│ design/tokens/zarescope.tokens.json    │
└────────────────────────────────────────┘
```

## Libraries

| Target | Holds | Depends on |
|---|---|---|
| `zarescope_theme` | `DesignTokens` | nlohmann/json |
| `zarescope_ui` | screens, style, fonts, strings | `zarescope_theme`, Dear ImGui |
| `zarescope_platform` | Metal renderer, macOS paths | Dear ImGui's Metal backend, Cocoa |
| `Zarescope` | the `.app` bundle | all of the above |
| `zarescope_tests` | doctest tests | `zarescope_ui` |

Dependencies point one way. Nothing outside `src/platform/macos` includes an Apple header or uses `#ifdef __APPLE__`; a Linux or Windows port adds `src/platform/<os>/` with its own `Renderer` and `AppPaths`.

## Start-up

`App`'s constructor runs these steps in order. Any failure throws, and `main()` prints the reason and exits with status 1.

1. GLFW, then the window. The renderer sets its window hints first (Metal: no OpenGL context).
2. The ImGui context: keyboard navigation and docking on, no `imgui.ini` file.
3. `AppPaths`: the bundle's `Contents/Resources`.
4. Design tokens from `Resources/zarescope.tokens.json`. `DesignTokens::fromJson` refuses a file that:
   - is missing a section;
   - has a colour that isn't `#RRGGBB`;
   - defines a token in one theme and not the other;
   - maps an ImGui colour to a token that doesn't exist;
   - has a waterfall LUT that isn't exactly 256 entries.
5. Fonts from `Resources/fonts`, then the type scale (`Typography`).
6. The theme: `applyTheme` writes every `ImGuiCol` colour and the style values. ImGuiCol entries newer than the token file borrow the token of their closest neighbour (`kColorFallbacks`), so colours still come only from the tokens.
7. The GLFW and Metal backends.

Teardown runs in reverse in `App::State`'s destructor. That destructor also runs when the constructor throws halfway.

## Frame loop

- **Idle.** `glfwWaitEventsTimeout(0.25 s)` sleeps when nothing happens, so an idle window costs no CPU. It still wakes four times a second to tick the clock.
- **After input.** The loop polls for 500 ms after any input, paced by the display's vsync, so hover states and tooltips show without delay. When live data streams in (M1), the loop will draw continuously.
- **Drawing a frame.** `Renderer::renderFrame` acquires a Metal drawable, then calls back into the app for the whole ImGui frame (`NewFrame` → `MainWindow::draw` → `Render`), then encodes and presents.
- **Theme switch.** When the user switches theme, `applyTheme` runs again with the other variant before the next frame. The whole token set is swapped, so nothing from the old theme remains. `platform::setWindowAppearance` makes the native title bar follow too (on macOS, `NSWindow.appearance`).

## Fonts

- **Families.** Inter (400/500/600) and JetBrains Mono NL (400/500), static TTFs in `resources/fonts/`.
- **Glyph loading.** Dear ImGui ≥ 1.92 rasterizes glyphs on demand at the size and pixel density they're drawn at. Fonts are loaded once, with no fixed size and no glyph ranges, and stay sharp on Retina screens.
- **Thin space.** JetBrains Mono lacks U+2009 and U+202F, which the number formats need (`137.900 000 MHz`, `36 000 ft`). Those two glyphs are merged in from Inter of the same weight.
- **Glyph test.** A test checks that every glyph the design lists is present in every font, including Romanian Ș ș Ț ț with the comma below.

## UI text

All UI text comes from `ui/Strings.cpp`: one `StringId` per string, English and Romanian side by side. A `static_assert` fails the build if a row is missing or out of order, and a test rejects the cedilla forms Ş ş Ţ ţ in Romanian. M0 shows English; the language setting arrives with Settings (M6).

## Tests

`ctest --test-dir build --output-on-failure`. Each doctest case is a CTest test. ImGui runs headless (a context with no window), which is enough to check styles and fonts.

| File | Checks |
|---|---|
| `test_smoke.cpp` | the runner works; the version is `MAJOR.MINOR.PATCH` |
| `test_design_tokens.cpp` | hex parsing; anchor values from DESIGN.md; both themes define the same tokens; colour expressions; the LUT matches `waterfall-lut.csv` and its luminance rises; broken files are refused; the colour pairs the shell draws meet WCAG AA |
| `test_imgui_theme.cpp` | every style field and `ImGuiCol` maps in both themes; values land where the design says |
| `test_typography.cpp` | the fonts load; every glyph the design needs is present; every type role maps to a shipped weight |
| `test_strings_clock.cpp` | both languages are filled in; Romanian uses comma-below letters; the clock format |
