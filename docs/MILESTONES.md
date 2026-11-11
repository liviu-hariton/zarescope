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
- [x] No CI, by decision (see [DECISIONS.md](DECISIONS.md)). Build, tests and formatting are checked locally.

**Acceptance.**

- `cmake --build build` produces the app with no warnings under `-Werror`.
- The app opens in both themes, checked on screen.
- `ctest` passes 18/18.

**Left for later.**

- The native title bar follows the macOS appearance, not the app theme. Matching it needs a small platform hook.
- The language follows a setting that arrives in M6; English is shown until then.
- clang-tidy hasn't been run yet; it needs `brew install llvm`.

## M1 — Device, spectrum and waterfall

Not started. The receiver is not here yet. Most of M1 can be built and tested on synthetic signals and on recorded IQ files (`FileSource`); the live check waits for the stick.
