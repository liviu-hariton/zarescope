# Zarescope — Design Language v1.0

Source of truth for the UI. Values are in `tokens/zarescope.tokens.json`. This file explains how to use them.
Visual reference: the "Zarescope — Brand & Design Language" canvas (6 artboards).

## 1. Brand

- **Name:** Zarescope ("zare" = horizon in Romanian, "scope" = instrument). **Tagline:** "Listen to the sky." **Site:** zarescope.eu
- **Personality:** precise, calm, curious, nocturnal. A friendly scientific instrument, not a military gadget or a toy.
- **Logo:** *Polar Sky*. A sky plot seen from below:
  - outer ring = the horizon (0°)
  - inner ring = 45° elevation
  - tick = north
  - the pass track starts and ends on the horizon: solid = flown, dotted = predicted
  - the dot = the satellite, with a knockout gap around it
- **Logo colours:** rings use the text colour, pass and dot use Horizon Amber.
  - Dark: `#E8EBF4` / `#FFB547`
  - Light: `#121624` / `#C06A00` (`#C06A00` is for graphics only, never for text)
- **Logo files:**
  - Below 40 px, use `symbol-small-*` (the 16-grid drawing: one ring, solid track).
  - At 40 px and above, use `symbol-*`.
  - The horizontal lockup is at least 112 px wide.
- **Don't:** rotate the logo, add rings, fill the rings, add glow or gradients, remove the knockout, or colour the pass red (red means recording).
- **App icon:** background `#0E1320`, sky dome `#161C2C`, symbol at 78%. Built on Apple's 1024 template: 824 content, 100 inset, ~185 corner.

## 2. Colour

Dark is the default theme. Every token is an opaque hex value, with no alpha stacking, so each one maps 1:1 to an `ImGuiCol_` entry.

| Role | Dark | Light | Use |
|---|---|---|---|
| bg/base | #080B14 | #F4F5F9 | window background |
| bg/surface | #0E1320 | #FFFFFF | panels, bars |
| bg/raised | #161C2C | #ECEEF4 | cards, inputs, secondary buttons |
| bg/hover | #1E2538 | #E2E5EE | hover, slider/meter tracks |
| text/primary | #E8EBF4 | #121624 | |
| text/secondary | #9AA3B8 | #4A5268 | labels, units, axes |
| text/disabled | #5C6479 | #9097A8 | disabled only (exempt from AA) |
| border/subtle · strong · input | #252C40 · #343C55 · #5C6479 | #E2E5EE · #D8DCE6 · #7D8597 | input border ≥ 3:1 |
| accent (Horizon Amber) | #FFB547 | #9A5B00 | primary action, selection, plot line, focus |
| accent hover · pressed · soft | #FFC56E · #F0A030 · #2E2414 | #864F00 · #714300 · #FBF0DF | |
| on-accent | #0B0E1A | #FFFFFF | text on accent buttons |

**Status colours.** Each status has a foreground colour and a `/soft` background colour, used together in banners and chips.

| Status | Dark fg / soft | Light fg / soft | Meaning |
|---|---|---|---|
| live | #2FD4C4 / #0F2A2D | #007274 / #E0F3F3 | receiving, device streaming |
| recording | #FF5577 / #2E1420 | #C81E4A / #FBE6EC | recording in progress (REC) |
| pass-imminent | #A99BFF / #211D3D | #5B4BC4 / #EEEBFB | a pass starts soon, recorder armed |
| success | #5CD68A / #12291D | #17773F / #E3F2E8 | image decoded, saved |
| warning | #F2D04B / #2B2812 | #7A6200 / #F6F0D6 | sample drops, low SNR |
| error | #FF6A4D / #2E1712 | #C2370F / #FBE9E3 | device lost, decode failed |

**Rules:**

- Status colour is never the only signal. Always pair it with an icon or a label: REC dot plus "REC", the LIVE chip text, the banner icon.
- Warning yellow sits close to the amber accent on the dark theme. Warning must always appear with its icon and its soft background, and the accent never sits on a soft yellow background.
- Every text pair meets WCAG AA. Measured ratios are in `docs/contrast.md`.

**Waterfall / spectrum ramp "Zare Deep":**

