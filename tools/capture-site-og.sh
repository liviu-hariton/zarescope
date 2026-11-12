#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The social cards (site/public/og-en.png, og-ro.png): the home page's first
# screen at 1200 × 630, dark, as a card: the brand, the headline, the lede and
# the sky, without the site's controls (links, theme and language buttons,
# Download), which can't be clicked on a card. Needs Node and Google Chrome.
#
# The server caches these files for a long time: when the hero changes, save
# them under new names and update Base.astro (docs/RELEASE.md, Caching).
#
# Usage: tools/capture-site-og.sh
set -euo pipefail

DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$DIR/site"
[[ -d node_modules ]] || npm ci
npm run build >/dev/null
# Astro's preview server runs as one background service: stop any that is
# already running (it would hold its own port), start ours, stop it at the end.
npx astro preview stop >/dev/null 2>&1 || true
npx astro preview --port 4330 >/dev/null 2>&1 &
trap 'npx astro preview stop >/dev/null 2>&1 || true' EXIT
up=""
for _ in $(seq 1 120); do curl -s -o /dev/null http://localhost:4330/ && { up=1; break; }; sleep 0.25; done
[[ -n "$up" ]] || { echo "The preview server didn't start on port 4330." >&2; exit 1; }

CARD_CSS='.top nav, .top .tools, .hero .get, .to-top { display: none !important; } .top { border-bottom-color: transparent; }'
node "$DIR/tools/site-shot.mjs" http://localhost:4330/ "$DIR/site/public/og-en.png" 1200 630 0 0 dark "$CARD_CSS"
node "$DIR/tools/site-shot.mjs" http://localhost:4330/ro/ "$DIR/site/public/og-ro.png" 1200 630 0 0 dark "$CARD_CSS"
echo "Saved site/public/og-en.png and og-ro.png"
