#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Publishes the disk image built by tools/package.sh as a GitHub release, tagged
# v<version>, so the website's download button (which links to
# releases/download/v<version>/Zarescope-<version>-arm64.dmg) works. Needs the
# GitHub CLI, signed in once: brew install gh && gh auth login
#
# Usage: tools/publish-release.sh [--yes]   (after tools/package.sh)
# Then deploy the site, so it names the new version: tools/deploy-site.sh
set -euo pipefail

DIR="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(tr '\n' ' ' < "$DIR/CMakeLists.txt" | sed -nE 's/.*project\(Zarescope[[:space:]]+VERSION[[:space:]]+([0-9.]+).*/\1/p')"
DIST="$DIR/build-release/dist"
DMG="$DIST/Zarescope-$VERSION-arm64.dmg"
ZIP="$DIST/Zarescope-$VERSION-arm64.zip"
TAG="v$VERSION"

command -v gh >/dev/null || { echo "Install the GitHub CLI: brew install gh && gh auth login" >&2; exit 1; }
[[ -f "$DMG" && -f "$ZIP" ]] || { echo "No $DMG: run tools/package.sh first." >&2; exit 1; }
[[ -z "$(git -C "$DIR" status --porcelain --untracked-files=no)" ]] || { echo "Commit your changes first." >&2; exit 1; }

SHA_DMG="$(shasum -a 256 "$DMG" | cut -d' ' -f1)"
SHA_ZIP="$(shasum -a 256 "$ZIP" | cut -d' ' -f1)"
NOTES="Zarescope $VERSION for macOS 14 (Sonoma) or later, on Apple Silicon.

An early version: tested on recordings and simulated signals, with live tests under way.

The app isn't notarized yet: the first time, open it, then go to System Settings → Privacy & Security and click Open Anyway. Details: https://zarescope.eu/download/

For satellite pictures, install SatDump separately: \`brew install --cask satdump\`.

SHA-256:
- \`$SHA_DMG\`  Zarescope-$VERSION-arm64.dmg
- \`$SHA_ZIP\`  Zarescope-$VERSION-arm64.zip"

echo "Publishing $TAG with $(basename "$DMG") and $(basename "$ZIP")"
if [[ "${1:-}" != "--yes" ]]; then
    read -r -p "Continue? [y/N] " answer
    [[ "$answer" == "y" || "$answer" == "Y" ]] || { echo "Cancelled."; exit 0; }
fi

# The tag points at the commit the disk image was built from.
git -C "$DIR" tag -a "$TAG" -m "Zarescope $VERSION" 2>/dev/null || echo "Tag $TAG already exists; using it."
git -C "$DIR" push origin "$TAG"
gh release create "$TAG" "$DMG" "$ZIP" --repo liviu-hariton/zarescope --title "Zarescope $VERSION" --notes "$NOTES" --latest
echo "Published: https://github.com/liviu-hariton/zarescope/releases/tag/$TAG"
