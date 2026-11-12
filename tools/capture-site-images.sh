#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Screenshots of the app for the website (site/src/assets/screens/), in English
# and Romanian, dark and light: the spectrum (a synthetic FM-band recording), the
# aircraft (synthetic ADS-B around Bucharest), the satellites (real orbits), the
# Discover and Help tabs.
#
# Each run uses a throwaway data folder (ZARESCOPE_HOME), so the user's own
# settings and journal are never touched. The orbit cache is copied from the
# user's data folder when there is one, so CelesTrak isn't asked again.
# Only the window of the copy this script launched is captured (found by its
# process id, with screencapture -l), never the screen, and only that copy is
# closed: a Zarescope you have open yourself is left alone.
#
# Needs: cmake --build build (the app and zarescope-synth). The app's windows
# open in front for a few seconds each; leave the Mac alone meanwhile (the
# pointer is moved to the menu bar before each shot, so it can't hover).
#
# Usage: tools/capture-site-images.sh
set -euo pipefail

DIR="$(cd "$(dirname "$0")/.." && pwd)"
APP="$DIR/build/Zarescope.app/Contents/MacOS/Zarescope"
SYNTH="$DIR/build/tools/zarescope-synth"
OUT="$DIR/site/src/assets/screens"
STATION="44.4268,26.1025"   # Bucharest: the synthetic aircraft fly around it
[[ -x "$APP" && -x "$SYNTH" ]] || { echo "Build first: cmake --build build" >&2; exit 1; }

WORK="$(mktemp -d)"
CURRENT_PID=""
trap '[[ -n "$CURRENT_PID" ]] && kill "$CURRENT_PID" 2>/dev/null; rm -rf "$WORK"' EXIT
mkdir -p "$OUT"

# The window's id, for screencapture -l: the largest normal window of the
# process whose id is given (the copy this script launched, not any other).
cat > "$WORK/winid.swift" <<'SWIFT'
import CoreGraphics
let pid = Int(CommandLine.arguments[1]) ?? -1
let list = CGWindowListCopyWindowInfo([.optionAll], kCGNullWindowID) as! [[String: Any]]
var best = (0, 0.0)
for w in list where (w[kCGWindowOwnerPID as String] as? Int) == pid && (w[kCGWindowLayer as String] as? Int) == 0 {
    let b = w[kCGWindowBounds as String] as! [String: Double]
    let area = (b["Width"] ?? 0) * (b["Height"] ?? 0)
    if let n = w[kCGWindowNumber as String] as? Int, area > best.1 { best = (n, area) }
}
print(best.0)
SWIFT
swiftc -O -o "$WORK/winid" "$WORK/winid.swift"

# Parks the pointer in the menu bar, away from the app's window, so no hover
# effect or tooltip (the waterfall's "Click to listen here") gets captured.
cat > "$WORK/park.swift" <<'SWIFT'
import CoreGraphics
CGWarpMouseCursorPosition(CGPoint(x: 400, y: 4))
SWIFT
swiftc -O -o "$WORK/park" "$WORK/park.swift"

"$SYNTH" "$WORK/fm.cu8" 20 >/dev/null
"$SYNTH" "$WORK/adsb.cu8" 30 --adsb --station "$STATION" >/dev/null

USER_TLE="$HOME/Library/Application Support/Zarescope/tle_cache"

capture() {  # capture <home> <file> <app arguments…>; WAIT=seconds before the shot (7)
    local home="$1" file="$2"; shift 2
    "$WORK/park"
    ZARESCOPE_HOME="$home" "$APP" "$@" >/dev/null 2>&1 &
    local pid=$!
    CURRENT_PID=$pid
    sleep "${WAIT:-7}"
    local id; id="$("$WORK/winid" "$pid")"
    if [[ "$id" == "0" ]]; then echo "no window for $file" >&2; kill "$pid" 2>/dev/null; return 1; fi
    screencapture -x -o -l"$id" "$file"
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    CURRENT_PID=""
    echo "  $(basename "$file")"
}

for lang in en ro; do
    for theme in dark light; do
        home="$WORK/home-$lang-$theme"
        mkdir -p "$home"
        [[ -d "$USER_TLE" ]] && cp -R "$USER_TLE" "$home/tle_cache"
        cat > "$home/settings.json" <<JSON
{ "theme": "$theme", "language": "$lang", "discoverySeen": true, "minElevationDeg": 20,
  "station": { "latDeg": 44.4268, "lonDeg": 26.1025, "altitudeM": 80 } }
JSON
        echo "$lang, $theme:"
        # The waterfall needs about 25 s to fill the window.
        WAIT=25 capture "$home" "$OUT/spectrum-$lang-$theme.png" --open "$WORK/fm.cu8" --tab spectrum
        capture "$home" "$OUT/aircraft-$lang-$theme.png" --open "$WORK/adsb.cu8" --tab aircraft
        capture "$home" "$OUT/satellites-$lang-$theme.png" --tab satellites
        capture "$home" "$OUT/discover-$lang-$theme.png" --tab discover
        capture "$home" "$OUT/help-$lang-$theme.png" --tab help
    done
done
echo "Saved in site/src/assets/screens/"