- Stops: `#070A12 #141B3D #20306E #2A4FA0 #2A78B8 #23A3B8 #4FC9A8 #A6E28C #F2F5C8`.
- L\* rises monotonically from 3 to 95, so the ramp reads correctly for colour-blind users.
- The ramp is the same in both themes. The waterfall stays dark even in the light theme.
- Use the ready-made 256-entry LUT, interpolated in OKLab: `tokens/waterfall-lut.csv`, also in the tokens JSON.
- Mapping: the floor (default −120 dBFS) is index 0, and the ceiling (default −20 dBFS, user-adjustable) is index 255.

## 3. Typography

- **UI font:** Inter, weights 400/500/600. Mono font: JetBrains Mono, 400/500; its digits are tabular by design.
  - Both are OFL. Download the official static TTFs from rsms.me/inter and jetbrains.com/lp/mono.
- **Glyph ranges:**
  - Basic Latin, Latin-1, Latin Extended-A
  - U+0218–U+021B: Romanian Ș ș Ț ț, **comma-below**, never the cedilla forms
  - Symbols: U+2009 thin space, U+2212 minus, U+2013–2014 dashes, U+00B0 degree sign, U+2191–2193 arrows
- **Wordmark:** Space Grotesk 500. It is already outlined in the SVGs; do not load it in the app.

| Token | Font | Size/line (px) | Use |
|---|---|---|---|
| display | Inter 600 | 32/38, −1% | onboarding, empty states |
| title | Inter 600 | 22/28 | dialog titles |
| heading | Inter 600 | 17/24 | panel and card headings |
| body | Inter 400 | 15/22 | **default** |
| label | Inter 500 | 14/18 | buttons, field labels, tabs |
| caption | Inter 500 | 12/16, +6%, UPPERCASE | overlines, column headers |
| mono/freq | JB Mono 500 | 40/44 | frequency display |
| mono/freq-compact | JB Mono 500 | 30/34 | frequency in the top bar |
| mono/data | JB Mono 400 | 14/20 | table numbers, coordinates, timers |
| mono/small | JB Mono 400 | 12/16 | axis ticks |

**Size (revised 2026-11-12).** The first scale (body 13, caption 11) read too small on a 27-inch 1440p monitor at 1×, and on a 13-inch laptop. Every role moved up one clear step, and controls and rows grew to match. The Settings › Text size choice (Standard 100 %, Large 110 %, Larger 120 %) scales the whole interface on top of this, so text, controls and spacing keep their proportions.

**Number formats:**

- Frequency: `137.900 000 MHz`. MHz with 6 decimals, a thin space after the kHz group. Leading digits use text/primary, trailing zeros use text/secondary.
- Countdown: `T−04:32`, with the true minus sign.
- Altitude: `36 000 ft`, with a thin space.
- Other units: `452 kt`, `18.4 km`, `67°`, `−42.3 dBFS`.
- Units are always rendered in text/secondary.

**Retina:** load fonts at size × framebuffer scale and set `FontGlobalScale = 1/scale`.

## 4. Spacing, shape, elevation

- **Spacing (4-px base):** 2, 4, 8, 12, 16, 24, 32, 48.
  - Panel padding is 16, item spacing is 8, section gaps are 24.
- **Sizes:**
  - Controls: 36 tall (compact: 28). Table rows: 44 (compact: 40).
  - Hit area: at least 44 for standalone controls.
  - Table rows: 40 (compact: 36).
- **Radius:**
  - xs 3: meter segments, slider grab
  - sm 4: inputs, checkbox
  - md 6: buttons, tabs
  - lg 10: cards, panels
  - xl 14: dialogs
  - pill: toggles, chips
- **Borders:** 1 px. The focus ring is 2 px in focus-ring colour, offset 2 px, and shows only for keyboard navigation.
- **Elevation:** one shadow, for popups and menus only. If it can't be drawn, use a border/strong outline instead.
  - No gradients, no blur, no translucent layering, no elaborate animation.
- **Motif ("horizon rule"):** a 2-px accent line under the active tab, echoing the logo's horizon.

## 5. Components (see artboard 5)

- **Frequency display:**
  - mono/freq digits; each digit is its own hit target.
  - Scroll or ↑↓ over a digit changes that decimal place; typing enters a new value.
  - The focused digit gets a 2-px accent underline.
  - A LIVE chip sits to the right.
