#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds a Zarescope.app ready to copy to another Apple Silicon Mac (M7),
# and packs it as a .zip and a .dmg. See docs/RELEASE.md.
#
#   tools/package.sh               Release build in build-release/, tests,
#                                  ad-hoc signature, checks, zip + dmg
#   tools/package.sh --skip-tests  the same without running the tests
#
# Output: build-release/dist/Zarescope-<version>-arm64.{zip,dmg}
#
# The signature is ad-hoc ("-"): enough for this Mac and, after one
# confirmation, for others. Developer ID signing and notarization are on hold
# (docs/RELEASE.md).

set -eu

cd "$(dirname "$0")/.."
root=$(pwd)
build="$root/build-release"
skip_tests=0
for arg in "$@"; do
    case "$arg" in
        --skip-tests) skip_tests=1 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

step() { printf '\n==> %s\n' "$1"; }
fail() { printf 'package: %s\n' "$1" >&2; exit 1; }

step "Configuring a Release build in build-release/"
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null

step "Building"
cmake --build "$build"

if [ "$skip_tests" -eq 0 ]; then
    step "Running the tests"
    ctest --test-dir "$build" --output-on-failure
fi

app="$build/Zarescope.app"
exe="$app/Contents/MacOS/Zarescope"
version=$(plutil -extract CFBundleShortVersionString raw "$app/Contents/Info.plist")

step "Stripping local symbols"
# Keeps the exported ones crash reports need; drops everything that only
# helps a debugger (and any build-machine paths that come with it).
strip -x "$exe"

step "Signing ad hoc, with the hardened runtime"
# Extended attributes from this machine (quarantine tags naming the app a
# file was downloaded with, Finder info) don't belong in the bundle; the
# signature doesn't use them. macOS keeps com.apple.provenance, which the
# zip and the disk image leave out below.
xattr -cr "$app" 2>/dev/null || true
# The bundle is signed as a whole, so its resources (tokens, fonts,
# catalog) are sealed too: a changed file makes the signature fail.
codesign --force --sign - --options runtime --timestamp=none \
    --entitlements "$root/cmake/Zarescope.entitlements" "$app"
codesign --verify --strict --deep --verbose=1 "$app"

step "Checking the bundle"
# 1. Only system libraries: nothing outside /usr/lib and /System.
outside=$(otool -L "$exe" | tail -n +2 | awk '{print $1}' | grep -v -e '^/usr/lib/' -e '^/System/' || true)
[ -z "$outside" ] || fail "links to libraries outside macOS: $outside"
# 2. No path from this machine left inside (the user's home or the build tree).
if strings -a "$exe" | grep -q -e "$HOME" -e "$root"; then
    fail "the executable still contains paths from this machine"
fi
# 3. Apple Silicon, and the minimum macOS from the Info.plist.
lipo -archs "$exe" | grep -qx arm64 || fail "the executable isn't arm64-only"
minimum=$(plutil -extract LSMinimumSystemVersion raw "$app/Contents/Info.plist")
# 4. The location entitlement (Settings › Use my current location).
codesign -d --entitlements - "$app" 2>/dev/null | grep -q personal-information.location ||
    fail "the location entitlement is missing"
# 5. Everything the app reads at start-up is in the bundle.
for file in zarescope.tokens.json presets.json sat_catalog.json Zarescope.icns \
            fonts/Inter-Regular.ttf fonts/JetBrainsMonoNL-Regular.ttf \
            help/guide.en.md help/guide.ro.md; do
    [ -f "$app/Contents/Resources/$file" ] || fail "missing from the bundle: $file"
done
echo "arm64, macOS $minimum or later, system libraries only, resources present"

step "Packing"
dist="$build/dist"
rm -rf "$dist"
mkdir -p "$dist"
name="Zarescope-$version-arm64"
# ditto keeps the bundle's structure and signature intact (plain zip doesn't);
# no extended attributes or resource forks go in (the signature is in the files).
ditto -c -k --norsrc --noextattr --keepParent "$app" "$dist/$name.zip"
# A disk image with the app and a link to /Applications, to drag it across.
stage="$dist/stage"
mkdir -p "$stage"
ditto --norsrc --noextattr "$app" "$stage/Zarescope.app"
ln -s /Applications "$stage/Applications"
hdiutil create -quiet -volname "Zarescope $version" -srcfolder "$stage" -format UDZO \
    -ov "$dist/$name.dmg"
rm -rf "$stage"

# Nothing from this machine travels in the zip: no AppleDouble (._) entries.
if unzip -Z1 "$dist/$name.zip" | grep -q '/\._'; then
    echo "The zip carries extended attributes (._ files)" >&2; exit 1
fi

step "Done"
ls -lh "$dist"
echo
echo "Copy the .dmg (or .zip) to the other Mac. The first time, macOS asks for"
echo "confirmation because the app isn't notarized: docs/RELEASE.md says how."
