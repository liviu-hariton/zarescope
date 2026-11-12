# Releasing Zarescope (macOS)

How a `Zarescope.app` for other Macs is built, checked and installed. One command does the build:

```bash
tools/package.sh               # Release build, tests, ad-hoc signature, checks, zip + dmg
tools/package.sh --skip-tests
```

The output lands in `build-release/dist/Zarescope-<version>-arm64.zip` and `.dmg`.

## What the script does

1. **Build.** Configures and builds a Release build in `build-release/` (it never touches `build/`).
2. **Test.** Runs the tests, unless `--skip-tests`.
3. **Strip.** Strips local symbols (`strip -x`), so no debugging paths from the build machine stay in the executable.
4. **Sign.** Signs the whole bundle ad hoc with the hardened runtime: `codesign --sign - --options runtime`. The resources (design tokens, fonts, catalog, presets) are sealed: editing any of them breaks the signature. `codesign --verify --strict --deep` must pass.
5. **Check.** It stops if any of these fails:
   - the executable links only to `/usr/lib` and `/System`;
   - it contains no path from this machine;
   - it is arm64;
   - every resource read at start-up is in the bundle.
6. **Pack.**
   - The `.zip` is made with `ditto`, which keeps the signature.
   - The `.dmg` holds the app and a link to `/Applications`, to drag it across.

## Why there's nothing to embed

Every third-party library (GLFW, Dear ImGui, ImPlot, KissFFT, librtlsdr, libusb, miniaudio, sgp4, nlohmann/json) is compiled from source and linked statically. libcurl and SQLite are part of macOS. The bundle therefore carries no dylibs and needs no RPATH, and it runs on macOS 14 or later on Apple Silicon.

## Installing on another Mac

1. Open the `.dmg` and drag Zarescope to Applications (or unzip the `.zip` there).
2. **The first time.** The app is signed ad hoc, not by a Developer ID, so Gatekeeper blocks it when it came from a download or another Mac.
   - **macOS 15 and later.** Open it once; macOS refuses. Then go to System Settings → Privacy & Security, find "Zarescope was blocked", and click **Open Anyway**.
   - **macOS 14.** Right-click the app, choose Open, then Open again.
   - **Or, in Terminal**, for an app you built yourself: `xattr -dr com.apple.quarantine /Applications/Zarescope.app`.
3. **For satellite images**, install SatDump: `brew install --cask satdump`. Everything else works without it.
4. **The first notification** (five minutes before a pass) asks for permission. If it's refused, the countdown in the app still works.

## What the app writes

- **Settings.** `~/Library/Application Support/Zarescope/settings.json`.
- **The data folder.** By default the same folder; it can be changed in Settings. It holds:
  - `recordings/`
  - `products/` (decoded images)
  - `tle_cache/`
  - `journal.sqlite`
- **Uninstalling.** Delete the app and that folder.

## On hold: Developer ID signing and notarization

Decided on 2026-11-12: not now. When it is wanted, the steps are:

1. **Get a certificate.** A paid Apple Developer account, and a "Developer ID Application" certificate in the login keychain.
2. **Sign.** Use that identity instead of `-`, with a timestamp: `codesign --force --sign "Developer ID Application: <name> (<team>)" --options runtime --timestamp Zarescope.app`. The hardened runtime is already on. No entitlements are needed, because nothing is sandboxed, there's no JIT and no microphone.
3. **Notarize** the `.zip` or `.dmg` with `xcrun notarytool submit … --wait`, using an app-specific password or an API key.
4. **Staple.** Attach the ticket to the `.dmg` (and the `.app`) with `xcrun stapler staple`, so Gatekeeper accepts it offline.

`tools/package.sh` would take the identity from an environment variable and add steps 3–4. Nothing else in the build changes.
