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
4. **Sign.** Signs the whole bundle ad hoc with the hardened runtime: `codesign --sign - --options runtime --entitlements cmake/Zarescope.entitlements`. The one entitlement, `com.apple.security.personal-information.location`, lets Settings › Use my current location work under the hardened runtime. The resources (design tokens, fonts, catalog, presets) are sealed: editing any of them breaks the signature. `codesign --verify --strict --deep` must pass.
5. **Check.** It stops if any of these fails:
   - the executable links only to `/usr/lib` and `/System`;
   - it contains no path from this machine;
   - it is arm64;
   - it has the location entitlement (see below);
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
4. **Use my current location** (Settings) makes macOS ask for permission once. If the answer is no, or Location Services are off, the app says where to change it; typing the coordinates still works.
5. **The first notification** (five minutes before a pass) asks for permission. If it's refused, the countdown in the app still works.

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

## Publishing a release and the website

1. `tools/package.sh` builds and checks the disk image and the zip (above).
2. `tools/publish-release.sh` tags `v<version>` and creates the GitHub release with both files and their SHA-256 (needs the GitHub CLI, signed in: `gh auth login`).
3. `tools/deploy-site.sh` rebuilds the website, so its Download button names the new version, and copies it to zarescope.eu.

Keep this order. The site takes its version from `CMakeLists.txt`, so deploying it before the release exists makes every Download button point to a file that isn't there yet.

## The website

The site at https://zarescope.eu lives in `site/`: Astro, static, English at `/` and Romanian at `/ro/`.

- **What it reads from the repo.** The version comes from `CMakeLists.txt`, the user guide from `resources/help/guide.*.md` (the same files as the app's Help tab), and the waterfall colours from the design tokens. So it changes with the app. It loads nothing from other servers and sets no cookies.
- **Preview.** `cd site && npm install && npm run dev`, then open http://localhost:4321.
- **Screenshots.** `tools/capture-site-images.sh`, after building the app, captures the app's tabs in both languages and themes from synthetic recordings and real orbits. It runs each copy in a throwaway data folder (`ZARESCOPE_HOME`), so your own settings and journal are untouched, and captures and closes only the copy it launched.
- **Social images.** `site/public/og-en.png` and `og-ro.png` are the home page's first screen at 1200 × 630, dark. Take them again when the hero changes, under a new file name, because the server caches them for a long time.
- **Deploy, one-time setup.** Create `site/.deploy.env` (git-ignored) with `ZARESCOPE_SITE_SSH="user@server"`, an SSH login with a key. Add `ZARESCOPE_SITE_PORT` when SSH isn't on port 22, and `ZARESCOPE_SITE_PATH` when the web root isn't `web/zarescope.eu/public_html/`.
- **Deploy.** `tools/deploy-site.sh --dry-run` lists what would change. `tools/deploy-site.sh` builds, asks, and `rsync`s `site/dist/` into the HestiaCP web root. It deletes files the build no longer has, but never `.well-known`, which Let's Encrypt uses.
- **Server settings.** They live in `site/public/.htaccess`: security headers, long caching for the hashed files in `/_astro/`, the 404 page, and `ModPagespeed off`. `site/public/ro/.htaccess` gives `/ro/` its Romanian 404.