- **Signal meter:**
  - 30 segments (24 in the top bar), 3-px gap, radius xs.
  - Lit segments use `live`, unlit use `bg/hover`, and the peak-hold segment uses text/primary.
  - Scale labels underneath: S1…S9, +20, +40.
  - Readout: `−42.3 dBFS`, plus SNR, peak and noise.
- **Next-pass card:**
  - Caption "NEXT PASS" in pass-imminent colour, then the satellite name (heading) and frequency/mode (mono/data).
  - Countdown `T−MM:SS` in 32-px mono, in pass-imminent colour.
  - A 6-px progress bar showing time until AOS.
  - Three tiles: Max el, AOS (time · azimuth), LOS.
- **Pass polar plot:**
  - Rings at 0°, 30° and 60°, with N/E/S/W labels.
  - The track is accent-coloured: solid for flown, dotted (0.1/6) for predicted.
  - Satellite dot is accent with a knockout gap; there is a marker at AOS and a label at max elevation.
  - The plot is north-up with east on the right.
- **Aircraft row:**
  - Columns: heading arrow (rotated), callsign, altitude (ft), speed (kt), distance (km).
  - Numbers are right-aligned mono, units in secondary.
  - Selected row uses accent/soft; stale rows (more than 30 s since the last update) use text/disabled.
- **Status banner:**
  - 48 px tall, soft background with a 1-px border in the status colour.
  - Layout: icon or dot, the message, an optional progress bar (4 px) and an optional action.
  - Recording example: `● REC  Recording Meteor-M N2-4 — 7 min left  [progress]  [■ Stop recording]`.
- **Buttons:**
  - Primary: accent background, on-accent text.
  - Secondary: bg/raised background, border/strong border, text/primary.
  - Danger: error background, on-error text.
  - Disabled: text/disabled.
  - Labels use the label style; icons are 16 px with an 8-px gap.
- **Toggle:** 36×20 pill with a 14-px knob. On: accent background, on-accent knob. Off: bg/hover background, border/input border, text/secondary knob.
- **Slider:** 4-px track (bg/hover, filled part in accent) with a 14×14 grab at radius 3. The value is shown in the label row.
- **Text field:** 36 tall, bg/raised, 1-px border/input, radius 4. Units are a mono suffix.
  - Error state: border in error colour, plus an error message underneath.
- **Tabs:** label style with a 16-px icon. The active tab gets text/primary and the 2-px accent underline; inactive tabs use text/secondary.

**Icons:**

- Line icons on a 24 grid, 1.75 stroke, round caps and joins, drawn in `currentColor`. Use them at 16, 20 or 24 px.
- Set: spectrum, aircraft, satellite, log, settings, record, antenna, pass, stop, alert, check-circle, heading-arrow.
- For ImGui, rasterize them into the font atlas in merge mode, or into a texture atlas at 2×.

## 6. Main window (artboard 6, 1440×900)

From top to bottom:

1. **Native title bar:** 28 px.
2. **Top bar (64):** logo (small symbol, 28) · frequency (mono/freq-compact) · mode segmented control · compact meter · gain · spacer · device chip · LIVE chip · settings button.
3. **Recording banner (44):** full width, shown only while recording.
4. **Body.** The main area on the left holds, from top to bottom:
   - the tabs (Spectrum / Aircraft map / Satellites / Log) with FFT info right-aligned
   - the spectrum plot (210 px, accent line and accent/soft fill, dashed marker at the tuned frequency)
   - the waterfall, filling the remaining height, with time labels on the left
   - the frequency axis

   On the right, a 420-px side panel holds Passes (live polar plot plus the list) and Aircraft (table).
5. **Status bar (28):** mono/small text — device states, coordinates, drops/CPU, UTC and local time.

## 7. Localisation (EN / RO)

- Leave about 30% slack for Romanian. For example, "Recording Meteor-M N2-4 — 7 min left" becomes "Înregistrare Meteor-M N2-4 — 7 min rămase".
- Never use fixed-width buttons for text labels. Let them grow, and truncate with an ellipsis only in banners and table cells.
- Times and numbers are not translated. Unit labels stay SI or aviation (ft, kt).

## 8. Accessibility

- Text contrast is AA or better in both themes, and non-text UI is at least 3:1.
- Everything is reachable with the keyboard, with a visible focus ring.
- Status is never conveyed by colour alone.
- The waterfall ramp is safe for colour vision deficiency.
- Hit targets are at least 44 px where a control stands alone.
